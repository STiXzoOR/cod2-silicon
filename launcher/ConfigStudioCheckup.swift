import Foundation

// The Check-up tab's view store, and the rule badges and tips Settings shows.
// Rules and advice come from A1 (ConfigRules, ConfigCheckup). Nothing here
// scores, measures or invents a requirement; missing data stays "Not measured".

extension ConfigTuneFor {
    var title: String {
        switch self { case .competitive: "Competitive"; case .balanced: "Balanced"; case .bestLooking: "Best looking" }
    }
}
extension ConfigCheckupArea {
    var title: String {
        switch self { case .movement: "Movement"; case .aim: "Aim"; case .connection: "Connection"; case .visibility: "Visibility" }
    }
}
extension ConfigRuleStatus {
    var tone: StudioStatusTone {
        switch self { case .ok: .positive; case .warning: .caution; case .violation: .negative; case .unknown: .unknown; case .conflict: .caution }
    }
    var title: String {
        switch self {
        case .ok: "Within the rules"; case .warning: "Advisory"; case .violation: "Breaks the rules"
        case .unknown: "Not assessed"; case .conflict: "The rules disagree"
        }
    }
}

/// One dial: the status words come from evidence, never from a guessed score.
struct ConfigCheckupGauge: Identifiable {
    let area: ConfigCheckupArea
    let value: String
    let detail: String
    let status: String
    let tone: StudioStatusTone
    var id: ConfigCheckupArea { area }
}

/// A suggestion applied in this view, kept so it can be undone from its card.
struct ConfigAppliedSuggestion: Identifiable {
    let suggestion: ConfigCheckupSuggestion
    let receipt: ConfigCheckupReceipt
    var id: String { suggestion.id }
}

@MainActor final class ConfigCheckupModel: ObservableObject {
    unowned let studio: ConfigStudioModel
    private let loadRules: @Sendable () throws -> ConfigRules
    private let loadOptimizer: @Sendable () throws -> ConfigOptimizer
    @Published private(set) var rules: ConfigRules?
    @Published private(set) var optimizer: ConfigOptimizer?
    @Published private(set) var loadError: String?

    /// Shared by Check-up, Expert badges and Guided tips.
    @Published var rulesetIDs: [String] = []
    @Published var tuneFor: ConfigTuneFor = .competitive
    @Published var showing: Set<String> = []
    @Published private(set) var applied: [ConfigAppliedSuggestion] = []
    /// “Not now” lasts for this loadout, rules and tuning, as A1 asks.
    @Published private var dismissed: [String: Set<String>] = [:]
    private var reportCache: (key: String, report: ConfigCheckupReport)?
    private var evaluationCache: (key: String, values: [String: ConfigRuleEvaluation])?

    init(studio: ConfigStudioModel,
         loadRules: @escaping @Sendable () throws -> ConfigRules = { try ConfigCheckupModel.bundled("ConfigRulesets", ConfigRules.init(data:)) },
         loadOptimizer: @escaping @Sendable () throws -> ConfigOptimizer = { try ConfigCheckupModel.bundled("ConfigOptimizer", ConfigOptimizer.init(data:)) }) {
        self.studio = studio; self.loadRules = loadRules; self.loadOptimizer = loadOptimizer
    }

    /// Bundle Resources in the app; beside the executable in development builds.
    nonisolated static func bundled<T>(_ name: String, _ decode: (Data) throws -> T) throws -> T {
        let candidates = [Bundle.main.url(forResource: name, withExtension: "json"),
                          Bundle.main.url(forResource: name, withExtension: "json", subdirectory: "Resources"),
                          Bundle.main.executableURL?.deletingLastPathComponent().appendingPathComponent(name + ".json")]
        guard let url = candidates.compactMap({ $0 }).first(where: { FileManager.default.fileExists(atPath: $0.path) }) else {
            throw ConfigRulesError.missingResource
        }
        return try decode(Data(contentsOf: url))
    }

    func load() {
        guard rules == nil, loadError == nil else { return }
        do {
            let rules = try loadRules()
            optimizer = try loadOptimizer()
            self.rules = rules
            if rulesetIDs.isEmpty, let first = available.first(where: { $0.id == "public-cod2x" }) ?? available.first { rulesetIDs = [first.id] }
        } catch { loadError = "Check-up’s rules or advice data are missing or damaged. Reinstall CoD2 Silicon to restore them." }
    }

