import Foundation

enum ConfigDialect: String, Codable, Sendable { case stock, cod2x }

struct ConfigToken: Equatable, Sendable {
    enum Kind: Sendable { case bare, quoted, remainder }
    let range: Range<Int>
    let value: Data
    let kind: Kind
    let closed: Bool
    var text: String { ConfigDocument.text(value) }
}

struct ConfigTokenization: Equatable, Sendable {
    enum Failure: Sendable { case tooManyTokens, tokenBufferOverflow }
    let tokens: [ConfigToken]
    let comments: [Range<Int>]
    let failure: Failure?
}

struct ConfigCommand: Equatable, Sendable {
    enum Kind: Sendable { case setting, bind, unbind, unbindall, exec, vstr, comment, empty, unknown }
    let range: Range<Int>
    let separatorRange: Range<Int>
    let tokens: [ConfigToken]
    let comments: [Range<Int>]
    let tokenizationFailure: ConfigTokenization.Failure?
    let splitAtLimit: Bool
    var kind: Kind {
        guard let name = tokens.first?.text.lowercased() else { return comments.isEmpty ? .empty : .comment }
        switch name {
        case "set", "seta", "sets", "setu": return .setting
        case "bind": return .bind
        case "unbind": return .unbind
        case "unbindall": return .unbindall
        case "exec": return .exec
        case "vstr": return .vstr
        default: return .unknown
        }
    }
}

enum ConfigDiagnostic: Equatable, Sendable {
    case ignoredNULSuffix(Range<Int>)
    case commandBufferOverflow
    case lineLimit(command: Range<Int>, discarded: Range<Int>)
    case tokenizationFailure(command: Range<Int>, failure: ConfigTokenization.Failure)
    case unterminatedQuote(Range<Int>)
}

enum ConfigEditError: Error, LocalizedError, Sendable {
    case invalidSettingName, invalidKey, unrepresentableText, unsafeValue, unsafeBinding
    case bindTooLong, commandTooLong, commandBufferOverflow, lossyEdit
    var errorDescription: String? {
        switch self {
        case .invalidSettingName: return "Setting names must contain only ASCII letters, numbers and underscores."
        case .invalidKey: return "This key name is not supported by the game."
        case .unrepresentableText: return "The game uses single-byte text. This text contains a character it cannot preserve."
        case .unsafeValue: return "Setting values cannot contain quotes, line breaks or NUL bytes, or end in a backslash."
        case .unsafeBinding: return "This binding cannot be quoted without changing the commands the game reads."
        case .bindTooLong: return "Bindings can contain at most 1023 bytes."
        case .commandTooLong: return "This command exceeds the game's safe line length of 4094 bytes."
        case .commandBufferOverflow: return "This config exceeds the game's command buffer of 65535 bytes."
        case .lossyEdit: return "This edit would change other commands or discard part of the existing config."
        }
    }
}

/// A byte document, not a Unicode text serializer. All ranges are zero-based byte
/// offsets in serialized(). Parsed commands include empty CRLF commands and the
/// byte consumed by the engine when a command reaches its line limit.
struct ConfigDocument: Sendable {
    private var bytes: [UInt8]
    let dialect: ConfigDialect
    private(set) var commands: [ConfigCommand] = []
    private(set) var diagnostics: [ConfigDiagnostic] = []
    private(set) var ignoredSuffixRange: Range<Int>?
    private var activeEnd: Int { ignoredSuffixRange?.lowerBound ?? bytes.count }

    init(data: Data, dialect: ConfigDialect) {
        self.bytes = Array(data)
        self.dialect = dialect
        parse()
    }
    func serialized() -> Data { Data(bytes) }

    /// These commands require engine state or another file. A static lookup below
    /// describes only direct assignments in this document; it is not the result
    /// of executing exec/vstr chains, server policy or dvar type coercion.
    var unresolvedCommands: [ConfigCommand] {
        commands.filter { command in
            ["exec", "vstr", "toggle", "togglep", "reset", "setfromdvar"].contains(command.tokens.first?.text.lowercased() ?? "")
        }
    }

