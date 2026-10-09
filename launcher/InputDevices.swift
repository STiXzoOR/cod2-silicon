import Foundation
import Combine
import Carbon
import IOKit
import IOKit.hid

/// Config files identify engine key numbers; physical positions and layout
/// labels are presentation/capture inputs, never persistent binding identity.
struct EngineKey: Hashable, Comparable, Sendable, Identifiable {
    let number: Int
    var id: Int { number }
    var spelling: String { ConfigDocument.keySpelling(number) }
    init?(number: Int) {
        guard (1...255).contains(number) else { return nil }
        self.number = (65...90).contains(number) ? number + 32 : number
    }
    init?(spelling: String) {
        guard let number = ConfigDocument.keyNumber(spelling) else { return nil }
        self.init(number: number)
    }
    static func < (lhs: Self, rhs: Self) -> Bool { lhs.number < rhs.number }
}

enum KeyboardSize: String, CaseIterable, Sendable { case full, tkl, percent75, percent65, percent60, unknown }
enum KeyboardModifierStyle: String, CaseIterable, Sendable { case mac, pc }
enum InputDeviceKind: String, Sendable { case keyboard, mouse, trackpad }
struct InputDevice: Identifiable, Equatable, Sendable {
    let id: String
    let name: String
    let vendorID: Int?
    let kind: InputDeviceKind
    var keyboardShape: KeyboardShape? = nil
    var buttonCount: Int? = nil
    static func detectedKind(usagePage: Int?, usage: Int?, name: String) -> InputDeviceKind? {
        if usagePage == 1 && usage == 6 { return .keyboard }
        if usagePage == 13 && usage == 5 { return .trackpad }
        if usagePage == 1 && (usage == 1 || usage == 2) {
            return name.lowercased().contains("trackpad") ? .trackpad : .mouse
        }
        return nil
    }
}
struct InputDeviceOverride: Equatable, Sendable {
    var shape: KeyboardShape? = nil
    var size: KeyboardSize? = nil
    var modifiers: KeyboardModifierStyle? = nil
}
struct InputDeviceChoice<Value: Equatable & Sendable>: Equatable, Sendable {
    enum Basis: Sendable { case detected, guessed, userOverride, unknown }
    let value: Value
    let basis: Basis
    let explanation: String
}
struct InputDevicePresentation: Sendable {
    let device: InputDevice
    let shape: InputDeviceChoice<KeyboardShape>
    let size: InputDeviceChoice<KeyboardSize>
    let modifiers: InputDeviceChoice<KeyboardModifierStyle>
}
@MainActor protocol InputDeviceProvider: AnyObject {
    func inputSource() -> InputSourceSnapshot
    func devices() -> [InputDevice]
    func startObserving(_ changed: @escaping @MainActor () -> Void)
    func stopObserving()
}
@MainActor final class InputDevicesModel: ObservableObject {
    @Published private(set) var inputSource: InputSourceSnapshot
    @Published private(set) var devices: [InputDevice]
    @Published private(set) var overrides: [String: InputDeviceOverride] = [:]
    private let provider: any InputDeviceProvider
    init(provider: any InputDeviceProvider) {
        self.provider = provider
        inputSource = provider.inputSource(); devices = provider.devices()
    }
    func startObserving() { provider.startObserving { [weak self] in self?.refresh() } }
    func stopObserving() { provider.stopObserving() }
    func refresh() { inputSource = provider.inputSource(); devices = provider.devices() }
    func setOverride(_ value: InputDeviceOverride?, for deviceID: String) { overrides[deviceID] = value }
    func presentation(for device: InputDevice) -> InputDevicePresentation {
        let override = overrides[device.id] ?? .init(), name = device.name.lowercased()
        let size: KeyboardSize
        if name.contains("60%") { size = .percent60 }
        else if name.contains("65%") { size = .percent65 }
        else if name.contains("75%") || name.contains("magic keyboard") && !name.contains("numeric") { size = .percent75 }
        else if name.contains("tkl") || name.contains("tenkeyless") { size = .tkl }
        else if name.contains("numeric") || name.contains("full size") { size = .full }
        else { size = .unknown }
        return .init(device: device,
            shape: .init(value: override.shape ?? device.keyboardShape ?? inputSource.shape,
                         basis: override.shape != nil ? .userOverride : device.keyboardShape != nil ? .detected : .guessed,
                         explanation: override.shape != nil ? "Your keyboard shape" : device.keyboardShape != nil ? "macOS keyboard identification" : "Current macOS keyboard shape; this device does not report its own"),
            size: .init(value: override.size ?? size, basis: override.size != nil ? .userOverride : size == .unknown ? .unknown : .guessed,
                        explanation: override.size != nil ? "Your keyboard size" : size == .unknown ? "Choose your keyboard size" : "Guess from the device name; choose a different size if needed"),
            modifiers: .init(value: override.modifiers ?? (device.vendorID == 0x05ac ? .mac : .pc),
                             basis: override.modifiers != nil ? .userOverride : .guessed,
                             explanation: override.modifiers != nil ? "Your modifier labels" : "Guess from the keyboard vendor"))
    }
}

