import AppKit
import Foundation

// The Configs page's view store. Disk work goes through ConfigStudioSession (and its
// guarded store); this type owns selection, presentation state, value formatting and
// the single-transaction intents the controls call. It never invents loadouts or values.

enum ConfigStudioTab: String, CaseIterable, Identifiable {
    case settings, binds, checkup, history
    var id: String { rawValue }
    var title: String {
        switch self { case .settings: "Settings"; case .binds: "Binds"; case .checkup: "Check-up"; case .history: "History" }
    }
    static let available: [ConfigStudioTab] = [.settings, .binds, .checkup, .history]
}

enum ConfigDetailLevel: String, CaseIterable, Identifiable {
    case guided, expert
    var id: String { rawValue }
    var title: String { self == .guided ? "Guided" : "Expert" }
}

enum ConfigExpertFilter: String, CaseIterable, Identifiable {
    case changed, locked
    var id: String { rawValue }
    var title: String { self == .changed ? "Changed" : "Locked" }
}

enum ConfigHistoryFilter: String, CaseIterable, Identifiable {
    case all, you, imports, checkup, game
    var id: String { rawValue }
    var title: String {
        switch self { case .all: "All"; case .you: "You"; case .imports: "Imports"; case .checkup: "Check-up"; case .game: "Game" }
    }
    func includes(_ source: ConfigHistorySource) -> Bool {
        switch self {
        case .all: true
        case .you: source == .you || source == .restore
        case .imports: source == .import
        case .checkup: source == .checkup
        case .game: source == .gameSession
        }
    }
}

extension ConfigPurposeGroup {
    var symbol: String {
        switch self {
        case .frame: "gauge.with.dots.needle.33percent"; case .aim: "scope"; case .net: "chart.bar"; case .vis: "eye"
        case .hud: "viewfinder"; case .audio: "speaker.wave.2"; case .gfx: "photo"; case .other: "ellipsis.circle"
        }
    }
    /// Short descriptions from the approved Guided board, kept to what the catalog supports.
    var blurb: String {
        switch self {
        case .frame: "How smoothly the game runs, and how high and far you can jump."
        case .aim: "How the game responds to your mouse or trackpad."
        case .net: "How often you and the server talk."
        case .vis: "How bright and clear the picture is."
        case .hud: "What stays on screen while you play."
        case .audio: "How loud the game is."
        case .gfx: "How the world looks."
        case .other: "Your name, and what the game may download."
        }
    }
}

extension ConfigHistorySource {
    var title: String {
        switch self {
        case .you: "You"; case .import: "Import"; case .checkup: "Check-up"; case .gameSession: "Game session"
        case .restore: "Restore"; case .migration: "CoD2 Silicon"
        }
    }
}

// MARK: - Values

/// How one catalog setting reads and writes in a config for the current engine.
struct ConfigSettingSpec: Identifiable {
    let setting: ConfigCatalogSetting
    let type: ConfigCatalogType
    let minimum: Double?
    let maximum: Double?
    /// Names of an enumeration's values, in registration order.
    let choices: [String]
    /// The registered default as config text; nil when the default is dynamic.
    let defaultText: String?
    /// CoD2 Silicon sets this at every launch from Settings, so a loadout can't own it.
    let launcherOwned: Bool
    /// The catalog's names for particular values, in value order.
    let valueLabels: [ConfigValueLabel]
    var id: String { setting.name }
    var name: String { setting.name }
    var title: String { setting.title }
    var group: ConfigPurposeGroup { setting.group }
    var unit: String? { setting.control.unit }
    var step: Double {
        if let step = setting.control.step, step > 0 { return step }
        return type == .int ? 1 : 0
    }
    var isSecret: Bool { name.lowercased().contains("password") }
    var isNumeric: Bool { type == .int || type == .float }
    var vectorCount: Int? {
        switch type { case .vec2: 2; case .vec3: 3; case .vec4, .color: 4; default: nil }
    }

    init?(setting: ConfigCatalogSetting, mode: ConfigCatalogMode) {
        guard setting.isEditable(in: mode), let variant = setting.variant(for: mode), let type = variant.type else { return nil }
        self.setting = setting
        self.type = type
        func literal(_ fact: ConfigCatalogFact?) -> Double? {
            guard fact?.status == .literal, case .number(let value)? = fact?.value else { return nil }
            return value
        }
        minimum = literal(variant.minimum)
        maximum = literal(variant.maximum)
        var names: [String] = []
        if variant.enumValues?.status == .literal, case .array(let values)? = variant.enumValues?.value {
            names = values.compactMap { if case .string(let name) = $0 { return name }; return nil }
        }
        choices = names
        let fact = variant.defaultValue
        defaultText = fact.status == .literal ? fact.value.flatMap { ConfigValues.configText($0, type: type, choices: names) } : nil
        launcherOwned = GameSettings.machineDvarNames.contains(setting.name.lowercased())
        valueLabels = (setting.control.labels ?? [:]).map { ConfigValueLabel(text: $0.key, title: $0.value) }
            .sorted { (ConfigValues.number($0.text) ?? 0, $0.text) < (ConfigValues.number($1.text) ?? 0, $1.text) }
    }

    /// The catalog's label for a value, compared as the type the game reads.
    func label(for text: String) -> String? {
        valueLabels.first { ConfigValues.same(self, $0.text, text) }?.title
    }
    /// Every value the game accepts has a label, so a picker of labels can't hide one.
    var labelsCoverRange: Bool {
        guard !valueLabels.isEmpty else { return false }
        switch type {
        case .bool: return label(for: "0") != nil && label(for: "1") != nil
        case .int:
            guard let minimum, let maximum, maximum - minimum < 64 else { return false }
            return stride(from: minimum, through: maximum, by: 1).allSatisfy { label(for: ConfigValues.format($0, integer: true)) != nil }
        default: return false
        }
    }
    /// The range most players use, kept inside the registered one.
    var practicalRange: ClosedRange<Double>? {
        guard let range = setting.control.practicalRange else { return nil }
        let low = Swift.max(range.min, minimum ?? range.min), high = Swift.min(range.max, maximum ?? range.max)
        return low < high ? low...high : nil
    }
    /// The practical range, or a literal range with few enough steps to slide through; otherwise a field.
    var slideRange: ClosedRange<Double>? {
        if let practicalRange { return practicalRange }
        guard let minimum, let maximum, maximum > minimum else { return nil }
        let step = self.step > 0 ? self.step : (maximum - minimum) / 100
        return (maximum - minimum) / step <= 200 ? minimum...maximum : nil
    }
}

/// A catalog value label: the config text and the name the game's menus give it.
struct ConfigValueLabel: Hashable {
    let text: String
    let title: String
}

/// Pure value conversions between config text, typed values and plain display.
enum ConfigValues {
    private static let posix = Locale(identifier: "en_US_POSIX")