    func effectiveSetting(named name: String) -> String? { effectiveSettingBytes(named: name).map(Self.text) }
    func effectiveSettingBytes(named name: String) -> Data? {
        guard Self.validName(name) else { return nil }
        var value: Data?
        for command in commands {
            if let first = assignmentStart(command, name: name) { value = Self.combine(command.tokens, from: first) }
            else if ["reset", "toggle", "togglep", "setfromdvar"].contains(command.tokens.first?.text.lowercased() ?? ""),
                    command.tokens.count > 1, Self.equal(command.tokens[1].text, name) { value = nil }
        }
        return value
    }
    func effectiveBind(key: String) -> String? { effectiveBindBytes(key: key).map(Self.text) }
    func effectiveBindBytes(key: String) -> Data? {
        guard let keynum = Self.keyNumber(key, folding: true) else { return nil }
        return bindingState(keynum).value
    }

    mutating func setSetting(named name: String, value: String) throws {
        try setSetting(named: name, value: Self.encode(value))
    }
    mutating func setSetting(named name: String, value: Data) throws {
        guard Self.validName(name) else { throw ConfigEditError.invalidSettingName }
        guard !value.contains(where: { [0, 10, 13, 34].contains($0) }), value.last != 92 else { throw ConfigEditError.unsafeValue }
        let spelling = Data([34]) + value + Data([34])
        // Choose the last direct assignment. If a later dynamic command changes
        // this dvar, append an explicit assignment after it instead.
        let match = commands.indices.last { assignmentStart(commands[$0], name: name) != nil }
        let barrier = commands.indices.last { index in
            let tokens = commands[index].tokens
            return tokens.count > 1 && ["reset", "toggle", "togglep", "setfromdvar"].contains(tokens[0].text.lowercased()) && Self.equal(tokens[1].text, name)
        }
        if let index = match, barrier == nil || index > barrier! {
            let first = assignmentStart(commands[index], name: name)!
            try replaceValue(index: index, first: first, spelling: spelling, expectedValue: value)
        } else {
            let line = Self.ascii("seta \(name) ") + spelling
            try insert(line: line, setting: name, keynum: nil, expectedValues: [Self.ascii("seta"), Self.ascii(name), value])
        }
    }
    mutating func setBind(key: String, binding: String) throws { try setBind(key: key, binding: Self.encode(binding)) }
    mutating func setBind(key: String, binding: Data) throws {
        guard let keynum = Self.keyNumber(key, folding: true) else { throw ConfigEditError.invalidKey }
        guard binding.count <= 1023 else { throw ConfigEditError.bindTooLong }
        guard !binding.contains(where: { [0, 10, 13].contains($0) }), binding.last != 92 else { throw ConfigEditError.unsafeBinding }
        var spelling = Data([34])
        for byte in binding { if byte == 34 { spelling.append(92) }; spelling.append(byte) }
        spelling.append(34)
        if let index = bindingState(keynum).index {
            try replaceValue(index: index, first: 2, spelling: spelling, expectedValue: binding)
        } else {
            let name = Self.keySpelling(keynum)
            try insert(line: Self.ascii("bind \(name) ") + spelling, setting: nil, keynum: keynum, expectedValues: [Self.ascii("bind"), Self.ascii(name), binding])
        }
    }

    /// Remove every direct assignment, so an earlier duplicate cannot reappear.
    /// Script separators survive; canonical entries lose their whole physical line.
    /// Runtime-dependent commands are retained and remain in unresolvedCommands.
    mutating func removeSetting(named name: String) throws {
        guard Self.validName(name) else { throw ConfigEditError.invalidSettingName }
        try remove(indices: commands.indices.filter { assignmentStart(commands[$0], name: name) != nil })
    }
    mutating func removeBind(key: String) throws {
        guard let keynum = Self.keyNumber(key, folding: true) else { throw ConfigEditError.invalidKey }
        try remove(indices: commands.indices.filter { index in
            let tokens = commands[index].tokens
            return tokens.count >= 3 && tokens[0].text.lowercased() == "bind" && Self.keyNumber(tokens[1].text, folding: true) == keynum
        })
    }

