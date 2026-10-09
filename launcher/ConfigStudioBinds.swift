import AppKit
import Foundation

// The Binds tab's view store. Key identity, capture lifecycle and edits come from
// B1 (ConfigBindings, InputDevices); advanced binds from B2 (ConfigChains). This
// type owns only presentation state and turns reviewed proposals into guarded
// session transactions.

enum ConfigBindsLayout: String, CaseIterable, Identifiable {
    case list, visual
    var id: String { rawValue }
    var title: String { self == .list ? "List" : "Visual" }
    var symbol: String { self == .list ? "list.bullet" : "keyboard" }
}

extension ConfigBindingGroup {
    var title: String {
        switch self { case .movement: "Movement"; case .combat: "Combat"; case .communication: "Communication"; case .gameUI: "Game & UI" }
    }
    var role: StudioKeyRole {
        switch self { case .movement: .movement; case .combat: .combat; case .communication: .communication; case .gameUI: .game }
    }
}

/// What a running capture fills in.
enum ConfigCaptureTarget: Equatable {
    case slot(action: String, slot: ConfigBindingSlot)
    /// Visual: press a key to select it on the drawing.
    case find
    /// Advanced binds: the key a chain uses.
    case chainKey
}

/// Wraps the real (or a test) event source so the store learns about every
/// capture state change without observing the capture from a view.
@MainActor final class NotifyingCaptureSource: BindingCaptureEventSource {
    private let inner: any BindingCaptureEventSource
    private let notify: @MainActor () -> Void
    init(inner: any BindingCaptureEventSource, notify: @escaping @MainActor () -> Void) { self.inner = inner; self.notify = notify }
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition) {
        let notify = notify
        inner.start { event in
            let disposition = handler(event)
            notify()
            return disposition
        }
    }
    func stop() { inner.stop() }
}

// MARK: - Keyboard drawing

/// Apple keyboard positions by macOS virtual key code, in key units. Labels come
/// from the current input source; identities from B1's mapping. Never persisted.
struct ConfigKeyboardKey: Identifiable, Equatable {
    let virtualKey: UInt16
    let x: Double, y: Double, width: Double, height: Double
    var id: UInt16 { virtualKey }
}

