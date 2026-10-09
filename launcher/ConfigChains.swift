import Foundation
import CryptoKit

enum ConfigChainBehavior: Codable, Equatable, Sendable {
    case commands([String])
    case settingCycle(name: String, values: [String])
    case commandCycle(steps: [[String]])
}

struct ConfigChain: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let key: String
    let behavior: ConfigChainBehavior
}

enum ConfigChainIssue: Equatable, Sendable {
    case usesWait, waitDisabled, nonYieldingRecursion, yieldingLoop, executionLimit, bufferLimit
    case unresolvedVariable(String), unresolvedFile(String), invalidTokens, negativeWait
    case releaseTail, heldActionWithoutRelease, symbolicSetting(String)
    case nativeSetTruncation(String)
    var blocksCompilation: Bool {
        switch self {
        case .nonYieldingRecursion, .yieldingLoop, .executionLimit, .bufferLimit, .invalidTokens, .negativeWait: return true
        default: return false
        }
    }
}

struct ConfigChainTrace: Sendable {
    /// Leaf command argv after the real splitter/tokenizer rules. Expansion
    /// commands are replaced by their effects. Setting values are symbolic:
    /// registration types, server policy and gameplay are not executed here.
    var commands: [[Data]] = []
    var issues: [ConfigChainIssue] = []
    var complete = true
    mutating func flag(_ issue: ConfigChainIssue) {
        if !issues.contains(issue) { issues.append(issue) }
        if issue.blocksCompilation { complete = false }
    }
}

/// A bounded, stateful command-buffer preview. Stored set text creates no
/// execution edge until vstr runs it; pointer assignments between presses are
/// therefore cycles, while repeated immediate expansion is recursion.
struct ConfigChainMachine: Sendable {
    let dialect: ConfigDialect
    var files: [String: Data]
    var variables: [String: Data] = [:]
    var waitDisabled: Bool
    private(set) var referencedVariables: Set<String> = []
    private(set) var archivedVariables: Set<String> = []
    private(set) var unsafeArchivedVariables: Set<String> = []
    init(dialect: ConfigDialect, files: [String: Data] = [:], waitDisabled: Bool = false) {
        self.dialect = dialect; self.files = files; self.waitDisabled = waitDisabled
    }
    private struct Frame {
        var pending: Data
        let label: String?
        let state: [Data]
        let epoch: Int
    }
    private var state: [Data] { variables.keys.sorted().flatMap { [Data($0.utf8), variables[$0]!] } }

    mutating func keyEvent(binding: Data, key: Int, down: Bool, time: UInt32) -> ConfigChainTrace {
        if binding.isEmpty || (!down && binding.first != 43) { return ConfigChainTrace() }
        var text = binding
        if text.first == 43 {
            if !down { text[0] = 45 }
            // CL_KeyEvent formats unsigned time through %i in a 1024-byte array.
            text.append(Data(" \(key) \(Int32(bitPattern: time))\n".utf8))
            text = Data(text.prefix(1023))
        } else { text.append(10) }
        var trace = execute(text)
        if binding.first == 43 && binding.contains(59) { trace.flag(.releaseTail) }
        if binding.first == 43 && trace.commands.dropFirst().contains(where: { $0.first?.first == 43 }) { trace.flag(.heldActionWithoutRelease) }
        if binding.first != 43 && trace.commands.contains(where: { $0.first?.first == 43 }) { trace.flag(.heldActionWithoutRelease) }
        return trace
    }

