import Foundation

@MainActor private final class CaptureFixture: BindingCaptureEventSource {
    var handler: (@MainActor (BindingCaptureEvent) -> BindingCaptureDisposition)?
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition) { self.handler = handler }
    func stop() { handler = nil }
    func send(_ event: BindingCaptureEvent) -> BindingCaptureDisposition { handler?(event) ?? .passThrough }
}
@MainActor private final class CaptureLayoutFixture {
    var snapshot: InputSourceSnapshot
    init(_ snapshot: InputSourceSnapshot) { self.snapshot = snapshot }
}
private final class RunningGameFixture: @unchecked Sendable {
    private let lock = NSLock()
    private var running = false
    func set(_ value: Bool) { lock.lock(); running = value; lock.unlock() }
    func get() -> Bool { lock.lock(); defer { lock.unlock() }; return running }
}

@main struct ConfigBindingsTests {
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: \(message)") }
    }
    static func abcLabels() -> [UInt16: String] { InputDevicesTests.abcLabels() }
    @MainActor static func capture() {
        let source = CaptureFixture(), layout = InputSourceSnapshot(id: "ABC", name: "ABC", shape: .ansi, labels: abcLabels())
        let capture = ConfigBindingCapture(source: source, mapping: { layout.mapping(virtualKey: $0) })
        capture.start()
        require(capture.state == .listening && capture.isMonitoring, "start installs injectable capture source")
        let stale = source.handler
        require(source.send(.key(virtualKey: 36, pressed: true)) == .consume && capture.state == .captured(EngineKey(number: 13)!),
                "captured Return is consumed instead of activating default button")
        require(capture.held == [.key(36)] && capture.isMonitoring, "captured down retains drain until release")
        require(source.send(.key(virtualKey: 36, pressed: true, repeated: true)) == .consume, "Return repeats are consumed")
        require(source.send(.key(virtualKey: 36, pressed: false)) == .consume && capture.held.isEmpty && !capture.isMonitoring,
                "captured Return release drains and removes source")
        require(source.handler == nil && stale?(.key(virtualKey: 13, pressed: true)) == .passThrough, "late source events cannot restart capture")
        capture.start()
        require(source.send(.key(virtualKey: 49, pressed: true)) == .consume && capture.state == .captured(EngineKey(number: 32)!), "Space cannot toggle a control")
        capture.cancel()
        require(capture.held.isEmpty && capture.released == [.key(49)] && source.handler == nil, "cancel releases held state and source")
        capture.start(); _ = source.send(.key(virtualKey: 10, pressed: true))
        require(capture.state == .listening && capture.unavailable != nil, "unsupported ISO section key explains failure and keeps listening")
        _ = source.send(.focusLost)
        require(capture.state == .cancelled(.focusLost) && capture.held.isEmpty && capture.released == [.key(10)], "focus loss releases unsupported held input too")
        capture.start(); _ = source.send(.key(virtualKey: 55, pressed: true)); capture.gameDidStart()
        require(capture.state == .cancelled(.gameStarted) && capture.held.isEmpty && source.handler == nil, "game start cancels even after a modifier capture")
        capture.start(runningGame: true)
        require(!capture.isMonitoring && capture.state == .cancelled(.gameStarted), "cannot start capture while game owns config")
        capture.start(); require(source.send(.key(virtualKey: 53, pressed: true)) == .consume, "Escape cancel is consumed")
        require(capture.state == .cancelled(.escape) && source.handler == nil, "Escape removes event source")
        for button in 0...4 {
            capture.start(); _ = source.send(.mouse(button: button, pressed: true))
            require(capture.state == .captured(EngineKey(number: 200 + button)!), "AppKit Mouse \(button + 1) uses engine identity")
            require(source.send(.mouse(button: button, pressed: false)) == .consume && !capture.isMonitoring, "mouse release cleans capture")
        }
        capture.start(); _ = source.send(.mouse(button: 5, pressed: true))
        require(capture.state == .listening && capture.unavailable != nil, "Mouse6 is explained as unavailable")
        _ = source.send(.gameStarted)
        require(capture.held.isEmpty && capture.released == [.mouse(5)], "game notification clears unsupported mouse state")
        for (delta, number) in [(1.0,206),(-1.0,205)] {
            capture.start(); require(source.send(.wheel(delta: delta)) == .consume, "wheel event consumed")
            require(capture.state == .captured(EngineKey(number: number)!) && !capture.isMonitoring && capture.held.isEmpty, "wheel capture is an immediate pulse")
        }
        capture.start(); _ = source.send(.wheel(delta: 0)); _ = source.send(.wheel(delta: .nan))
        require(capture.state == .listening, "zero/nonfinite scroll does not capture a key")
        _ = source.send(.key(virtualKey: 50, pressed: true))
        require(capture.state == .listening && capture.unavailable?.isReserved == true, "console key remains inspectable and reserved")
        capture.start()
        require(capture.held.isEmpty && capture.state == .listening, "restart cleans prior held state")
        capture.cancel()
        capture.start(); _ = source.send(.keyPulse(virtualKey: 57))
        require(capture.state == .captured(EngineKey(number: 151)!) && capture.held.isEmpty && !capture.isMonitoring,
                "Caps Lock is a latch pulse; capture does not swallow input until Caps Lock turns off")
        let changingLayout = CaptureLayoutFixture(layout)
        let changing = ConfigBindingCapture(source: source, mapping: { changingLayout.snapshot.mapping(virtualKey: $0) })
        changing.start(); _ = source.send(.key(virtualKey: 13, pressed: true))
        changingLayout.snapshot = .init(id: "Dvorak", name: "Dvorak", shape: .ansi, labels: abcLabels().merging([13:","]) { _, new in new })
        require(source.send(.key(virtualKey: 13, pressed: false)) == .consume && changing.held.isEmpty && !changing.isMonitoring &&
                changing.state == .captured(EngineKey(number: 119)!), "layout changes during capture preserve chosen identity and release by physical position")
        print("PASS: capture down/release, Return/Space consumption, Mouse1–5/wheel, unsupported inputs, cancel/focus/game cleanup")
    }
    static func rejects<E: Error & Equatable>(_ expected: E, _ operation: () throws -> Void) {
        do { try operation(); fatalError("FAIL: expected \(expected)") }
        catch let error as E { require(error == expected, "expected \(expected), got \(error)") }
        catch { fatalError("FAIL: unexpected \(error)") }
    }
    static func bindings(_ catalogPath: String) throws {
        let catalog = try ConfigCatalog(data: Data(contentsOf: URL(fileURLWithPath: catalogPath)))
        let raw = Data("// keep exactly\r\nbind W \"+forward\"\r\nbind u \"+forward\"\r\nbind i \"+forward\"\r\nbind q \"+attack; say hi\"\r\nbind r \" +attack\"\r\nbind MOUSE1 \"+attack\"\r\nbind s \"+back\"\r\nseta name \"".utf8) + Data([233,255]) + Data("\"\r\nmystery x; vstr keep\r\n".utf8)
        let document = ConfigDocument(data: raw, dialect: .cod2x)
        let model = ConfigBindings(document: document, catalog: catalog, mode: .cod2x)
        require(model.actions.first(where: { $0.name == "+attack" })?.title == "Fire", "action title comes from current catalog menu label")
        require(!model.actions.contains(where: { $0.name == "-attack" || $0.name == "bind" }), "release and script commands are not primary action rows")
        let row = model.row(for: "+forward")
        require(row?.primary?.number == 105 && row?.secondary?.number == 117 && row?.additional.map(\.number) == [119],
                "primary/secondary use engine key order and retain extra binds")
        require(model.row(for: "+attack")?.keys.map(\.number) == [200], "plus-action with tails stays opaque, not a plain Fire slot")
        let complex = model.entry(for: EngineKey(spelling: "q")!)
        require(complex?.hasReleaseTail == true && complex?.text == "+attack; say hi", "inspector exposes repeated release tail without rewriting it")
        let home = FileManager.default.temporaryDirectory.appendingPathComponent("bindings-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: home, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: home) }
        let running = RunningGameFixture()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, gameIsRunning: { running.get() })
        let read = try store.create(id: "binds", displayName: "Binds", document: document)
        let edit = try model.prepareEdit(read: read, action: "+forward", slot: .primary, key: EngineKey(spelling: "q"))
        require(edit.conflict?.binding == Data("+attack; say hi".utf8), "conflict review retains exact displaced complex binding")
        let historyCount = try store.history(id: "binds").count
        rejects(ConfigBindingError.conflictNeedsReview) { _ = try ConfigBindings.apply(edit, to: store) }
        require(try store.read(id: "binds").data == raw && store.history(id: "binds").count == historyCount, "unapproved conflict leaves disk and history unchanged")
        running.set(true)
        rejects(ConfigProfileError.gameRunning) { _ = try ConfigBindings.apply(edit, to: store, replaceConflict: true) }
        running.set(false)
        rejects(ConfigProfileError.gameRunning) { _ = try ConfigBindings.apply(edit, to: store, replaceConflict: true, runningGame: true) }
        let transaction = try ConfigBindings.apply(edit, to: store, replaceConflict: true)
        let changed = try store.read(id: "binds")
        require(changed.document.effectiveBind(key: "q") == "+forward" && changed.document.effectiveBind(key: "i") == nil,
                "one edit replaces reviewed target and removes only selected primary key")
        require(changed.document.effectiveBind(key: "w") == "+forward" && changed.document.effectiveBind(key: "u") == "+forward" && changed.document.effectiveBind(key: "mouse1") == "+attack",
                "secondary/additional/other action bindings remain intact")
        let committedCount = try store.history(id: "binds").count
        require(transaction?.beforeData == raw && transaction?.data == changed.data && committedCount == historyCount + 1,
                "one committed gesture is one F2 transaction")
        require(changed.data.contains(Data([233,255])) && changed.data.contains(Data("mystery x; vstr keep\r\n".utf8)), "unrelated bytes survive edit")
        rejects(ConfigProfileError.conflict) { _ = try ConfigBindings.apply(edit, to: store, replaceConflict: true) }
        let current = ConfigBindings(document: changed.document, catalog: catalog, mode: .cod2x)
        let noop = try current.prepareEdit(read: changed, action: "+forward", slot: .primary, key: EngineKey(spelling: "q"))
        require(try ConfigBindings.apply(noop, to: store) == nil && store.history(id: "binds").count == historyCount + 1, "no-op creates no spurious history")
        rejects(ConfigBindingError.duplicateActionKey) { _ = try current.prepareEdit(read: changed, action: "+forward", slot: .primary, key: EngineKey(spelling: "u")) }
        rejects(ConfigBindingError.reservedKey) { _ = try current.prepareEdit(read: changed, action: "+forward", slot: .primary, key: EngineKey(spelling: "`")) }
        rejects(ConfigBindingError.unavailableAction) { _ = try current.prepareEdit(read: changed, action: "invented", slot: .primary, key: EngineKey(spelling: "e")) }
        var metadata = changed.metadata; metadata.locks = ["bind:0x71"]
        _ = try store.update(id: "binds", document: changed.document, expected: changed.version, metadata: metadata, summary: "Lock Q")
        let locked = try store.read(id: "binds"), lockedModel = ConfigBindings(document: locked.document, catalog: catalog, mode: .cod2x)
        rejects(ConfigBindingError.locked("bind:0x71")) { _ = try lockedModel.prepareEdit(read: locked, action: "+forward", slot: .primary, key: EngineKey(spelling: "e")) }
        rejects(ConfigBindingError.locked("bind:0x71")) { _ = try lockedModel.prepareEdit(read: locked, action: "+back", slot: .primary, key: EngineKey(spelling: "Q")) }
        require(try store.read(id: "binds").data == changed.data, "locks protect both source and conflict destination")
        metadata.locks = ["action:+forward"]
        _ = try store.update(id: "binds", document: locked.document, expected: locked.version, metadata: metadata, summary: "Lock action")
        let actionLocked = try store.read(id: "binds")
        rejects(ConfigBindingError.locked("action:+forward")) {
            _ = try ConfigBindings(document: actionLocked.document, catalog: catalog, mode: .cod2x).prepareEdit(read: actionLocked, action: "+forward", slot: .primary, key: nil)
        }
        let visual = try store.create(id: "visual", displayName: "Visual", document: document)
        let clear = try model.prepareKeyEdit(read: visual, key: EngineKey(spelling: "q")!, binding: nil)
        let cleared = try ConfigBindings.apply(clear, to: store)
        let afterClear = try store.read(id: "visual")
        require(afterClear.document.effectiveBind(key: "q") == nil && afterClear.document.effectiveBind(key: "mouse1") == "+attack",
                "visual clear removes opaque key without removing another action binding")
        require(cleared?.beforeData == raw && afterClear.data.contains(Data("mystery x; vstr keep\r\n".utf8)), "visual clear is a byte-preserving F2 transaction")
        let visualModel = ConfigBindings(document: afterClear.document, catalog: catalog, mode: .cod2x)
        rejects(ConfigEditError.bindTooLong) {
            _ = try visualModel.prepareKeyEdit(read: afterClear, key: EngineKey(spelling: "j")!, binding: Data(repeating: 120, count: 1024))
        }
        rejects(ConfigEditError.unsafeBinding) {
            _ = try visualModel.prepareKeyEdit(read: afterClear, key: EngineKey(spelling: "j")!, binding: Data([43,97,0]))
        }
        let bounded = try visualModel.prepareKeyEdit(read: afterClear, key: EngineKey(spelling: "j")!, binding: Data(repeating: 120, count: 1023))
        _ = try ConfigBindings.apply(bounded, to: store)
        let boundedRead = try store.read(id: "visual")
        require(boundedRead.document.effectiveBindBytes(key: "j")?.count == 1023, "generic binding edits obey actual 1023-byte boundary")
        let boundedModel = ConfigBindings(document: boundedRead.document, catalog: catalog, mode: .cod2x)
        let pending = try boundedModel.prepareKeyEdit(read: boundedRead, key: EngineKey(spelling: "k")!, binding: Data("+reload".utf8))
        let configURL = home.appendingPathComponent("main/players/visual/config_mp.cfg"), external = boundedRead.data + Data("// external\n".utf8)
        try external.write(to: configURL)
        rejects(ConfigProfileError.conflict) { _ = try ConfigBindings.apply(pending, to: store) }
        require(try Data(contentsOf: configURL) == external, "external disk edit survives a stale visual proposal")
        var emptyMetadata = ConfigStudioMetadata(displayName: "Empty"); emptyMetadata.locks = ["bind:w"]
        let empty = try store.create(id: "empty", displayName: "Empty", document: ConfigDocument(data: Data("bind w \"\"\n".utf8), dialect: .cod2x), metadata: emptyMetadata)
        let emptyModel = ConfigBindings(document: empty.document, catalog: catalog, mode: .cod2x)
        rejects(ConfigBindingError.locked("bind:w")) { _ = try emptyModel.prepareKeyEdit(read: empty, key: EngineKey(spelling: "w")!, binding: nil) }
        require(try store.read(id: "empty").data == empty.data, "empty and shadowed key assignments still obey binding locks")
        print("PASS: catalog actions, primary/secondary/extras, opaque plus tails, reviewed conflicts, F2 history/locks/running/stale guards")
    }
    @MainActor static func main() throws {
        try InputDevicesTests.run(CommandLine.arguments[1])
        capture()
        try bindings(CommandLine.arguments[2])
        if CommandLine.arguments.contains("--system-probe") { try InputDevicesTests.systemProbe() }
    }
}