    static func configText(_ value: ConfigCatalogValue, type: ConfigCatalogType, choices: [String]) -> String? {
        switch (type, value) {
        case (.bool, .bool(let flag)): return flag ? "1" : "0"
        case (.enumeration, .number(let index)):
            let i = Int(index)
            return choices.indices.contains(i) ? choices[i] : String(i)
        case (_, .number(let number)): return format(number, integer: type == .int)
        case (_, .string(let text)): return text
        case (_, .array(let values)):
            let numbers = values.compactMap { if case .number(let n) = $0 { return format(n, integer: false) }; return nil }
            return numbers.count == values.count ? numbers.joined(separator: " ") : nil
        default: return nil
        }
    }

    /// Plain numerals without grouping or locale: "2.6", "250", "0.022".
    static func format(_ number: Double, integer: Bool = false) -> String {
        guard number.isFinite else { return "0" }
        if integer || (number == number.rounded() && abs(number) < 1e15) { return String(Int64(number.rounded())) }
        var text = String(format: "%.6f", locale: posix, number)
        while text.hasSuffix("0") { text.removeLast() }
        if text.hasSuffix(".") { text.removeLast() }
        return text == "-0" ? "0" : text
    }

    /// The engine reads numbers like atof: the leading numeric part, else zero.
    static func number(_ text: String) -> Double? {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        if let value = Double(trimmed), value.isFinite { return value }
        let scanner = Scanner(string: trimmed)
        scanner.locale = posix
        return scanner.scanDouble().flatMap { $0.isFinite ? $0 : nil }
    }
    static func bool(_ text: String) -> Bool { (number(text) ?? 0) != 0 }
    static func enumIndex(_ text: String, choices: [String]) -> Int? {
        if let index = choices.firstIndex(where: { $0.caseInsensitiveCompare(text) == .orderedSame }) { return index }
        if let value = Int(text.trimmingCharacters(in: .whitespaces)), choices.indices.contains(value) { return value }
        return nil
    }
    static func vector(_ text: String) -> [Double] { text.split(separator: " ").compactMap { number(String($0)) } }

    /// Typed equality, so "5", "5.0" and "5.000" are the same mouse speed.
    static func same(_ spec: ConfigSettingSpec, _ a: String, _ b: String) -> Bool {
        switch spec.type {
        case .bool: return bool(a) == bool(b)
        case .int, .float: return number(a).map { x in number(b).map { abs(x - $0) < 1e-9 } ?? false } ?? (a == b)
        case .enumeration: return enumIndex(a, choices: spec.choices).map { $0 == enumIndex(b, choices: spec.choices) } ?? (a == b)
        case .vec2, .vec3, .vec4, .color: return vector(a) == vector(b)
        case .string: return a == b
        }
    }

    /// Plain words for people: On/Off, an enumeration's name, a number with its unit.
    static func display(_ spec: ConfigSettingSpec, _ text: String?) -> String {
        guard let text else { return "Not set" }
        if let label = spec.label(for: text) {
            guard spec.isNumeric, let value = number(text) else { return label }
            let numeral = spec.type == .int ? String(Int64(value.rounded())) : format(value)
            return spec.unit.map { "\(label) (\(numeral) \($0))" } ?? label
        }
        switch spec.type {
        case .bool: return bool(text) ? "On" : "Off"
        case .enumeration: return enumIndex(text, choices: spec.choices).map { spec.choices[$0] } ?? text
        case .int, .float:
            guard let value = number(text) else { return text }
            if spec.name.lowercased() == "snd_volume" { return "\(Int((value * 100).rounded()))%" }
            let numeral = spec.type == .int ? String(Int64(value.rounded())) : format(value)
            return spec.unit.map { "\(numeral) \($0)" } ?? numeral
        case .string: return spec.isSecret ? (text.isEmpty ? "None" : "Hidden") : (text.isEmpty ? "Empty" : text)
        case .vec2, .vec3, .vec4, .color: return vector(text).map { format($0) }.joined(separator: " ")
        }
    }

    /// The text written for a typed control, clamped to the registered range.
    static func text(number value: Double, spec: ConfigSettingSpec) -> String {
        var clamped = value
        if let minimum = spec.minimum { clamped = max(minimum, clamped) }
        if let maximum = spec.maximum { clamped = min(maximum, clamped) }
        return format(clamped, integer: spec.type == .int)
    }
}

/// One row's facts for a document: value, default, changed, lock and note.
struct ConfigSettingState {
    let spec: ConfigSettingSpec
    /// The direct assignment in this config; nil means the game uses its default.
    let raw: String?
    let locked: Bool
    let note: String?
    var value: String? { raw ?? spec.defaultText }
    var changed: Bool {
        guard let raw else { return false }
        guard let defaultText = spec.defaultText else { return true }
        return !ConfigValues.same(spec, raw, defaultText)
    }
    var display: String { ConfigValues.display(spec, value) }
    var number: Double? { value.flatMap(ConfigValues.number) }
    var flag: Bool { value.map(ConfigValues.bool) ?? false }
}

// MARK: - History differences

struct ConfigChange: Identifiable, Equatable {
    let id: String
    let label: String
    let consoleName: String?
    let then: String
    let now: String
}