    mutating func execute(_ text: Data) -> ConfigChainTrace {
        var trace = ConfigChainTrace(), epoch = 0, budget = 8192
        if text.contains(0) { trace.flag(.invalidTokens); return trace }
        var frames = [Frame(pending: text, label: nil, state: state, epoch: epoch)]
        if text.count > 65535 { trace.flag(.bufferLimit); return trace }
        while !frames.isEmpty {
            if frames.last!.pending.isEmpty { frames.removeLast(); continue }
            guard budget > 0, frames.count <= 64 else { trace.flag(.executionLimit); break }
            budget -= 1
            let parsed = ConfigDocument(data: frames.last!.pending, dialect: dialect)
            guard let command = parsed.commands.first else { frames.removeLast(); continue }
            let consumed = command.separatorRange.upperBound
            frames[frames.count - 1].pending.removeFirst(consumed)
            if command.tokenizationFailure != nil || command.splitAtLimit { trace.flag(.invalidTokens); break }
            let argv = command.tokens.map(\.value)
            guard let first = argv.first else { continue }
            let name = ConfigDocument.text(first).lowercased()
            var expansion: (String, Data)?
            if name == "vstr", argv.count == 2 {
                let key = ConfigDocument.text(argv[1]).lowercased()
                referencedVariables.insert(key)
                if let value = variables[key] { expansion = ("vstr:" + key, value + Data([10, 10])) }
                else { trace.flag(.unresolvedVariable(key)); trace.complete = false }
            } else if name == "exec", argv.count == 2 {
                if let path = ConfigImportScript.execPath(argv[1]), let value = files[path] {
                    expansion = ("exec:" + path, value + Data([10]))
                } else { trace.flag(.unresolvedFile(ConfigDocument.text(argv[1]))); trace.complete = false }
            } else {
                trace.commands.append(argv)
                if ["set", "seta", "sets", "setu"].contains(name), argv.count >= 3 {
                    let value = ConfigDocument(data: parsed.serialized().subdata(in: command.range), dialect: dialect)
                        .effectiveSettingBytes(named: ConfigDocument.text(argv[1]))
                    let key = ConfigDocument.text(argv[1]).lowercased()
                    if let value, value.count > 1022 { trace.flag(.nativeSetTruncation(key)) }
                    variables[key] = value.map { Data($0.prefix(1022)) }
                    if name == "seta" { archivedVariables.insert(key) }
                } else if ["toggle", "togglep"].contains(name), argv.count >= 3 {
                    let key = ConfigDocument.text(argv[1]).lowercased(), values = Array(argv.dropFirst(2))
                    let current = variables[key].map(ConfigDocument.text)
                    let match = values.dropLast().firstIndex { ConfigDocument.text($0).caseInsensitiveCompare(current ?? "") == .orderedSame }
                    variables[key] = match.map { values[$0 + 1] } ?? values[0]
                    trace.flag(.symbolicSetting(key))
                } else if name == "setfromdvar", argv.count == 3 {
                    let key = ConfigDocument.text(argv[2]).lowercased()
                    referencedVariables.insert(key)
                    if let value = variables[key] { variables[ConfigDocument.text(argv[1]).lowercased()] = value }
                    else { trace.flag(.unresolvedVariable(key)); trace.complete = false }
                } else if name == "wait" {
                    trace.flag(.usesWait)
                    if waitDisabled && dialect == .cod2x { trace.flag(.waitDisabled) }
                    else {
                        let count = argv.count == 2 ? Self.atoi(ConfigDocument.text(argv[1])) : 1
                        if count < 0 { trace.flag(.negativeWait); break }
                        if count > 0 { epoch += 1; recordArchivedValues() }
                    }
                } else if variables[name] != nil, argv.count > 1 {
                    let value = argv.dropFirst().reduce(Data()) { $0 + ($0.isEmpty ? Data() : Data([32])) + $1 }
                    if value.count > 1022 { trace.flag(.nativeSetTruncation(name)) }
                    variables[name] = Data(value.prefix(1022))
                }
            }
            if let (label, value) = expansion {
                let snapshot = state
                if let previous = frames.last(where: { $0.label == label && $0.state == snapshot }) {
                    trace.flag(previous.epoch == epoch ? .nonYieldingRecursion : .yieldingLoop); break
                }
                // Cbuf_InsertText permits exactly maxsize; vstr adds two LF.
                guard frames.reduce(value.count, { $0 + $1.pending.count }) <= 65536 else { trace.flag(.bufferLimit); break }
                frames.append(Frame(pending: value, label: label, state: snapshot, epoch: epoch))
            }
        }
        recordArchivedValues()
        return trace
    }
    private mutating func recordArchivedValues() {
        // Automatic config writes can run while wait yields or between key
        // events. Retain an unsafe archived value observed at either boundary,
        // even when a later command restores a quote-free value.
        for key in archivedVariables {
            if let value = variables[key], value.contains(34) || value.last == 92 { unsafeArchivedVariables.insert(key) }
        }
    }
    private static func atoi(_ text: String) -> Int {
        let bytes = Array(text.utf8.drop(while: { $0 <= 32 })), negative = bytes.first == 45
        let digits = bytes.dropFirst(bytes.first == 45 || bytes.first == 43 ? 1 : 0).prefix(while: { (48...57).contains($0) })
        let number = digits.reduce(0) { min(1_000_000, $0 * 10 + Int($1 - 48)) }
        return negative ? -number : number
    }
}