    /// Rule sets for this engine; a stock policy can't judge a CoD2x client.
    var available: [ConfigRuleset] { rules?.rulesets.filter { $0.mode == studio.mode } ?? [] }
    var selectionTitle: String {
        let titles = rulesetIDs.compactMap { id in rules?.ruleset(id: id)?.title }
        return titles.isEmpty ? "No rules" : titles.joined(separator: " + ")
    }
    func toggle(_ id: String) {
        if rulesetIDs.contains(id) { rulesetIDs.removeAll { $0 == id } } else { rulesetIDs.append(id) }
        showing = []
    }

    var engine: ConfigCheckup? {
        guard let catalog = studio.catalog, let rules, let optimizer else { return nil }
        return ConfigCheckup(catalog: catalog, rules: rules, optimizer: optimizer)
    }
    private var scope: String {
        "\(studio.current?.profile.id ?? "")|\(rulesetIDs.sorted().joined(separator: ","))|\(tuneFor.rawValue)"
    }
    var dismissedHere: Set<String> { dismissed[scope] ?? [] }

    /// A1's review of the loadout on screen, regenerated whenever its bytes, locks,
    /// rules, tuning or set-aside suggestions change.
    var report: ConfigCheckupReport? {
        guard let engine, let current = studio.current else { return nil }
        let version = current.version.config + (current.version.metadata ?? Data()) + (current.version.chain ?? Data())
        let key = scope + "|" + dismissedHere.sorted().joined(separator: ",") + "|\(version.hashValue)|\(version.count)"
        if let cache = reportCache, cache.key == key, cache.report.expectedVersion == current.version { return cache.report }
        let report = engine.evaluate(profile: current, rulesetIDs: rulesetIDs, tuneFor: tuneFor, targetMode: studio.mode,
                                     measurements: ConfigCheckupMeasurements(), dismissedSuggestionIDs: dismissedHere)
        reportCache = (key, report)
        return report
    }

    func notNow(_ id: String) { dismissed[scope, default: []].insert(id); showing.remove(id) }
    func showAgain() { dismissed[scope] = [] }

    func apply(_ id: String, undo: UndoManager? = nil) {
        guard let engine, let report, let session = studio.session,
              let suggestion = report.suggestions.first(where: { $0.id == id }) else { return }
        do {
            let receipt = try session.mutate(profileID: report.profileID) { try engine.apply(report: report, suggestionIDs: [id], store: $0) }
            applied.removeAll { $0.id == id }
            applied.insert(ConfigAppliedSuggestion(suggestion: suggestion, receipt: receipt), at: 0)
            showing.remove(id)
            studio.announce("Applied. \(suggestion.title). Saved to History.")
            if let undo {
                undo.registerUndo(withTarget: self) { model in model.undoApplied(id) }
                undo.setActionName("Apply Suggestion")
            }
        } catch {
            studio.failure = ConfigStudioFailure(title: "Couldn’t apply this suggestion", message: Self.message(error), retry: nil)
        }
    }
    func undoApplied(_ id: String) {
        guard let engine, let session = studio.session, let item = applied.first(where: { $0.id == id }) else { return }
        do {
            _ = try session.mutate(profileID: item.receipt.profileID) {
                try engine.undo(receipt: item.receipt, rulesetIDs: rulesetIDs.isEmpty ? report?.rulesetIDs ?? [] : rulesetIDs, store: $0, targetMode: studio.mode)
            }
            applied.removeAll { $0.id == id }
            studio.announce("Undid \(item.suggestion.title).")
        } catch {
            studio.failure = ConfigStudioFailure(title: "Couldn’t undo this suggestion", message: Self.message(error), retry: nil)
        }
    }
    /// Applied cards belong to the loadout they changed.
    func appliedHere() -> [ConfigAppliedSuggestion] {
        applied.filter { $0.receipt.profileID == studio.current?.profile.id }
    }
    static func message(_ error: Error) -> String {
        switch error as? ConfigCheckupError {
        case .staleSuggestion?: return "The loadout or rules changed since this suggestion was made. Review the updated suggestions."
        case .unknownPolicy?: return "Choose rules for this game version first."
        case .emptyAction?: return "Choose a suggestion to apply."
        case .protectedSetting(let name)?: return "\(name) is locked or set by the server under these rules, so it can’t be changed back here. Use History instead."
        default: return error.localizedDescription
        }
    }