enum ConfigKeyboardLayout {
    /// Rows of (virtual key, width); nil key is a gap. Heights: function row 0.6, others 1.
    static func keys(shape: KeyboardShape, size: KeyboardSize) -> [ConfigKeyboardKey] {
        let compactTop = size == .percent60 || size == .percent65
        var rows: [(height: Double, keys: [(UInt16?, Double)])] = []
        if !compactTop {
            rows.append((0.6, [(53, 1.5)] + [122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111].map { (UInt16?($0), 1.0) }))
        }
        let iso = shape == .iso, jis = shape == .jis
        let numberRow: [(UInt16?, Double)] = [(iso ? 10 : 50, 1)] + [18, 19, 20, 21, 23, 22, 26, 28, 25, 29, 27, 24].map { (UInt16?($0), 1.0) }
            + (jis ? [(93, 1), (51, 1)] : [(51, 1.5)])
        rows.append((1, numberRow))
        rows.append((1, [(48, 1.5)] + [12, 13, 14, 15, 17, 16, 32, 34, 31, 35, 33, 30].map { (UInt16?($0), 1.0) } + (iso || jis ? [(36, 1.0)] : [(42, 1.0)])))
        rows.append((1, [(57, 1.75)] + [0, 1, 2, 3, 5, 4, 38, 40, 37, 41, 39].map { (UInt16?($0), 1.0) } + (iso || jis ? [(42, 1.0), (nil, 0.75)] : [(36, 1.75)])))
        rows.append((1, (iso ? [(56, 1.25), (50, 1.0)] : [(56, 2.25)]) + [6, 7, 8, 9, 11, 45, 46, 43, 47, 44].map { (UInt16?($0), 1.0) }
            + (jis ? [(94, 1.0), (60, 1.25)] : [(60, 2.25)])))
        rows.append((1, [(63, 1), (59, 1), (58, 1), (55, 1.25)] + (jis ? [(102, 1.0), (49, 3.0), (104, 1.0)] : [(49, 5.0)])
            + [(54, 1.25), (61, 1), (123, 1), (nil, 1), (124, 1)]))
        var result: [ConfigKeyboardKey] = [], y = 0.0
        let gap = 0.1
        for row in rows {
            var x = 0.0
            for (key, width) in row.keys {
                if let key {
                    if (iso || jis) && key == 36 {
                        // The tall Return spans this row and the next.
                        result.append(.init(virtualKey: key, x: x, y: y, width: width + 0.25, height: row.height * 2 + gap))
                    } else {
                        result.append(.init(virtualKey: key, x: x, y: y, width: width, height: row.height))
                    }
                }
                x += width + gap
            }
            y += row.height + gap
        }
        // Inverted-T arrows share the bottom row's last three positions.
        if let left = result.first(where: { $0.virtualKey == 123 }) {
            let half = (left.height - gap) / 2, x = left.x + left.width + gap
            result.append(.init(virtualKey: 126, x: x, y: left.y, width: 1, height: half))
            result.append(.init(virtualKey: 125, x: x, y: left.y + half + gap, width: 1, height: half))
        }
        if size == .full || size == .tkl {
            let right = (result.map { $0.x + $0.width }.max() ?? 15) + 0.6
            let top = compactTop ? 0 : 0.7
            for (index, key) in [114, 115, 116].enumerated() { result.append(.init(virtualKey: UInt16(key), x: right + Double(index) * 1.1, y: top, width: 1, height: 1)) }
            for (index, key) in [117, 119, 121].enumerated() { result.append(.init(virtualKey: UInt16(key), x: right + Double(index) * 1.1, y: top + 1.1, width: 1, height: 1)) }
            if size == .full {
                let pad = right + 3.9
                let grid: [[UInt16]] = [[71, 81, 75, 67], [89, 91, 92, 78], [86, 87, 88, 69], [83, 84, 85]]
                for (r, row) in grid.enumerated() {
                    for (c, key) in row.enumerated() { result.append(.init(virtualKey: key, x: pad + Double(c) * 1.1, y: top + Double(r) * 1.1, width: 1, height: 1)) }
                }
                result.append(.init(virtualKey: 76, x: pad + 3.3, y: top + 3.3, width: 1, height: 2.1))
                result.append(.init(virtualKey: 82, x: pad, y: top + 4.4, width: 2.1, height: 1))
                result.append(.init(virtualKey: 65, x: pad + 2.2, y: top + 4.4, width: 1, height: 1))
            }
        }
        return result
    }
}

// MARK: - Advanced binds drafts

/// An editable advanced bind. `chain` is nil until it is complete enough for B2.
struct ConfigChainDraft: Equatable {
    enum Kind: String, CaseIterable, Identifiable {
        case several, toggle, cycle
        var id: String { rawValue }
        var title: String {
            switch self { case .several: "Runs several commands"; case .toggle: "Toggle"; case .cycle: "Cycle on each press" }
        }
        var short: String { switch self { case .several: "Several"; case .toggle: "Toggle"; case .cycle: "Cycle" } }
    }
    enum StepKind: String, CaseIterable, Identifiable {
        case name, say, team, action, command
        var id: String { rawValue }
        var title: String {
            switch self {
            case .name: "Set my name to"; case .say: "Say to everyone"; case .team: "Say to team"; case .action: "Do an action"; case .command: "Run a console command"
            }
        }
    }
    struct Step: Equatable, Identifiable {
        var id = UUID()
        var kind: StepKind
        var text: String
        /// The exact command line B2 receives.
        var command: String {
            let trimmed = text.trimmingCharacters(in: .whitespaces)
            switch kind {
            case .name: return "name " + Self.quoted(trimmed)
            case .say: return "say " + Self.quoted(trimmed)
            case .team: return "say_team " + Self.quoted(trimmed)
            case .action, .command: return trimmed
            }
        }
        static func quoted(_ text: String) -> String { text.contains(" ") ? "\"\(text)\"" : text }
        var isEmpty: Bool { text.trimmingCharacters(in: .whitespaces).isEmpty }
    }
    var id: String
    var isNew: Bool
    var key: String = ""
    var kind: Kind = .several
    var commands: [Step] = [Step(kind: .action, text: "+gostand")]
    var setting = ""
    var values: [String] = []
    var steps: [Step] = [Step(kind: .say, text: "")]