enum ConfigDiff {
    /// Differences between two saved states, in plain names and units.
    static func changes(from before: Data, _ beforeMetadata: Data?, to after: Data, _ afterMetadata: Data?,
                        chains: (before: Data?, after: Data?) = (nil, nil),
                        specs: [String: ConfigSettingSpec], catalog: ConfigCatalog?, dialect: ConfigDialect) -> [ConfigChange] {
        let old = ConfigDocument(data: before, dialect: dialect), new = ConfigDocument(data: after, dialect: dialect)
        var names: [String: String] = [:]
        for document in [old, new] {
            for command in document.commands where command.kind == .setting && command.tokens.count >= 2 {
                let name = command.tokens[1].text
                names[name.lowercased()] = names[name.lowercased()] ?? name
            }
        }
        var result: [ConfigChange] = [], others = 0
        for key in names.keys.sorted() {
            let name = names[key]!
            let a = old.effectiveSetting(named: name), b = new.effectiveSetting(named: name)
            guard a != b else { continue }
            if let spec = specs[key] {
                let state = { (raw: String?) in ConfigSettingState(spec: spec, raw: raw, locked: false, note: nil) }
                if let a, let b, ConfigValues.same(spec, a, b) { continue }
                result.append(ConfigChange(id: key, label: spec.title, consoleName: spec.name, then: state(a).display, now: state(b).display))
            } else if let setting = catalog?.setting(named: name) {
                result.append(ConfigChange(id: key, label: setting.title, consoleName: setting.name, then: a ?? "Not set", now: b ?? "Not set"))
            } else { others += 1 }
        }
        var binds = Set<Int>()
        for document in [old, new] {
            for command in document.commands where (command.kind == .bind || command.kind == .unbind) && command.tokens.count >= 2 {
                if let key = ConfigDocument.keyNumber(command.tokens[1].text) { binds.insert(key) }
            }
        }
        let changedBinds = binds.filter { key in
            let spelling = ConfigDocument.keySpelling(key)
            return old.effectiveBind(key: spelling) != new.effectiveBind(key: spelling)
        }.count
        if changedBinds > 0 {
            result.append(ConfigChange(id: "binds", label: "Key bindings", consoleName: nil, then: "", now: changedBinds == 1 ? "1 key changed" : "\(changedBinds) keys changed"))
        }
        if others > 0 {
            result.append(ConfigChange(id: "other", label: "Other lines", consoleName: nil, then: "", now: others == 1 ? "1 line changed" : "\(others) lines changed"))
        }
        // The loadout's launcher-owned advanced-bind file travels with History too.
        if chains.before != chains.after {
            let now = chains.before == nil ? "Added" : chains.after == nil ? "Removed" : "Changed"
            result.append(ConfigChange(id: "chain", label: "Advanced binds", consoleName: nil, then: "", now: now))
        }
        let decoder = JSONDecoder()
        let m0 = beforeMetadata.flatMap { try? decoder.decode(ConfigStudioMetadata.self, from: $0) }
        let m1 = afterMetadata.flatMap { try? decoder.decode(ConfigStudioMetadata.self, from: $0) }
        if let m0, let m1 {
            if m0.displayName != m1.displayName { result.append(ConfigChange(id: "name", label: "Loadout name", consoleName: nil, then: m0.displayName, now: m1.displayName)) }
            if m0.locks != m1.locks {
                func list(_ locks: Set<String>) -> String {
                    locks.isEmpty ? "None" : locks.map { specs[$0.lowercased()]?.title ?? catalog?.setting(named: $0)?.title ?? $0 }.sorted().joined(separator: ", ")
                }
                result.append(ConfigChange(id: "locks", label: "Locked settings", consoleName: nil, then: list(m0.locks), now: list(m1.locks)))
            }
            if m0.notes != m1.notes {
                func noted(_ notes: [String: String]) -> String {
                    notes.isEmpty ? "None" : notes.keys.map { specs[$0.lowercased()]?.title ?? catalog?.setting(named: $0)?.title ?? $0 }.sorted().joined(separator: ", ")
                }
                let label = Set(m0.notes.keys) == Set(m1.notes.keys) ? "Edited notes" : "Notes"
                result.append(ConfigChange(id: "notes", label: label, consoleName: nil, then: noted(m0.notes), now: noted(m1.notes)))
            }
            if m0.dpi != m1.dpi {
                func dpi(_ value: Double?) -> String { value.map { "\(ConfigValues.format($0)) DPI" } ?? "Not set" }
                result.append(ConfigChange(id: "dpi", label: "Mouse DPI", consoleName: nil, then: dpi(m0.dpi), now: dpi(m1.dpi)))
            }
        }
        return result
    }

    /// "Mouse speed 2.8 → 2.6. Show frame rate On → Off." — at most three, then a count.
    static func sentence(_ changes: [ConfigChange]) -> String {
        guard !changes.isEmpty else { return "No settings changed." }
        let shown = changes.prefix(3).map { $0.then.isEmpty ? "\($0.label): \($0.now)" : "\($0.label) \($0.then) → \($0.now)" }
        let rest = changes.count - shown.count
        return shown.joined(separator: ". ") + "." + (rest > 0 ? " And \(rest) more." : "")
    }
}

// MARK: - Edit as text

struct ConfigTextIssue: Identifiable, Equatable {
    let id: Int
    let line: Int
    let column: Int
    let message: String
    let excerpt: String
    /// Blocking issues stop Apply; warnings describe lines the game reads differently.
    let blocking: Bool
    /// UTF-16 range in the editor text (Latin-1 text is one unit per byte).
    let range: NSRange
}

enum ConfigTextCheck {
    /// Validates editor text exactly as the game would read its bytes.
    static func issues(text: String, dialect: ConfigDialect, catalog: ConfigCatalog?, mode: ConfigCatalogMode) -> (data: Data?, issues: [ConfigTextIssue]) {
        let scalars = Array(text.unicodeScalars)
        var lineStarts = [0]
        for (index, scalar) in scalars.enumerated() where scalar == "\n" || (scalar == "\r" && (index + 1 == scalars.count || scalars[index + 1] != "\n")) {
            lineStarts.append(index + 1)
        }
        func location(_ offset: Int) -> (line: Int, column: Int) {
            let line = (lineStarts.lastIndex { $0 <= offset } ?? 0)
            return (line + 1, offset - lineStarts[line] + 1)
        }
        func excerpt(_ range: Range<Int>) -> String {
            let upper = min(scalars.count, max(range.lowerBound + 1, min(range.upperBound, range.lowerBound + 60)))
            guard range.lowerBound < upper else { return "" }
            var view = String.UnicodeScalarView(); view.append(contentsOf: scalars[range.lowerBound..<upper])
            return String(view).trimmingCharacters(in: .whitespacesAndNewlines)
        }
        var issues: [ConfigTextIssue] = []
        func add(_ range: Range<Int>, _ message: String, blocking: Bool) {
            let place = location(range.lowerBound)
            issues.append(ConfigTextIssue(id: issues.count, line: place.line, column: place.column, message: message, excerpt: excerpt(range),
                                          blocking: blocking, range: NSRange(location: range.lowerBound, length: max(1, range.count))))
        }
        if let bad = scalars.firstIndex(where: { $0.value > 255 }) {
            add(bad..<(bad + 1), "The game stores single-byte text, so this character can’t be saved.", blocking: true)
            return (nil, issues)
        }
        let data = Data(scalars.map { UInt8($0.value) })
        let document = ConfigDocument(data: data, dialect: dialect)
        for diagnostic in document.diagnostics {
            switch diagnostic {
            case .unterminatedQuote(let range): add(range, "This value opens a quote but never closes it.", blocking: true)
            case .lineLimit(let command, _): add(command, "This line is longer than the 4,095 characters the game reads.", blocking: true)
            case .tokenizationFailure(let command, let failure):
                add(command, failure == .tooManyTokens ? "This line has more than 512 words, so the game skips it." : "This line is too long for the game to read.", blocking: true)
            case .commandBufferOverflow: add(65535..<min(data.count, 65536), "The config is longer than the 65,535 characters the game reads.", blocking: true)
            case .ignoredNULSuffix(let range): add(range, "The game stops reading at an invisible NUL character here.", blocking: false)
            }
        }
        if let catalog {
            for command in document.commands where command.kind == .setting && command.tokens.count >= 3 {
                let token = command.tokens[1]
                if let setting = catalog.setting(named: token.text) {
                    if setting.availability == .registered, setting.variant(for: mode) != nil, !setting.isEditable(in: mode) {
                        add(token.range, "The game protects \(setting.title.lowercased()) and normally refuses it from a config.", blocking: false)
                    }
                } else {
                    add(token.range, "The game doesn’t register this setting. It keeps the line, but nothing reads it unless a mod does.", blocking: false)
                }
            }
        }
        return (data, issues.sorted { ($0.line, $0.column) < ($1.line, $1.column) }.enumerated().map {
            ConfigTextIssue(id: $0.offset, line: $0.element.line, column: $0.element.column, message: $0.element.message,
                            excerpt: $0.element.excerpt, blocking: $0.element.blocking, range: $0.element.range)
        })
    }
}