/// Registry metadata and layout notifications only. Never opens HID devices,
/// reads raw input or requests Input Monitoring permission.
@MainActor final class SystemInputDeviceProvider: NSObject, InputDeviceProvider {
    private var changed: (@MainActor () -> Void)?
    func startObserving(_ changed: @escaping @MainActor () -> Void) {
        stopObserving(); self.changed = changed
        DistributedNotificationCenter.default().addObserver(self, selector: #selector(sourceChanged(_:)),
            name: NSNotification.Name(kTISNotifySelectedKeyboardInputSourceChanged as String), object: nil)
    }
    func stopObserving() { DistributedNotificationCenter.default().removeObserver(self); changed = nil }
    @objc nonisolated private func sourceChanged(_ notification: Notification) {
        Task { @MainActor [weak self] in self?.changed?() }
    }
    deinit { DistributedNotificationCenter.default().removeObserver(self) }

    func inputSource() -> InputSourceSnapshot {
        let selected = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
        let layout = TISCopyCurrentKeyboardLayoutInputSource().takeRetainedValue()
        func string(_ property: CFString) -> String? {
            guard let value = TISGetInputSourceProperty(selected, property) else { return nil }
            return Unmanaged<CFString>.fromOpaque(value).takeUnretainedValue() as String
        }
        let type = Int(LMGetKbdType()), shape = Self.shape(keyboardType: type) ?? .ansi
        var labels: [UInt16: String] = [:], shifted: [UInt16: String] = [:]
        if let pointer = TISGetInputSourceProperty(layout, kTISPropertyUnicodeKeyLayoutData) {
            let data = Unmanaged<CFData>.fromOpaque(pointer).takeUnretainedValue()
            if let bytes = CFDataGetBytePtr(data) {
                let keyboard = UnsafeRawPointer(bytes).assumingMemoryBound(to: UCKeyboardLayout.self)
                for key in InputSourceSnapshot.scancodes.keys {
                    for modifier in [UInt32(0), UInt32(shiftKey >> 8)] {
                        var deadState: UInt32 = 0, length = 0, buffer = [UniChar](repeating: 0, count: 8)
                        let status = UCKeyTranslate(keyboard, key, UInt16(kUCKeyActionDown), modifier, UInt32(type),
                            OptionBits(kUCKeyTranslateNoDeadKeysMask), &deadState, buffer.count, &length, &buffer)
                        guard status == noErr, length > 0, buffer[0] != 0x10 else { continue }
                        let text = String(utf16CodeUnits: buffer, count: length)
                        // The model supplies readable names for control keys.
                        if buffer[0] >= 32 { if modifier == 0 { labels[key] = text } else { shifted[key] = text } }
                    }
                }
            }
        }
        return .init(id: string(kTISPropertyInputSourceID) ?? "unknown", name: string(kTISPropertyLocalizedName) ?? "Unknown input source",
                     shape: shape, labels: labels, shiftedLabels: shifted)
    }
    private static func shape(keyboardType: Int) -> KeyboardShape? {
        guard let type = Int16(exactly: keyboardType) else { return nil }
        switch Int(KBGetLayoutType(type)) {
        case kKeyboardANSI: return .ansi
        case kKeyboardISO: return .iso
        case kKeyboardJIS: return .jis
        default: return nil
        }
    }
    func devices() -> [InputDevice] {
        var iterator: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOHIDDevice"), &iterator) == KERN_SUCCESS else { return [] }
        defer { IOObjectRelease(iterator) }
        var result: [InputDevice] = []
        while case let service = IOIteratorNext(iterator), service != 0 {
            defer { IOObjectRelease(service) }
            var properties: Unmanaged<CFMutableDictionary>?
            guard IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, 0) == KERN_SUCCESS,
                  let values = properties?.takeRetainedValue() as? [String: Any] else { continue }
            let page = (values["PrimaryUsagePage"] as? NSNumber)?.intValue,
                usage = (values["PrimaryUsage"] as? NSNumber)?.intValue,
                name = values["Product"] as? String ?? "Unknown input device"
            guard let kind = InputDevice.detectedKind(usagePage: page, usage: usage, name: name) else { continue }
            var registryID: UInt64 = 0
            guard IORegistryEntryGetRegistryEntryID(service, &registryID) == KERN_SUCCESS else { continue }
            var buttons: Int?
            if kind != .keyboard, let hid = IOHIDDeviceCreate(kCFAllocatorDefault, service),
               let elements = IOHIDDeviceCopyMatchingElements(hid, nil, IOOptionBits(0)) as? [IOHIDElement] {
                let usages = Set(elements.filter { IOHIDElementGetUsagePage($0) == 9 && IOHIDElementGetType($0) == kIOHIDElementTypeInput_Button }
                    .map { IOHIDElementGetUsage($0) }.filter { $0 > 0 })
                if !usages.isEmpty { buttons = usages.count }
            }
            let keyboardType = (values["KeyboardType"] as? NSNumber)?.intValue
            result.append(.init(id: String(registryID), name: name, vendorID: (values["VendorID"] as? NSNumber)?.intValue,
                                kind: kind, keyboardShape: keyboardType.flatMap(Self.shape), buttonCount: buttons))
        }
        return result.sorted { $0.id < $1.id }
    }
}

