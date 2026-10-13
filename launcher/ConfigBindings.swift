import Foundation
import Combine
import AppKit

enum ConfigBindingGroup: String, CaseIterable, Sendable { case movement, combat, communication, gameUI }
enum ConfigBindingSlot: Sendable { case primary, secondary }
struct ConfigBindingAction: Identifiable, Equatable, Sendable {
    let name: String
    let title: String
    let summary: String
    let group: ConfigBindingGroup
    var id: String { name }
}
struct ConfigBindingEntry: Equatable, Sendable {
    let key: EngineKey
    let binding: Data
    let actionName: String?
    let hasReleaseTail: Bool
    var text: String { ConfigDocument.text(binding) }
}
struct ConfigBindingRow: Sendable {
    let action: ConfigBindingAction
    let keys: [EngineKey]
    var primary: EngineKey? { keys.first }
    var secondary: EngineKey? { keys.dropFirst().first }
    var additional: [EngineKey] { Array(keys.dropFirst(2)) }
}
enum ConfigBindingError: Error, LocalizedError, Equatable, Sendable {
    case unavailableAction, reservedKey, duplicateActionKey, conflictNeedsReview, locked(String)
    var errorDescription: String? {
        switch self {
        case .unavailableAction: return "This action is not available in this game build."
        case .reservedKey: return "This key opens the console and cannot be reassigned."
        case .duplicateActionKey: return "This key already runs this action."
        case .conflictNeedsReview: return "This key already has a binding. Review it before replacing it."
        case .locked(let name): return "Unlock \(name) before changing this binding."
        }
    }
}
struct ConfigBindingEdit: Sendable {
    let profileID: String
    let action: ConfigBindingAction?
    let oldKey: EngineKey?
    let key: EngineKey?
    let conflict: ConfigBindingEntry?
    let before: ConfigProfileVersion
    fileprivate let document: ConfigDocument
    var preview: Data { document.serialized() }
    var hasChanges: Bool { preview != before.config }
    var summary: String {
        if let action { return "Changed \(action.title) binding" }
        return key.map { "Changed \($0.spelling) binding" } ?? "Cleared \(oldKey?.spelling ?? "key") binding"
    }
}
struct ConfigBindings: Sendable {
    let document: ConfigDocument
    let catalog: ConfigCatalog
    let mode: ConfigCatalogMode
    let actions: [ConfigBindingAction]
    let entries: [ConfigBindingEntry]
    init(document: ConfigDocument, catalog: ConfigCatalog, mode: ConfigCatalogMode) {
        self.document = document; self.catalog = catalog; self.mode = mode
        actions = catalog.commands.compactMap { command in
            guard command.bindable, command.variant(for: mode) != nil, !command.name.hasPrefix("-"),
                  let label = command.menuLabel, !label.isEmpty else { return nil }
            return .init(name: command.name, title: label, summary: command.summary, group: Self.group(command.name.lowercased()))
        }.sorted { $0.name < $1.name }
        let names = Dictionary(actions.map { ($0.name.lowercased(), $0.name) }, uniquingKeysWith: { first, _ in first })
        entries = Set((1...255).compactMap { EngineKey(number: $0) }).sorted().compactMap { key in
            guard let value = document.effectiveBindBytes(key: key.spelling), !value.isEmpty else { return nil }
            // Exact single actions only. In particular, a leading space stops
            // CL_KeyEvent's plus-release logic, so it is not a plain + action.
            return .init(key: key, binding: value, actionName: names[ConfigDocument.text(value).lowercased()],
                         hasReleaseTail: value.first == 43 && value.contains(59))
        }
    }
    func row(for action: String) -> ConfigBindingRow? {
        guard let value = actions.first(where: { $0.name.lowercased() == action.lowercased() }) else { return nil }
        return .init(action: value, keys: entries.filter { $0.actionName == value.name }.map(\.key))
    }
    func entry(for key: EngineKey) -> ConfigBindingEntry? { entries.first { $0.key == key } }
    func prepareEdit(read: ConfigProfileRead, action: String, slot: ConfigBindingSlot, key: EngineKey?) throws -> ConfigBindingEdit {
        guard read.data == document.serialized(), read.document.dialect == document.dialect else { throw ConfigProfileError.conflict }
        guard let row = row(for: action) else { throw ConfigBindingError.unavailableAction }
        if let key, [96,126].contains(key.number) { throw ConfigBindingError.reservedKey }
        let oldKey = slot == .primary ? row.primary : row.secondary
        if let key, key != oldKey, row.keys.contains(key) { throw ConfigBindingError.duplicateActionKey }
        let conflict = key == oldKey ? nil : key.flatMap(entry)
        var edited = document
        if key != oldKey {
            if let oldKey { try edited.removeBind(key: oldKey.spelling) }
            if let key { try edited.setBind(key: key.spelling, binding: row.action.name) }
        }
        let edit = ConfigBindingEdit(profileID: read.profile.id, action: row.action, oldKey: oldKey, key: key,
                                     conflict: conflict, before: read.version, document: edited)
        try Self.checkLocks(edit, read: read)
        return edit
    }
    /// The visual inspector can clear any key (including additional and opaque
    /// imported binds). B2 can use the same F1/F2 boundary for reviewed scripts.
    func prepareKeyEdit(read: ConfigProfileRead, key: EngineKey, binding: Data?) throws -> ConfigBindingEdit {
        guard read.data == document.serialized(), read.document.dialect == document.dialect else { throw ConfigProfileError.conflict }
        guard ![96,126].contains(key.number) else { throw ConfigBindingError.reservedKey }
        let value = binding.flatMap { $0.isEmpty ? nil : $0 }, existing = entry(for: key)
        var edited = document
        if let value { try edited.setBind(key: key.spelling, binding: value) }
        else { try edited.removeBind(key: key.spelling) }
        let action = value.flatMap { data in actions.first { $0.name.lowercased() == ConfigDocument.text(data).lowercased() } }
        // Clearing an empty or shadowed assignment still edits this key. Keep
        // its identity in the proposal so bind locks cannot be bypassed.
        let edit = ConfigBindingEdit(profileID: read.profile.id, action: action, oldKey: key,
                                     key: value == nil ? nil : key, conflict: value != nil && value != existing?.binding ? existing : nil,
                                     before: read.version, document: edited)
        try Self.checkLocks(edit, read: read)
        return edit
    }
    @discardableResult static func apply(_ edit: ConfigBindingEdit, to store: ConfigProfileStore,
                                        replaceConflict: Bool = false, runningGame: Bool = false) throws -> ConfigHistoryEntry? {
        if runningGame { throw ConfigProfileError.gameRunning }
        let current = try store.read(id: edit.profileID)
        guard current.version == edit.before, current.document.dialect == edit.document.dialect else { throw ConfigProfileError.conflict }
        try checkLocks(edit, read: current)
        guard edit.conflict == nil || replaceConflict else { throw ConfigBindingError.conflictNeedsReview }
        guard edit.hasChanges else { return nil }
        return try store.update(id: edit.profileID, document: edit.document, expected: edit.before,
                                summary: edit.summary, runningGame: runningGame)
    }
    private static func checkLocks(_ edit: ConfigBindingEdit, read: ConfigProfileRead) throws {
        let keys = Set([edit.oldKey,edit.key].compactMap { $0 })
        var commands = Set(edit.action.map { [$0.name.lowercased()] } ?? [])
        for key in keys {
            for document in [read.document,edit.document] {
                if let data = document.effectiveBindBytes(key: key.spelling),
                   let name = ConfigDocument(data: data, dialect: document.dialect).commands.first?.tokens.first?.text {
                    commands.insert(name.lowercased())
                }
            }
        }
        for lock in read.metadata.locks.sorted() {
            let name = lock.lowercased()
            if name.hasPrefix("bind:"), let key = EngineKey(spelling: String(name.dropFirst(5))), keys.contains(key) {
                throw ConfigBindingError.locked(lock)
            }
            let action = name.hasPrefix("action:") ? String(name.dropFirst(7)) : name
            if commands.contains(action) { throw ConfigBindingError.locked(lock) }
        }
    }
    private static func group(_ name: String) -> ConfigBindingGroup {
        if ["+forward","+back","+moveleft","+moveright","+leanleft","+leanright","+gostand","+stance","+left","+right","+strafe","+mlook","+lookup","+lookdown","gocrouch","goprone","togglecrouch","toggleprone"].contains(name) { return .movement }
        if ["+attack","+speed","+reload","+frag","+smoke","+melee","+holdbreath","+binoculars","toggleads","weapnext","weapprev"].contains(name) { return .combat }
        if ["chatmodepublic","chatmodeteam","say","say_team","mp_quickmessage","+talk"].contains(name) { return .communication }
        return .gameUI
    }
}