    var chain: ConfigChain? {
        guard !key.isEmpty else { return nil }
        switch kind {
        case .several:
            let lines = commands.filter { !$0.isEmpty }.map(\.command)
            return lines.isEmpty ? nil : ConfigChain(id: id, key: key, behavior: .commands(lines))
        case .toggle:
            return setting.isEmpty || values.count < 2 ? nil : ConfigChain(id: id, key: key, behavior: .settingCycle(name: setting, values: values))
        case .cycle:
            let lines = steps.filter { !$0.isEmpty }.map { [$0.command] }
            return lines.count < 2 ? nil : ConfigChain(id: id, key: key, behavior: .commandCycle(steps: lines))
        }
    }
    init(newID existing: [String]) {
        var number = existing.count + 1
        while existing.contains("bind_\(number)") { number += 1 }
        id = "bind_\(number)"; isNew = true
    }
    init(chain: ConfigChain) {
        id = chain.id; isNew = false; key = chain.key
        switch chain.behavior {
        case .commands(let lines):
            kind = .several; commands = lines.map { Self.step($0) }
        case .settingCycle(let name, let values):
            kind = .toggle; setting = name; self.values = values
        case .commandCycle(let steps):
            kind = .cycle; self.steps = steps.map { Self.step($0.joined(separator: "; ")) }
        }
    }
    static func step(_ line: String) -> Step {
        func unquote(_ text: String) -> String { text.count >= 2 && text.hasPrefix("\"") && text.hasSuffix("\"") ? String(text.dropFirst().dropLast()) : text }
        let parts = line.split(separator: " ", maxSplits: 1).map(String.init)
        switch parts.first?.lowercased() {
        case "name" where parts.count == 2: return Step(kind: .name, text: unquote(parts[1]))
        case "say" where parts.count == 2: return Step(kind: .say, text: unquote(parts[1]))
        case "say_team" where parts.count == 2: return Step(kind: .team, text: unquote(parts[1]))
        default: return Step(kind: parts.count == 1 ? .action : .command, text: line)
        }
    }
}

/// A plain reading of one leaf command for the press/release preview.
struct ConfigCommandWords: Equatable { let text: String; let literal: Bool }

// MARK: - Store

@MainActor final class ConfigBindsModel: ObservableObject {
    unowned let studio: ConfigStudioModel
    let devices: InputDevicesModel
    /// The window the editor lives in; capture listens to it alone.
    weak var window: NSWindow?
    private let makeSource: @MainActor (NSWindow?) -> (any BindingCaptureEventSource)?

    @Published var layout: ConfigBindsLayout = .list
    @Published var query = ""
    @Published private(set) var capture: ConfigBindingCapture?
    @Published private(set) var target: ConfigCaptureTarget?
    @Published private(set) var captureNote: String?
    /// A proposal that displaces another binding, waiting for the person's decision.
    @Published private(set) var review: ConfigBindingEdit?
    @Published var selectedKey: EngineKey? = EngineKey(spelling: "w")
    @Published var pointer: InputDeviceKind = .mouse
    @Published var keyboardID: String?