    // MARK: Plain presentation

    /// “Frame rate cap: 333 fps → 250 fps” in catalog names and units.
    func changeText(_ change: ConfigCheckupChange) -> String {
        guard let spec = studio.spec(named: change.setting) else {
            return "\(studio.catalog?.setting(named: change.setting)?.title ?? change.setting): \(change.oldValue ?? "Not set") → \(change.newValue)"
        }
        let state = { (raw: String?) in ConfigSettingState(spec: spec, raw: raw, locked: false, note: nil).display }
        return "\(spec.title): \(change.oldValue.map { state($0) } ?? "Not set") → \(state(change.newValue))"
    }
    func sourceText(_ source: ConfigRuleSource) -> (text: String, url: URL?) {
        if source.sourceURL.hasPrefix("git:") {
            // Local engine citations name an object at a commit, not a web page.
            let parts = source.sourceURL.split(separator: ":", maxSplits: 2)
            return ("CoD2 Silicon source \(parts.count == 3 ? String(parts[2]) : source.sourceURL)", nil)
        }
        let url = URL(string: source.sourceURL)
        return (url?.host.map { "\($0) · \(source.sourceDate)" } ?? source.sourceURL, url)
    }
    func evidenceText(_ suggestion: ConfigCheckupSuggestion) -> String {
        suggestion.evidence.first { $0.kind == .inference }?.detail ?? "Based on your settings"
    }

    /// The four dials. A1 supplies measurements; with none recorded every
    /// measured value reads “Not measured”, alongside what the loadout itself says.
    func gauges(_ report: ConfigCheckupReport) -> [ConfigCheckupGauge] {
        report.dials.map { dial in
            if let value = dial.value {
                return ConfigCheckupGauge(area: dial.area, value: ConfigValues.format(value) + (dial.unit.map { " " + $0 } ?? ""),
                                          detail: dial.source ?? "", status: "Measured", tone: .information)
            }
            switch dial.area {
            case .movement:
                let cap = studio.spec(named: "com_maxfps").map { studio.state($0) }
                let value = cap?.number.map { $0 == 0 ? "NO CAP" : "\(Int($0)) FPS" } ?? "—"
                return .init(area: .movement, value: value, detail: "Your frame rate cap. The frame rate you actually get isn’t measured.",
                             status: "Not measured", tone: .unknown)
            case .aim:
                let dpi = studio.metadata?.dpi
                let speed = studio.spec(named: "sensitivity").flatMap { studio.state($0).number }
                let yaw = studio.spec(named: "m_yaw").flatMap { studio.state($0).number } ?? 0.022
                let distance = dpi.flatMap { dpi in speed.flatMap { MouseMath.centimetresPer360(dpi: dpi, sensitivity: $0, yaw: yaw) } }
                return .init(area: .aim, value: distance.map { MouseMath.label($0) + " CM" } ?? "—",
                             detail: distance == nil ? "Enter your mouse’s DPI in Settings › Aim to see how far you turn. Polling rate isn’t measured."
                                                     : "Per full turn at \(ConfigValues.format(dpi ?? 0)) DPI. Polling rate isn’t measured.",
                             status: "Not measured", tone: .unknown)
            case .connection:
                return .init(area: .connection, value: "—", detail: "Ping and lost updates aren’t measured.", status: "Not measured", tone: .unknown)
            case .visibility:
                let names = Set((studio.catalog?.settings(in: .vis) ?? []).map { $0.name.lowercased() })
                let relevant = report.policy.filter { names.contains($0.setting.lowercased()) }
                if relevant.isEmpty {
                    return .init(area: .visibility, value: "—", detail: "The chosen rules say nothing about brightness or texture detail.", status: "Not assessed", tone: .unknown)
                }
                let worst = relevant.map(\.status).max { Self.rank($0) < Self.rank($1) } ?? .unknown
                let title = rules.map { rules in report.rulesetIDs.compactMap { rules.ruleset(id: $0)?.title }.joined(separator: " + ") } ?? ""
                switch worst {
                case .ok: return .init(area: .visibility, value: "WITHIN RULES", detail: "Brightness and texture detail follow \(title).", status: "Good", tone: .positive)
                case .violation: return .init(area: .visibility, value: "OUTSIDE RULES", detail: "Brightness or texture detail breaks \(title).", status: "Needs a change", tone: .negative)
                case .conflict: return .init(area: .visibility, value: "—", detail: "The chosen rules disagree about these settings.", status: "Rules disagree", tone: .caution)
                case .warning: return .init(area: .visibility, value: "ADVISORY", detail: "\(title) recommends a different value.", status: "Watch", tone: .caution)
                case .unknown: return .init(area: .visibility, value: "—", detail: "Scripts or missing values prevent a judgement.", status: "Not assessed", tone: .unknown)
                }
            }
        }
    }
    private static func rank(_ status: ConfigRuleStatus) -> Int {
        switch status { case .ok: 0; case .unknown: 1; case .warning: 2; case .conflict: 3; case .violation: 4 }
    }