    private mutating func parse() {
        let end = bytes.firstIndex(of: 0) ?? bytes.count
        ignoredSuffixRange = end < bytes.count ? end..<bytes.count : nil
        if let suffix = ignoredSuffixRange { diagnostics.append(.ignoredNULSuffix(suffix)) }
        if end > 65535 { diagnostics.append(.commandBufferOverflow) }
        var start = 0
        while start < end {
            var scan = start, quotes = 0
            while scan < end {
                if bytes[scan] == 34 { quotes += 1 }
                if bytes[scan] == 10 || bytes[scan] == 13 || (bytes[scan] == 59 && quotes % 2 == 0) { break }
                scan += 1
            }
            let stop = min(scan, start + 4095)
            let after = stop < end ? stop + 1 : stop
            let range = start..<stop, separator = stop..<after
            let result = Self.tokenize(data: Data(bytes[range]), dialect: dialect, byteOffset: start)
            let limited = scan - start >= 4095
            commands.append(ConfigCommand(range: range, separatorRange: separator, tokens: result.tokens, comments: result.comments, tokenizationFailure: result.failure, splitAtLimit: limited))
            if limited { diagnostics.append(.lineLimit(command: range, discarded: separator)) }
            if let failure = result.failure { diagnostics.append(.tokenizationFailure(command: range, failure: failure)) }
            for token in result.tokens where token.kind == .quoted && !token.closed { diagnostics.append(.unterminatedQuote(token.range)) }
            start = after
        }
    }

    /// Also models direct Cmd_TokenizeString2 calls: tokenLimit's final argument
    /// is the verbatim remainder before whitespace/comment/quote processing.
    /// byteOffset makes token/comment ranges refer to a containing document.
    static func tokenize(data: Data, dialect: ConfigDialect, tokenLimit: Int = 0, byteOffset: Int = 0) -> ConfigTokenization {
        let bytes = Array(data), end = bytes.firstIndex(of: 0) ?? data.count
        var p = 0, tokens: [ConfigToken] = [], comments: [Range<Int>] = [], used = 0
        func whitespace(_ byte: UInt8) -> Bool { byte <= 32 || byte >= 128 }
        func lineComment(_ p: Int) -> Bool {
            p + 1 < end && bytes[p] == 47 && bytes[p + 1] == 47 && (dialect == .stock || p == 0 || bytes[p - 1] != 58)
        }
        func blockComment(_ p: Int) -> Bool { p + 1 < end && bytes[p] == 47 && bytes[p + 1] == 42 }
        func result(_ failure: ConfigTokenization.Failure? = nil) -> ConfigTokenization {
            ConfigTokenization(tokens: failure == nil ? tokens : [], comments: comments, failure: failure)
        }
        while true {
            if tokens.count == 512 { return result(.tooManyTokens) }
            if tokenLimit > 0 && tokens.count == tokenLimit - 1 {
                if p == end { return result() }
                let remainder = Data(bytes[p..<end])
                if used + remainder.count + 1 > 8704 { return result(.tokenBufferOverflow) }
                tokens.append(ConfigToken(range: (p + byteOffset)..<(end + byteOffset), value: remainder, kind: .remainder, closed: true))
                return result()
            }
            while p < end && whitespace(bytes[p]) { p += 1 }
            if p == end { return result() }
            if lineComment(p) { comments.append((p + byteOffset)..<(end + byteOffset)); return result() }
            if blockComment(p) {
                let start = p; p += 1 // /*/ overlaps the opener and closer.
                while p < end && !(bytes[p] == 42 && p + 1 < end && bytes[p + 1] == 47) { p += 1 }
                if p < end { p += 2 }
                comments.append((start + byteOffset)..<(p + byteOffset))
                continue
            }
            let start = p, quoted = bytes[p] == 34
            var value = Data(), closed = true
            if quoted {
                p += 1
                while p < end && bytes[p] != 34 {
                    if bytes[p] == 92 && p + 1 < end && bytes[p + 1] == 34 { p += 1 }
                    value.append(bytes[p]); p += 1
                }
                closed = p < end
                if closed { p += 1 }
            } else {
                while p < end && !whitespace(bytes[p]) && bytes[p] != 34 && !lineComment(p) && !blockComment(p) {
                    value.append(bytes[p]); p += 1
                }
            }
            used += value.count + 1
            if used > 8704 { return result(.tokenBufferOverflow) }
            tokens.append(ConfigToken(range: (start + byteOffset)..<(p + byteOffset), value: value, kind: quoted ? .quoted : .bare, closed: closed))
            if p == end { return result() } // Exactly 512 tokens at immediate EOF is accepted.
            if whitespace(bytes[p]) { p += 1 }
        }
    }