    @Published var chainID: String?
    @Published private(set) var draft: ConfigChainDraft?
    @Published private(set) var chainEdit: ConfigChainEdit?
    @Published private(set) var chainError: String?
    @Published private(set) var chainRisks: [ConfigImportRisk] = []
    @Published private(set) var chainBlocked = false
    @Published var showLines = false

    private var bindingsCache: (data: Data, value: ConfigBindings)?
    private var chainsCache: (version: ConfigProfileVersion, value: Result<[ConfigChain], Error>)?

    init(studio: ConfigStudioModel, devices: InputDevicesModel,
         makeSource: @escaping @MainActor (NSWindow?) -> (any BindingCaptureEventSource)? = { window in window.map { AppKitBindingEventSource(window: $0) } }) {
        self.studio = studio; self.devices = devices; self.makeSource = makeSource
    }

    // MARK: Reading

    var bindings: ConfigBindings? {
        guard let catalog = studio.catalog, let current = studio.current else { return nil }
        if let cache = bindingsCache, cache.data == current.data { return cache.value }
        let value = ConfigBindings(document: current.document, catalog: catalog, mode: studio.mode)
        bindingsCache = (current.data, value)
        return value
    }
    func rows(in group: ConfigBindingGroup) -> [ConfigBindingRow] {
        guard let bindings else { return [] }
        let text = query.trimmingCharacters(in: .whitespaces)
        return bindings.actions.filter { $0.group == group }.compactMap { bindings.row(for: $0.name) }.filter { row in
            text.isEmpty || row.action.title.localizedCaseInsensitiveContains(text)
                || row.keys.contains { keyName($0).localizedCaseInsensitiveContains(text) || $0.spelling.localizedCaseInsensitiveContains(text) }
        }.sorted { $0.action.title.localizedStandardCompare($1.action.title) == .orderedAscending }
    }
    /// Keys whose binding is something other than one known action (scripts, chains, imports).
    var otherEntries: [ConfigBindingEntry] {
        guard let bindings else { return [] }
        let text = query.trimmingCharacters(in: .whitespaces)
        return bindings.entries.filter { $0.actionName == nil }.filter {
            text.isEmpty || keyName($0.key).localizedCaseInsensitiveContains(text) || $0.text.localizedCaseInsensitiveContains(text)
        }
    }
    func action(named name: String?) -> ConfigBindingAction? {
        guard let name else { return nil }
        return bindings?.actions.first { $0.name.caseInsensitiveCompare(name) == .orderedSame }
    }

    /// The label people see for an engine key: the current layout's label when a
    /// physical key sends it, otherwise the engine's own name made readable.
    func keyName(_ key: EngineKey) -> String {
        let source = devices.inputSource
        if let match = Self.physicalKeys.first(where: { source.mapping(virtualKey: $0).engineKey == key }) {
            let label = source.label(virtualKey: match)
            return label.count == 1 ? label.uppercased() : label
        }
        return Self.readable(key)
    }
    static func readable(_ key: EngineKey) -> String {
        switch key.number {
        case 200...204: return "Mouse \(key.number - 199)"
        case 206: return "Wheel up"
        case 205: return "Wheel down"
        case 32: return "Space"
        case 9: return "Tab"
        case 13: return "Return"
        case 27: return "Escape"
        case 127: return "Delete"
        case 150: return "Command"
        case 151: return "Caps lock"
        case 158: return "Option"
        case 159: return "Control"
        case 160: return "Shift"
        case 154: return "↑"; case 155: return "↓"; case 156: return "←"; case 157: return "→"
        case 167...181: return "F\(key.number - 166)"
        default:
            let spelling = key.spelling
            if spelling.count == 1 { return spelling.uppercased() }
            return spelling.replacingOccurrences(of: "_", with: " ").capitalized
        }
    }
    private static let physicalKeys: [UInt16] = Array(InputSourceSnapshot.scancodes.keys).sorted()