// MARK: - Store

struct ConfigExpertSection: Identifiable {
    let group: ConfigPurposeGroup
    let specs: [ConfigSettingSpec]
    var id: ConfigPurposeGroup { group }
}

/// A player's setup to compare in Expert: values from a signed entry that was fetched,
/// verified and reviewed against the active loadout. Only what Review would tick by default
/// (and settings the person locked) is here; lines needing an OK, blocked lines and choices
/// kept on this Mac never are.
struct ConfigPlayerComparison: Equatable {
    let entryID: String
    let name: String
    let provenance: String
    /// Config text by lowercased setting name.
    let values: [String: String]
}

struct ConfigStudioFailure: Identifiable {
    let id = UUID()
    let title: String
    let message: String
    /// Re-applies the same intent to the latest disk version.
    let retry: (@MainActor () -> Void)?
}

@MainActor final class ConfigStudioModel: ObservableObject {
    /// Optional: preview mode and an initialization error leave Configs unavailable.
    @Published private(set) var session: ConfigStudioSession?
    let mode: ConfigCatalogMode
    let dialect: ConfigDialect
    @Published private(set) var catalog: ConfigCatalog?
    @Published private(set) var catalogError: String?
    private var catalogLoading = false
    private let catalogSource: @Sendable () throws -> ConfigCatalog

    @Published var tab: ConfigStudioTab = .settings
    @Published var level: ConfigDetailLevel = .guided
    @Published var group: ConfigPurposeGroup = .frame
    @Published var filter: ConfigExpertFilter?
    @Published var search = ""
    @Published var compareID: String? {
        didSet {
            if compareID != nil { player = nil }
            if compareID != oldValue { refreshComparison() }
        }
    }
    /// Another of the person's own loadouts, read for Compare.
    @Published private(set) var comparison: ConfigProfileRead?
    /// A player from Discover, compared instead of a loadout.
    @Published private(set) var player: ConfigPlayerComparison?
    /// A setting brought into view from Jump to any setting.
    @Published var highlighted: String?
    @Published var historyFilter: ConfigHistoryFilter = .all
    /// The selected snapshot; nil is “now”.
    @Published var snapshotID: String?
    @Published private(set) var restoredSnapshotID: String?
    @Published var failure: ConfigStudioFailure?
    /// A completed action worth a calm line (export, for example); errors use `failure`.
    @Published var notice: String?
    @Published var textMode = false
    @Published private(set) var text = ""
    @Published private(set) var textIssues: [ConfigTextIssue] = []
    @Published var textSelection: NSRange?
    private var textBase: Data?

    private(set) var specs: [ConfigSettingSpec] = []
    private var specsByName: [String: ConfigSettingSpec] = [:]
    private var protectedCounts: [ConfigPurposeGroup: Int] = [:]
    private var valueCache: (data: Data, values: [String: String])?
    private var diffCache: [String: [ConfigChange]] = [:]
    /// Keyed by the whole current revision, so an absent companion never matches an empty one.
    private var sinceCache: (id: String, base: ConfigProfileVersion, changes: [ConfigChange])?
    /// Input devices for Binds; the review harness and tests supply their own.
    var deviceProvider: @MainActor () -> any InputDeviceProvider = { SystemInputDeviceProvider() }
    var captureSource: @MainActor (NSWindow?) -> (any BindingCaptureEventSource)? = { window in window.map { AppKitBindingEventSource(window: $0) } }
    private(set) lazy var binds = ConfigBindsModel(studio: self, devices: InputDevicesModel(provider: deviceProvider()), makeSource: captureSource)
    private(set) lazy var checkup = ConfigCheckupModel(studio: self)
    /// Speaks a completed change once (VoiceOver). Tests replace it.
    var announce: (String) -> Void = { message in
        guard let app = NSApp else { return }
        NSAccessibility.post(element: app.mainWindow ?? app, notification: .announcementRequested,
                             userInfo: [.announcement: message, .priority: NSAccessibilityPriorityLevel.medium.rawValue])
    }

    init(session: ConfigStudioSession?, dialect: ConfigDialect = .cod2x,
         catalog: @escaping @Sendable () throws -> ConfigCatalog = { try ConfigStudioModel.bundledCatalog() }) {
        self.session = session
        self.dialect = dialect
        mode = dialect == .stock ? .stock : .cod2x
        catalogSource = catalog
    }

    /// Bundled resource in the app; beside the executable in development builds.
    nonisolated static func bundledCatalog() throws -> ConfigCatalog {
        if let catalog = try? ConfigCatalog.load(bundle: .main) { return catalog }
        guard let folder = Bundle.main.executableURL?.deletingLastPathComponent() else { throw ConfigCatalogError.missingResource }
        let url = folder.appendingPathComponent("ConfigCatalog.json")
        guard FileManager.default.fileExists(atPath: url.path) else { throw ConfigCatalogError.missingResource }
        return try ConfigCatalog(data: Data(contentsOf: url))
    }

    /// The review harness hosts a session over a temporary home.
    func attach(_ session: ConfigStudioSession?) {
        self.session = session
        comparison = nil; player = nil; valueCache = nil; diffCache = [:]; sinceCache = nil
    }

    /// Decodes the 2 MB catalog off the main thread; the page shows a loading state meanwhile.
    func loadCatalog() {
        guard catalog == nil, !catalogLoading else { return }
        catalogLoading = true
        let source = catalogSource
        Task { [weak self] in
            let result = await Task.detached(priority: .userInitiated) { Result { try source() } }.value
            self?.finishLoading(result)
        }
    }
    func loadCatalogNow() {
        guard catalog == nil else { return }
        finishLoading(Result { try catalogSource() })
    }
    private func finishLoading(_ result: Result<ConfigCatalog, Error>) {
        catalogLoading = false
        guard catalog == nil else { return }
        switch result {
        case .success(let value): install(value)
        case .failure: catalogError = "The settings catalog is missing or damaged. Reinstall CoD2 Silicon to restore it."
        }
    }
    private func install(_ catalog: ConfigCatalog) {
        var protected: [ConfigPurposeGroup: Int] = [:]
        var built: [ConfigSettingSpec] = []
        for setting in catalog.settings {
            if let spec = ConfigSettingSpec(setting: setting, mode: mode) { built.append(spec) }
            else if setting.availability == .registered, setting.variant(for: mode) != nil { protected[setting.group, default: 0] += 1 }
        }
        specs = built.sorted { $0.title.localizedStandardCompare($1.title) == .orderedAscending }
        specsByName = Dictionary(specs.map { ($0.name.lowercased(), $0) }, uniquingKeysWith: { a, _ in a })
        protectedCounts = protected
        diffCache = [:]; sinceCache = nil
        self.catalog = catalog
        catalogError = nil
    }

