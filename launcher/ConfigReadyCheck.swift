import Foundation
import Combine

enum ConfigReadyStatus: String, Sendable { case matchesKnownRules, needsChanges, unknown }
enum ConfigReadyCheckError: Error, LocalizedError, Sendable {
    case noVerifiedFix, staleReview
    var errorDescription: String? {
        switch self {
        case .noVerifiedFix: return "There is no verified temporary fix for this server and loadout."
        case .staleReview: return "The server rules or loadout changed. Review this connection again."
        }
    }
}

struct ConfigReadyReport: Sendable {
    let address: String?
    let profileID: String
    let expectedVersion: ConfigProfileVersion
    let advancedChainBytes: Data?
    let mode: ConfigCatalogMode
    let rulesetIDs: [String]
    let status: ConfigReadyStatus
    let summary: String
    let details: [ConfigRuleEvaluation]
    let facts: [String: String]
    let fixes: [String: String]
    let limitations: [String]
}

/// Status packets advertise claims, not authenticated rules or gameplay
/// measurements. A match is bounded to A1's cited, directly readable rules.
struct ConfigReadyCheck: Sendable {
    let catalog: ConfigCatalog
    let rules: ConfigRules

    func evaluate(server: GameServer?, profile: ConfigProfileRead, mode: ConfigCatalogMode, advancedChain: ConfigDocument? = nil) -> ConfigReadyReport {
        var facts: [String: String] = [:], ambiguous = false
        let observedKeys: Set<String> = ["protocol", "shortversion", "fs_game", "mod", "_zpam", "g_competitive",
                                         "sv_fps", "sv_minrate", "sv_maxrate", "sv_ruleset", "sv_maxfps", "g_maxfps", "com_maxfps_limit"]
        for (key, value) in server?.fields ?? [:] {
            let name = key.lowercased()
            if let previous = facts[name], previous != value { ambiguous = true }
            facts[name] = value
        }
        let identity = identify(facts)
        let ids = identity.ids
        var limitations = ["Only the cited, listed config rules are checked; full match, league and live compatibility are not certified.",
                           "Frame pacing, mouse polling and packet loss: Not measured. Server status ping is a query round trip."]
        let fresh = server?.observedAt.map { (0...60).contains(Date().timeIntervalSince($0)) } == true
        let address = server.flatMap { try? LaunchLink.canonicalAddress($0.address) }
        var unknown = !fresh || address == nil || ambiguous || identity.reason != nil || !rules.knownSelection(ids, mode: mode)
        if !fresh { limitations.append("No fresh status observation is available. Refresh before reviewing a connection fix.") }
        if let conflicts = server?.queryConflicts, !Set(conflicts).isDisjoint(with: observedKeys) {
            unknown = true; limitations.append("Status and info replies advertise conflicting rule facts.")
        }
        if let reason = identity.reason { limitations.append(reason) }
        if ambiguous { limitations.append("Conflicting case variants of advertised fields prevent a verified review.") }
        if !ids.isEmpty && !rules.knownSelection(ids, mode: mode) { limitations.append("The advertised rules do not match this client mode.") }
        let chain = advancedChain?.serialized() ?? profile.version.chain
        let reviewedProfile = ConfigProfileRead(profile: profile.profile, document: profile.document, metadata: profile.metadata,
            version: ConfigProfileVersion(config: profile.version.config, metadata: profile.version.metadata, chain: chain))
        let scripts = ConfigScriptReview(profile: reviewedProfile, catalog: catalog)
        let unresolved = scripts.unresolved
        if unresolved {
            unknown = true
            limitations.append("External configs, dynamic commands or parser diagnostics prevent a complete static review.")
        }
        for key in ["sv_fps", "sv_minrate", "sv_maxrate"] {
            if let text = facts[key], ConfigRules.number(text).map({ $0 >= 0 && $0.rounded() == $0 }) != true {
                unknown = true; limitations.append("The advertised \(key) value is not a valid nonnegative integer.")
            }
        }
        if let min = facts["sv_minrate"].flatMap(ConfigRules.number), let max = facts["sv_maxrate"].flatMap(ConfigRules.number), max > 0 && min > max {
            unknown = true; limitations.append("The advertised rate bounds contradict each other.")
        }
        if facts["sv_maxfps"] != nil || facts["g_maxfps"] != nil {
            unknown = true; limitations.append("The advertised custom FPS limit has no verified rule mapping.")
        }
        if [facts["com_maxfps_limit"], profile.document.effectiveSetting(named: "com_maxfps_limit")].compactMap({ $0 }).contains(where: { $0 != "0" }) {
            unknown = true; limitations.append("The legacy FPS-limit flag has no verified advertised policy mapping.")
        }
        var details: [ConfigRuleEvaluation] = [], fixes: [String: String] = [:]
        let names = Set(ids.flatMap { rules.resolvedRules(id: $0) }.map { $0.setting.lowercased() })
        if let conflicts = server?.queryConflicts, !Set(conflicts).isDisjoint(with: names) { unknown = true }
        for name in names.sorted() {
            let matched = ids.flatMap { rules.resolvedRules(id: $0) }.filter { $0.setting.lowercased() == name }
            if matched.contains(where: { $0.constraint.kind == .disabledCommand }) {
                let used = scripts.uses(name)
                if used {
                    details.append(.init(setting: name, value: nil, status: .violation,
                                         reason: "This server disables \(name). Binds that pause with wait will not work here; review the bind before joining.",
                                         serverOwned: false, sources: matched.flatMap(\.sources)))
                }
                continue
            }
            let value = profile.document.effectiveSetting(named: name)
            let result = rules.evaluate(setting: name, value: value, rulesetIDs: ids, mode: mode)
            if scripts.changedSettings.contains(name) {
                unknown = true
                details.append(.init(setting: name, value: value, status: .unknown,
                                     reason: "A config or bind command can change this setting; its runtime value is not assessed.",
                                     serverOwned: result.serverOwned, sources: result.sources))
                continue
            }
            if let advertised = facts[name], matched.contains(where: { $0.constraint.kind != .ownership }),
               rules.evaluate(setting: name, value: advertised, rulesetIDs: ids, mode: mode).status != .ok {
                unknown = true; limitations.append("The advertised \(name) value contradicts or cannot verify the cited rule.")
            }
            var reason = result.reason
            if name == "com_maxfps", let value, ConfigRules.number(value) == 333,
               matched.contains(where: { ($0.constraint.maximum ?? 1000) <= 250 }) {
                reason = "333 fps is unavailable here. " + reason
            }
            if result.serverOwned { reason += " The server controls this value; no temporary override is offered." }
            details.append(.init(setting: name, value: value, status: result.status, reason: reason,
                                 serverOwned: result.serverOwned, sources: result.sources))
            if result.status == .unknown && !result.serverOwned { unknown = true }
            if result.status == .violation && !result.serverOwned,
               !profile.metadata.locks.contains(where: { $0.lowercased() == name }),
               let entry = catalog.setting(named: name), entry.isEditable(in: mode),
               entry.variant(for: mode)?.serverControlled == false,
               let proposed = replacement(value: value, rules: matched), valid(proposed, entry: entry, mode: mode),
               rules.evaluate(setting: name, value: proposed, rulesetIDs: ids, mode: mode).status == .ok {
                fixes[name] = proposed
            }
        }
        if let value = profile.document.effectiveSetting(named: "rate"), let n = ConfigRules.number(value) {
            let min = facts["sv_minrate"].flatMap(ConfigRules.number), max = facts["sv_maxrate"].flatMap(ConfigRules.number)
            if min.map({ $0 > 0 && n < $0 }) == true || max.map({ $0 > 0 && n > $0 }) == true {
                details.append(.init(setting: "rate", value: value, status: .warning,
                                     reason: "Your saved rate is outside the server's advertised bounds. These bounds are claims; their runtime effect is not measured.",
                                     serverOwned: true, sources: []))
            }
        }
        let problems = details.contains { [.violation, .warning, .conflict].contains($0.status) }
        let status: ConfigReadyStatus = unknown ? .unknown : (problems ? .needsChanges : .matchesKnownRules)
        let summary: String
        switch status {
        case .unknown: summary = "We cannot verify this server's rules against this loadout."
        case .needsChanges: summary = "Review the listed differences before joining."
        case .matchesKnownRules: summary = "The directly saved values match the listed, cited rules."
        }
        return .init(address: address, profileID: profile.profile.id,
                     expectedVersion: profile.version, advancedChainBytes: chain, mode: mode, rulesetIDs: ids, status: status, summary: summary,
                     details: details, facts: facts.filter { observedKeys.contains($0.key) || names.contains($0.key) },
                     fixes: unknown ? [:] : fixes, limitations: limitations)
    }