    /// A key's role colour, from its one known action; other scripts are Game & UI.
    func role(of entry: ConfigBindingEntry?) -> StudioKeyRole {
        guard let entry else { return .unbound }
        return action(named: entry.actionName)?.group.role ?? .game
    }
    func describe(_ entry: ConfigBindingEntry?) -> String {
        guard let entry else { return "Not bound" }
        if let action = action(named: entry.actionName) { return action.title }
        if ownedChain(on: entry.key) != nil { return "Advanced bind" }
        let kind = studio.current.map { ConfigChains.inspect(key: entry.key.spelling, document: $0.document).kind }
        switch kind {
        case .multiCommand?: return "Several commands"
        case .settingCycle?: return "Toggle"
        default: return "Custom bind"
        }
    }

    // MARK: Capture

    func beginCapture(_ target: ConfigCaptureTarget) {
        cancelCapture()
        guard target == .find || !studio.isReadOnly else { return }
        guard let source = makeSource(window) else {
            studio.failure = ConfigStudioFailure(title: "Can’t listen for keys here", message: "Open Configs in its window, then try again.", retry: nil)
            return
        }
        let devices = devices
        let capture = ConfigBindingCapture(source: NotifyingCaptureSource(inner: source) { [weak self] in self?.captureChanged() },
                                           mapping: { devices.inputSource.mapping(virtualKey: $0) })
        self.capture = capture; self.target = target; captureNote = nil
        capture.start(runningGame: studio.gameRunning)
        captureChanged()
    }
    func cancelCapture() {
        if let capture, capture.state == .listening || capture.isMonitoring { capture.cancel() }
        target = nil; captureNote = nil
    }
    /// The game took over: listening stops at once.
    func gameStarted() { capture?.gameDidStart(); target = nil; captureNote = nil }

    private func captureChanged() {
        guard let capture else { return }
        switch capture.state {
        case .listening: captureNote = capture.unavailable?.explanation
        case .captured(let key):
            let finished = target
            target = nil; captureNote = nil
            if let finished { use(key, for: finished) }
        case .cancelled, .idle: target = nil; captureNote = nil
        }
    }
    private func use(_ key: EngineKey, for target: ConfigCaptureTarget) {
        switch target {
        case .find: selectedKey = key
        case .chainKey:
            guard var draft else { return }
            draft.key = key.spelling
            self.draft = draft
            preview()
        case .slot(let action, let slot): assign(key, to: action, slot: slot)
        }
    }

    // MARK: Binding edits

    func assign(_ key: EngineKey?, to action: String, slot: ConfigBindingSlot, undo: UndoManager? = nil) {
        guard let bindings, let current = studio.current else { return }
        do {
            let edit = try bindings.prepareEdit(read: current, action: action, slot: slot, key: key)
            if edit.conflict != nil { review = edit } else { commit(edit, replaceConflict: false, undo: undo) }
        } catch { studio.failure = ConfigStudioFailure(title: "Couldn’t change this key", message: error.localizedDescription, retry: nil) }
    }
    /// Visual inspector: put one action on a key, or clear it.
    func setKey(_ key: EngineKey, action: String?, undo: UndoManager? = nil) {
        guard let bindings, let current = studio.current else { return }
        do {
            let edit = try bindings.prepareKeyEdit(read: current, key: key, binding: action.map { Data($0.utf8) })
            if edit.conflict != nil { review = edit } else { commit(edit, replaceConflict: false, undo: undo) }
        } catch { studio.failure = ConfigStudioFailure(title: "Couldn’t change this key", message: error.localizedDescription, retry: nil) }
    }
    func confirmReview(undo: UndoManager? = nil) {
        guard let review else { return }
        self.review = nil
        commit(review, replaceConflict: true, undo: undo)
    }
    func cancelReview() { review = nil }
    /// Plain words for a pending replacement: what the key does now.
    func reviewText(_ edit: ConfigBindingEdit) -> String {
        guard let conflict = edit.conflict, let key = edit.key else { return "" }
        return "\(keyName(key)) is also \(describe(conflict)), so one press would do both. Use it for \(edit.action?.title ?? "this") only?"
    }