    private func assignmentStart(_ command: ConfigCommand, name: String) -> Int? {
        let tokens = command.tokens
        if tokens.count >= 3 && command.kind == .setting && Self.equal(tokens[1].text, name) { return 2 }
        if tokens.count >= 2 && Self.equal(tokens[0].text, name) { return 1 }
        return nil
    }
    private func bindingState(_ keynum: Int) -> (value: Data?, index: Int?) {
        var value: Data?, index: Int?
        for (i, command) in commands.enumerated() {
            let tokens = command.tokens
            if command.kind == .unbindall { value = nil; index = nil }
            if tokens.count == 2 && command.kind == .unbind && Self.keyNumber(tokens[1].text, folding: false) == keynum { value = nil; index = nil }
            if tokens.count >= 3 && command.kind == .bind && Self.keyNumber(tokens[1].text, folding: true) == keynum {
                value = Self.binding(tokens); index = i
            }
        }
        return (value, index)
    }
    private static func combine(_ tokens: [ConfigToken], from first: Int) -> Data {
        var value = Data(), length = 0
        for index in first..<tokens.count {
            length += tokens[index].value.count
            if length > 4093 { break }
            value.append(tokens[index].value)
            if index + 1 < tokens.count { value.append(32) }
        }
        return Data(value.prefix(4095))
    }
    private static func binding(_ tokens: [ConfigToken]) -> Data {
        if tokens.count == 3 { return Data(tokens[2].value.prefix(1023)) }
        var value = Data()
        for index in 2..<tokens.count {
            let quoted = tokens[index].value.contains(32)
            if quoted { value.append(34) }
            value.append(tokens[index].value)
            if quoted { value.append(34) }
            if index + 1 < tokens.count { value.append(32) }
        }
        return Data(value.prefix(1023))
    }

    private struct Edit { let range: Range<Int>; let replacement: Data }
    private mutating func replaceValue(index: Int, first: Int, spelling: Data, expectedValue: Data) throws {
        let command = commands[index]
        let range = command.tokens[first].range.lowerBound..<command.tokens.last!.range.upperBound
        guard !command.comments.contains(where: { $0.overlaps(range) }), !command.splitAtLimit else { throw ConfigEditError.lossyEdit }
        let prefix = Array(command.tokens.prefix(first)).map(\.value)
        try apply(edits: [Edit(range: range, replacement: spelling)], affected: [index], expected: [prefix + [expectedValue]])
    }
    private mutating func insert(line: Data, setting: String?, keynum: Int?, expectedValues: [Data]) throws {
        guard line.count <= 4094 else { throw ConfigEditError.commandTooLong }
        let tokens = Self.tokenize(data: line, dialect: dialect)
        let split = ConfigDocument(data: line, dialect: dialect)
        guard tokens.failure == nil, tokens.tokens.map(\.value) == expectedValues,
              split.commands.count == 1, split.commands[0].tokens == tokens.tokens else { throw ConfigEditError.lossyEdit }
        var position = activeEnd
        if let canonical = canonicalLines() {
            for row in canonical.dropFirst(3) {
                let command = commands[row.index]
                if let keynum {
                    if command.kind == .setting || Self.keyNumber(command.tokens[1].text, folding: true)! > keynum { position = row.range.lowerBound; break }
                } else if let setting, command.kind == .setting,
                          command.tokens[1].text.lowercased() > setting.lowercased() { position = row.range.lowerBound; break }
            }
        }
        let ending = preferredLineEnding()
        let needsEnding = position == activeEnd && position > 0 && bytes[position - 1] != 10 && bytes[position - 1] != 13
        let replacement = (needsEnding ? ending : Data()) + line + ending
        try apply(edits: [Edit(range: position..<position, replacement: replacement)], affected: [], expected: [expectedValues])
    }
    private mutating func remove(indices: [Int]) throws {
        if indices.isEmpty { return }
        let canonical = canonicalLines()
        let edits = try indices.map { index -> Edit in
            guard !commands[index].splitAtLimit else { throw ConfigEditError.lossyEdit }
            let range = canonical?.first { $0.index == index }?.range ?? commands[index].range
            return Edit(range: range, replacement: Data())
        }
        try apply(edits: edits, affected: Set(indices), expected: [])
    }