enum ConfigChainError: Error, LocalizedError, Sendable {
    case invalidIdentity, duplicateKey, invalidCommands, unsafeQuotes, heldActionInCycle
    case helperTooLong, unsafeExecution, opaqueOwnedFile, conflictNeedsReview, locked(String), unsafeExport
    case portableDependency
    var errorDescription: String? {
        switch self {
        case .invalidIdentity: return "Use a unique chain identifier with ASCII letters, numbers and underscores."
        case .duplicateKey: return "Two chains use the same game key. Choose a different key."
        case .invalidCommands: return "Each step needs a complete command within the game's line and token limits."
        case .unsafeQuotes: return "The game's quote rules change this script. Remove embedded quotes or use a direct multi-command bind. Windows export cannot preserve quotes inside archived helper values."
        case .heldActionInCycle: return "A vstr cycle cannot release a held + action. Use a direct bind starting with that action."
        case .helperTooLong: return "This step exceeds the current native engine's 1022-byte set value limit. Split it into shorter steps."
        case .unsafeExecution: return "This chain repeats without another keypress, or exceeds the preview's execution limits. Remove the recursive exec/vstr call."
        case .opaqueOwnedFile: return "This chain file or its saved definitions changed outside Config Studio. Preserve it and review the text before replacing it."
        case .conflictNeedsReview: return "This key already has a different binding. Review and confirm replacing it."
        case .locked(let name): return "Unlock \(name) before changing this chain."
        case .unsafeExport: return "Choose an export file outside the active game home and its linked folders."
        case .portableDependency: return "This loadout requires an exec file, an unresolved variable, or an unarchived script helper. Include and review those commands in the loadout before exporting for Windows."
        }
    }
}

struct ConfigChainEdit: Sendable {
    let profileID: String
    let before: ConfigProfileVersion
    let document: ConfigDocument
    let metadata: ConfigStudioMetadata
    let script: Data
    let runtimeVariables: [String]
    let issues: [ConfigChainIssue]
    let conflictingKeys: [String]
    let chains: [ConfigChain]
    var runtimeVariableCount: Int { runtimeVariables.count }
    var runtimeVariablePoolCapacity: Int { document.dialect == .cod2x ? 4096 : 1280 }
    var hasChanges: Bool { before.config != document.serialized() || before.chain != (script.isEmpty ? nil : script) || before.metadata != (try? ConfigChains.encode(metadata)) }
}

struct ConfigChainInspection: Sendable {
    enum Kind: Sendable { case empty, direct, multiCommand, settingCycle, opaque }
    let kind: Kind
    let binding: Data?
    let press: ConfigChainTrace
    let release: ConfigChainTrace
}

enum ConfigChains {
    static func inspect(key: String, document: ConfigDocument, files: [String: Data] = [:], waitDisabled: Bool = false) -> ConfigChainInspection {
        let binding = document.effectiveBindBytes(key: key)
        var machine = ConfigChainMachine(dialect: document.dialect, files: files, waitDisabled: waitDisabled)
        _ = machine.execute(document.serialized())
        let press = machine.keyEvent(binding: binding ?? Data(), key: ConfigDocument.keyNumber(key) ?? 0, down: true, time: 100)
        let release = machine.keyEvent(binding: binding ?? Data(), key: ConfigDocument.keyNumber(key) ?? 0, down: false, time: 200)
        let parsed = ConfigDocument(data: binding ?? Data(), dialect: document.dialect)
        let live = parsed.commands.filter { !$0.tokens.isEmpty }
        let kind: ConfigChainInspection.Kind
        if binding == nil || binding!.isEmpty { kind = .empty }
        else if !parsed.diagnostics.isEmpty || parsed.commands.contains(where: { !$0.comments.isEmpty }) || live.contains(where: { [.vstr, .exec, .setting, .bind, .unbind, .unbindall].contains($0.kind) }) { kind = .opaque }
        else if live.count == 1, live[0].tokens.first?.text.lowercased() == "toggle", live[0].tokens.count >= 4 { kind = .settingCycle }
        else { kind = live.count > 1 ? .multiCommand : .direct }
        return ConfigChainInspection(kind: kind, binding: binding, press: press, release: release)
    }