    private func commit(_ edit: ConfigBindingEdit, replaceConflict: Bool, undo: UndoManager?) {
        guard let session = studio.session else { return }
        do {
            guard let entry = try session.mutate(profileID: edit.profileID, { try ConfigBindings.apply(edit, to: $0, replaceConflict: replaceConflict) }) else { return }
            studio.didSave(entry, name: "Change Key", undo: undo)
        } catch { studio.failure = ConfigStudioFailure(title: "Couldn’t save: \(edit.summary)", message: error.localizedDescription, retry: nil) }
    }

    // MARK: Advanced binds

    var chains: [ConfigChain] {
        guard let current = studio.current else { return [] }
        if let cache = chainsCache, cache.version == current.version { return (try? cache.value.get()) ?? [] }
        let value = Result { try ConfigChains.ownedChains(read: current) }
        chainsCache = (current.version, value)
        return (try? value.get()) ?? []
    }
    /// Set when the chain file or its definitions changed outside Config Studio.
    var chainsUnreadable: String? {
        _ = chains
        if case .failure(let error)? = chainsCache?.value { return error.localizedDescription }
        return nil
    }
    func ownedChain(on key: EngineKey) -> ConfigChain? {
        chains.first { EngineKey(spelling: $0.key) == key }
    }
    func selectChain(_ id: String?) {
        chainID = id; showLines = false
        draft = id.flatMap { id in chains.first { $0.id == id } }.map(ConfigChainDraft.init(chain:))
        preview()
    }
    func newChain() {
        var fresh = ConfigChainDraft(newID: chains.map(\.id))
        if let toggle = studio.spec(named: "cg_drawFPS") { fresh.setting = toggle.name; fresh.values = Self.cycleValues(toggle) }
        chainID = fresh.id; draft = fresh; showLines = false
        preview()
    }
    func updateDraft(_ change: (inout ConfigChainDraft) -> Void) {
        guard var draft else { return }
        change(&draft)
        if draft.kind == .toggle, let spec = studio.spec(named: draft.setting), draft.values.isEmpty { draft.values = Self.cycleValues(spec) }
        self.draft = draft
        preview()
    }
    static func cycleValues(_ spec: ConfigSettingSpec) -> [String] {
        switch spec.type {
        case .bool: return ["1", "0"]
        case .enumeration: return spec.choices
        default: return [spec.defaultText ?? "0", ""].filter { !$0.isEmpty }
        }
    }
    /// Settings a toggle can cycle: editable switches and menus the loadout owns.
    var toggleSettings: [ConfigSettingSpec] {
        studio.specs.filter { ($0.type == .bool || $0.type == .enumeration) && !$0.launcherOwned }
            .sorted { ($0.setting.guided ? 0 : 1, $0.title) < ($1.setting.guided ? 0 : 1, $1.title) }
    }

