import Foundation

enum ConfigTuneFor: String, Codable, CaseIterable, Sendable { case competitive, balanced, bestLooking }
enum ConfigCheckupArea: String, Codable, CaseIterable, Sendable { case movement, aim, connection, visibility }

struct ConfigCheckupMeasurements: Equatable, Sendable {
    var frameTimeP99MS: Double? = nil
    var mousePollingHz: Double? = nil
    var pingMS: Double? = nil
    var packetLossPercent: Double? = nil
    var source: String? = nil
}
struct ConfigCheckupDial: Equatable, Sendable {
    let area: ConfigCheckupArea
    let value: Double?
    let unit: String?
    let source: String?
    var statusText: String { value == nil ? "Not measured" : "Measured" }
}
struct ConfigCheckupChange: Equatable, Sendable {
    let setting: String
    let oldValue: String?
    let newValue: String
}
struct ConfigCheckupEvidence: Equatable, Sendable {
    enum Kind: String, Sendable { case configuration, policy, inference, measurement }
    let kind: Kind
    let detail: String
}
struct ConfigCheckupSuggestion: Equatable, Sendable, Identifiable {
    let id: String
    let area: ConfigCheckupArea
    let title: String
    let why: String
    let notice: String
    let changes: [ConfigCheckupChange]
    let evidence: [ConfigCheckupEvidence]
    let sources: [ConfigRuleSource]
}
struct ConfigOptimizerRule: Codable, Equatable, Sendable, Identifiable {
    enum Predicate: String, Codable, Sendable { case outsideCompetitiveBand, different, greaterThan }
    let id: String
    let area: ConfigCheckupArea
    let when: Predicate
    let setting: String
    let suggest: [String: String]
    let additionalChanges: [ConfigOptimizerChange]?
    let title: String
    let why: String
    let notice: String
    let evidence: String
    let sources: [ConfigRuleSource]
    let tuneFor: [ConfigTuneFor]
}
struct ConfigOptimizerChange: Codable, Equatable, Sendable {
    let setting: String
    let suggest: [String: String]
}
struct ConfigOptimizer: Sendable {
    let schemaVersion: Int
    let version: String
    let rules: [ConfigOptimizerRule]
    private struct Resource: Decodable {
        let schemaVersion: Int
        let version: String
        let rules: [ConfigOptimizerRule]
    }
    init(data: Data) throws {
        let resource = try JSONDecoder().decode(Resource.self, from: data)
        schemaVersion = resource.schemaVersion
        version = resource.version
        rules = resource.rules
        guard schemaVersion == 1, !version.isEmpty, !rules.isEmpty,
              Set(rules.map(\.id)).count == rules.count else { throw ConfigCheckupError.invalidOptimizer }
        for rule in rules {
            guard !rule.id.isEmpty, !rule.title.isEmpty, !rule.why.isEmpty, !rule.notice.isEmpty,
                  !rule.evidence.isEmpty, !rule.setting.isEmpty, !rule.tuneFor.isEmpty,
                  !rule.suggest.isEmpty, Set(rule.suggest.keys).isSubset(of: Set(ConfigCatalogMode.allCases.map(\.rawValue))),
                  rule.suggest.values.allSatisfy({ ConfigRules.number($0) != nil }),
                  !rule.sources.isEmpty, rule.sources.allSatisfy({ URL(string: $0.sourceURL)?.scheme != nil && !$0.sourceDate.isEmpty && !$0.quoteReference.isEmpty && !$0.quote.isEmpty }) else {
                throw ConfigCheckupError.invalidOptimizer
            }
            let changes = rule.additionalChanges ?? []
            guard Set(([rule.setting] + changes.map(\.setting)).map { $0.lowercased() }).count == changes.count + 1,
                  changes.allSatisfy({ !$0.setting.isEmpty && !$0.suggest.isEmpty &&
                      Set($0.suggest.keys).isSubset(of: Set(ConfigCatalogMode.allCases.map(\.rawValue))) &&
                      $0.suggest.values.allSatisfy({ ConfigRules.number($0) != nil }) }) else { throw ConfigCheckupError.invalidOptimizer }
        }
    }
    static func load(bundle: Bundle = .main) throws -> Self {
        guard let url = bundle.url(forResource: "ConfigOptimizer", withExtension: "json") ??
                bundle.url(forResource: "ConfigOptimizer", withExtension: "json", subdirectory: "Resources") else {
            throw ConfigCheckupError.missingResource
        }
        return try Self(data: Data(contentsOf: url))
    }
}
enum ConfigCheckupError: Error, Equatable, Sendable {
    case missingResource, invalidOptimizer, emptyAction, staleSuggestion, unknownPolicy, protectedSetting(String)
}
struct ConfigCheckupReport: Sendable {
    let profileID: String
    let expectedVersion: ConfigProfileVersion
    let rulesetIDs: [String]
    let tuneFor: ConfigTuneFor
    let mode: ConfigCatalogMode
    let suggestions: [ConfigCheckupSuggestion]
    let dials: [ConfigCheckupDial]
    let policy: [ConfigRuleEvaluation]
    let limitations: [String]
}
struct ConfigCheckupReceipt: Sendable {
    let profileID: String
    let historyEntry: ConfigHistoryEntry
    let afterVersion: ConfigProfileVersion
    let changes: [ConfigCheckupChange]
}

