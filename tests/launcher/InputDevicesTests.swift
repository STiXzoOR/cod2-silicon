import Foundation

@MainActor private final class DeviceFixture: InputDeviceProvider {
    var snapshot = InputSourceSnapshot(id: "ABC", name: "ABC", shape: .iso, labels: InputDevicesTests.abcLabels())
    var connected = [InputDevice(id: "keyboard", name: "Magic Keyboard", vendorID: 0x05ac, kind: .keyboard, keyboardShape: .iso),
                     InputDevice(id: "mouse", name: "Fixture mouse", vendorID: nil, kind: .mouse, buttonCount: 8)]
    var changed: (@MainActor () -> Void)?
    func inputSource() -> InputSourceSnapshot { snapshot }
    func devices() -> [InputDevice] { connected }
    func startObserving(_ changed: @escaping @MainActor () -> Void) { self.changed = changed }
    func stopObserving() { changed = nil }
}
enum InputDevicesTests {
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: \(message)") }
    }
    static func engineMapping(_ path: String) throws {
        let rows = try String(contentsOfFile: path, encoding: .utf8).split(separator: "\n")
        for row in rows {
            let cells = row.split(separator: " ")
            if cells[0] == "name" {
                let key = EngineKey(spelling: String(cells[1]))
                require(key?.number == Int(cells[2]), "engine key name \(cells[1]) → \(cells[2])")
                require(key.flatMap { EngineKey(spelling: $0.spelling) } == key, "canonical spelling round-trips \(cells[1])")
                continue
            }
            let input = Int(cells[1])!, output = Int(cells[2])!
            let actual = cells[0] == "key" ? EngineInputMapping.keyNumber(sdlKeycode: Int32(input)) :
                EngineInputMapping.mouseKeyNumber(sdlButton: input)
            require(actual == (output == 0 ? nil : output), "production SDL \(cells[0]) \(input) → \(output), got \(String(describing: actual))")
        }
        require(EngineKey(spelling: "§") == nil, "section sign is unsupported, never F1")
        require(EngineKey(spelling: "F1")?.number == 167, "F1 retains its engine identity")
        require(EngineKey(spelling: "W")?.spelling == "w", "config bind spelling is lowercase")
        require(EngineKey(number: 0) == nil && EngineKey(number: 256) == nil, "reject invalid engine identities")
        require(EngineKey(number: 34)?.spelling == "0x22" && EngineKey(number: 59)?.spelling == "semicolon", "quote/semicolon bind grammar names")
        print("PASS: 1029 production SDL conversions, 98 key names and canonical engine identities")
    }
    static func abcLabels() -> [UInt16: String] {
        [0:"a", 1:"s", 2:"d", 3:"f", 4:"h", 5:"g", 6:"z", 7:"x", 8:"c", 9:"v", 10:"§", 11:"b",
         12:"q", 13:"w", 14:"e", 15:"r", 16:"y", 17:"t", 18:"1", 19:"2", 20:"3", 21:"4", 22:"6", 23:"5",
         24:"=", 25:"9", 26:"7", 27:"-", 28:"8", 29:"0", 30:"]", 31:"o", 32:"u", 33:"[", 34:"i", 35:"p",
         37:"l", 38:"j", 39:"'", 40:"k", 41:";", 42:"\\", 43:",", 44:"/", 45:"n", 46:"m", 47:".", 50:"`"]
    }
    static func layouts() {
        let abc = InputSourceSnapshot(id: "ABC", name: "ABC", shape: .ansi, labels: abcLabels())
        require(abc.mapping(virtualKey: 13).engineKey?.number == 119, "ABC physical W binds w")
        let uppercase = InputSourceSnapshot(id: "Uppercase", name: "Custom", shape: .ansi, labels: abcLabels().merging([13:"W"]) { _, new in new })
        require(uppercase.mapping(virtualKey: 13).engineKey == nil,
                "custom uppercase keycodes cannot silently become lowercase bindings; engine bind folds but key events do not")
        let remappedReturn = InputSourceSnapshot(id: "Remapped", name: "Custom", shape: .ansi, labels: abcLabels().merging([36:"a"]) { _, new in new })
        require(remappedReturn.mapping(virtualKey: 36).engineKey?.number == 97, "custom control-key translation follows SDL instead of inventing Return identity")
        let iso = InputSourceSnapshot(id: "ABC-ISO", name: "ABC", shape: .iso, labels: abcLabels().merging([10:"`",50:"§"]) { _, new in new })
        require(iso.mapping(virtualKey: 10).isReserved && iso.mapping(virtualKey: 50).engineKey == nil && iso.label(virtualKey: 50) == "§",
                "ISO ABC keeps the top section sign unavailable and console beside Shift")
        var greekLabels = abcLabels()
        for (key, value) in [UInt16(0):"α", 2:"δ", 8:"ψ", 11:"β", 13:"ς"] { greekLabels[key] = value }
        let greek = InputSourceSnapshot(id: "Greek", name: "Greek", shape: .iso, labels: greekLabels)
        require(greek.label(virtualKey: 13) == "ς" && greek.usesLatinFallback, "Greek labels stay Greek while SDL falls back")
        require(greek.mapping(virtualKey: 13).engineKey?.number == 119, "Greek physical W uses SDL Latin fallback")
        require(greek.mapping(virtualKey: 50).engineKey == nil && greek.mapping(virtualKey: 10).isReserved,
                "Greek fallback preserves ISO unsupported top key and console beside Shift")
        let digits: [UInt16] = [18,19,20,21,23,22,26,28,25,29]
        var frenchLabels = abcLabels(), frenchShifted: [UInt16: String] = [:]
        frenchLabels[0] = "q"; frenchLabels[12] = "a"; frenchLabels[13] = "z"; frenchLabels[6] = "w"
        for (i, symbol) in ["&","é","\"","'","(","-","è","_","ç","à"].enumerated() {
            frenchLabels[digits[i]] = symbol; frenchShifted[digits[i]] = String((i + 1) % 10)
        }
        let french = InputSourceSnapshot(id: "French", name: "AZERTY", shape: .iso, labels: frenchLabels, shiftedLabels: frenchShifted)
        require(french.mapping(virtualKey: 13).engineKey?.number == 122 && french.mapping(virtualKey: 12).engineKey?.number == 97,
                "AZERTY maps symbols, not QWERTY positions")
        require(french.label(virtualKey: 19) == "é" && french.mapping(virtualKey: 19).engineKey?.number == 50,
                "French number row follows SDL french_numbers option")
        frenchShifted[29] = "x"
        let partialFrench = InputSourceSnapshot(id: "Partial", name: "Partial", shape: .iso, labels: frenchLabels, shiftedLabels: frenchShifted)
        require(partialFrench.mapping(virtualKey: 19).engineKey == nil, "partial French number rows do not invent the SDL digit fallback")
        var germanLabels = abcLabels(); germanLabels[16] = "z"; germanLabels[6] = "y"; germanLabels[33] = "ü"
        let german = InputSourceSnapshot(id: "German", name: "QWERTZ", shape: .iso, labels: germanLabels)
        require(german.mapping(virtualKey: 16).engineKey?.number == 122 && german.mapping(virtualKey: 6).engineKey?.number == 121,
                "QWERTZ Y/Z follow layout")
        require(german.mapping(virtualKey: 33).engineKey == nil, "Latin-layout umlaut remains unsupported")
        var dvorakLabels = abcLabels(); dvorakLabels[12] = "'"; dvorakLabels[13] = ","; dvorakLabels[2] = "e"
        let dvorak = InputSourceSnapshot(id: "Dvorak", name: "Dvorak", shape: .ansi, labels: dvorakLabels)
        require(dvorak.mapping(virtualKey: 13).engineKey?.number == 44 && dvorak.mapping(virtualKey: 2).engineKey?.number == 101,
                "Dvorak preserves punctuation/letter remapping")
        let jis = InputSourceSnapshot(id: "Japanese", name: "JIS", shape: .jis, labels: abcLabels().merging([93:"¥",94:"\\",102:"英数",104:"かな"]) { _, new in new })
        for key: UInt16 in [93,94,95,102,104] { require(jis.mapping(virtualKey: key).engineKey == nil, "JIS special \(key) is unavailable to engine") }
        require(abc.mapping(virtualKey: 36).engineKey?.spelling == "enter" && abc.mapping(virtualKey: 76).engineKey?.spelling == "kp_enter", "Return differs from keypad Enter")
        require(abc.mapping(virtualKey: 63).engineKey == nil, "Fn is a hardware modifier, no binding")
        require(abc.label(virtualKey: 122) == "F1" && abc.label(virtualKey: 49) == "Space", "function/control keys have readable labels")
        require(abc.mapping(virtualKey: 105).engineKey == nil && abc.mapping(virtualKey: 107).engineKey == nil && abc.mapping(virtualKey: 113).engineKey?.number == 153,
                "pinned SDL Darwin F13/F14/F15 positions remain PrintScreen/ScrollLock/Pause")
        print("PASS: ABC, Greek, AZERTY, QWERTZ, Dvorak, JIS and unsupported physical keys")
    }
    @MainActor static func devices() {
        require(InputDevice.detectedKind(usagePage: 0xff00, usage: 1, name: "Magic Trackpad") == nil,
                "vendor HID collections do not become extra trackpads just because they share the product name")
        require(InputDevice.detectedKind(usagePage: 1, usage: 6, name: "Internal Keyboard / Trackpad") == .keyboard,
                "keyboard usage takes precedence over a shared trackpad name")
        require(InputDevice.detectedKind(usagePage: 1, usage: 2, name: "Magic Trackpad") == .trackpad &&
                InputDevice.detectedKind(usagePage: 13, usage: 5, name: "Touchpad") == .trackpad,
                "recognize pointer and digitizer trackpads without claiming vendor collections")
        let provider = DeviceFixture(), model = InputDevicesModel(provider: DeviceFixture())
        let live = InputDevicesModel(provider: provider)
        live.startObserving()
        let original = ConfigDocument(data: Data("bind w \"+forward\"\n".utf8), dialect: .stock)
        provider.snapshot = InputSourceSnapshot(id: "Dvorak", name: "Dvorak", shape: .ansi, labels: abcLabels().merging([13:","]) { _, new in new })
        provider.changed?()
        require(live.inputSource.name == "Dvorak" && live.inputSource.mapping(virtualKey: 13).engineKey?.number == 44,
                "input-source notification refreshes current labels and capture mapping")
        require(original.effectiveBind(key: "w") == "+forward" && original.effectiveBind(key: ",") == nil,
                "layout change never rewrites binding identity")
        let keyboard = model.devices[0], detected = model.presentation(for: keyboard)
        require(detected.shape.value == .iso && detected.shape.basis == .detected, "device-reported shape takes precedence")
        require(detected.size.value == .percent75 && detected.size.basis == .guessed, "Magic Keyboard size is labelled as guess")
        model.setOverride(.init(shape: .jis, size: .percent60, modifiers: .pc), for: keyboard.id)
        let overridden = model.presentation(for: keyboard)
        require(overridden.shape.value == .jis && overridden.size.value == .percent60 && overridden.modifiers.value == .pc,
                "shape, size, modifiers have explicit user overrides")
        require(overridden.size.basis == .userOverride, "override is distinguishable from guess")
        provider.connected = []; provider.changed?()
        require(live.devices.isEmpty && original.effectiveBind(key: "w") == "+forward", "device unplug refreshes inventory without rewriting binds")
        model.refresh()
        require(model.presentation(for: keyboard).shape.value == .jis, "refresh retains user overrides")
        model.setOverride(nil, for: keyboard.id)
        require(model.presentation(for: keyboard).shape.value == .iso, "clear override restores detection")
        live.stopObserving()
        require(provider.changed == nil, "input-source observer has explicit cleanup")
        require(model.devices[1].buttonCount == 8, "report real extra buttons without binding them")
        print("PASS: input-source/device refresh, labelled guesses, overrides, unchanged binding identity")
    }
    /// Press keys to verify: each answer is a physical key position, never a guess from labels.
    @MainActor static func verification() {
        func answers(_ steps: [UInt16?]) -> KeyboardVerification {
            var flow = KeyboardVerification()
            for step in steps { if let key = step { flow.press(key) } else { flow.skip() } }
            return flow
        }
        var flow = KeyboardVerification()
        require(flow.step == .besideShift && !flow.canSkip, "every keyboard has a key beside left Shift, so the first step can't be skipped")
        flow.press(0)
        require(flow.step == .besideShift && flow.note != nil && flow.result == nil, "a different key is explained and the step stays")
        flow.press(50)
        require(flow.step == .besideSpace && flow.note == nil, "the extra key beside Shift identifies ISO")
        flow.press(58)
        require(flow.step == .keypad && flow.canSkip, "Option beside Space means PC modifier labels")
        flow.press(19)
        require(flow.step == .keypad && flow.note?.contains("keypad") == true, "a number-row digit isn't taken for the keypad")
        flow.skip(); flow.press(122); flow.press(115)
        require(flow.step == .done && flow.result == InputDeviceOverride(shape: .iso, size: .tkl, modifiers: .pc, verified: true),
                "ISO, PC labels, function row and Home make a verified tenkeyless override")
        require(answers([6, 55, nil, 122, nil]).result == InputDeviceOverride(shape: .ansi, size: .percent75, modifiers: .mac, verified: true), "ANSI Mac 75%")
        require(answers([6, 102, 82]).result == InputDeviceOverride(shape: .jis, size: .full, modifiers: .mac, verified: true), "英数 beside Space is JIS; a keypad is full size")
        require(answers([6, 55, nil, nil, 123]).result?.size == .percent65, "no function row, but arrows: 65%")
        require(answers([6, 55, nil, nil, nil]).result?.size == .percent60, "no function row and no arrows: 60%")
        require(answers([6, 55, nil, nil]).step == .arrows && answers([6, 55, nil, 122]).step == .navigation, "65% and 60% are told apart by the arrows")

        let model = InputDevicesModel(provider: DeviceFixture()), keyboard = model.devices[0]
        require(model.presentation(for: keyboard).size.explanation.hasPrefix("Estimated"), "a guessed size says Estimated until confirmed")
        model.setOverride(flow.result, for: keyboard.id)
        let verified = model.presentation(for: keyboard)
        require(verified.shape.basis == .verified && verified.size.basis == .verified && verified.modifiers.basis == .verified, "a verified answer is its own basis")
        require(verified.size.explanation == "You confirmed this by pressing keys", "the explanation says how it was confirmed")
        print("PASS: Press keys to verify finds shape, modifier labels and size from physical key positions")
    }
    /// Learn button positions: which physical button sends which number, and what the game can use.
    @MainActor static func learning() {
        var flow = PointerButtonLearning()
        require(flow.step == .position(.left), "learning starts with the left button")
        flow.click(0); flow.click(1)
        flow.click(0)
        require(flow.step == .position(.wheel) && flow.note?.contains("left") == true, "a button already learned is explained, not reused")
        flow.click(2); flow.click(4); flow.click(3)
        require(flow.step == .extras, "the five positions the game can use come first")
        flow.click(6)
        require(flow.extras == [6] && flow.note?.contains("can’t use") == true, "a seventh button is explained as unusable by the game")
        flow.skip()
        let map = flow.result
        require(flow.step == .done && map?.buttons == [.left: 0, .right: 1, .wheel: 2, .back: 4, .forward: 3] && map?.extras == [6], "every position is recorded as clicked")
        require(map?.position(appKitButton: 3) == .forward && map?.position(appKitButton: 4) == .back, "side buttons are placed as learned")
        var short = PointerButtonLearning()
        short.click(0); short.click(1); short.skip(); short.skip(); short.skip(); short.skip()
        require(short.result?.buttons == [.left: 0, .right: 1] && short.result?.position(appKitButton: 2) == nil, "missing buttons stay missing")
        let model = InputDevicesModel(provider: DeviceFixture())
        model.setLearnedButtons(map, for: "mouse")
        require(model.learnedButtons["mouse"] == map, "the learned map is kept for the device")
        print("PASS: Learn button positions places side buttons and explains extra buttons")
    }
    @MainActor static func run(_ fixture: String) throws {
        try engineMapping(fixture)
        layouts()
        devices()
        verification()
        learning()
    }
    @MainActor static func systemProbe() throws {
        let provider = SystemInputDeviceProvider(), source = provider.inputSource(), devices = provider.devices()
        let record: [String: Any] = ["inputSource": source.name, "shape": source.shape.rawValue,
            "translatedLabels": source.labels.count, "latinFallback": source.usesLatinFallback,
            "key13": source.mapping(virtualKey: 13).engineKey?.number as Any? ?? NSNull(),
            "key10": source.mapping(virtualKey: 10).engineKey?.number as Any? ?? NSNull(),
            "key50": source.mapping(virtualKey: 50).engineKey?.number as Any? ?? NSNull(),
            "keyboards": devices.filter { $0.kind == .keyboard }.count,
            "mice": devices.filter { $0.kind == .mouse }.count, "trackpads": devices.filter { $0.kind == .trackpad }.count,
            "reportedButtonCounts": devices.compactMap(\.buttonCount)]
        let json = try JSONSerialization.data(withJSONObject: record, options: [.sortedKeys])
        print("SYSTEM PROBE: " + String(decoding: json, as: UTF8.self))
    }
}