    /// Compiles the draft with B2 against the loadout on screen; nothing is written.
    func preview() {
        chainEdit = nil; chainError = nil; chainRisks = []; chainBlocked = false
        guard let draft, let current = studio.current else { return }
        guard let chain = draft.chain else {
            chainError = draft.key.isEmpty ? "Choose a key for this bind." : draft.kind == .cycle ? "A cycle needs at least two steps." : "Add what this key should do."
            return
        }
        let others = chains.filter { $0.id != draft.id }
        do {
            let edit = try ConfigChains.prepare(chains: others + [chain], read: current)
            chainEdit = edit
            if let catalog = studio.catalog { assess(edit, chain: chain, catalog: catalog) }
        } catch { chainError = error.localizedDescription }
    }
    /// I1's labels for the commands this bind adds, shown before saving.
    private func assess(_ edit: ConfigChainEdit, chain: ConfigChain, catalog: ConfigCatalog) {
        let review = ConfigChains.safetyReview(edit: edit, catalog: ConfigImportCatalogAdapter(catalog: catalog))
        let scriptStart = edit.document.serialized().count + 1
        let key = EngineKey(spelling: chain.key)
        let relevant = review.candidates.filter { candidate in
            if candidate.location.range.lowerBound >= scriptStart { return true }
            let tokens = candidate.command.tokens
            return tokens.count >= 2 && candidate.command.kind == .bind && EngineKey(spelling: tokens[1].text) == key
        }
        var risks: [ConfigImportRisk] = []
        for candidate in relevant where candidate.classification != .safe {
            for reason in candidate.reasons + candidate.effects.flatMap(\.reasons) where !risks.contains(reason) && reason != .script && reason != .unknownSetting {
                risks.append(reason)
            }
        }
        chainRisks = risks
        chainBlocked = relevant.contains { $0.classification == .blocked }
    }
    var chainNeedsConfirmation: Bool { !(chainEdit?.conflictingKeys.isEmpty ?? true) || !chainRisks.isEmpty }

    func saveChain(confirmed: Bool = false, undo: UndoManager? = nil) {
        guard let edit = chainEdit, let session = studio.session, !studio.isReadOnly else { return }
        guard !chainBlocked else { chainError = "This bind contains a command Config Studio won’t save. Remove it, or use Edit as Text."; return }
        guard confirmed || !chainNeedsConfirmation else { return }
        do {
            try ConfigChains.apply(edit, to: session, replaceConflicts: confirmed)
            if let entry = session.history.first { studio.didSave(entry, name: "Advanced Bind", undo: undo) }
            if let draft { var saved = draft; saved.isNew = false; self.draft = saved }
            preview()
        } catch { chainError = error.localizedDescription }
    }
    func deleteChain(undo: UndoManager? = nil) {
        guard let draft, let current = studio.current, let session = studio.session else { return }
        if draft.isNew { self.draft = nil; chainID = nil; preview(); return }
        do {
            let edit = try ConfigChains.prepare(chains: chains.filter { $0.id != draft.id }, read: current)
            try ConfigChains.apply(edit, to: session)
            if let entry = session.history.first { studio.didSave(entry, name: "Delete Advanced Bind", undo: undo) }
            self.draft = nil; chainID = nil; chainEdit = nil
        } catch { chainError = error.localizedDescription }
    }

