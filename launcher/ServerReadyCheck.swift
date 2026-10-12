import Foundation

// The Servers ready check in plain words, and the gestures above Deploy. Evaluation,
// staging and launch stay in A3 (ConfigReadyCheck, ConfigServerConnection, LauncherModel);
// this never writes a loadout. A match is scoped to the cited rules; unknown is never a pass.

struct ReadyCheckPresentation {
    enum Kind: Equatable { case unavailable, unknown, needsChange, serverSets, matches }
    struct Line: Identifiable, Equatable {
        let id: String
        let tone: StudioStatusTone
        let text: String
    }
    let kind: Kind
    let title: String
    let message: String?
    let lines: [Line]
    /// What “Fix for this server only” would use, in words: “Frame rate cap 250 fps instead of 333 fps”.
    let fixes: [String]
    /// Details: the cited rules, console names, sources and what isn't checked.
    let exact: [String]

    var tone: StudioStatusTone {
        switch kind {
        case .needsChange: .caution
        case .matches: .positive
        case .serverSets: .information
        case .unknown, .unavailable: .unknown
        }
    }
    var canFix: Bool { kind == .needsChange && !fixes.isEmpty }

    @MainActor init(report: ConfigReadyReport?, loadout: String, studio: ConfigStudioModel, rules: ConfigRules?) {
        guard let report else {
            kind = .unavailable; title = "We can’t check this server"
            message = "Configs aren’t available, so your loadout can’t be compared with this server’s rules."
            lines = []; fixes = []; exact = []
            return
        }
        func spec(_ name: String) -> ConfigSettingSpec? { studio.spec(named: name) }
        func name(_ setting: String) -> String { spec(setting)?.title ?? studio.catalog?.setting(named: setting)?.title ?? "A setting" }
        func shown(_ setting: String, _ value: String?) -> String {
            guard let value else { return "not set" }
            return spec(setting).map { ConfigValues.display($0, value) } ?? value
        }
        let matched = report.rulesetIDs.flatMap { rules?.resolvedRules(id: $0) ?? [] }
        func allowed(_ setting: String) -> String? {
            for rule in matched where rule.setting.lowercased() == setting.lowercased() && !rule.advisory {
                let c = rule.constraint, unit = spec(setting)?.unit.map { " " + $0 } ?? ""
                switch c.kind {
                case .range:
                    if let low = c.minimum, let high = c.maximum { return "This server allows \(ConfigValues.format(low))–\(ConfigValues.format(high))\(unit)." }
                case .exact:
                    if let value = c.value { return "This server uses \(shown(setting, value))." }
                case .forbiddenValues:
                    if let values = c.forbiddenValues, !values.isEmpty { return "This server doesn’t allow \(values.map { shown(setting, $0) }.joined(separator: " or "))." }
                default: continue
                }
            }
            return nil
        }
        var lines: [Line] = [], serverSet: [String] = []
        for detail in report.details {
            let setting = detail.setting.lowercased()
            if setting == "wait" {
                lines.append(.init(id: setting, tone: .caution, text: "Binds that pause between commands won’t work here, because this server turns pauses off. Review those binds before you join."))
                continue
            }
            if detail.serverOwned {
                if detail.status != .ok || detail.value == nil { serverSet.append(name(setting)) }
                continue
            }
            switch detail.status {
            case .violation:
                lines.append(.init(id: setting, tone: .caution, text: "\(name(setting)) is \(shown(setting, detail.value)) in this loadout." + (allowed(setting).map { " " + $0 } ?? "")))
            case .conflict:
                lines.append(.init(id: setting, tone: .caution, text: "The listed rules disagree about \(name(setting).lowercased()), so check it before you join."))
            case .warning:
                lines.append(.init(id: setting, tone: .caution, text: "\(name(setting)) is \(shown(setting, detail.value)). This server’s rules advise against it."))
            case .ok, .unknown: break
            }
        }
        if !serverSet.isEmpty {
            // Name a few; Details lists every one.
            let names = Array(Set(serverSet)).sorted().map { $0.lowercased() }
            let shown = names.count > 3 ? Array(names.prefix(2)) + ["\(names.count - 2) other settings"] : names
            let list = shown.count == 1 ? shown[0] : shown.dropLast().joined(separator: ", ") + " and " + shown.last!
            lines.append(.init(id: "server", tone: .information, text: "The server sets your \(list) while you play here."))
        }
        let titles = report.rulesetIDs.map { id in rules?.ruleset(id: id)?.title ?? id }
        var exact: [String] = []
        if !report.rulesetIDs.isEmpty { exact.append("Rules: " + zip(titles, report.rulesetIDs).map { "\($0) (\($1))" }.joined(separator: ", ")) }
        for detail in report.details {
            var line = "\(detail.setting) \"\(detail.value ?? "not set")\" · \(detail.status.rawValue)\(detail.serverOwned ? " · set by the server" : ""): \(detail.reason)"
            if let source = detail.sources.first { line += " Source: \(source.sourceURL) (\(source.sourceDate))." }
            exact.append(line)
        }
        exact += report.limitations
        if !report.facts.isEmpty { exact.append("Advertised: " + report.facts.keys.sorted().map { "\($0) \"\(report.facts[$0]!)\"" }.joined(separator: ", ")) }
        self.exact = exact
        fixes = report.fixes.keys.sorted().map { setting in
            let current = report.details.first { $0.setting.lowercased() == setting.lowercased() }?.value
            return "\(name(setting)) \(shown(setting, report.fixes[setting])) instead of \(shown(setting, current))"
        }
        switch report.status {
        case .unknown:
            kind = .unknown; title = "We don’t know this server’s rules"
            message = "League and competitive servers often limit your frame rate cap and connection settings. If you keep a loadout for those servers, choose it for this game."
            self.lines = []
        case .needsChanges:
            let actionable = lines.filter { $0.tone == .caution }.count
            if actionable == 0 {
                kind = .serverSets; title = "This server sets some of your settings"
                message = "Your loadout doesn’t need to change. They apply only while you play here."
            } else {
                kind = .needsChange
                title = "Your “\(loadout)” loadout needs \(actionable == 1 ? "one change" : "\(actionable) changes") here"
                message = nil
            }
            self.lines = lines
        case .matchesKnownRules:
            kind = .matches; title = "Matches this server’s listed rules"
            message = "Your saved settings match the rules listed for \(titles.joined(separator: " and ")). Only those settings are checked."
            self.lines = lines.filter { $0.tone == .information }
        }
    }
}