    private func identify(_ fields: [String: String]) -> (ids: [String], reason: String?) {
        func unknown(_ reason: String) -> (ids: [String], reason: String?) { ([], reason) }
        guard let protocolNumber = fields["protocol"], ["118", "120"].contains(protocolNumber) else {
            return unknown("No supported protocol was advertised.")
        }
        let cod2x = protocolNumber == "120" || fields["shortversion"]?.hasPrefix("1.4.") == true
        let mod = fields["fs_game"]?.lowercased()
        guard mod != nil || fields["mod"] == "0" || fields["_zpam"] != nil else { return unknown("The server did not identify its mod or a stock installation.") }
        let competitive = fields["g_competitive"]
        guard competitive == nil || ["0", "1"].contains(competitive) else { return unknown("The competitive flag is unknown.") }
        // Pinned zPAM 3.36/4.08 default_mp.cfg advertise `sets _zpam`.
        // Arbitrary aliases and names without an exact version are unverified.
        if fields["zpam_version"] != nil || fields["_pam"] != nil { return unknown("An unverified mod identity key was advertised.") }
        let version = fields["_zpam"]
        let folderVersion = ["zpam336": "3.36", "zpam408": "4.08"][mod ?? ""]
        if let version, let folderVersion, version != folderVersion { return unknown("The zPAM identities contradict each other.") }
        let zpamVersion = version ?? folderVersion
        let base: String
        if let zpamVersion {
            guard mod == nil || mod == "" || mod == "zpam" || folderVersion != nil else { return unknown("The zPAM version does not identify this mod folder.") }
            switch zpamVersion {
            case "3.36":
                guard !cod2x, competitive != "1" else { return unknown("The zPAM 3.36 policy is verified only for the stock client.") }
                base = "zpam-3.36"
            case "4.08":
                guard cod2x, competitive != "0" else { return unknown("zPAM 4.08 requires CoD2x competitive rules, including pub.") }
                base = "zpam-4.08"
            default: return unknown("This zPAM version has no cited policy.")
            }
        } else {
            guard (mod ?? "").isEmpty else { return unknown("This mod has no verified policy mapping.") }
            if cod2x {
                guard let competitive else { return unknown("CoD2x status does not advertise whether competitive rules apply.") }
                base = competitive == "1" ? "cod2x-competitive" : "public-cod2x"
            } else {
                guard competitive != "1" else { return unknown("A stock identity contradicts the CoD2x competitive flag.") }
                base = "stock-1.3"
            }
        }
        if let advertised = fields["sv_ruleset"], advertised != base {
            // League IDs are claims too. Only known, mode-compatible A1 policy
            // data can join the base review; unknown IDs suppress all fixes.
            guard let extra = rules.ruleset(id: advertised), extra.mode == rules.ruleset(id: base)?.mode else {
                return unknown("The advertised extra ruleset has no compatible cited policy.")
            }
            return ([base, advertised], nil)
        }
        return ([base], nil)
    }
    private func replacement(value: String?, rules: [ConfigSettingRule]) -> String? {
        for rule in rules where !rule.advisory {
            let c = rule.constraint
            if c.kind == .exact { return c.value }
            if c.kind == .range {
                if let reset = c.resetValue { return reset }
                if let n = value.flatMap(ConfigRules.number), let min = c.minimum, let max = c.maximum {
                    let bounded = Swift.max(min, Swift.min(max, n))
                    return String(c.integer == true ? bounded.rounded(.down) : bounded)
                }
            }
        }
        return nil
    }
    private func valid(_ value: String, entry: ConfigCatalogSetting, mode: ConfigCatalogMode) -> Bool {
        guard ConfigConnectionLaunch.allowedSettings.contains(entry.name.lowercased()),
              let variant = entry.variant(for: mode), let n = ConfigRules.number(value) else { return false }
        switch variant.type {
        case .bool: guard n == 0 || n == 1 else { return false }
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
}

/// One explicit connection choice. It never writes settings, profile bytes,
/// sidecars or history. The launch consumes it only for the reviewed endpoint.
@MainActor final class ConfigServerConnection: ObservableObject {
    @Published private(set) var address: String?
    @Published private(set) var profileID: String?
    @Published private(set) var overrides: [String: String] = [:]
    private var review: ConfigReadyReport?

    func select(address: String?) throws {
        let normalized = try address.map(LaunchLink.canonicalAddress)
        guard normalized != self.address else { return }
        self.address = normalized; clear()
    }
    func clear() { profileID = nil; overrides = [:]; review = nil }
    func useProfile(_ id: String) throws {
        try ConfigStudioLaunch.validateProfileID(id)
        guard address != nil else { throw ConfigReadyCheckError.staleReview }
        profileID = id; overrides = [:]; review = nil
    }
    func fix(report: ConfigReadyReport) throws {
        guard report.status != .unknown, !report.fixes.isEmpty else { throw ConfigReadyCheckError.noVerifiedFix }
        guard let address, address == report.address, profileID == nil || profileID == report.profileID else { throw ConfigReadyCheckError.staleReview }
        overrides = report.fixes; review = report
    }
    func launch(for link: LaunchLink, profile: ConfigProfileRead, report: ConfigReadyReport) throws -> ConfigConnectionLaunch? {
        guard try LaunchLink.canonicalAddress(link.address) == address else { return nil }
        guard !overrides.isEmpty else { return nil }
        guard let review, review.profileID == profile.profile.id, review.expectedVersion == profile.version,
              report.address == address, report.expectedVersion == profile.version, report.mode == review.mode,
              report.rulesetIDs == review.rulesetIDs, report.facts == review.facts, report.advancedChainBytes == review.advancedChainBytes,
              report.fixes == overrides, report.status != .unknown else {
            throw ConfigReadyCheckError.staleReview
        }
        return ConfigConnectionLaunch(address: address!, values: overrides)
    }
}