    static func ownedChains(read: ConfigProfileRead) throws -> [ConfigChain] {
        guard let definitions = read.metadata.chainDefinitions else {
            guard read.version.chain == nil else { throw ConfigChainError.opaqueOwnedFile }
            return []
        }
        let chains: [ConfigChain]
        do { chains = try JSONDecoder().decode([ConfigChain].self, from: definitions) }
        catch { throw ConfigChainError.opaqueOwnedFile }
        let compiled = try compile(chains: chains, profileID: read.profile.id, dialect: read.document.dialect)
        guard read.version.chain == (compiled.script.isEmpty ? nil : compiled.script) else { throw ConfigChainError.opaqueOwnedFile }
        return chains
    }

    static func prepare(chains: [ConfigChain], read: ConfigProfileRead) throws -> ConfigChainEdit {
        let previous = try ownedChains(read: read)
        let old = try compile(chains: previous, profileID: read.profile.id, dialect: read.document.dialect)
        let new = try compile(chains: chains, profileID: read.profile.id, dialect: read.document.dialect)
        var document = read.document, metadata = read.metadata, conflicts: [String] = []
        // Remove only a managed trigger that still matches our original value.
        // A game/external rebind remains intact until explicitly replaced.
        for (key, binding) in old.bindings where new.bindings[key] == nil && document.effectiveBindBytes(key: key) == binding {
            try checkLocks(key: key, old: binding, new: nil, read: read)
            try checkExistingLocks(key: key, binding: binding, read: read)
            try document.removeBind(key: key)
        }
        for key in new.bindings.keys.sorted() {
            let binding = new.bindings[key]!, existing = document.effectiveBindBytes(key: key)
            if existing != binding {
                try checkLocks(key: key, old: existing, new: binding, read: read)
                if let existing { try checkExistingLocks(key: key, binding: existing, read: read) }
                if let existing, !existing.isEmpty, existing != old.bindings[key] { conflicts.append(key) }
                try document.setBind(key: key, binding: binding)
            }
        }
        // Helper content changes count as a bind edit even if vstr's trigger
        // text stays identical. Inspect the old and new steps for action locks.
        if previous != chains {
            for chain in previous + chains {
                try checkLocks(key: chain.key, old: old.bindings[canonicalKey(chain.key)], new: new.bindings[canonicalKey(chain.key)], read: read)
                for text in commandTexts(chain.behavior) {
                    try checkLocks(key: chain.key, old: nil, new: try ConfigDocument.encode(text), read: read)
                }
            }
        }
        metadata.chainDefinitions = chains.isEmpty ? nil : try encode(chains)
        // Do not collide with imported or registered names in the document.
        for name in new.variables {
            if read.document.commands.contains(where: { $0.tokens.count > 1 && $0.kind == .setting && $0.tokens[1].text.lowercased() == name }) { throw ConfigChainError.opaqueOwnedFile }
        }
        var issues = new.issues
        for disabled in read.document.dialect == .cod2x ? [false, true] : [false] {
            var machine = ConfigChainMachine(dialect: read.document.dialect, waitDisabled: disabled)
            _ = machine.execute(document.serialized())
            _ = machine.execute(new.script)
            for (key, binding) in new.bindings.sorted(by: { $0.key < $1.key }) {
                for _ in 0..<128 {
                    for down in [true, false] {
                        let trace = machine.keyEvent(binding: binding, key: ConfigDocument.keyNumber(key)!, down: down, time: down ? 100 : 200)
                        if trace.issues.contains(where: \.blocksCompilation) { throw ConfigChainError.unsafeExecution }
                        if previous != chains { try checkTraceLocks(key: key, commands: trace.commands, read: read) }
                        for issue in trace.issues where !issues.contains(issue) { issues.append(issue) }
                    }
                }
            }
        }
        return ConfigChainEdit(profileID: read.profile.id, before: read.version, document: document, metadata: metadata,
                               script: new.script, runtimeVariables: new.variables, issues: issues, conflictingKeys: conflicts, chains: chains)
    }