/// Pure review plus explicit persistence through the existing profile store.
struct ConfigCheckup: Sendable {
    let catalog: ConfigCatalog
    let rules: ConfigRules
    let optimizer: ConfigOptimizer
    func evaluate(profile: ConfigProfileRead, rulesetIDs: [String], tuneFor: ConfigTuneFor,
                  targetMode: ConfigCatalogMode? = nil, measurements: ConfigCheckupMeasurements = .init(),
                  dismissedSuggestionIDs: Set<String> = []) -> ConfigCheckupReport {
        let mode: ConfigCatalogMode = targetMode ?? (profile.document.dialect == .stock ? .stock : .cod2x)
        let known = rules.knownSelection(rulesetIDs, mode: mode)
        let unresolved = Self.unresolved(profile.document)
        var limitations = ["Advice uses direct config values and cited code. Performance and compatibility beyond the listed rules are not assessed."]
        if !known { limitations.append("Unknown rules or a client-mode mismatch: compatibility is not assessed.") }
        if unresolved { limitations.append("Scripts, external configs or parser diagnostics prevent a complete static review. Resolve them before applying advice.") }
        if measurements.packetLossPercent == nil { limitations.append("Packet loss: Not measured.") }
        let locks = Set(profile.metadata.locks.map { $0.lowercased() })
        let suggestions = known && !unresolved ? optimizer.rules.compactMap { rule -> ConfigCheckupSuggestion? in
            guard rule.tuneFor.contains(tuneFor), !dismissedSuggestionIDs.contains(rule.id),
                  !locks.contains(rule.setting.lowercased()),
                  !rules.isServerOwned(rule.setting, rulesetIDs: rulesetIDs),
                  let setting = catalog.setting(named: rule.setting), setting.isEditable(in: mode),
                  setting.variant(for: mode)?.serverControlled == false,
                  let proposed = rule.suggest[mode.rawValue], Self.validValue(proposed, setting: setting, mode: mode),
                  let current = profile.document.effectiveSetting(named: rule.setting), let number = ConfigRules.number(current),
                  !ConfigRules.valuesEqual(current, proposed),
                  rules.evaluate(setting: rule.setting, value: proposed, rulesetIDs: rulesetIDs, mode: mode).status == .ok else { return nil }
            switch rule.when {
            case .different: break
            case .greaterThan: guard number > (ConfigRules.number(proposed) ?? number) else { return nil }
            case .outsideCompetitiveBand:
                let band = rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.first {
                    $0.setting.lowercased() == rule.setting.lowercased() && $0.constraint.kind == .range && $0.constraint.maximum == 250
                }
                guard let band, let min = band.constraint.minimum, let max = band.constraint.maximum,
                      number < min || number > max || number.rounded() != number else { return nil }
            }
            var changes = [ConfigCheckupChange(setting: rule.setting, oldValue: current, newValue: proposed)]
            for extra in rule.additionalChanges ?? [] {
                guard let proposedExtra = extra.suggest[mode.rawValue] else { return nil }
                let old = profile.document.effectiveSetting(named: extra.setting)
                if let old, ConfigRules.valuesEqual(old, proposedExtra) { continue }
                guard !locks.contains(extra.setting.lowercased()), !rules.isServerOwned(extra.setting, rulesetIDs: rulesetIDs),
                      let entry = catalog.setting(named: extra.setting), entry.isEditable(in: mode),
                      entry.variant(for: mode)?.serverControlled == false, Self.validValue(proposedExtra, setting: entry, mode: mode),
                      rules.evaluate(setting: extra.setting, value: proposedExtra, rulesetIDs: rulesetIDs, mode: mode).status == .ok else { return nil }
                changes.append(.init(setting: extra.setting, oldValue: old, newValue: proposedExtra))
            }
            return ConfigCheckupSuggestion(id: rule.id, area: rule.area, title: rule.title, why: rule.why, notice: rule.notice,
                                           changes: changes,
                                           evidence: changes.map { .init(kind: .configuration, detail: "\($0.setting) = \($0.oldValue ?? "not present")") } + [
                                                      .init(kind: .policy, detail: rulesetIDs.joined(separator: ", ")),
                                                      .init(kind: .inference, detail: rule.evidence)], sources: rule.sources)
        } : []
        let names = Set(rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.map { $0.setting.lowercased() })
        let policy = names.sorted().map { name in
            let disabled = rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.contains { $0.setting.lowercased() == name && $0.constraint.kind == .disabledCommand }
            let value = disabled ? (Self.usesCommand(name, document: profile.document) ? "used" : nil) : profile.document.effectiveSetting(named: name)
            let evaluation = rules.evaluate(setting: name, value: value, rulesetIDs: rulesetIDs, mode: mode)
            if unresolved && evaluation.status != .violation && evaluation.status != .conflict {
                return ConfigRuleEvaluation(setting: name, value: value, status: .unknown,
                                            reason: "Only direct assignments are visible; unresolved scripts or parser diagnostics prevent a policy judgment.",
                                            serverOwned: evaluation.serverOwned, sources: evaluation.sources)
            }
            return evaluation
        }
        return ConfigCheckupReport(profileID: profile.profile.id, expectedVersion: profile.version, rulesetIDs: rulesetIDs,
                                   tuneFor: tuneFor, mode: mode, suggestions: suggestions, dials: Self.dials(measurements),
                                   policy: policy, limitations: limitations)
    }
    func apply(report: ConfigCheckupReport, suggestionIDs: Set<String>, store: ConfigProfileStore,
               runningGame: Bool = false) throws -> ConfigCheckupReceipt {
        guard !runningGame else { throw ConfigProfileError.gameRunning }
        guard !suggestionIDs.isEmpty else { throw ConfigCheckupError.emptyAction }
        guard rules.knownSelection(report.rulesetIDs, mode: report.mode) else { throw ConfigCheckupError.unknownPolicy }
        let current = try store.read(id: report.profileID)
        guard current.version == report.expectedVersion else { throw ConfigProfileError.conflict }
        let reviewed = evaluate(profile: current, rulesetIDs: report.rulesetIDs, tuneFor: report.tuneFor, targetMode: report.mode)
        guard reviewed.mode == report.mode else { throw ConfigCheckupError.staleSuggestion }
        let chosen = reviewed.suggestions.filter { suggestionIDs.contains($0.id) }
        guard chosen.count == suggestionIDs.count, chosen.allSatisfy({ suggestion in report.suggestions.contains(suggestion) }) else {
            throw ConfigCheckupError.staleSuggestion
        }
        let changes = chosen.flatMap(\.changes)
        guard Set(changes.map { $0.setting.lowercased() }).count == changes.count else { throw ConfigCheckupError.staleSuggestion }
        var document = current.document
        for change in changes { try document.setSetting(named: change.setting, value: change.newValue) }
        let entry = try store.update(id: report.profileID, document: document, expected: current.version,
                                     source: .checkup, summary: chosen.map(\.title).joined(separator: "; "), runningGame: runningGame)
        return ConfigCheckupReceipt(profileID: report.profileID, historyEntry: entry,
                                    afterVersion: ConfigProfileVersion(config: entry.data, metadata: entry.metadata), changes: changes)
    }
    func undo(receipt: ConfigCheckupReceipt, rulesetIDs: [String], store: ConfigProfileStore,
              targetMode: ConfigCatalogMode? = nil, runningGame: Bool = false) throws -> ConfigHistoryEntry {
        guard !runningGame else { throw ConfigProfileError.gameRunning }
        let current = try store.read(id: receipt.profileID)
        guard current.version == receipt.afterVersion else { throw ConfigProfileError.conflict }
        let mode: ConfigCatalogMode = targetMode ?? (store.dialect == .stock ? .stock : .cod2x)
        guard rules.knownSelection(rulesetIDs, mode: mode) else { throw ConfigCheckupError.unknownPolicy }
        let locks = Set(current.metadata.locks.map { $0.lowercased() })
        for change in receipt.changes {
            if locks.contains(change.setting.lowercased()) || rules.isServerOwned(change.setting, rulesetIDs: rulesetIDs) ||
                catalog.setting(named: change.setting)?.isEditable(in: mode) != true {
                throw ConfigCheckupError.protectedSetting(change.setting)
            }
        }
        // Restore exact pre-Apply bytes, including pre-existing rule violations.
        // Disk/metadata revision checks prevent undoing over any later edit.
        return try store.restore(id: receipt.profileID, snapshotID: receipt.historyEntry.id, expected: current.version,
                                 revision: .before, runningGame: runningGame)
    }
    private static func validValue(_ value: String, setting: ConfigCatalogSetting, mode: ConfigCatalogMode) -> Bool {
        guard let variant = setting.variant(for: mode), let type = variant.type, let n = ConfigRules.number(value) else { return false }
        switch type {
        case .bool: return n == 0 || n == 1
        case .int: guard n.rounded() == n else { return false }
        case .float: break
        default: return false
        }
        for (fact, lower) in [(variant.minimum, true), (variant.maximum, false)] {
            guard let fact, fact.status == .literal, case .number(let bound) = fact.value else { return false }
            if lower ? n < bound : n > bound { return false }
        }
        return true
    }
    private static func bindingDocuments(_ document: ConfigDocument) -> [ConfigDocument] {
        let keys = Set(document.commands.filter { $0.kind == .bind }.compactMap { $0.tokens.count > 1 ? $0.tokens[1].text : nil })
        return keys.compactMap { key in
            document.effectiveBindBytes(key: key).map { ConfigDocument(data: $0, dialect: document.dialect) }
        }
    }
    private static func unresolved(_ document: ConfigDocument) -> Bool {
        ([document] + bindingDocuments(document)).contains { !$0.unresolvedCommands.isEmpty || !$0.diagnostics.isEmpty }
    }
    private static func usesCommand(_ name: String, document: ConfigDocument) -> Bool {
        ([document] + bindingDocuments(document)).contains { $0.commands.contains { $0.tokens.first?.text.lowercased() == name.lowercased() } }
    }
    private static func dials(_ data: ConfigCheckupMeasurements) -> [ConfigCheckupDial] {
        let source = data.source.flatMap { $0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? nil : $0 }
        func measured(_ value: Double?, positive: Bool) -> Double? {
            guard source != nil, let value, value.isFinite, positive ? value > 0 : value >= 0 else { return nil }
            return value
        }
        return [.init(area: .movement, value: measured(data.frameTimeP99MS, positive: true), unit: "ms (p99)", source: source),
                .init(area: .aim, value: measured(data.mousePollingHz, positive: true), unit: "Hz", source: source),
                .init(area: .connection, value: measured(data.pingMS, positive: false), unit: "ms (ping)", source: source),
                .init(area: .visibility, value: nil, unit: nil, source: nil)]
    }
}