    /// Check the actual engine parse after replacing spans. Every unaffected
    /// command must still exist at its shifted range with identical bytes/tokens.
    /// Every other nonempty command must be one of the requested replacements.
    private mutating func apply(edits: [Edit], affected: Set<Int>, expected: [[Data]]) throws {
        let edits = edits.sorted { $0.range.lowerBound < $1.range.lowerBound }
        var newBytes = bytes
        for edit in edits.reversed() { newBytes.replaceSubrange(edit.range, with: edit.replacement) }
        let candidate = ConfigDocument(data: Data(newBytes), dialect: dialect)
        guard candidate.activeEnd <= 65535 else { throw ConfigEditError.commandBufferOverflow }
        var matched = Set<Int>()
        for (index, command) in commands.enumerated() where !affected.contains(index) {
            // An empty CRLF command removed with its canonical line is harmless.
            if edits.contains(where: { $0.range.contains(command.range.lowerBound) }) && command.range.isEmpty { continue }
            let shift = edits.filter { $0.range.upperBound <= command.range.lowerBound }.reduce(0) { $0 + $1.replacement.count - $1.range.count }
            let range = (command.range.lowerBound + shift)..<(command.range.upperBound + shift)
            guard let match = candidate.commands.indices.first(where: { candidate.commands[$0].range == range }),
                  Array(bytes[command.range]) == Array(newBytes[range]),
                  command.tokens.map(\.value) == candidate.commands[match].tokens.map(\.value),
                  command.tokenizationFailure == candidate.commands[match].tokenizationFailure else { throw ConfigEditError.lossyEdit }
            matched.insert(match)
        }
        let changed = candidate.commands.indices.filter { !matched.contains($0) && !candidate.commands[$0].range.isEmpty }
        guard changed.count == expected.count else { throw ConfigEditError.lossyEdit }
        for (index, values) in zip(changed, expected) {
            let command = candidate.commands[index]
            guard command.range.count <= 4094 else { throw ConfigEditError.commandTooLong }
            guard command.tokenizationFailure == nil, command.tokens.map(\.value) == values else { throw ConfigEditError.lossyEdit }
        }
        self = candidate
    }

    private struct CanonicalLine { let range: Range<Int>; let index: Int }
    var isCanonical: Bool { canonicalLines() != nil }
    private func canonicalLines() -> [CanonicalLine]? {
        guard ignoredSuffixRange == nil, diagnostics.isEmpty else { return nil }
        var rows: [CanonicalLine] = [], p = 0
        while p < bytes.count {
            let start = p
            while p < bytes.count && bytes[p] != 10 && bytes[p] != 13 { p += 1 }
            let content = start..<p
            guard let index = commands.indices.first(where: { commands[$0].range == content }) else { return nil }
            if p < bytes.count {
                if bytes[p] == 13 && p + 1 < bytes.count && bytes[p + 1] == 10 { p += 1 }
                p += 1
            }
            rows.append(CanonicalLine(range: start..<p, index: index))
        }
        guard rows.count >= 3,
              Data(bytes[commands[rows[0].index].range]) == Self.ascii("// generated by Call of Duty, do not modify"),
              rows[1...2].allSatisfy({ Data(bytes[commands[$0.index].range]) == Self.ascii("unbindall") }) else { return nil }
        var lastKey = -1, lastName: String?, seenSettings = false
        for row in rows.dropFirst(3) {
            let command = commands[row.index], tokens = command.tokens
            guard tokens.count == 3, tokens[0].kind == .bare, tokens[1].kind == .bare,
                  tokens[2].kind == .quoted, tokens[2].closed, command.comments.isEmpty else { return nil }
            if tokens[0].text == "bind", !seenSettings, let key = Self.keyNumber(tokens[1].text, folding: true), key > lastKey {
                guard Self.binding(tokens).count == tokens[2].value.count else { return nil }
                lastKey = key
            } else if tokens[0].text == "seta", Self.validName(tokens[1].text),
                      lastName == nil || tokens[1].text.lowercased() > lastName! {
                guard !tokens[2].value.contains(34) else { return nil }
                seenSettings = true; lastName = tokens[1].text.lowercased()
            } else { return nil }
            let exact = Self.ascii("\(tokens[0].text) \(tokens[1].text) ") + Data(bytes[tokens[2].range])
            guard Data(bytes[command.range]) == exact else { return nil }
        }
        return rows
    }
    private func preferredLineEnding() -> Data {
        for index in 0..<activeEnd {
            if bytes[index] == 10 { return Data([10]) }
            if bytes[index] == 13 { return index + 1 < activeEnd && bytes[index + 1] == 10 ? Data([13, 10]) : Data([13]) }
        }
        return Data([10])
    }

