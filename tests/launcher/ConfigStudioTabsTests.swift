import AppKit
import Foundation

// Binds and Check-up view-store checks: synthetic input source and capture events,
// real B1/B2/A1 models, private temporary homes and exact bytes on disk.

@MainActor final class FakeInputProvider: InputDeviceProvider {
    var source: InputSourceSnapshot
    init(name: String = "ABC") {
        var labels: [UInt16: String] = [:]
        let letters: [UInt16: String] = [0: "a", 1: "s", 2: "d", 3: "f", 4: "h", 5: "g", 6: "z", 7: "x", 8: "c", 9: "v", 11: "b", 12: "q", 13: "w",
                                         14: "e", 15: "r", 16: "y", 17: "t", 31: "o", 32: "u", 34: "i", 35: "p", 37: "l", 38: "j", 40: "k", 45: "n", 46: "m"]
        for (key, label) in letters { labels[key] = label }
        for (key, digit) in zip([18, 19, 20, 21, 23, 22, 26, 28, 25, 29] as [UInt16], ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]) { labels[key] = digit }
        for (key, symbol) in [(27, "-"), (24, "="), (33, "["), (30, "]"), (42, "\\"), (41, ";"), (39, "'"), (43, ","), (47, "."), (44, "/"), (50, "`")] as [(UInt16, String)] { labels[key] = symbol }
        source = InputSourceSnapshot(id: "com.apple.keylayout.ABC", name: name, shape: .ansi, labels: labels)
    }
    func inputSource() -> InputSourceSnapshot { source }
    func devices() -> [InputDevice] {
        [InputDevice(id: "1", name: "Magic Keyboard", vendorID: 0x05ac, kind: .keyboard, keyboardShape: .ansi),
         InputDevice(id: "2", name: "Mouse", vendorID: nil, kind: .mouse, buttonCount: 5)]
    }
    func startObserving(_ changed: @escaping @MainActor () -> Void) {}
    func stopObserving() {}
}

@MainActor final class SyntheticCaptureSource: BindingCaptureEventSource {
    private(set) var handler: (@MainActor (BindingCaptureEvent) -> BindingCaptureDisposition)?
    private(set) var stops = 0
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition) { self.handler = handler }
    func stop() { handler = nil; stops += 1 }
    @discardableResult func send(_ event: BindingCaptureEvent) -> BindingCaptureDisposition? { handler?(event) }
}

enum ConfigStudioTabsTests {
    static func require(_ condition: Bool, _ message: String = "config studio tabs", line: Int = #line) {
        if !condition { fatalError("FAIL: \(message) (line \(line))") }
    }
    static func bytes(_ text: String) -> Data { Data(text.utf8) }

    @MainActor static func run(scratch: URL, catalog: ConfigCatalog, resources: URL) throws {
        let rules = try ConfigRules(data: Data(contentsOf: resources.appendingPathComponent("ConfigRulesets.json")))
        let optimizer = try ConfigOptimizer(data: Data(contentsOf: resources.appendingPathComponent("ConfigOptimizer.json")))
        try binds(scratch: scratch, catalog: catalog)
        try checkup(scratch: scratch, catalog: catalog, rules: rules, optimizer: optimizer)
    }

    @MainActor static func studio(home: URL, config: Data, catalog: ConfigCatalog) throws -> (ConfigStudioModel, ConfigStudioSession, URL) {
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        _ = try store.create(id: "Main", displayName: "Main", document: ConfigDocument(data: config, dialect: .cod2x))
        try store.select(id: "Main", expectedActive: nil)
        let session = try ConfigStudioSession(home: home, dialect: .cod2x)
        try session.refresh()
        let model = ConfigStudioModel(session: session, catalog: { catalog })
        model.announce = { _ in }
        model.loadCatalogNow()
        return (model, session, home.appendingPathComponent("main/players/Main/config_mp.cfg"))
    }