    // MARK: Reading

    var profiles: [ConfigProfile] { session?.profiles ?? [] }
    var current: ConfigProfileRead? { session?.current }
    var history: [ConfigHistoryEntry] { session?.history ?? [] }
    /// The game owns its profile config while it runs (or is being launched).
    var isReadOnly: Bool { guard let session else { return true }; return session.launchReserved || !session.runningPIDs.isEmpty }
    var gameRunning: Bool { guard let session else { return false }; return session.launchReserved || !session.runningPIDs.isEmpty }
    var activeProfile: ConfigProfile? { profiles.first { $0.isActive } }
    /// A failed save keeps its draft on screen until it is retried or discarded.
    var document: ConfigDocument? {
        guard let current else { return nil }
        if let session, let draft = session.draft, session.draftProfileID?.caseInsensitiveCompare(current.profile.id) == .orderedSame { return draft }
        return current.document
    }
    var metadata: ConfigStudioMetadata? { current?.metadata }

    func spec(named name: String) -> ConfigSettingSpec? { specsByName[name.lowercased()] }
    func specs(in group: ConfigPurposeGroup) -> [ConfigSettingSpec] { specs.filter { $0.group == group } }
    func protectedCount(in group: ConfigPurposeGroup) -> Int { protectedCounts[group] ?? 0 }
    private static let importance = ["com_maxfps", "cg_drawfps", "sensitivity", "in_rawmouse", "cl_mouseaccel", "m_filter", "m_pitch", "rate", "snaps",
                                     "cl_maxpackets", "cg_drawlagometer", "r_gamma", "r_picmip", "cg_drawcrosshair", "cg_drawcrosshairnames",
                                     "cg_chatheight", "snd_volume", "r_anisotropy", "cg_brass", "cg_marks", "name", "cl_allowdownload"]
    /// Guided entries first, in a fixed order of importance, then the rest by name.
    private func ordered(_ list: [ConfigSettingSpec]) -> [ConfigSettingSpec] {
        func rank(_ spec: ConfigSettingSpec) -> Int { spec.setting.guided ? Self.importance.firstIndex(of: spec.name.lowercased()) ?? Self.importance.count : Self.importance.count + 1 }
        return list.enumerated().sorted { (rank($0.element), $0.offset) < (rank($1.element), $1.offset) }.map(\.element)
    }
    /// Guided shows the catalog's guided entries in a fixed order of importance.
    func guidedSpecs(in group: ConfigPurposeGroup) -> [ConfigSettingSpec] {
        ordered(specs.filter { $0.group == group && $0.setting.guided && !$0.launcherOwned })
    }

    /// Every direct assignment's effective value, computed once per document.
    private func values(of document: ConfigDocument) -> [String: String] {
        let data = document.serialized()
        if let cache = valueCache, cache.data == data { return cache.values }
        var names = Set<String>()
        for command in document.commands {
            if command.kind == .setting, command.tokens.count >= 3 { names.insert(command.tokens[1].text.lowercased()) }
            else if command.kind == .unknown, command.tokens.count >= 2 { names.insert(command.tokens[0].text.lowercased()) }
        }
        var values: [String: String] = [:]
        for name in names { values[name] = document.effectiveSetting(named: name) }
        valueCache = (data, values)
        return values
    }
    func state(_ spec: ConfigSettingSpec, in document: ConfigDocument? = nil, metadata: ConfigStudioMetadata? = nil) -> ConfigSettingState {
        let raw: String?
        if let document { raw = document.effectiveSetting(named: spec.name) }
        else { raw = self.document.flatMap { values(of: $0)[spec.name.lowercased()] } }
        let metadata = metadata ?? self.metadata
        return ConfigSettingState(spec: spec, raw: raw,
                                  locked: metadata?.locks.contains { $0.caseInsensitiveCompare(spec.name) == .orderedSame } ?? false,
                                  note: metadata?.notes.first { $0.key.caseInsensitiveCompare(spec.name) == .orderedSame }?.value)
    }

    var changedCount: Int { specs.filter { !$0.launcherOwned && state($0).changed }.count }
    var lockedCount: Int { metadata?.locks.count ?? 0 }

    /// Expert rows: one group, or Changed/Locked across groups, narrowed by search.
    func expertSections() -> [ConfigExpertSection] {
        let query = search.trimmingCharacters(in: .whitespaces)
        let rows = specs.filter { spec in
            if !query.isEmpty { return matches(spec, query) }
            switch filter {
            case .changed: return !spec.launcherOwned && state(spec).changed
            case .locked: return state(spec).locked
            case nil: return spec.group == group
            }
        }
        return ConfigPurposeGroup.allCases.compactMap { group in
            let list = ordered(rows.filter { $0.group == group })
            return list.isEmpty ? nil : ConfigExpertSection(group: group, specs: list)
        }
    }
    func matches(_ spec: ConfigSettingSpec, _ query: String) -> Bool {
        [spec.title, spec.setting.summary, spec.name].contains { $0.localizedCaseInsensitiveContains(query) }
    }
    /// Command-K results: title, description and console name.
    func jumpResults(_ query: String) -> [ConfigSettingSpec] {
        let query = query.trimmingCharacters(in: .whitespaces)
        guard !query.isEmpty else { return Array(specs.filter { $0.setting.guided }.prefix(8)) }
        let titleMatches = specs.filter { $0.title.localizedCaseInsensitiveContains(query) || $0.name.localizedCaseInsensitiveContains(query) }
        let summaryMatches = specs.filter { !titleMatches.map(\.name).contains($0.name) && $0.setting.summary.localizedCaseInsensitiveContains(query) }
        return Array((titleMatches + summaryMatches).prefix(30))
    }
    /// Opens Expert at the setting. With unapplied text edits, it selects the setting's line instead.
    func jump(to spec: ConfigSettingSpec) {
        tab = .settings; level = .expert
        if textMode && textEdited {
            let scalars = text.unicodeScalars
            guard scalars.allSatisfy({ $0.value <= 255 }) else { return }
            let document = ConfigDocument(data: Data(scalars.map { UInt8($0.value) }), dialect: dialect)
            if let command = document.commands.last(where: { $0.kind == .setting && $0.tokens.count >= 2 && $0.tokens[1].text.caseInsensitiveCompare(spec.name) == .orderedSame }) {
                textSelection = NSRange(location: command.range.lowerBound, length: command.range.count)
            }
            return
        }
        textMode = false; textBase = nil; textIssues = []
        filter = nil; search = ""; group = spec.group; highlighted = spec.name
    }