    static func text(_ data: Data) -> String { String(String.UnicodeScalarView(data.map { UnicodeScalar(UInt32($0))! })) }
    static func encode(_ string: String) throws -> Data {
        var result = Data()
        for scalar in string.unicodeScalars {
            guard scalar.value <= 255 else { throw ConfigEditError.unrepresentableText }
            result.append(UInt8(scalar.value))
        }
        return result
    }
    private static func ascii(_ string: String) -> Data { Data(string.utf8) }
    private static func equal(_ left: String, _ right: String) -> Bool { left.lowercased() == right.lowercased() }
    private static func validName(_ name: String) -> Bool {
        !name.isEmpty && name.utf8.allSatisfy { (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0) || $0 == 95 }
    }

    // Factual key table from the engine's Key_StringToKeynum/keynames. The
    // decimal-looking names are named extended keys, not arbitrary numbers.
    private static let keyNames: [String: Int] = {
        var names = ["tab": 9, "enter": 13, "escape": 27, "space": 32, "backspace": 127,
                     "uparrow": 154, "downarrow": 155, "leftarrow": 156, "rightarrow": 157,
                     "alt": 158, "ctrl": 159, "shift": 160, "capslock": 151,
                     "ins": 161, "del": 162, "pgdn": 163, "pgup": 164, "home": 165, "end": 166,
                     "mwheelup": 206, "mwheeldown": 205, "pause": 153, "semicolon": 59, "command": 150]
        for index in 1...15 { names["f\(index)"] = 166 + index }
        for index in 1...5 { names["mouse\(index)"] = 199 + index }
        for index in 1...16 { names["aux\(index)"] = 206 + index }
        let keypad = ["home", "uparrow", "pgup", "leftarrow", "5", "rightarrow", "end", "downarrow", "pgdn", "enter", "ins", "del", "slash", "minus", "plus", "numlock", "star", "equals"]
        for (index, name) in keypad.enumerated() { names["kp_" + name] = 182 + index }
        let extended = [181, 191, 223, 224, 225, 228, 229, 230, 231, 232, 233, 236, 241, 242, 243, 246, 248, 249, 250, 252]
        for (index, name) in extended.enumerated() { names[String(name)] = 128 + index }
        return names
    }()
    static func keyNumber(_ name: String, folding: Bool = true) -> Int? {
        guard let data = try? encode(name) else { return nil }
        let bytes = Array(data)
        var value: Int?
        if bytes.count == 1 { value = bytes[0] < 128 ? Int(bytes[0]) : nil }
        else if bytes.count == 4 && bytes[0] == 48 && bytes[1] == 120 {
            func hex(_ byte: UInt8) -> Int { (48...57).contains(byte) ? Int(byte - 48) : (97...102).contains(byte) ? Int(byte - 87) : 0 }
            value = hex(bytes[2]) * 16 + hex(bytes[3])
        } else { value = keyNames[name.lowercased()] }
        if let key = value, folding && (65...90).contains(key) { return key + 32 }
        return value
    }
    static func keySpelling(_ keynum: Int) -> String {
        if keynum > 32 && keynum < 127 && keynum != 34 && keynum != 59 { return text(Data([UInt8(keynum)])) }
        if let name = keyNames.first(where: { $0.value == keynum })?.key { return name }
        return String(format: "0x%02x", keynum)
    }
}
