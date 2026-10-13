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

/// What Check-up last read from the game's local match summaries (`main/stats/matches.jsonl`).
enum ConfigMatchState: Equatable {
    case notRead
    /// Oldest first, as the reader returns them.
    case read([MatchSummary])
    case failed(String)
}

/// The latest recorded match, in words, for the panel above the dials.
struct ConfigMatchPresentation: Equatable {
    let title: String
    let detail: String
    let lines: [String]
    let tone: StudioStatusTone
}

/// How the loadout reads against the chosen rules, and what an empty suggestion list means.
/// A violation or conflict comes first; one row within the rules never speaks for rows that
/// weren't assessed or are only advisory.
struct ConfigCheckupStanding {
    struct Status: Equatable { let title: String; let tone: StudioStatusTone }
    let status: Status?
    let empty: (title: String, message: String)

    init(policy: [ConfigRuleEvaluation], rulesetIDs: [String], selectionTitle: String, tuneFor: ConfigTuneFor) {
        guard !rulesetIDs.isEmpty else {
            status = nil; empty = ("NO RULES", "Choose the rules you play under to get advice for them."); return
        }
        func count(_ status: ConfigRuleStatus) -> Int { policy.filter { $0.status == status }.count }
        let violations = count(.violation), conflicts = count(.conflict), unknown = count(.unknown), warnings = count(.warning), ok = count(.ok)
        let total = policy.count
        if violations > 0 {
            status = .init(title: violations == 1 ? "1 setting breaks these rules" : "\(violations) settings break these rules", tone: .negative)
        } else if conflicts > 0 {
            status = .init(title: "The chosen rules disagree", tone: .caution)
        } else if ok == 0 && warnings == 0 {
            status = .init(title: "Not assessed", tone: .unknown)
        } else if unknown > 0 {
            status = .init(title: "\(unknown) of \(total) setting\(total == 1 ? "" : "s") not assessed", tone: .unknown)
        } else if warnings > 0 {
            status = .init(title: "Within the limits, \(warnings) advisory", tone: .caution)
        } else {
            status = .init(title: "Follows \(selectionTitle)", tone: .positive)
        }
        let tune = tuneFor.title.lowercased()
        if violations > 0 || conflicts > 0 {
            empty = ("NO SUGGESTIONS", "Check-up has no change to suggest for \(tune) play, but some settings break these rules or the rules disagree. Show Rules lists them.")
        } else if unknown > 0 {
            empty = ("NO SUGGESTIONS", "Check-up couldn’t assess \(unknown) of \(total) setting\(total == 1 ? "" : "s") these rules cover, so it doesn’t claim they all match. Show Rules lists them.")
        } else if warnings > 0 {
            empty = ("NO SUGGESTIONS", "No change to suggest for \(tune) play, but the rules mark \(warnings == 1 ? "1 setting" : "\(warnings) settings") as advisory. Show Rules lists them.")
        } else if total == 0 {
            // Nothing was checked, so nothing can be said to match.
            empty = ("NO SUGGESTIONS", "No change to suggest for \(tune) play. These rules don’t cover any setting Check-up can assess here, so it doesn’t claim this loadout matches them.")
        } else {
            empty = ("NOTHING TO SUGGEST", "These settings already match every suggestion for \(tune) play under these rules.")
        }
    }
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
    private var reportCache: (key: String, measurements: ConfigCheckupMeasurements, report: ConfigCheckupReport)?

    /// Local match summaries: read after a game exits or on Refresh, never on a timer.
    @Published private(set) var matchState: ConfigMatchState = .notRead
    /// The current Settings › Privacy choice. Turning it off applies from the next game and
    /// leaves summaries already on this Mac where they are.
    @Published var matchSummariesEnabled = true
    private var matchHome: URL?
    /// The reader; tests count its reads or make it fail.
    var readMatches: @MainActor (URL) throws -> [MatchSummary] = { try MatchSummaries.read(homePath: $0) }

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
    /// rules, tuning, set-aside suggestions or the latest match's measurements change.
    var report: ConfigCheckupReport? {
        guard let engine, let current = studio.current else { return nil }
        let version = current.version.config + (current.version.metadata ?? Data()) + (current.version.chain ?? Data())
        let key = scope + "|" + dismissedHere.sorted().joined(separator: ",") + "|\(version.hashValue)|\(version.count)"
        let measurements = self.measurements
        if let cache = reportCache, cache.key == key, cache.measurements == measurements, cache.report.expectedVersion == current.version { return cache.report }
        let report = engine.evaluate(profile: current, rulesetIDs: rulesetIDs, tuneFor: tuneFor, targetMode: studio.mode,
                                     measurements: measurements, dismissedSuggestionIDs: dismissedHere)
        reportCache = (key, measurements, report)
        return report
    }