enum BindingCaptureCancellation: Equatable, Sendable { case user, escape, focusLost, gameStarted }
enum BindingCaptureState: Equatable, Sendable { case idle, listening, captured(EngineKey), cancelled(BindingCaptureCancellation) }
enum BindingCaptureControl: Hashable, Sendable { case key(UInt16), mouse(Int) }
enum BindingCaptureDisposition: Equatable, Sendable { case consume, passThrough }
enum BindingCaptureEvent: Sendable {
    case key(virtualKey: UInt16, pressed: Bool, repeated: Bool = false)
    /// Caps Lock flags describe its latched state, not a held physical key.
    case keyPulse(virtualKey: UInt16)
    case mouse(button: Int, pressed: Bool)
    /// Positive means engine wheel-up. The AppKit adapter removes natural-scroll inversion.
    case wheel(delta: Double)
    case focusLost, gameStarted
}
@MainActor protocol BindingCaptureEventSource: AnyObject, Sendable {
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition)
    func stop()
    /// Clicks at these window points (top-left origin) go to the window's controls without
    /// reaching the handler, until `stop`. Nil, the default, sends every click to the handler.
    func passClicks(inside region: (@MainActor (CGPoint) -> Bool)?)
}
extension BindingCaptureEventSource {
    func passClicks(inside region: (@MainActor (CGPoint) -> Bool)?) {}
}
@MainActor final class ConfigBindingCapture: ObservableObject {
    @Published private(set) var state: BindingCaptureState = .idle
    @Published private(set) var held: Set<BindingCaptureControl> = []
    @Published private(set) var released: Set<BindingCaptureControl> = []
    @Published private(set) var unavailable: InputKeyMapping? = nil
    @Published private(set) var isMonitoring = false
    private let source: any BindingCaptureEventSource
    private let mapping: @MainActor (UInt16) -> InputKeyMapping
    private var generation: UUID?
    init(source: any BindingCaptureEventSource, mapping: @escaping @MainActor (UInt16) -> InputKeyMapping) {
        self.source = source; self.mapping = mapping
    }
    func start(runningGame: Bool = false) {
        cleanup(); released = []; unavailable = nil
        guard !runningGame else { state = .cancelled(.gameStarted); return }
        state = .listening; isMonitoring = true
        let token = UUID(); generation = token
        source.start { [weak self] event in
            guard let self, self.generation == token else { return .passThrough }
            return self.receive(event)
        }
    }
    func cancel(_ reason: BindingCaptureCancellation = .user) { cleanup(); state = .cancelled(reason) }
    func gameDidStart() { cancel(.gameStarted) }
    private func cleanup() {
        generation = nil; source.stop(); isMonitoring = false
        released.formUnion(held); held = []
    }
    private func receive(_ event: BindingCaptureEvent) -> BindingCaptureDisposition {
        switch event {
        case .focusLost: cancel(.focusLost)
        case .gameStarted: cancel(.gameStarted)
        case .keyPulse(let virtualKey):
            accept(mapping(virtualKey)); if case .captured = state, held.isEmpty { cleanup() }
        case .key(let virtualKey, let pressed, let repeated):
            if virtualKey == 53 && pressed { cancel(.escape); return .consume }
            let control = BindingCaptureControl.key(virtualKey)
            if pressed {
                held.insert(control)
                if !repeated { accept(mapping(virtualKey)) }
            } else { release(control) }
        case .mouse(let button, let pressed):
            let control = BindingCaptureControl.mouse(button)
            if pressed {
                held.insert(control)
                if let number = EngineInputMapping.mouseKeyNumber(appKitButton: button), let key = EngineKey(number: number) { accept(.available(key)) }
                else { accept(.unavailable(.unsupportedSymbol("Mouse \(button + 1); map extra buttons to a key in your mouse software"))) }
            } else { release(control) }
        case .wheel(let delta):
            if let number = EngineInputMapping.wheelKeyNumber(delta: delta), let key = EngineKey(number: number) {
                accept(.available(key)); if case .captured = state, held.isEmpty { cleanup() }
            }
        }
        return .consume
    }
    private func accept(_ value: InputKeyMapping) {
        guard state == .listening else { return }
        switch value {
        case .available(let key): state = .captured(key); unavailable = nil
        case .console, .unavailable: unavailable = value
        }
    }
    private func release(_ control: BindingCaptureControl) {
        if held.remove(control) != nil { released.insert(control) }
        if case .captured = state, held.isEmpty { cleanup() }
    }
    deinit {
        // Teardown also works if a view drops its owner without an explicit
        // cancel. Event handlers hold the capture weakly and pass through then.
        let source = source
        Task { @MainActor in source.stop() }
    }
}