    /// Rereads the compared loadout; the page calls this when it appears or the loadout changes.
    func refreshComparison() {
        guard let id = compareID, let session, id.caseInsensitiveCompare(current?.profile.id ?? "") != .orderedSame,
              profiles.contains(where: { $0.id == id }), let read = try? session.readProfile(id) else { comparison = nil; return }
        if read.version != comparison?.version || read.profile != comparison?.profile { comparison = read }
    }
    func theirs(_ spec: ConfigSettingSpec) -> ConfigSettingState? {
        let result: ConfigSettingState
        if let player {
            // Only settings their config actually sets; a missing line isn't their default.
            guard let value = player.values[spec.name.lowercased()] else { return nil }
            result = ConfigSettingState(spec: spec, raw: value, locked: false, note: nil)
        } else {
            guard let other = comparison else { return nil }
            result = state(spec, in: other.document, metadata: other.metadata)
        }
        let mine = state(spec)
        guard let a = mine.value, let b = result.value, !ConfigValues.same(spec, a, b) else { return nil }
        return result
    }
    /// Opens Expert comparing with a player's reviewed setup. Nothing is written.
    func compare(with player: ConfigPlayerComparison) {
        compareID = nil
        self.player = player
        tab = .settings; level = .expert
        // The markers live on the controls; unapplied text edits keep the text editor open.
        if textMode && !textEdited { toggleText() }
    }
    func stopComparing() { player = nil; compareID = nil }
    /// Whose values the comparison markers show.
    var comparisonName: String? { player?.name ?? comparison?.profile.displayName }
    /// Where the compared values came from: the player's catalog entry, or the credit a loadout
    /// saved when it was imported. Nil for a loadout made here.
    var comparisonProvenance: String? {
        if let player { return player.provenance }
        guard let origin = comparison?.metadata.origin, origin.kind == .imported, let credit = origin.credit else { return nil }
        return "Imported · credit \(credit)"
    }

    // MARK: History

    var visibleHistory: [ConfigHistoryEntry] { history.filter { historyFilter.includes($0.source) } }
    func changes(in entry: ConfigHistoryEntry) -> [ConfigChange] {
        if let cached = diffCache[entry.id] { return cached }
        let result = ConfigDiff.changes(from: entry.beforeData, entry.beforeMetadata, to: entry.data, entry.metadata,
                                        chains: (entry.beforeChain, entry.chain), specs: specsByName, catalog: catalog, dialect: dialect)
        diffCache[entry.id] = result
        return result
    }
    /// What differs between a snapshot's saved state and now.
    func changesSince(_ entry: ConfigHistoryEntry) -> [ConfigChange] {
        guard let current else { return [] }
        let base = current.version
        if let cache = sinceCache, cache.id == entry.id, cache.base == base { return cache.changes }
        let result = ConfigDiff.changes(from: entry.data, entry.metadata, to: current.data, current.version.metadata,
                                        chains: (entry.chain, current.version.chain), specs: specsByName, catalog: catalog, dialect: dialect)
        sinceCache = (entry.id, base, result)
        return result
    }
    /// The exact revision a restore would write: config, metadata and advanced-bind companion.
    func restoreTarget(_ entry: ConfigHistoryEntry, revision: ConfigSnapshotRevision) -> ConfigProfileVersion {
        revision == .before ? ConfigProfileVersion(config: entry.beforeData, metadata: entry.beforeMetadata, chain: entry.beforeChain)
                            : ConfigProfileVersion(config: entry.data, metadata: entry.metadata, chain: entry.chain)
    }
    /// Restore is offered when it would change something on disk, companion-only changes included.
    func canRestore(_ entry: ConfigHistoryEntry, revision: ConfigSnapshotRevision) -> Bool {
        guard let current, !isReadOnly else { return false }
        return restoreTarget(entry, revision: revision) != current.version
    }
    func title(of entry: ConfigHistoryEntry) -> String {
        switch entry.source {
        case .gameSession: "Changed while you played"
        case .migration: "Brought over your launcher settings"
        case .restore: "Restored a snapshot"
        default: entry.summary
        }
    }

    // MARK: Intents (one transaction per committed gesture)

    func refresh() {
        guard let session else { return }
        do { try session.refresh() } catch { fail("Couldn’t read this loadout", error) }
    }

    func select(_ id: String) {
        guard let session, id != current?.profile.id else { return }
        do {
            try session.focus(id)
            if compareID == id { compareID = nil } else { refreshComparison() }
            snapshotID = nil; restoredSnapshotID = nil; highlighted = nil
            if textMode { loadText() }
        } catch { fail("Couldn’t open this loadout", error) }
    }

    func useForNextGame() {
        guard let session, let current, !isReadOnly else { return }
        do {
            try session.selectProfile(current.profile.id)
            announce("\(current.profile.displayName) is in use for your next game.")
        } catch { fail("Couldn’t choose this loadout", error) }
    }