    // MARK: Match summaries

    /// Reads the summaries once: after the game exits, or when the person asks. The home is
    /// the launcher's own (the game's fs_homepath); a failure is kept and shown, never measured.
    func refreshMatches(home: URL? = nil) {
        if let home { matchHome = home }
        guard let home = matchHome ?? studio.session?.home else { return }
        do { matchState = .read(try readMatches(home)) } catch { matchState = .failed(error.localizedDescription) }
    }
    var latestMatch: MatchSummary? { if case .read(let all) = matchState { return all.last }; return nil }
    var matchCount: Int { if case .read(let all) = matchState { return all.count }; return 0 }

    /// What the latest match measured, for A1. A p99 past the exact range is only an upper
    /// bound, so it isn't passed on as a measurement; mouse polling is never recorded.
    var measurements: ConfigCheckupMeasurements {
        guard let match = latestMatch else { return .init() }
        return ConfigCheckupMeasurements(frameTimeP99MS: match.frameQuantilesAreExact ? match.frameTimeP99MS : nil, mousePollingHz: nil,
                                         pingMS: match.pingAverageMS, packetLossPercent: match.packetLossPercent,
                                         source: "Your last match: \(MapCatalog.displayName(match.map)), \(GameModes.long(match.mode))")
    }

    var matchPresentation: ConfigMatchPresentation? {
        switch matchState {
        case .notRead: return nil
        case .failed(let message):
            return .init(title: "Couldn’t read your match summaries", detail: message + " Nothing here is shown as measured until they can be read.", lines: [], tone: .negative)
        case .read(let all):
            guard let match = all.last else {
                return matchSummariesEnabled
                    ? .init(title: "No matches recorded yet", detail: "After your next match, the game saves a short summary on this Mac and Check-up shows it here.", lines: [], tone: .unknown)
                    : .init(title: "Match summaries are off", detail: "Turn on local match summaries in Settings › Privacy to see measurements from your next game.", lines: [], tone: .unknown)
            }
            let off = matchSummariesEnabled ? "" : " Match summaries are off now, so new games aren’t added."
            let ended = Self.matchDate(match.endedAt).map { StencilDate.dayAndTime($0) } ?? match.endedAt
            return .init(title: "Your last match: \(MapCatalog.displayName(match.map)), \(GameModes.long(match.mode))",
                         detail: "Measured by the game on this Mac. It may have used a different loadout than the one shown here." + off,
                         lines: ["Ended \(ended) · \(Self.duration(match.durationSeconds))",
                                 all.count == 1 ? "1 match recorded on this Mac" : "\(all.count) matches recorded on this Mac"],
                         tone: .information)
        }
    }
    private static func matchDate(_ text: String) -> Date? {
        let formatter = ISO8601DateFormatter()
        if let date = formatter.date(from: text) { return date }
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter.date(from: text)
    }
    static func duration(_ seconds: Double) -> String {
        // Whole seconds without trapping on any value: the reader allows up to UInt64.max milliseconds.
        let total = seconds.isFinite && seconds > 0 ? UInt64(min(seconds.rounded(), 0x1p63)) : 0
        if total < 60 { return "\(total) s" }
        let minutes = total / 60
        return minutes < 60 ? "\(minutes) min" : "\(minutes / 60) h \(minutes % 60) min"
    }
    /// Milliseconds and percentages to one decimal place, as people read them.
    static func figure(_ value: Double) -> String { ConfigValues.format((value * 10).rounded() / 10) }