/// Production adapter, scoped to the editing window. Tests inject their own
/// source and never install NSEvent monitors or observe the user's input.
@MainActor final class AppKitBindingEventSource: BindingCaptureEventSource {
    private weak var window: NSWindow?
    private var monitor: Any?
    private var observers: [NSObjectProtocol] = []
    private var handler: (@MainActor (BindingCaptureEvent) -> BindingCaptureDisposition)?
    private var clickRegion: (@MainActor (CGPoint) -> Bool)?
    init(window: NSWindow) { self.window = window }
    func passClicks(inside region: (@MainActor (CGPoint) -> Bool)?) { clickRegion = region }
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition) {
        stop(); self.handler = handler
        let mask: NSEvent.EventTypeMask = [.keyDown,.keyUp,.flagsChanged,.leftMouseDown,.leftMouseUp,.rightMouseDown,.rightMouseUp,.otherMouseDown,.otherMouseUp,.scrollWheel]
        monitor = NSEvent.addLocalMonitorForEvents(matching: mask) { [weak self] event in
            let consume = MainActor.assumeIsolated {
                guard let self, let window = self.window, event.window === window else { return false }
                guard let input = Self.input(event) else { return false }
                if case .mouse = input, let region = self.clickRegion, let point = Self.topLeftPoint(event, in: window), region(point) { return false }
                return self.handler?(input) == .consume
            }
            return consume ? nil : event
        }
        for (name, object) in [(NSApplication.didResignActiveNotification, nil), (NSWindow.didResignKeyNotification, window as Any?)] {
            observers.append(NotificationCenter.default.addObserver(forName: name, object: object, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { _ = self?.handler?(.focusLost) }
            })
        }
    }
    func stop() {
        if let monitor { NSEvent.removeMonitor(monitor) }; monitor = nil
        for observer in observers { NotificationCenter.default.removeObserver(observer) }; observers = []
        handler = nil; clickRegion = nil
    }
    /// The click in the content view's points from its top left, as SwiftUI's global frames are.
    private static func topLeftPoint(_ event: NSEvent, in window: NSWindow) -> CGPoint? {
        guard let content = window.contentView else { return nil }
        let point = content.convert(event.locationInWindow, from: nil)
        return content.isFlipped ? point : CGPoint(x: point.x, y: content.bounds.height - point.y)
    }
    private static func input(_ event: NSEvent) -> BindingCaptureEvent? {
        switch event.type {
        case .keyDown, .keyUp: return .key(virtualKey: event.keyCode, pressed: event.type == .keyDown, repeated: event.isARepeat)
        case .flagsChanged:
            if event.keyCode == 57 { return .keyPulse(virtualKey: 57) }
            // Device-dependent flags distinguish left/right releases when the
            // other side remains held; the aggregate modifier bit cannot.
            let masks: [UInt16: UInt] = [54:0x10,55:0x08,56:0x02,58:0x20,59:0x01,60:0x04,61:0x40,62:0x2000]
            let pressed = masks[event.keyCode].map { event.modifierFlags.rawValue & $0 != 0 } ?? false
            return .key(virtualKey: event.keyCode, pressed: pressed)
        case .leftMouseDown, .rightMouseDown, .otherMouseDown: return .mouse(button: event.buttonNumber, pressed: true)
        case .leftMouseUp, .rightMouseUp, .otherMouseUp: return .mouse(button: event.buttonNumber, pressed: false)
        case .scrollWheel: return .wheel(delta: event.scrollingDeltaY * (event.isDirectionInvertedFromDevice ? -1 : 1))
        default: return nil
        }
    }
}