enum EngineInputMapping {
    // src/unix/linux_input.c:sdl_to_keynum. SDL2 uses bit 30 for scancodes;
    // SDL3 extended keycodes are converted by the pinned sdl2-compat backend.
    static func keyNumber(sdlKeycode: Int32) -> Int? {
        let value = Int(sdlKeycode)
        if let special = specialKeys[value] { return special }
        return (32..<127).contains(value) ? value : nil
    }
    static func mouseKeyNumber(sdlButton: Int) -> Int? {
        // SDL buttons: left=1, middle=2, right=3, X1=4, X2=5.
        [1: 200, 2: 202, 3: 201, 4: 203, 5: 204][sdlButton]
    }
    static func mouseKeyNumber(appKitButton: Int) -> Int? {
        // AppKit numbers left=0, right=1, middle=2; never infer MOUSE6+.
        (0...4).contains(appKitButton) ? 200 + appKitButton : nil
    }
    static func wheelKeyNumber(delta: Double) -> Int? {
        guard delta.isFinite, delta != 0 else { return nil }
        return delta > 0 ? 206 : 205
    }
    private static let specialKeys: [Int: Int] = {
        var values = [8: 127, 9: 9, 13: 13, 27: 27, 32: 32, 96: 96, 127: 162]
        let scancodes = [158: 13, 82: 154, 81: 155, 80: 156, 79: 157,
                         226: 158, 230: 158, 224: 159, 228: 159,
                         225: 160, 229: 160, 227: 150, 231: 150,
                         57: 151, 72: 153, 73: 161,
                         78: 163, 75: 164, 74: 165, 77: 166,
                         95: 182, 96: 183, 97: 184, 92: 185, 93: 186,
                         94: 187, 89: 188, 90: 189, 91: 190, 88: 191,
                         98: 192, 99: 193, 84: 194, 86: 195, 87: 196,
                         83: 197, 85: 198, 103: 199]
        for (scan, key) in scancodes { values[(1 << 30) | scan] = key }
        for index in 0..<12 { values[(1 << 30) | (58 + index)] = 167 + index }
        for index in 0..<3 { values[(1 << 30) | (104 + index)] = 179 + index }
        return values
    }()
}