    /// Sets one setting's config text. Locks never block the person's own edit.
    func set(_ spec: ConfigSettingSpec, to value: String, summary: String? = nil, undo: UndoManager? = nil) {
        guard !spec.launcherOwned else { return }
        let previous = document?.effectiveSetting(named: spec.name)
        if let previous, ConfigValues.same(spec, previous, value), previous == value { return }
        let summary = summary ?? "Changed \(spec.title)"
        commit(summary: summary, undo: undo, undoName: "Change \(spec.title)", retry: { [weak self] in self?.set(spec, to: value, summary: summary, undo: undo) },
               edit: { try $0.setSetting(named: spec.name, value: value) }, revert: { [weak self] in
            self?.restoreRaw(spec, previous, undo: undo)
        })
    }
    func reset(_ spec: ConfigSettingSpec, undo: UndoManager? = nil) {
        guard let value = spec.defaultText else { return }
        set(spec, to: value, summary: "Reset \(spec.title)", undo: undo)
    }
    /// Why Take Theirs can't change this setting, or nil when it can. The button and the action
    /// share it: the game running, the launcher, a lock, the catalog's server-controlled flag or
    /// the rules chosen in Check-up keep the person's own value.
    func takeTheirsBlock(_ spec: ConfigSettingSpec) -> String? {
        if isReadOnly { return "Configs are read-only while Call of Duty 2 is running." }
        if spec.launcherOwned { return "CoD2 Silicon sets this from Settings each time the game starts." }
        if state(spec).locked { return "Locked, so it stays yours." }
        if spec.setting.variant(for: mode)?.serverControlled == true { return "The server sets this when you join, so it stays yours." }
        if let rules = checkup.rules, rules.isServerOwned(spec.name, rulesetIDs: checkup.rulesetIDs) {
            return "Set by the server under \(checkup.selectionTitle), so it stays yours."
        }
        return nil
    }
    func takeTheirs(_ spec: ConfigSettingSpec, undo: UndoManager? = nil) {
        guard !isReadOnly else { fail("Quit the game to change this loadout", ConfigProfileError.gameRunning); return }
        guard takeTheirsBlock(spec) == nil, let theirs = theirs(spec), let value = theirs.value, let source = comparisonName,
              let session, let profileID = current?.profile.id else { return }
        takeTheirs(spec, to: value, source: source, session: session, profileID: profileID, undo: undo)
    }
    /// Retry the reviewed comparison value, even if another comparison is selected, but
    /// never carry it to a different loadout or bypass its current ownership guards.
    private func takeTheirs(_ spec: ConfigSettingSpec, to value: String, source: String, session: ConfigStudioSession,
                            profileID: String, undo: UndoManager?) {
        guard self.session === session, current?.profile.id.caseInsensitiveCompare(profileID) == .orderedSame else {
            failure = ConfigStudioFailure(title: "Couldn’t take \(spec.title) from \(source)",
                                          message: "This loadout is no longer selected. Open it and take the value again.", retry: nil)
            return
        }
        if let reason = takeTheirsBlock(spec) {
            failure = ConfigStudioFailure(title: "Couldn’t take \(spec.title) from \(source)", message: reason, retry: nil)
            return
        }
        let previous = document?.effectiveSetting(named: spec.name)
        if let previous, ConfigValues.same(spec, previous, value), previous == value { return }
        let summary = "Took \(spec.title) from \(source)"
        commit(summary: summary, undo: undo, undoName: "Change \(spec.title)", retry: { [weak self] in
            guard let self, self.failure == nil else { return }
            self.takeTheirs(spec, to: value, source: source, session: session, profileID: profileID, undo: undo)
        }, edit: { try $0.setSetting(named: spec.name, value: value) }, revert: { [weak self] in
            self?.restoreRaw(spec, previous, undo: undo)
        })
    }
    private func restoreRaw(_ spec: ConfigSettingSpec, _ raw: String?, undo: UndoManager?) {
        let current = document?.effectiveSetting(named: spec.name)
        commit(summary: "Undid a change to \(spec.title)", undo: undo, undoName: "Change \(spec.title)", retry: nil, edit: { document in
            if let raw { try document.setSetting(named: spec.name, value: raw) } else { try document.removeSetting(named: spec.name) }
        }, revert: { [weak self] in self?.restoreRaw(spec, current, undo: undo) })
    }

    func setLock(_ spec: ConfigSettingSpec, _ locked: Bool, undo: UndoManager? = nil) {
        guard var meta = metadata else { return }
        meta.locks = meta.locks.filter { $0.caseInsensitiveCompare(spec.name) != .orderedSame }
        if locked { meta.locks.insert(spec.name) }
        commitMetadata(meta, summary: locked ? "Locked \(spec.title)" : "Unlocked \(spec.title)", undo: undo, undoName: locked ? "Lock" : "Unlock")
    }
    func setNote(_ spec: ConfigSettingSpec, _ note: String, undo: UndoManager? = nil) {
        guard var meta = metadata else { return }
        let text = note.trimmingCharacters(in: .whitespacesAndNewlines)
        let key = meta.notes.keys.first { $0.caseInsensitiveCompare(spec.name) == .orderedSame } ?? spec.name
        if meta.notes[key] == (text.isEmpty ? nil : text) { return }
        meta.notes[key] = text.isEmpty ? nil : text
        commitMetadata(meta, summary: text.isEmpty ? "Removed the note on \(spec.title)" : "Noted \(spec.title)", undo: undo, undoName: "Note")
    }
    func setDPI(_ dpi: Double?, undo: UndoManager? = nil) {
        guard var meta = metadata else { return }
        let value = dpi.flatMap { $0.isFinite && $0 > 0 ? $0 : nil }
        guard meta.dpi != value else { return }
        meta.dpi = value
        commitMetadata(meta, summary: "Set mouse DPI", undo: undo, undoName: "Mouse DPI")
    }
    func rename(_ name: String, undo: UndoManager? = nil) {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard var meta = metadata, !trimmed.isEmpty, trimmed != meta.displayName else { return }
        meta.displayName = trimmed
        commitMetadata(meta, summary: "Renamed to \(trimmed)", undo: undo, undoName: "Rename")
    }
    /// Records the current state as a History row (the board's “Save a snapshot now”).
    func saveSnapshot() {
        guard let current else { return }
        commit(summary: "Saved a snapshot", undo: nil, undoName: "", retry: nil, metadata: current.metadata, edit: { _ in }, revert: nil)
    }
    private func commitMetadata(_ meta: ConfigStudioMetadata, summary: String, undo: UndoManager?, undoName: String) {
        let previous = metadata
        commit(summary: summary, undo: undo, undoName: undoName, retry: { [weak self] in self?.commitMetadataAgain(meta, summary: summary, undo: undo, undoName: undoName) },
               metadata: meta, edit: { _ in }, revert: { [weak self] in
            if let previous { self?.commitMetadataAgain(previous, summary: "Undid: \(summary)", undo: undo, undoName: undoName) }
        })
    }
    private func commitMetadataAgain(_ meta: ConfigStudioMetadata, summary: String, undo: UndoManager?, undoName: String) {
        commitMetadata(meta, summary: summary, undo: undo, undoName: undoName)
    }

    /// The one write path for ordinary controls: apply the edit to the latest saved
    /// document and save it as one History transaction.
    private func commit(summary: String, undo: UndoManager?, undoName: String, retry: (@MainActor () -> Void)?,
                        metadata: ConfigStudioMetadata? = nil, edit: (inout ConfigDocument) throws -> Void, revert: (@MainActor () -> Void)?) {
        guard let session, let current else { return }
        guard !isReadOnly else { fail("Quit the game to change this loadout", ConfigProfileError.gameRunning); return }
        guard session.draft == nil else {
            failure = failure ?? ConfigStudioFailure(title: "An earlier change isn’t saved yet", message: "Try it again or discard it first.", retry: nil)
            return
        }
        var updated = current.document
        do { try edit(&updated) } catch { fail("Couldn’t change this setting", error); return }
        do {
            try session.update(document: updated, metadata: metadata, summary: summary)
            failure = nil
            diffCache = [:]
            if let revert, let undo {
                undo.registerUndo(withTarget: self) { _ in revert() }
                undo.setActionName(undoName)
            }
            announce("Saved. \(summary).")
        } catch {
            fail("Couldn’t save: \(summary)", error, retry: retry)
        }
    }