    /// Press-by-press preview from B2's machine: the saved profile, then its
    /// chain file, then key events. `nil` on a press means its trace is incomplete.
    func pressPreview(presses: Int? = nil) -> [(press: [ConfigCommandWords], release: [ConfigCommandWords], complete: Bool)] {
        guard let edit = chainEdit, let draft, let key = ConfigDocument.keyNumber(draft.key),
              let binding = edit.document.effectiveBindBytes(key: draft.key) else { return [] }
        var machine = ConfigChainMachine(dialect: edit.document.dialect, files: [:], waitDisabled: false)
        _ = machine.execute(edit.document.serialized())
        _ = machine.execute(edit.script)
        let count = presses ?? (draft.kind == .cycle ? draft.steps.filter { !$0.isEmpty }.count + 1 : draft.kind == .toggle ? max(2, draft.values.count) : 1)
        return (0..<count).map { _ in
            let press = machine.keyEvent(binding: binding, key: key, down: true, time: 100)
            let release = machine.keyEvent(binding: binding, key: key, down: false, time: 200)
            return (press.commands.compactMap(words), release.commands.compactMap(words), press.complete && release.complete)
        }
    }
    /// Leaf command → words. Helper pointer assignments are internal and hidden.
    func words(_ argv: [Data]) -> ConfigCommandWords? {
        let tokens = argv.map(ConfigDocument.text)
        guard let first = tokens.first else { return nil }
        let rest = tokens.dropFirst().joined(separator: " ")
        switch first.lowercased() {
        case "set", "seta": return tokens.count > 1 && tokens[1].hasPrefix("cs_b2_") ? nil : ConfigCommandWords(text: tokens.joined(separator: " "), literal: true)
        case "name": return .init(text: "Name: \(rest)", literal: false)
        case "say": return .init(text: "Say “\(rest)”", literal: false)
        case "say_team": return .init(text: "Say to team “\(rest)”", literal: false)
        case "toggle":
            guard tokens.count >= 2, let spec = studio.spec(named: tokens[1]) else { return .init(text: tokens.joined(separator: " "), literal: true) }
            return .init(text: "Switch \(spec.title)", literal: false)
        case "wait": return .init(text: "Wait a frame", literal: false)
        default:
            if first.hasPrefix("-"), let action = action(named: "+" + first.dropFirst()) { return .init(text: "Let go of \(action.title)", literal: false) }
            if let action = action(named: first) { return .init(text: action.title, literal: false) }
            return .init(text: tokens.joined(separator: " "), literal: true)
        }
    }
    /// Issues worth saying out loud, in plain words, most important first.
    var chainNotes: [(text: String, tone: StudioStatusTone)] {
        guard let edit = chainEdit else { return [] }
        var notes: [(String, StudioStatusTone)] = []
        let issues = edit.issues
        if issues.contains(.waitDisabled) || (issues.contains(.usesWait) && edit.document.dialect == .cod2x) {
            notes.append(("Competitive CoD2x servers turn off wait, so there everything in this bind happens in the same frame.", .caution))
        } else if issues.contains(.usesWait) {
            notes.append(("This bind waits between commands.", .information))
        }
        if issues.contains(.heldActionWithoutRelease) { notes.append(("Only the first held action is let go when you release the key; later ones keep running.", .caution)) }
        if issues.contains(.releaseTail) { notes.append(("Some commands run again when you let go of the key.", .information)) }
        for issue in issues {
            if case .nativeSetTruncation = issue { notes.append(("A step is longer than the 1,022 characters this game build keeps, so it would be cut short. Split it into shorter steps.", .negative)) }
            if case .unresolvedFile(let name) = issue { notes.append(("Runs \(name), which isn’t part of this loadout, so its effect can’t be shown.", .caution)) }
            if case .unresolvedVariable(let name) = issue { notes.append(("Uses \(name), which isn’t set in this loadout, so its effect can’t be shown.", .caution)) }
        }
        if edit.runtimeVariableCount > 0 {
            notes.append(("Uses \(edit.runtimeVariableCount) of the game’s \(edit.runtimeVariablePoolCapacity.formatted()) setting slots. Mods and the game use others too.", .information))
        }
        return notes
    }
    /// The exact lines B2 writes: the trigger in the config, helpers in the chain file.
    var chainLines: [String] {
        guard let edit = chainEdit, let draft, let binding = edit.document.effectiveBindBytes(key: draft.key) else { return [] }
        let trigger = "bind \(draft.key) \"\(ConfigDocument.text(binding))\""
        let helpers = ConfigDocument.text(edit.script).split(whereSeparator: \.isNewline).map(String.init)
        return [trigger] + helpers
    }

    /// Windows export: B2 compiles helpers to archived lines and refuses what it can't carry.
    func exportWindows(to url: URL) {
        guard let current = studio.current, let session = studio.session else { return }
        do {
            try ConfigChains.exportWindows(read: current, to: url, home: session.home)
            studio.announce("Exported for Windows.")
            studio.notice = "Exported \(url.lastPathComponent) for Windows."
        } catch {
            studio.failure = ConfigStudioFailure(title: "Couldn’t export for Windows", message: error.localizedDescription, retry: nil)
        }
    }
}