    @discardableResult static func apply(_ edit: ConfigChainEdit, to store: ConfigProfileStore, replaceConflicts: Bool = false, runningGame: Bool = false) throws -> ConfigHistoryEntry {
        let current = try store.read(id: edit.profileID)
        guard current.version == edit.before, current.document.dialect == edit.document.dialect else { throw ConfigProfileError.conflict }
        let verified = try prepare(chains: edit.chains, read: current)
        guard verified.conflictingKeys.isEmpty || replaceConflicts else { throw ConfigChainError.conflictNeedsReview }
        return try store.updateChain(id: edit.profileID, document: verified.document, expected: edit.before, metadata: verified.metadata,
                                     chain: verified.script.isEmpty ? nil : verified.script, summary: "Changed advanced binds", runningGame: runningGame)
    }

    @MainActor static func apply(_ edit: ConfigChainEdit, to session: ConfigStudioSession, replaceConflicts: Bool = false) throws {
        guard !session.isReadOnly else { throw ConfigProfileError.gameRunning }
        guard !session.hasDraft, let read = session.current, read.profile.id == edit.profileID, read.version == edit.before else { throw ConfigProfileError.conflict }
        let verified = try prepare(chains: edit.chains, read: read)
        guard verified.conflictingKeys.isEmpty || replaceConflicts else { throw ConfigChainError.conflictNeedsReview }
        try session.updateChain(profileID: verified.profileID, document: verified.document, metadata: verified.metadata,
                                chain: verified.script.isEmpty ? nil : verified.script, expected: verified.before, summary: "Changed advanced binds")
    }