enum KeyboardShape: String, CaseIterable, Codable, Sendable { case ansi, iso, jis }
enum InputKeyUnavailable: Equatable, Sendable {
    case noPhysicalKey, hardwareModifier, unsupportedSymbol(String), unsupportedSDLKey(Int32)
    var explanation: String {
        switch self {
        case .noPhysicalKey: return "This physical key is not known to the game's keyboard backend."
        case .hardwareModifier: return "Fn changes other keys in the keyboard hardware and cannot be bound by itself."
        case .unsupportedSymbol(let symbol): return "The original game cannot use \(symbol) as a binding key."
        case .unsupportedSDLKey: return "The game's keyboard backend has no binding for this key."
        }
    }
}
enum InputKeyMapping: Equatable, Sendable {
    case available(EngineKey), console(EngineKey), unavailable(InputKeyUnavailable)
    var engineKey: EngineKey? {
        switch self { case .available(let key), .console(let key): return key; case .unavailable: return nil }
    }
    var isReserved: Bool { if case .console = self { return true }; return false }
    var explanation: String? {
        switch self {
        case .available: return nil
        case .console: return "This key opens the console and is reserved by the game."
        case .unavailable(let reason): return reason.explanation
        }
    }
}

struct InputSourceSnapshot: Equatable, Sendable {
    let id: String
    let name: String
    let shape: KeyboardShape
    let labels: [UInt16: String]
    var shiftedLabels: [UInt16: String] = [:]
    // SDL3's default latin_letters option examines scancodes A–D. Thai
    // keymaps also use QWERTY. An absent layout is explicitly unavailable.
    var usesLatinFallback: Bool {
        let values = [UInt16(0), 11, 8, 2].compactMap { labels[$0]?.utf16.first }
        return values.count == 4 && (!values.contains(where: { $0 <= 255 }) || values.contains(where: { (0x0e00...0x0e7f).contains($0) }))
    }
    var usesFrenchNumbers: Bool {
        Self.digitPositions.allSatisfy { key in
            guard let normal = labels[key]?.utf16.first, let shifted = shiftedLabels[key]?.utf16.first else { return false }
            return !(48...57).contains(normal) && (48...57).contains(shifted)
        }
    }
    func label(virtualKey: UInt16) -> String {
        let functionKeys: [UInt16: Int] = [122:1,120:2,99:3,118:4,96:5,97:6,98:7,100:8,101:9,109:10,103:11,111:12,105:13,107:14,113:15,106:16,64:17,79:18,80:19]
        if let number = functionKeys[virtualKey] { return "F\(number)" }
        if let label = Self.specialLabels[virtualKey] { return label }
        return labels[virtualKey].flatMap { $0.isEmpty ? nil : $0 } ?? Self.specialLabels[virtualKey] ?? "Key \(virtualKey)"
    }
    func mapping(virtualKey: UInt16) -> InputKeyMapping {
        if virtualKey == 63 { return .unavailable(.hardwareModifier) }
        guard var scan = Self.scancodes[virtualKey] else { return .unavailable(.noPhysicalKey) }
        if shape == .iso && (virtualKey == 10 || virtualKey == 50) { scan = scan == 100 ? 53 : 100 }
        var symbol: Int32
        if let ascii = Self.defaultCharacters[scan] {
            if usesLatinFallback || [36,48,49,51,53].contains(virtualKey) { symbol = ascii }
            else {
                let text = usesFrenchNumbers && Self.digitPositions.contains(virtualKey) ? shiftedLabels[virtualKey] : labels[virtualKey]
                guard let first = text?.utf16.first else { return .unavailable(.unsupportedSymbol(label(virtualKey: virtualKey))) }
                symbol = Int32(first)
            }
        } else {
            symbol = scan == 76 ? 127 : Int32((1 << 30) | scan)
        }
        guard let number = EngineInputMapping.keyNumber(sdlKeycode: symbol), let key = EngineKey(number: number) else {
            return .unavailable(symbol < (1 << 30) ? .unsupportedSymbol(label(virtualKey: virtualKey)) : .unsupportedSDLKey(symbol))
        }
        return number == 96 || number == 126 ? .console(key) : .available(key)
    }
    private static let digitPositions: [UInt16] = [18,19,20,21,23,22,26,28,25,29]
    // Factual virtual-key/scancode pairs from SDL 3.4.10's Darwin backend.
    // No OS shortcut is invented: Command is supported; Fn is not an event.
    static let scancodes: [UInt16: Int] = [
        0:4, 1:22, 2:7, 3:9, 4:11, 5:10, 6:29, 7:27, 8:6, 9:25, 10:100, 11:5,
        12:20, 13:26, 14:8, 15:21, 16:28, 17:23, 18:30, 19:31, 20:32, 21:33, 22:35, 23:34,
        24:46, 25:38, 26:36, 27:45, 28:37, 29:39, 30:48, 31:18, 32:24, 33:47, 34:12, 35:19,
        36:40, 37:15, 38:13, 39:52, 40:14, 41:51, 42:49, 43:54, 44:56, 45:17, 46:16, 47:55,
        48:43, 49:44, 50:53, 51:42, 52:88, 53:41, 54:231, 55:227, 56:225, 57:57, 58:226, 59:224,
        60:229, 61:230, 62:228, 64:108, 65:99, 67:85, 69:87, 71:83, 72:128, 73:129, 74:127,
        75:84, 76:88, 78:86, 79:109, 80:110, 81:103, 82:98, 83:89, 84:90, 85:91, 86:92,
        87:93, 88:94, 89:95, 91:96, 92:97, 93:137, 94:135, 95:133, 96:62, 97:63, 98:64,
        99:60, 100:65, 101:66, 102:145, 103:68, 104:144, 105:70, 106:107, 107:71,
        109:67, 110:101, 111:69, 113:72, 114:73, 115:74, 116:75, 117:76, 118:61,
        119:77, 120:59, 121:78, 122:58, 123:80, 124:79, 125:81, 126:82, 127:102
    ]
    static let defaultCharacters: [Int: Int32] = {
        var values: [Int: Int32] = [40:13, 41:27, 42:8, 43:9, 44:32, 45:45, 46:61,
            47:91, 48:93, 49:92, 50:35, 51:59, 52:39, 53:96, 54:44, 55:46, 56:47, 100:0]
        for scan in 4...29 { values[scan] = Int32(scan + 93) }
        for scan in 30...38 { values[scan] = Int32(scan + 19) }; values[39] = 48
        return values
    }()
    private static let specialLabels: [UInt16: String] = [36:"Return",48:"Tab",49:"Space",51:"Delete",52:"Keypad Enter",53:"Escape",
        54:"Right Command",55:"Command",56:"Shift",57:"Caps lock",58:"Option",59:"Control",60:"Right Shift",61:"Right Option",
        62:"Right Control",63:"Fn",76:"Keypad Enter",93:"¥",94:"ろ",95:"Keypad comma",102:"英数",104:"かな",
        105:"F13",107:"F14",113:"F15",114:"Help",115:"Home",116:"Page up",117:"Forward delete",119:"End",121:"Page down",
        123:"←",124:"→",125:"↓",126:"↑"]
}