/// The card's store: the current report for one server and the three gestures above Deploy.
@MainActor final class ServerReadyCheckModel: ObservableObject {
    @Published private(set) var report: ConfigReadyReport?
    @Published private(set) var evaluated = false
    @Published private(set) var checking = false
    private(set) var address: String?
    nonisolated static let rules: ConfigRules? = try? ConfigRules.load()

    /// Reviews the loadout this connection would use: the one chosen for it, or the active one.
    func evaluate(_ model: LauncherModel, address: String) {
        self.address = address
        report = try? model.readyCheck(for: address)
        evaluated = true
    }
    /// Asks this one server again; a failure leaves the check unknown.
    func checkAgain(_ model: LauncherModel, address: String) async {
        checking = true
        defer { checking = false }
        report = try? await model.refreshReadyCheck(for: address)
        self.address = address; evaluated = true
    }
    /// Another saved loadout for this connection only; nil returns to the active loadout.
    func useLoadout(_ id: String?, model: LauncherModel, address: String) {
        do {
            if let id, id != model.studio.activeProfile?.id {
                try model.useLoadoutForServer(id, address: address)
                let name = model.studio.profiles.first { $0.id == id }?.displayName ?? id
                model.studio.announce("Using \(name) for this game only.")
            } else {
                model.serverConnection.clear(); model.objectWillChange.send()
            }
        } catch { model.notice = ConfigImportReviewModel.message(for: error) }
        evaluate(model, address: address)
    }
    /// Stages the report's verified fixes for this connection. No loadout bytes change.
    func fix(_ model: LauncherModel, address: String) {
        do {
            try model.fixForServerOnly(address)
            model.studio.announce("Fixed for this server only. Your loadout is unchanged.")
        } catch { model.notice = ConfigImportReviewModel.message(for: error) }
        evaluate(model, address: address)
    }
    func undo(_ model: LauncherModel, address: String) {
        model.serverConnection.clear(); model.objectWillChange.send()
        evaluate(model, address: address)
    }
    func presentation(_ model: LauncherModel, loadout: String) -> ReadyCheckPresentation {
        ReadyCheckPresentation(report: report, loadout: loadout, studio: model.studio, rules: Self.rules)
    }
}