    private struct Compiled {
        var script = Data()
        var bindings: [String: Data] = [:]
        var variables: [String] = []
        var issues: [ConfigChainIssue] = []
    }
    private static func compile(chains: [ConfigChain], profileID: String, dialect: ConfigDialect, portable: Bool = false) throws -> Compiled {
        var result = Compiled(), ids = Set<String>(), keys = Set<Int>()
        _ = try ConfigStudioLaunch.chainBasename(profileID: profileID)
        for chain in chains {
            guard validName(chain.id), chain.id.utf8.count <= 40, ids.insert(chain.id.lowercased()).inserted else { throw ConfigChainError.invalidIdentity }
            guard let key = ConfigDocument.keyNumber(chain.key), ![96, 126].contains(key) else { throw ConfigEditError.invalidKey }
            guard keys.insert(key).inserted else { throw ConfigChainError.duplicateKey }
            // IDs stay stable across profile copies. Only the selected profile's
            // companion is loaded; copying needs no bind/helper renaming.
            let prefix = "cs_b2_" + SHA256.hash(data: Data(chain.id.lowercased().utf8)).prefix(20).map { String(format: "%02x", $0) }.joined()
            let trigger: Data
            switch chain.behavior {
            case .commands(let commands): trigger = try joined(commands, dialect: dialect)
            case .settingCycle(let name, let values):
                guard validName(name), values.count >= 2 else { throw ConfigChainError.invalidCommands }
                let tokens = try values.map { value -> Data in
                    let bytes = try ConfigDocument.encode(value)
                    guard !bytes.contains(where: { [0, 10, 13, 34].contains($0) }), bytes.last != 92 else { throw ConfigChainError.unsafeQuotes }
                    // Keep ordinary numeric values readable; quote other tokens.
                    if !bytes.isEmpty && bytes.allSatisfy({ (48...57).contains($0) || [43, 45, 46].contains($0) }) { return bytes }
                    return Data([34]) + bytes + Data([34])
                }
                trigger = Data("toggle \(name) ".utf8) + tokens.enumerated().reduce(Data()) { $0 + ($1.offset == 0 ? Data() : Data([32])) + $1.element }
                try validate(trigger, dialect: dialect)
            case .commandCycle(let steps):
                guard steps.count >= 2, steps.count <= 128 else { throw ConfigChainError.invalidCommands }
                let pointer = prefix + "_next", names = steps.indices.map { prefix + "_s\($0)" }
                for (index, step) in steps.enumerated() {
                    let body = try joined(step, dialect: dialect)
                    if ConfigDocument(data: body, dialect: dialect).commands.contains(where: { $0.tokens.first?.value.first == 43 }) { throw ConfigChainError.heldActionInCycle }
                    let value = body + Data("; \(portable ? "seta" : "set") \(pointer) vstr \(names[(index + 1) % names.count])".utf8)
                    try appendAssignment(name: names[index], value: value, portable: portable, dialect: dialect, to: &result.script)
                }
                try appendAssignment(name: pointer, value: Data("vstr \(names[0])".utf8), portable: portable, dialect: dialect, to: &result.script)
                result.variables += names + [pointer]
                trigger = Data("vstr \(pointer)".utf8)
            }
            guard trigger.count <= 1023 else { throw ConfigEditError.bindTooLong }
            // Stock native still drops escaped quote bytes on bind loading.
            // Keep new quoted scripts unavailable until that engine repair is
            // integrated, while imported bytes remain untouched.
            if dialect == .stock && trigger.contains(34) { throw ConfigChainError.unsafeQuotes }
            if trigger.first == 43 && trigger.count + " 255 -2147483648\n".utf8.count > 1023 { throw ConfigEditError.bindTooLong }
            var probe = ConfigDocument(data: Data(), dialect: dialect)
            try probe.setBind(key: ConfigDocument.keySpelling(key), binding: trigger)
            result.bindings[ConfigDocument.keySpelling(key)] = trigger
        }
        guard result.script.count <= 65535 else { throw ConfigEditError.commandBufferOverflow }
        var machine = ConfigChainMachine(dialect: dialect)
        let startup = machine.execute(result.script)
        if startup.issues.contains(where: \.blocksCompilation) { throw ConfigChainError.unsafeExecution }
        // Each press begins only after the previous one has drained. Also test
        // competitive wait-disabled execution so a wait loop cannot slip in.
        for disabled in dialect == .cod2x ? [false, true] : [false] {
            machine = ConfigChainMachine(dialect: dialect, waitDisabled: disabled)
            _ = machine.execute(result.script)
            for key in result.bindings.keys.sorted() {
                for _ in 0..<max(3, chains.map { if case .commandCycle(let steps) = $0.behavior { return steps.count }; return 1 }.max() ?? 1) {
                    for down in [true, false] {
                        let trace = machine.keyEvent(binding: result.bindings[key]!, key: ConfigDocument.keyNumber(key)!, down: down, time: down ? 100 : 200)
                        if trace.issues.contains(where: \.blocksCompilation) { throw ConfigChainError.unsafeExecution }
                        for issue in trace.issues where !result.issues.contains(issue) { result.issues.append(issue) }
                    }
                }
            }
        }
        return result
    }