    /// The movement dial from a match that recorded frames: p50, p99, maximum, frames within the
    /// cap, the cap recorded at the end and the frame count. Quantiles at or past the exact range
    /// are said as bounds. The cap can change during a match, so the capped-frame counts are never
    /// attributed to the final cap.
    private func frameGauge(_ match: MatchSummary) -> ConfigCheckupGauge? {
        guard match.frameCount > 0, let p50 = match.frameTimeP50MS, let p99 = match.frameTimeP99MS, let max = match.frameTimeMaxMS else { return nil }
        let limit = match.frameTimeHistogramLimitMS
        func quantile(_ name: String, _ value: Double) -> String {
            guard let limit else { return "\(name) at most \(Self.figure(value)) ms" }
            // A sample at the limit lands in the overflow bucket: at least the limit.
            return value < limit ? "\(name) \(Self.figure(value)) ms" : "\(name) at least \(Self.figure(limit)) ms (at most \(Self.figure(value)) ms)"
        }
        var parts = [quantile("p50", p50), quantile("p99", p99), "max \(Self.figure(max)) ms"]
        if let under = match.framesUnderCap, match.capSampleFrames > 0 {
            parts.append("\(under) of \(match.capSampleFrames) capped frames (\(Self.figure(100 * Double(under) / Double(match.capSampleFrames)))%) within the cap in force")
        } else {
            parts.append("frames within the cap: Not measured")
        }
        parts.append(match.capFPS > 0 ? "cap at the end of the match \(match.capFPS) fps" : "no cap at the end of the match")
        parts.append(match.frameCount == 1 ? "1 frame" : "\(match.frameCount) frames")
        let exact = match.frameQuantilesAreExact
        return .init(area: .movement, value: (exact ? "" : "≤ ") + Self.figure(p99) + " MS",
                     detail: "Frame time in your last match: " + parts.joined(separator: " · ") + ".",
                     status: exact ? "Measured" : "Upper bound", tone: .information)
    }
    /// The connection dial from a match: ping, and server updates that never arrived.
    private func connectionGauge(_ match: MatchSummary) -> ConfigCheckupGauge {
        let ping: String
        if match.pingSamples > 0, let average = match.pingAverageMS, let highest = match.pingMaximumMS {
            ping = "Ping in your last match: average \(Self.figure(average)) ms, highest \(Self.figure(highest)) ms over \(match.pingSamples) sample\(match.pingSamples == 1 ? "" : "s")."
        } else {
            ping = "Ping: Not measured in your last match."
        }
        let missing: String
        let (expected, overflow) = match.packetsReceived.addingReportingOverflow(match.packetsLost)
        if match.packetsReceived > 0, !overflow, let loss = match.packetLossPercent {
            missing = "Missing server updates: \(Self.figure(loss))% (\(match.packetsLost) of \(expected)), inferred from gaps in the update sequence; not a precise packet-loss measurement."
        } else {
            missing = "Missing server updates: Not measured."
        }
        let measured = match.pingSamples > 0 ? match.pingAverageMS : nil
        return .init(area: .connection, value: measured.map { Self.figure($0) + " MS" } ?? "—", detail: ping + " " + missing,
                     status: measured == nil ? "Not measured" : "Measured", tone: measured == nil ? .unknown : .information)
    }

