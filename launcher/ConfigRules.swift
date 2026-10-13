import Foundation

enum ConfigRuleStatus: String, Codable, Sendable { case ok, warning, violation, unknown, conflict }
enum ConfigRuleEnforcement: String, Codable, Sendable { case code, modScript, paper }

struct ConfigRuleSource: Codable, Equatable, Sendable {
    let sourceURL: String
    let sourceDate: String
    let quoteReference: String
    let quote: String
    let enforcedBy: ConfigRuleEnforcement
    let archiveTimestamp: String?
}

struct ConfigRuleConstraint: Codable, Equatable, Sendable {
    enum Kind: String, Codable, Sendable { case range, exact, forbiddenValues, disabledCommand, ownership }
    let kind: Kind
    let minimum: Double?
    let maximum: Double?
    let value: String?
    let forbiddenValues: [String]?
    let resetValue: String?
    let integer: Bool?
}

struct ConfigSettingRule: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let setting: String
    let constraint: ConfigRuleConstraint
    let serverOwned: Bool
    let advisory: Bool
    let reason: String
    let sources: [ConfigRuleSource]
}

struct ConfigRuleset: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let title: String
    let version: String
    let mode: ConfigCatalogMode
    let baseID: String?
    let scope: String
    let rules: [ConfigSettingRule]
    let sources: [ConfigRuleSource]
}

struct ConfigRuleConflict: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let setting: String
    let rulesetIDs: [String]
    let reason: String
    let sources: [ConfigRuleSource]
}

struct ConfigRuleEvaluation: Equatable, Sendable {
    let setting: String
    let value: String?
    let status: ConfigRuleStatus
    let reason: String
    let serverOwned: Bool
    let sources: [ConfigRuleSource]
}

enum ConfigRulesError: Error, Equatable, Sendable {
    case missingResource, unsupportedSchema(Int), invalidResource(String)
}