    private static func joined(_ commands: [String], dialect: ConfigDialect) throws -> Data {
        guard !commands.isEmpty else { throw ConfigChainError.invalidCommands }
        var expected: [[Data]] = [], result = Data()
        for text in commands {
            let bytes = try ConfigDocument.encode(text)
            try validate(bytes, dialect: dialect)
            let parsed = ConfigDocument(data: bytes, dialect: dialect)
            guard parsed.commands.count == 1, let command = parsed.commands.first, !command.tokens.isEmpty, command.comments.isEmpty else { throw ConfigChainError.invalidCommands }
            expected.append(command.tokens.map(\.value))
            if !result.isEmpty { result.append(Data("; ".utf8)) }; result.append(bytes)
        }
        try validate(result, dialect: dialect)
        guard ConfigDocument(data: result, dialect: dialect).commands.map({ $0.tokens.map(\.value) }) == expected else { throw ConfigChainError.unsafeQuotes }
        return result
    }
    private static func validate(_ data: Data, dialect: ConfigDialect) throws {
        guard !data.contains(where: { [0, 10, 13].contains($0) }), data.count <= 4094 else { throw ConfigChainError.invalidCommands }
        let parsed = ConfigDocument(data: data, dialect: dialect)
        guard parsed.diagnostics.isEmpty else { throw ConfigChainError.invalidCommands }
        // This checkout's Cod2x_Tokenize treats high bytes outside quotes
        // differently from Mac 1.3/F1. New scripts must not rely on that drift.
        let quoted = parsed.commands.flatMap(\.tokens).filter { $0.kind == .quoted }.map(\.range)
        for (offset, byte) in data.enumerated() where byte >= 128 {
            guard quoted.contains(where: { $0.contains(offset) }) else { throw ConfigChainError.invalidCommands }
        }
    }
    private static func appendAssignment(name: String, value: Data, portable: Bool, dialect: ConfigDialect, to output: inout Data) throws {
        // Current Dvar_GetCombinedString_impl passes 1023 to I_strncat, even
        // though Mac 1.3's join buffer supports 4093. Do not emit truncated text.
        guard value.count <= 1022 else { throw ConfigChainError.helperTooLong }
        if (portable || dialect == .stock) && value.contains(34) { throw ConfigChainError.unsafeQuotes }
        guard value.last != 92 else { throw ConfigChainError.unsafeQuotes }
        var quoted = Data([34])
        for byte in value { if byte == 34 { quoted.append(92) }; quoted.append(byte) }
        quoted.append(34)
        let verb = portable ? "seta" : "set", line = Data("\(verb) \(name) ".utf8) + quoted
        try validate(line, dialect: dialect)
        let document = ConfigDocument(data: line, dialect: dialect)
        guard document.commands.count == 1, document.commands[0].tokens.map(\.value) == [Data(verb.utf8), Data(name.utf8), value] else { throw ConfigChainError.unsafeQuotes }
        output.append(line); output.append(10)
    }
    private static func canonicalKey(_ key: String) -> String { ConfigDocument.keyNumber(key).map(ConfigDocument.keySpelling) ?? key }
    private static func validName(_ name: String) -> Bool {
        !name.isEmpty && name.utf8.allSatisfy { (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0) || $0 == 95 }
    }
    private static func commandTexts(_ behavior: ConfigChainBehavior) -> [String] {
        switch behavior {
        case .commands(let commands): return commands
        case .commandCycle(let steps): return steps.flatMap { $0 }
        case .settingCycle(let name, _): return ["toggle " + name]
        }
    }
    private static func checkLocks(key: String, old: Data?, new: Data?, read: ConfigProfileRead) throws {
        let commands = [old, new].compactMap { $0 }.flatMap { ConfigDocument(data: $0, dialect: read.document.dialect).commands }
        try checkTraceLocks(key: key, commands: commands.map { $0.tokens.map(\.value) }, read: read)
    }
    private static func checkExistingLocks(key: String, binding: Data, read: ConfigProfileRead) throws {
        guard !read.metadata.locks.isEmpty else { return }
        var machine = ConfigChainMachine(dialect: read.document.dialect)
        _ = machine.execute(read.document.serialized())
        if let script = read.version.chain { _ = machine.execute(script) }
        for down in [true, false] {
            let trace = machine.keyEvent(binding: binding, key: ConfigDocument.keyNumber(key) ?? 0, down: down, time: down ? 100 : 200)
            try checkTraceLocks(key: key, commands: trace.commands, read: read)
        }
    }
    private static func checkTraceLocks(key: String, commands: [[Data]], read: ConfigProfileRead) throws {
        let names = Set(commands.flatMap { command -> [String] in
            guard let name = command.first.map(ConfigDocument.text)?.lowercased() else { return [] }
            let settings = ["set", "seta", "sets", "setu", "toggle", "togglep", "reset", "setfromdvar"]
            return [name] + (settings.contains(name) && command.count > 1 ? [ConfigDocument.text(command[1]).lowercased()] : [])
        })
        for lock in read.metadata.locks.sorted() {
            let name = lock.lowercased(), action = name.hasPrefix("action:") ? String(name.dropFirst(7)) : name
            if (name.hasPrefix("bind:") && ConfigDocument.keyNumber(String(name.dropFirst(5))) == ConfigDocument.keyNumber(key)) || names.contains(action) { throw ConfigChainError.locked(lock) }
        }
    }
    static func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]; return try encoder.encode(value)
    }

    static func windowsExport(read: ConfigProfileRead) throws -> Data {
        let chains = try ownedChains(read: read)
        let portable = try compile(chains: chains, profileID: read.profile.id, dialect: read.document.dialect, portable: true)
        // Keep the live bind bytes, including any rebind the game saved. A
        // managed trigger's identity does not authorize restoring an old bind.
        var bytes = read.document.serialized()
        if !bytes.isEmpty && bytes.last != 10 && bytes.last != 13 { bytes.append(10) }
        bytes.append(portable.script)
        guard bytes.count <= 65535 else { throw ConfigEditError.commandBufferOverflow }
        guard ConfigDocument(data: bytes, dialect: read.document.dialect).diagnostics.isEmpty else { throw ConfigChainError.unsafeQuotes }
        // Imported archives are never converted. Refuse values the engine's
        // unescaped Dvar_WriteVariables cannot safely roundtrip, too.
        let parsed = ConfigDocument(data: bytes, dialect: read.document.dialect)
        for command in parsed.commands where command.tokens.first?.text.lowercased() == "seta" && command.tokens.count >= 3 {
            let name = command.tokens[1].text
            if let value = ConfigDocument(data: bytes.subdata(in: command.range), dialect: read.document.dialect).effectiveSettingBytes(named: name), value.contains(34) || value.last == 92 { throw ConfigChainError.unsafeQuotes }
        }
        var machine = ConfigChainMachine(dialect: read.document.dialect)
        let startup = machine.execute(bytes)
        guard startup.complete else { throw ConfigChainError.portableDependency }
        try checkArchivedValues(machine)
        for key in 0..<256 {
            guard let binding = parsed.effectiveBindBytes(key: ConfigDocument.keySpelling(key)) else { continue }
            for _ in 0..<128 {
                let press = machine.keyEvent(binding: binding, key: key, down: true, time: 100)
                let release = machine.keyEvent(binding: binding, key: key, down: false, time: 200)
                guard press.complete && release.complete else { throw ConfigChainError.portableDependency }
                try checkArchivedValues(machine)
            }
        }
        let archived = Set(parsed.commands.filter { $0.tokens.first?.text.lowercased() == "seta" && $0.tokens.count >= 3 }.map { $0.tokens[1].text.lowercased() })
        guard machine.referencedVariables.isSubset(of: archived) else { throw ConfigChainError.portableDependency }
        return bytes
    }
    private static func checkArchivedValues(_ machine: ConfigChainMachine) throws {
        guard machine.unsafeArchivedVariables.isEmpty else { throw ConfigChainError.unsafeQuotes }
        for key in machine.archivedVariables {
            if let value = machine.variables[key], value.contains(34) || value.last == 92 { throw ConfigChainError.unsafeQuotes }
        }
    }
    static func exportWindows(read: ConfigProfileRead, to url: URL, home: URL) throws {
        let root = home.resolvingSymlinksInPath().standardizedFileURL.path
        let target = url.resolvingSymlinksInPath().standardizedFileURL.path
        guard url.isFileURL, target != root, !target.hasPrefix(root + "/") else { throw ConfigChainError.unsafeExport }
        try ConfigProfileStore.checkPath(url, under: url.deletingLastPathComponent())
        let bytes = try windowsExport(read: read)
        try PrivateFile.write(bytes, to: url)
    }

    /// Reuse I1's risk labels in the editor. This never executes reviewed text.
    static func safetyReview(edit: ConfigChainEdit, catalog: any ConfigImportCatalog) -> ConfigImportReview {
        let file = ConfigImportFile(path: "profile.cfg", data: edit.document.serialized() + Data([10]) + edit.script, dialect: edit.document.dialect)
        return ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: file.path, files: [file]))
    }
}