    /// Why the review holds its advice back, found the way A1 looks: lines whose effect
    /// can't be known before the game runs them. Nil when nothing holds it back.
    var holdBack: String? {
        guard report?.unresolvedScripts == true else { return nil }
        return "Some lines run other files or commands that only the game can follow, or can’t be read. Edit as Text shows them."
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

    /// The four dials. Movement and Connection come from the latest recorded match when there
    /// is one; every value it didn't record reads “Not measured”, alongside what the loadout says.
    func gauges(_ report: ConfigCheckupReport) -> [ConfigCheckupGauge] {
        let match = latestMatch
        return report.dials.map { dial in
            switch dial.area {
            case .movement:
                if let match, let gauge = frameGauge(match) { return gauge }
                let cap = studio.spec(named: "com_maxfps").map { studio.state($0) }
                let value = cap?.number.map { $0 == 0 ? "NO CAP" : "\(ConfigValues.format($0, integer: true)) FPS" } ?? "—"
                return .init(area: .movement, value: value, detail: "Your frame rate cap. The frame rate you actually get isn’t measured.",
                             status: cap?.number == nil ? "Not measured" : "From settings", tone: cap?.number == nil ? .unknown : .information)
            case .aim:
                let dpi = studio.metadata?.dpi
                let speed = studio.spec(named: "sensitivity").flatMap { studio.state($0).number }
                let yaw = studio.spec(named: "m_yaw").flatMap { studio.state($0).number } ?? 0.022
                let distance = dpi.flatMap { dpi in speed.flatMap { MouseMath.centimetresPer360(dpi: dpi, sensitivity: $0, yaw: yaw) } }
                return .init(area: .aim, value: distance.map { MouseMath.label($0) + " CM" } ?? "—",
                             detail: distance == nil ? "Enter your mouse’s DPI in Settings › Aim to see how far you turn. Polling rate isn’t measured."
                                                     : "Per full turn at \(ConfigValues.format(dpi ?? 0)) DPI. Polling rate isn’t measured.",
                             status: distance == nil ? "Not measured" : "From settings", tone: distance == nil ? .unknown : .information)
            case .connection:
                if let match { return connectionGauge(match) }
                return .init(area: .connection, value: "—", detail: "Ping and missing server updates aren’t measured.", status: "Not measured", tone: .unknown)
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
                case .unknown:
                    let open = relevant.filter { $0.status == .unknown }
                    let detail = holdBack != nil ? "Advanced binds or scripts prevent a judgement."
                        : open.contains { $0.value == nil } ? "These rules check values this loadout doesn’t set."
                        : open.contains { $0.serverOwned } ? "The server sets these values when you join."
                        : "No verified rule covers these settings in this game mode."
                    return .init(area: .visibility, value: "—", detail: detail, status: "Not assessed", tone: .unknown)
                }
            }
        }
    }
    /// The rules summary and empty-suggestions message for this report.
    func standing(_ report: ConfigCheckupReport) -> ConfigCheckupStanding {
        ConfigCheckupStanding(policy: report.policy, rulesetIDs: report.rulesetIDs, selectionTitle: selectionTitle, tuneFor: tuneFor)
    }
    private static func rank(_ status: ConfigRuleStatus) -> Int {
        switch status { case .ok: 0; case .unknown: 1; case .warning: 2; case .conflict: 3; case .violation: 4 }
    }

    // MARK: Rule badges and tips for Settings

    /// The rule judgement for one setting under the chosen rules, or nil when
    /// none of them mentions it.
    func evaluation(_ spec: ConfigSettingSpec) -> ConfigRuleEvaluation? {
        report?.policy.first { $0.setting.caseInsensitiveCompare(spec.name) == .orderedSame }
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
        report?.policy.filter { $0.status == .violation } ?? []
    }
    /// Guided tip: a rule this setting breaks, with A1's fix when one exists.
    func tip(_ spec: ConfigSettingSpec) -> (text: String, fix: ConfigCheckupSuggestion?)? {
        guard let evaluation = evaluation(spec), evaluation.status == .violation || evaluation.status == .conflict else { return nil }
        let fix = report?.suggestions.first { suggestion in suggestion.changes.contains { $0.setting.caseInsensitiveCompare(spec.name) == .orderedSame } }
        return (Self.plain(evaluation.reason, spec: spec), fix)
    }
    /// Rule text names settings by console name; Guided says the plain name instead.
    static func plain(_ text: String, spec: ConfigSettingSpec) -> String {
        guard let regex = try? NSRegularExpression(pattern: "\\b" + NSRegularExpression.escapedPattern(for: spec.name) + "\\b", options: .caseInsensitive) else { return text }
        let title = spec.title
        // Mid-sentence, "Updates you send" reads "updates you send"; "CoD2x mouse input" keeps its case.
        let inner = title.count > 1 && title.dropFirst().first?.isLowercase == true ? title.prefix(1).lowercased() + title.dropFirst() : title
        var result = regex.stringByReplacingMatches(in: text, range: NSRange(text.startIndex..., in: text), withTemplate: NSRegularExpression.escapedTemplate(for: inner))
        if result.hasPrefix(inner) { result = title + result.dropFirst(inner.count) }
        return result
    }
}