struct ConfigRules: Sendable {
    let schemaVersion: Int
    let version: String
    let researchedOn: String
    let rulesets: [ConfigRuleset]
    let conflicts: [ConfigRuleConflict]
    private struct Resource: Decodable {
        let schemaVersion: Int
        let version: String
        let researchedOn: String
        let rulesets: [ConfigRuleset]
        let conflicts: [ConfigRuleConflict]
    }
    init(data: Data) throws {
        let resource = try JSONDecoder().decode(Resource.self, from: data)
        schemaVersion = resource.schemaVersion
        version = resource.version
        researchedOn = resource.researchedOn
        rulesets = resource.rulesets
        conflicts = resource.conflicts
        guard schemaVersion == 1 else { throw ConfigRulesError.unsupportedSchema(schemaVersion) }
        guard !version.isEmpty, !researchedOn.isEmpty, !rulesets.isEmpty,
              Set(rulesets.map(\.id)).count == rulesets.count,
              Set(conflicts.map(\.id)).count == conflicts.count else { throw ConfigRulesError.invalidResource("version or identities") }
        for policy in rulesets {
            guard !policy.id.isEmpty, !policy.title.isEmpty, !policy.version.isEmpty, !policy.scope.isEmpty,
                  Self.validSources(policy.sources), Set(policy.rules.map { $0.setting.lowercased() }).count == policy.rules.count,
                  Set(policy.rules.map(\.id)).count == policy.rules.count else { throw ConfigRulesError.invalidResource(policy.id) }
            var visited: Set<String> = [policy.id], base = policy.baseID
            while let id = base {
                guard visited.insert(id).inserted, let parent = ruleset(id: id), parent.mode == policy.mode else {
                    throw ConfigRulesError.invalidResource(policy.id)
                }
                base = parent.baseID
            }
            for rule in policy.rules {
                let c = rule.constraint
                guard !rule.id.isEmpty, !rule.setting.isEmpty, !rule.reason.isEmpty, Self.validSources(rule.sources) else {
                    throw ConfigRulesError.invalidResource(rule.id)
                }
                switch c.kind {
                case .range:
                    guard let min = c.minimum, let max = c.maximum, min.isFinite, max.isFinite, min <= max,
                          c.integer != true || (min.rounded() == min && max.rounded() == max),
                          c.resetValue.map({ Self.number($0).map { (min...max).contains($0) } == true }) != false else {
                        throw ConfigRulesError.invalidResource(rule.id)
                    }
                case .exact: guard !(c.value ?? "").isEmpty else { throw ConfigRulesError.invalidResource(rule.id) }
                case .forbiddenValues: guard !(c.forbiddenValues ?? []).isEmpty else { throw ConfigRulesError.invalidResource(rule.id) }
                case .ownership: guard rule.serverOwned else { throw ConfigRulesError.invalidResource(rule.id) }
                case .disabledCommand: break
                }
            }
        }
        for conflict in conflicts {
            guard !conflict.id.isEmpty, !conflict.setting.isEmpty, !conflict.reason.isEmpty,
                  conflict.rulesetIDs.count >= 2, conflict.rulesetIDs.allSatisfy({ ruleset(id: $0) != nil }),
                  Self.validSources(conflict.sources) else { throw ConfigRulesError.invalidResource(conflict.id) }
        }
    }
    private static func validSources(_ sources: [ConfigRuleSource]) -> Bool {
        !sources.isEmpty && sources.allSatisfy {
            URL(string: $0.sourceURL)?.scheme != nil && !$0.sourceDate.isEmpty && !$0.quoteReference.isEmpty && !$0.quote.isEmpty
        }
    }
    static func load(bundle: Bundle = .main) throws -> Self {
        guard let url = bundle.url(forResource: "ConfigRulesets", withExtension: "json") ??
                bundle.url(forResource: "ConfigRulesets", withExtension: "json", subdirectory: "Resources") else {
            throw ConfigRulesError.missingResource
        }
        return try Self(data: Data(contentsOf: url))
    }
    func ruleset(id: String) -> ConfigRuleset? { rulesets.first { $0.id == id } }
    func resolvedRules(id: String) -> [ConfigSettingRule] {
        guard let policy = ruleset(id: id) else { return [] }
        let inherited = policy.baseID.map { resolvedRules(id: $0) } ?? []
        let overridden = Set(policy.rules.map { $0.setting.lowercased() })
        return inherited.filter { !overridden.contains($0.setting.lowercased()) } + policy.rules
    }
    func knownSelection(_ ids: [String], mode: ConfigCatalogMode) -> Bool {
        !ids.isEmpty && ids.allSatisfy { ruleset(id: $0)?.mode == mode }
    }
    func relevantConflicts(rulesetIDs: [String]) -> [ConfigRuleConflict] {
        conflicts.filter { !Set($0.rulesetIDs).isDisjoint(with: rulesetIDs) }
    }
    func evaluate(setting: String, value: String?, rulesetIDs: [String], mode: ConfigCatalogMode) -> ConfigRuleEvaluation {
        let matched = rulesetIDs.flatMap { resolvedRules(id: $0) }.filter { $0.setting.lowercased() == setting.lowercased() }
        let sources = matched.flatMap(\.sources), owned = matched.contains { $0.serverOwned }
        func result(_ status: ConfigRuleStatus, _ reason: String) -> ConfigRuleEvaluation {
            ConfigRuleEvaluation(setting: setting, value: value, status: status, reason: reason, serverOwned: owned, sources: sources)
        }
        guard knownSelection(rulesetIDs, mode: mode), !matched.isEmpty else {
            return result(.unknown, "No verified rule for this setting and target. Compatibility is not assessed.")
        }
        if let conflict = conflicts.first(where: {
            $0.setting.lowercased() == setting.lowercased() && Set($0.rulesetIDs).isSubset(of: rulesetIDs)
        }) { return result(.conflict, conflict.reason) }
        guard let value else { return result(.unknown, "No direct value is available. Runtime or script state is not assessed.") }
        let constraints = matched.filter { $0.constraint.kind != .ownership }
        guard !constraints.isEmpty else { return result(.unknown, "This setting is controlled by the server; its runtime value is not assessed.") }
        if let rule = constraints.first(where: { !$0.advisory && !Self.accepts(value, constraint: $0.constraint) }) {
            return result(.violation, rule.reason)
        }
        if let rule = constraints.first(where: { $0.advisory }) { return result(.warning, rule.reason) }
        return result(.ok, "Matches the cited setting rules. " + constraints.map(\.reason).joined(separator: " "))
    }
    func isServerOwned(_ setting: String, rulesetIDs: [String]) -> Bool {
        rulesetIDs.flatMap { resolvedRules(id: $0) }.contains { $0.setting.lowercased() == setting.lowercased() && $0.serverOwned }
    }
    static func number(_ value: String) -> Double? {
        let text = value.trimmingCharacters(in: .whitespaces)
        // Scientific notation must not turn atoi("1.25e2") into an apparently
        // legal integer 125. Advice accepts plain decimal config values only.
        guard text.range(of: "^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)$", options: .regularExpression) != nil,
              let n = Double(text), n.isFinite else { return nil }
        return n
    }
    static func valuesEqual(_ left: String, _ right: String) -> Bool {
        if left == right { return true }
        let a = left.split(whereSeparator: \.isWhitespace), b = right.split(whereSeparator: \.isWhitespace)
        return !a.isEmpty && a.count == b.count && zip(a, b).allSatisfy {
            guard let x = number(String($0)), let y = number(String($1)) else { return false }
            return x == y
        }
    }
    private static func accepts(_ value: String, constraint c: ConfigRuleConstraint) -> Bool {
        switch c.kind {
        case .range:
            guard let n = number(value), let min = c.minimum, let max = c.maximum else { return false }
            return (min...max).contains(n) && (c.integer != true || n.rounded() == n)
        case .exact: return c.value.map { valuesEqual(value, $0) } == true
        case .forbiddenValues: return !(c.forbiddenValues ?? []).contains { valuesEqual(value, $0) }
        case .disabledCommand: return false
        case .ownership: return true
        }
    }
}