    // MARK: Rule badges and tips for Settings

    /// The rule judgement for one setting under the chosen rules, or nil when
    /// none of them mentions it.
    func evaluation(_ spec: ConfigSettingSpec) -> ConfigRuleEvaluation? {
        guard let rules, !rulesetIDs.isEmpty, let current = studio.current else { return nil }
        let key = "\(rulesetIDs.joined(separator: ","))|\(current.data.hashValue)|\(current.data.count)"
        if evaluationCache?.key != key {
            var values: [String: ConfigRuleEvaluation] = [:]
            let names = Set(rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.map { $0.setting.lowercased() })
            for name in names {
                values[name] = rules.evaluate(setting: name, value: current.document.effectiveSetting(named: name), rulesetIDs: rulesetIDs, mode: studio.mode)
            }
            evaluationCache = (key, values)
        }
        return evaluationCache?.values[spec.name.lowercased()]
    }
    /// A short badge for an Expert row: the cited limit, or the server's ownership.
    func badge(_ spec: ConfigSettingSpec) -> (text: String, tone: StudioStatusTone)? {
        guard let rules, let evaluation = evaluation(spec) else { return nil }
        if evaluation.status == .conflict { return ("Rules disagree", .caution) }
        if evaluation.serverOwned { return ("Set by the server", .information) }
        let rule = rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.first { $0.setting.caseInsensitiveCompare(spec.name) == .orderedSame && $0.constraint.kind != .ownership }
        guard let constraint = rule?.constraint else { return nil }
        let show = { (value: String) in ConfigSettingState(spec: spec, raw: value, locked: false, note: nil).display }
        let text: String
        switch constraint.kind {
        case .range:
            guard let min = constraint.minimum, let max = constraint.maximum else { return nil }
            text = min == max ? "Must be \(show(ConfigValues.format(min)))" : "\(show(ConfigValues.format(min)))–\(show(ConfigValues.format(max)))"
        case .exact: text = "Must be \(show(constraint.value ?? ""))"
        case .forbiddenValues: text = "Not " + (constraint.forbiddenValues ?? []).map(show).joined(separator: " or ")
        case .disabledCommand: text = "Not allowed"
        case .ownership: return nil
        }
        return (text, evaluation.status == .violation ? .negative : evaluation.status == .ok ? .positive : evaluation.status.tone)
    }
    var violations: [ConfigRuleEvaluation] {
        guard let rules, !rulesetIDs.isEmpty, let current = studio.current else { return [] }
        let names = Set(rulesetIDs.flatMap { rules.resolvedRules(id: $0) }.map { $0.setting.lowercased() })
        return names.sorted().compactMap { name in
            let value = rules.evaluate(setting: name, value: current.document.effectiveSetting(named: name), rulesetIDs: rulesetIDs, mode: studio.mode)
            return value.status == .violation ? value : nil
        }
    }
    /// Guided tip: a rule this setting breaks, with A1's fix when one exists.
    func tip(_ spec: ConfigSettingSpec) -> (text: String, fix: ConfigCheckupSuggestion?)? {
        guard let evaluation = evaluation(spec), evaluation.status == .violation || evaluation.status == .conflict else { return nil }
        let fix = report?.suggestions.first { suggestion in suggestion.changes.contains { $0.setting.caseInsensitiveCompare(spec.name) == .orderedSame } }
        return (evaluation.reason, fix)
    }
}