    @MainActor static func binds(scratch: URL, catalog: ConfigCatalog) throws {
        let config = bytes("bind TAB \"+scores\"\nbind w \"+forward\"\nbind s \"+back\"\nbind j \"say hi; wait\"\nseta sensitivity \"2.5\"\n")
        let (model, session, file) = try studio(home: scratch.appendingPathComponent("binds"), config: config, catalog: catalog)
        let source = SyntheticCaptureSource()
        model.deviceProvider = { FakeInputProvider() }
        model.captureSource = { _ in source }
        let binds = model.binds

        // Rows come from the catalog's bindable actions; labels from the input source.
        let movement = binds.rows(in: .movement)
        require(movement.contains { $0.action.name == "+forward" && $0.primary == EngineKey(spelling: "w") })
        require(binds.keyName(EngineKey(spelling: "w")!) == "W" && binds.keyName(EngineKey(spelling: "mouse1")!) == "Mouse 1")
        require(binds.otherEntries.map(\.key) == [EngineKey(spelling: "j")!] && binds.describe(binds.otherEntries.first) == "Several commands")
        binds.query = "scoreboard"
        require(binds.rows(in: .gameUI).map(\.action.name) == ["+scores"] && binds.rows(in: .movement).isEmpty)
        binds.query = ""
        print("PASS: binds rows from catalog actions, layout labels and other binds")

        // Capture → one exact edit; the held key drains before the source stops.
        binds.beginCapture(.slot(action: "+frag", slot: .primary))
        require(binds.target != nil && binds.capture?.isMonitoring == true)
        require(source.send(.key(virtualKey: 5, pressed: true)) == .consume)
        var expected = ConfigDocument(data: config, dialect: .cod2x)
        try expected.setBind(key: "g", binding: "+frag")
        require(try Data(contentsOf: file) == expected.serialized(), "capture writes exactly one bind")
        require(session.history.first?.summary == "Changed Throw frag grenade binding" && binds.target == nil)
        require(binds.capture?.isMonitoring == true, "the source stays until the key is released")
        source.send(.key(virtualKey: 5, pressed: false))
        require(binds.capture?.isMonitoring == false && source.handler == nil)
        print("PASS: capture and save write one exact bind and drain the held key")

        // Escape, focus loss and game start cancel without writing.
        let afterCapture = try Data(contentsOf: file)
        binds.beginCapture(.slot(action: "+smoke", slot: .primary))
        source.send(.key(virtualKey: 53, pressed: true))
        require(binds.target == nil && binds.capture?.state == .cancelled(.escape))
        binds.beginCapture(.slot(action: "+smoke", slot: .primary))
        source.send(.focusLost)
        require(binds.capture?.state == .cancelled(.focusLost) && binds.target == nil)
        binds.beginCapture(.slot(action: "+smoke", slot: .primary))
        binds.gameStarted()
        require(binds.capture?.state == .cancelled(.gameStarted) && source.handler == nil)
        binds.beginCapture(.slot(action: "+smoke", slot: .primary))
        source.send(.key(virtualKey: 50, pressed: true))
        require(binds.captureNote?.contains("console") == true && binds.target != nil, "the console key is explained, not bound")
        binds.cancelCapture()
        require(try Data(contentsOf: file) == afterCapture)
        print("PASS: Escape, focus loss, game start and the console key never write")

        // A key that already does something else waits for review.
        binds.beginCapture(.slot(action: "+back", slot: .secondary))
        source.send(.key(virtualKey: 13, pressed: true)); source.send(.key(virtualKey: 13, pressed: false))
        require(try binds.review != nil && Data(contentsOf: file) == afterCapture, "a conflict is never written before review")
        require(binds.reviewText(binds.review!) == "W is also Move forward, so one press would do both. Use it for Move backward only?")
        binds.cancelReview()
        require(try binds.review == nil && Data(contentsOf: file) == afterCapture)
        binds.beginCapture(.slot(action: "+back", slot: .secondary))
        source.send(.key(virtualKey: 13, pressed: true)); source.send(.key(virtualKey: 13, pressed: false))
        binds.confirmReview()
        var replaced = ConfigDocument(data: afterCapture, dialect: .cod2x)
        try replaced.setBind(key: "w", binding: "+back")
        require(try Data(contentsOf: file) == replaced.serialized() && binds.review == nil)
        print("PASS: conflicts wait for review; Keep here replaces exactly one binding")

        // Visual inspector: assign and clear any key, with undo.
        let undo = UndoManager(); undo.groupsByEvent = false
        let beforeVisual = try Data(contentsOf: file)
        undo.beginUndoGrouping(); binds.setKey(EngineKey(spelling: "x")!, action: "+melee", undo: undo); undo.endUndoGrouping()
        var melee = ConfigDocument(data: beforeVisual, dialect: .cod2x); try melee.setBind(key: "x", binding: "+melee")
        require(try Data(contentsOf: file) == melee.serialized())
        undo.undo()
        require(try Data(contentsOf: file) == beforeVisual, "Command-Z restores the exact bytes")
        binds.setKey(EngineKey(spelling: "tab")!, action: nil)
        var cleared = ConfigDocument(data: beforeVisual, dialect: .cod2x); try cleared.removeBind(key: "tab")
        require(try Data(contentsOf: file) == cleared.serialized())
        binds.beginCapture(.find)
        source.send(.key(virtualKey: 1, pressed: true)); source.send(.key(virtualKey: 1, pressed: false))
        require(binds.selectedKey == EngineKey(spelling: "s"))
        print("PASS: Visual assigns, clears, undoes and finds keys by pressing them")

        // Advanced binds through B2: toggle, cycle with review labels, wait warning, refusals, export.
        binds.newChain()
        binds.beginCapture(.chainKey)
        source.send(.key(virtualKey: 46, pressed: true)); source.send(.key(virtualKey: 46, pressed: false))
        binds.updateDraft { $0.kind = .toggle; $0.setting = "cg_drawFPS"; $0.values = ["Off", "Simple"] }
        require(binds.draft?.key == "m" && binds.chainEdit != nil && binds.chainError == nil && !binds.chainNeedsConfirmation)
        let preview = binds.pressPreview()
        require(preview.count == 3 && preview[0].press.first?.text == "Switch Show frame rate")
        require(preview.map(\.value) == ["Simple", "Off", "Simple"] || preview.map(\.value) == ["Off", "Simple", "Off"], "each press shows the value it leaves")
        let beforeChain = try Data(contentsOf: file)
        binds.saveChain()
        var toggled = ConfigDocument(data: beforeChain, dialect: .cod2x); try toggled.setBind(key: "m", binding: "toggle cg_drawFPS \"Off\" \"Simple\"")
        require(try Data(contentsOf: file) == toggled.serialized() && session.history.first?.summary == "Changed advanced binds")
        require(binds.chains.count == 1 && binds.ownedChain(on: EngineKey(spelling: "m")!) != nil)

        binds.newChain()
        binds.updateDraft { $0.key = "n"; $0.kind = .cycle; $0.steps = [.init(kind: .say, text: "hello"), .init(kind: .say, text: "bye")] }
        require(binds.chainRisks.contains(.chat) && binds.chainNeedsConfirmation, "chat needs the person's OK")
        let cycleWords = binds.pressPreview().map { $0.press.map(\.text) }
        require(cycleWords.prefix(3) == [["Say “hello”"], ["Say “bye”"], ["Say “hello”"]], "the cycle preview follows each press")
        binds.saveChain()
        require(try Data(contentsOf: file) == toggled.serialized(), "unconfirmed risky binds are not saved")
        binds.saveChain(confirmed: true)
        let chainFile = session.home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "Main")))
        require(FileManager.default.fileExists(atPath: chainFile.path) && binds.chains.count == 2)
        require(session.current?.version.chain == (try Data(contentsOf: chainFile)))

        binds.newChain()
        binds.updateDraft { $0.key = "k"; $0.kind = .several; $0.commands = [.init(kind: .action, text: "+gostand"), .init(kind: .command, text: "wait"), .init(kind: .action, text: "gocrouch")] }
        require(binds.chainNotes.contains { $0.text.hasPrefix("Competitive CoD2x servers turn off wait") }, "competitive wait is called out")
        binds.updateDraft { $0.kind = .cycle; $0.steps = [.init(kind: .say, text: String(repeating: "a", count: 1100)), .init(kind: .say, text: "b")] }
        require(binds.chainError == ConfigChainError.helperTooLong.errorDescription, "B2's 1022-byte refusal is actionable")
        binds.deleteChain()
        require(binds.draft == nil)

        let export = scratch.appendingPathComponent("export/config_mp.cfg")
        binds.exportWindows(to: export)
        require(try Data(contentsOf: export) == ConfigChains.windowsExport(read: session.current!) && model.failure == nil)
        binds.exportWindows(to: session.home.appendingPathComponent("main/windows.cfg"))
        require(model.failure?.message == ConfigChainError.unsafeExport.errorDescription, "exports into the game home are refused")
        model.failure = nil
        print("PASS: advanced binds preview, review labels, wait warning, B2 refusals, save and Windows export")

        // Read-only while the game runs.
        try session.reconcileGames([7001])
        let locked = try Data(contentsOf: file)
        binds.assign(EngineKey(spelling: "v"), to: "+melee", slot: .primary)
        binds.beginCapture(.slot(action: "+melee", slot: .primary))
        require(try Data(contentsOf: file) == locked && binds.target == nil && model.failure != nil)
        try session.reconcileGames([])
        model.failure = nil
        print("PASS: binds are read-only while the game runs")

        // The keyboard drawing covers every mapped key once in each shape.
        for shape in KeyboardShape.allCases {
            let keys = ConfigKeyboardLayout.keys(shape: shape, size: .full)
            require(Set(keys.map(\.virtualKey)).count == keys.count, "\(shape) keys are unique")
            require(keys.allSatisfy { InputSourceSnapshot.scancodes[$0.virtualKey] != nil || $0.virtualKey == 63 }, "\(shape) keys are physical keys")
        }
        require(ConfigKeyboardLayout.keys(shape: .ansi, size: .percent60).allSatisfy { $0.virtualKey != 122 }, "60% has no function row")
        print("PASS: keyboard drawings for ANSI, ISO and JIS at every size")
    }

    @MainActor static func checkup(scratch: URL, catalog: ConfigCatalog, rules: ConfigRules, optimizer: ConfigOptimizer) throws {
        let config = bytes("seta cl_maxpackets \"100\"\nseta com_maxfps \"333\"\nseta rate \"25000\"\nseta snaps \"30\"\nseta sensitivity \"2.6\"\n")
        let (model, session, file) = try studio(home: scratch.appendingPathComponent("checkup"), config: config, catalog: catalog)
        let checkup = ConfigCheckupModel(studio: model, loadRules: { rules }, loadOptimizer: { optimizer })
        checkup.load()
        require(checkup.rulesetIDs == ["public-cod2x"] && checkup.available.allSatisfy { $0.mode == .cod2x })
        checkup.rulesetIDs = ["zpam-4.08"]
        let report = try require(checkup.report)
        require(report.suggestions.contains { $0.id == "competitive-frame-cap" }, "a 333 cap is outside the competitive band")
        require(report.suggestions.allSatisfy { !$0.changes.contains { $0.setting.lowercased().hasPrefix("cg_fov") } }, "FOV is never suggested")
        let cap = try require(report.suggestions.first { $0.id == "competitive-frame-cap" })
        require(checkup.changeText(cap.changes[0]) == "Frame rate cap: 333 fps → 250 fps")
        let gauges = checkup.gauges(report)
        require(gauges.map(\.status).prefix(3) == ["Not measured", "Not measured", "Not measured"] && gauges[0].value == "333 FPS")
        let capSpec = try require(model.spec(named: "com_maxfps"))
        require(checkup.badge(capSpec)?.text == "125 fps–250 fps" && checkup.badge(capSpec)?.tone == .negative)
        require(checkup.tip(capSpec)?.fix?.id == "competitive-frame-cap")
        require(checkup.violations.contains { $0.setting.lowercased() == "com_maxfps" })
        print("PASS: Check-up suggestions, plain changes, Not measured dials, rule badges and tips")

        // Not now is scoped; Apply and Undo are exact transactions.
        checkup.notNow("competitive-frame-cap")
        require(checkup.report?.suggestions.contains { $0.id == "competitive-frame-cap" } == false && checkup.dismissedHere.count == 1)
        checkup.showAgain()
        let undo = UndoManager(); undo.groupsByEvent = false
        undo.beginUndoGrouping(); checkup.apply("competitive-frame-cap", undo: undo); undo.endUndoGrouping()
        var applied = ConfigDocument(data: config, dialect: .cod2x); try applied.setSetting(named: "com_maxfps", value: "250")
        require(try Data(contentsOf: file) == applied.serialized() && session.history.first?.source == .checkup)
        require(checkup.appliedHere().map(\.id) == ["competitive-frame-cap"])
        undo.undo()
        require(try Data(contentsOf: file) == config && checkup.appliedHere().isEmpty, "Undo restores the exact bytes")
        require(session.history.first?.source == .restore)
        checkup.apply("competitive-frame-cap")
        checkup.undoApplied("competitive-frame-cap")
        require(try Data(contentsOf: file) == config)
        print("PASS: Not now, Apply and Undo through the shared store with exact bytes")

        // Contradictions are shown as such; unknown policy gives no advice.
        checkup.rulesetIDs = ["zpam-4.08", "polish-2026"]
        let packets = try require(model.spec(named: "cl_maxpackets"))
        require(checkup.evaluation(packets)?.status == .conflict && checkup.badge(packets)?.text == "Rules disagree")
        checkup.rulesetIDs = []
        require(checkup.report?.suggestions.isEmpty == true && checkup.badge(capSpec) == nil)
        try session.reconcileGames([7002])
        checkup.rulesetIDs = ["zpam-4.08"]
        checkup.apply("competitive-frame-cap")
        require(try Data(contentsOf: file) == config && model.failure != nil, "Apply refuses while the game runs")
        try session.reconcileGames([])
        print("PASS: contradictions, missing rules and the running game")
    }

    static func require<T>(_ value: T?, line: Int = #line) throws -> T {
        guard let value else { fatalError("FAIL: missing value (line \(line))") }
        return value
    }
}