    /// After a model unit's own transaction (binds, advanced binds): announce it,
    /// and let Command-Z put back exactly what was there before.
    func didSave(_ entry: ConfigHistoryEntry, name: String, undo: UndoManager?) {
        failure = nil; diffCache = [:]
        announce("Saved. \(title(of: entry)).")
        guard let undo else { return }
        undo.registerUndo(withTarget: self) { model in model.undoEntry(entry.id, name: name) }
        undo.setActionName(name)
    }
    private func undoEntry(_ id: String, name: String) {
        guard let session, let row = session.history.first(where: { $0.id == id }), current?.version.config == row.data,
              current?.version.metadata == row.metadata, current?.version.chain == row.chain else {
            failure = ConfigStudioFailure(title: "Nothing to undo", message: "This loadout changed since. Pick a snapshot in History instead.", retry: nil)
            return
        }
        do { try session.restore(snapshotID: id, revision: .before); diffCache = [:]; announce("Undid \(name.lowercased()).") }
        catch { fail("Couldn’t undo", error) }
    }

    private func fail(_ title: String, _ error: Error, retry: (@MainActor () -> Void)? = nil) {
        failure = ConfigStudioFailure(title: title, message: error.localizedDescription, retry: retry)
    }
    /// Discards a failed change and re-applies the same intent to the latest disk version.
    func retryFailure() {
        let retry = failure?.retry
        discardFailedChange()
        retry?()
    }
    func discardFailedChange() {
        failure = nil
        guard let session else { return }
        do { if session.draft != nil { try session.discardDraft() } else { try session.refresh() } }
        catch { fail("Couldn’t reload this loadout", error) }
        if textMode && !textEdited { loadText() }
    }

    // MARK: New loadouts

    /// A profile folder name for a display name: what the game's own profile menu shows.
    static func identifier(for name: String, existing: [String]) -> String {
        let folded = name.folding(options: [.diacriticInsensitive, .widthInsensitive], locale: Locale(identifier: "en_US_POSIX"))
        var id = ""
        for scalar in folded.unicodeScalars {
            if scalar.isASCII, CharacterSet.alphanumerics.contains(scalar) { id.unicodeScalars.append(scalar) }
            else if [".", "-", "_"].contains(scalar) || CharacterSet.whitespaces.contains(scalar) {
                if !id.isEmpty, !id.hasSuffix("_") { id.append(scalar == "." || scalar == "-" ? Character(scalar) : "_") }
            }
        }
        while let last = id.last, !last.isLetter && !last.isNumber { id.removeLast() }
        if id.isEmpty { id = "Loadout" }
        id = String(id.prefix(36))
        var candidate = id, suffix = 2
        while existing.contains(where: { $0.caseInsensitiveCompare(candidate) == .orderedSame }) { candidate = "\(id)_\(suffix)"; suffix += 1 }
        return candidate
    }

    /// Creates from the game's defaults or as a copy of the loadout on screen, then shows it.
    /// It doesn't become the active loadout until the person chooses it.
    @discardableResult
    func createLoadout(named name: String, copyingCurrent: Bool) -> Bool {
        guard let session, !isReadOnly else { return false }
        let display = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !display.isEmpty else { return false }
        let id = Self.identifier(for: display, existing: profiles.map(\.id))
        do {
            let created = copyingCurrent ? try session.duplicateProfile(as: id, displayName: display) : try session.createProfile(id: id, displayName: display)
            try session.focus(created.profile.id)
            tab = .settings; snapshotID = nil
            announce("Created \(display).")
            return true
        } catch { fail("Couldn’t create \(display)", error); return false }
    }

    // MARK: Restore

    func restore(_ entry: ConfigHistoryEntry, revision: ConfigSnapshotRevision = .after, undo: UndoManager? = nil) {
        guard let session, !isReadOnly else { return }
        do {
            try session.restore(snapshotID: entry.id, revision: revision)
            restoredSnapshotID = entry.id
            diffCache = [:]
            if let undo, let row = session.history.first, row.source == .restore {
                undo.registerUndo(withTarget: self) { model in model.undoRestore(row.id) }
                undo.setActionName("Restore")
            }
            announce("Restored. Your previous state is saved as a snapshot.")
        } catch { fail("Couldn’t restore this snapshot", error) }
    }
    private func undoRestore(_ id: String) {
        guard let session, let row = session.history.first(where: { $0.id == id }), current?.version == restoreTarget(row, revision: .after) else {
            failure = ConfigStudioFailure(title: "Nothing to undo", message: "This loadout changed after the restore. Pick a snapshot in History instead.", retry: nil)
            return
        }
        do { try session.restore(snapshotID: id, revision: .before); restoredSnapshotID = nil; announce("Undid the restore.") }
        catch { fail("Couldn’t undo the restore", error) }
    }

    // MARK: Edit as text

    var textEdited: Bool { textBase != nil && encodedText != textBase }
    var textBlocked: Bool { textIssues.contains { $0.blocking } }
    /// The loadout changed on disk since the text was loaded.
    var textStale: Bool { guard let textBase, let current else { return false }; return current.data != textBase }
    private var encodedText: Data? {
        let scalars = text.unicodeScalars
        guard scalars.allSatisfy({ $0.value <= 255 }) else { return nil }
        return Data(scalars.map { UInt8($0.value) })
    }

    func toggleText() {
        if textMode { textMode = false; textBase = nil; textIssues = [] } else { textMode = true; loadText() }
    }
    func loadText() {
        guard let current else { return }
        textBase = current.data
        text = ConfigDocument.text(current.data)
        validateText()
    }
    func editText(_ value: String) {
        text = value
        validateText()
    }
    private func validateText() {
        textIssues = ConfigTextCheck.issues(text: text, dialect: dialect, catalog: catalog, mode: mode).issues
    }
    /// One explicit transaction after validation; the text stays on screen if saving fails.
    func applyText(undo: UndoManager? = nil) {
        guard let session, let current, !isReadOnly, textEdited, !textBlocked, let data = encodedText else { return }
        guard !textStale else {
            failure = ConfigStudioFailure(title: "This loadout changed while you were editing",
                                          message: "Reload it to see the new version. Your text edits are discarded when you reload.", retry: nil)
            return
        }
        let before = current.document
        do {
            try session.update(document: ConfigDocument(data: data, dialect: dialect), summary: "Edited as text")
            failure = nil; diffCache = [:]
            if let undo {
                let written = data
                undo.registerUndo(withTarget: self) { model in model.undoText(before: before, written: written) }
                undo.setActionName("Edit as Text")
            }
            loadText()
            announce("Saved. Edited as text.")
        } catch {
            // Keep the person's text; the session keeps the draft for retry or discard.
            fail("Couldn’t save your text edits", error, retry: { [weak self] in self?.applyText(undo: undo) })
        }
    }
    private func undoText(before: ConfigDocument, written: Data) {
        guard let session, current?.data == written else {
            failure = ConfigStudioFailure(title: "Nothing to undo", message: "This loadout changed after your text edit. Pick a snapshot in History instead.", retry: nil)
            return
        }
        do { try session.update(document: before, summary: "Undid a text edit"); if textMode { loadText() } }
        catch { fail("Couldn’t undo the text edit", error) }
    }
    func revertText() { loadText() }
}
