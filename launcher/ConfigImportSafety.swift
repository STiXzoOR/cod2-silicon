import Foundation

enum ConfigImportClassification: Int, Sendable { case safe, needsOK, blocked }
enum ConfigImportRisk: String, Sendable {
    case unknownForwardedCommand, unknownSetting, inertName, credential, filesystemPath, protectedSetting
    case connect, quit, remoteConsole, identity, chat, fileWrite, unbindAll, network, sideEffect
    case unresolvedExec, unresolvedVariable, cycle, limit, malformed, negativeWait, script, keyRelease

    var summary: String {
        switch self {
        case .unknownForwardedCommand: return "Unknown command; the game may send it to the server"
        case .unknownSetting: return "Creates a helper variable or a setting we cannot verify"
        case .inertName: return "Has no effect in CoD2"
        case .credential: return "Contains a password, key or login credential"
        case .filesystemPath: return "Contains a filesystem path"
        case .protectedSetting: return "Protected by the game; cannot be imported as a setting"
        case .connect: return "Joins or leaves a server you did not choose"
        case .quit: return "Closes the game"
        case .remoteConsole: return "Sends an administrator command to a server"
        case .identity: return "Changes your name or player identity"
        case .chat: return "Sends chat or a response to other players"
        case .fileWrite: return "Writes files on your Mac"
        case .unbindAll: return "Clears all your key bindings"
        case .network: return "Changes downloads, redirects or network destinations"
        case .sideEffect: return "Changes the game session or restarts a subsystem"
        case .unresolvedExec: return "Runs another file that was not included or cannot be safely applied"
        case .unresolvedVariable: return "Runs a variable whose text cannot be determined"
        case .cycle: return "Repeats a script dependency; the game has no loop protection"
        case .limit: return "Exceeds the import or game parser limits"
        case .malformed: return "The game cannot read this command reliably"
        case .negativeWait: return "Can stall the game command buffer"
        case .script: return "Stores or runs a command chain"
        case .keyRelease: return "Extra commands also run when you release the key"
        }
    }
}

struct ConfigImportSetting: Sendable {
    var archived = false
    var cheatProtected = false
    var readOnly = false
    var writeProtected = false
    /// F4 can identify additional proven foreign-game names without treating
    /// arbitrary user-created chain variables as inert.
    var inert = false
}
struct ConfigImportCommandInfo: Sendable { var bindable = false }

/// F4 adapts its current-source catalog to this policy-only interface. The
/// research JSON decoder below is a local fixture input, never a bundled catalog.
protocol ConfigImportCatalog: Sendable {
    func setting(named: String, dialect: ConfigDialect) -> ConfigImportSetting?
    func command(named: String, dialect: ConfigDialect) -> ConfigImportCommandInfo?
}
struct ConfigImportCatalogSnapshot: ConfigImportCatalog {
    private var stockSettings: [String: ConfigImportSetting]
    private var cod2xSettings: [String: ConfigImportSetting]
    private var stockCommands: [String: ConfigImportCommandInfo]
    private var cod2xCommands: [String: ConfigImportCommandInfo]
    init(settings: [String: ConfigImportSetting], commands: [String: ConfigImportCommandInfo]) {
        stockSettings = Dictionary(settings.map { ($0.key.lowercased(), $0.value) }, uniquingKeysWith: { _, new in new })
        cod2xSettings = stockSettings
        stockCommands = Dictionary(commands.map { ($0.key.lowercased(), $0.value) }, uniquingKeysWith: { _, new in new })
        cod2xCommands = stockCommands
    }
    init(researchDvars: Data, researchCommands: Data) throws {
        struct Dvar: Decodable {
            let name: String
            let flags: Int?
            let availability: [String: Bool]
        }
        struct Command: Decodable {
            let name: String
            let bindable: Bool?
            let availability: [String: Bool]
        }
        stockSettings = [:]; cod2xSettings = [:]; stockCommands = [:]; cod2xCommands = [:]
        for row in try JSONDecoder().decode([Dvar].self, from: researchDvars) {
            let flags = row.flags ?? 0
            let info = ConfigImportSetting(archived: flags & 1 != 0, cheatProtected: flags & 128 != 0,
                                           readOnly: flags & 64 != 0, writeProtected: flags & 16 != 0)
            if row.availability["portStock"] == true { stockSettings[row.name.lowercased()] = info }
            if row.availability["portCoD2x"] == true { cod2xSettings[row.name.lowercased()] = info }
        }
        for row in try JSONDecoder().decode([Command].self, from: researchCommands) {
            let info = ConfigImportCommandInfo(bindable: row.bindable == true)
            if row.availability["portStock"] == true { stockCommands[row.name.lowercased()] = info }
            if row.availability["portCoD2x"] == true { cod2xCommands[row.name.lowercased()] = info }
        }
    }
    func setting(named: String, dialect: ConfigDialect) -> ConfigImportSetting? {
        (dialect == .stock ? stockSettings : cod2xSettings)[named.lowercased()]
    }
    func command(named: String, dialect: ConfigDialect) -> ConfigImportCommandInfo? {
        (dialect == .stock ? stockCommands : cod2xCommands)[named.lowercased()]
    }
}

struct ConfigImportLocation: Equatable, Sendable {
    let file: String
    /// Empty for original file bytes; otherwise ranges address decoded script
    /// bytes in this enclosing bind/variable, not offsets in the original file.
    let context: [String]
    let range: Range<Int>
}
struct ConfigImportDependency: Equatable, Sendable {
    enum Kind: Sendable { case exec, vstr }
    let kind: Kind
    let name: String
    let resolved: Bool
    var locations: [ConfigImportLocation] = []
}
struct ConfigImportEffect: Sendable {
    let location: ConfigImportLocation
    let command: ConfigCommand
    let classification: ConfigImportClassification
    let reasons: [ConfigImportRisk]
    let details: String
}
struct ConfigImportCandidate: Sendable, Identifiable {
    var id: String { "\(location.file):\(location.range.lowerBound)" }
    let location: ConfigImportLocation
    let command: ConfigCommand
    let classification: ConfigImportClassification
    let importable: Bool
    let reasons: [ConfigImportRisk]
    let effects: [ConfigImportEffect]
    let dependencies: [ConfigImportDependency]
    /// Safe for review display, including parents of credential-bearing scripts.
    let details: String
    var selectedByDefault: Bool { classification == .safe && importable }
}
struct ConfigImportReview: Sendable {
    let bundle: ConfigImportBundle
    let candidates: [ConfigImportCandidate]
}

/// Static, conservative review. This never interprets or executes imported
/// commands. All possible variable definitions are followed, including later
/// definitions and assignments inside reachable scripts; ordering, clamping,
/// server policy and key/time arguments can make fewer commands run in game.
struct ConfigImportSafety: Sendable {
    let catalog: any ConfigImportCatalog
    var maximumEffects = 4096
    var maximumDepth = 32

    func review(_ bundle: ConfigImportBundle) -> ConfigImportReview {
        var graph = Graph(catalog: catalog)
        for file in bundle.files { graph.collect(file.document, file: file.path, context: [], depth: 0) }
        // A bare write may precede the explicit creation of a helper in another
        // file. Revisit once every collected variable name is known.
        graph.visited = []; graph.count = 0
        for file in bundle.files { graph.collect(file.document, file: file.path, context: [], depth: 0) }
        graph.resolveCopies()
        let files = Dictionary(bundle.files.map { ($0.path, $0) }, uniquingKeysWith: { first, _ in first })
        let invalidBundle = bundle.files.count > 64 || bundle.files.reduce(0, { $0 + $1.document.serialized().count }) > 262140
            || bundle.files.contains { $0.document.serialized().count > 65535 }
        var candidates: [ConfigImportCandidate] = []
        for file in bundle.files {
            for command in file.document.commands where !command.range.isEmpty {
                var budget = maximumEffects
                let location = ConfigImportLocation(file: file.path, context: [], range: command.range)
                var assessment = assess(command, document: file.document, location: location,
                                        files: files, graph: graph, stack: [], budget: &budget, previewStoredRoot: true)
                if invalidBundle || graph.limited { assessment.add(.limit, level: .blocked, importable: false) }
                let details = assessment.reasons.contains(.credential) ? "[credential-bearing command removed]" : ConfigDocument.text(file.document.serialized().subdata(in: command.range))
                candidates.append(.init(location: location, command: command, classification: assessment.level,
                                        importable: assessment.importable, reasons: assessment.reasons,
                                        effects: assessment.effects, dependencies: assessment.dependencies, details: details))
            }
        }
        return .init(bundle: bundle, candidates: candidates)
    }

    private struct Assessment {
        var level: ConfigImportClassification = .safe
        var importable = true
        var reasons: [ConfigImportRisk] = []
        var effects: [ConfigImportEffect] = []
        var dependencies: [ConfigImportDependency] = []
        mutating func add(_ risk: ConfigImportRisk, level: ConfigImportClassification = .needsOK, importable: Bool = true) {
            if !reasons.contains(risk) { reasons.append(risk) }
            if level.rawValue > self.level.rawValue { self.level = level }
            self.importable = self.importable && importable
        }
        mutating func merge(_ child: Assessment) {
            for reason in child.reasons { add(reason, level: child.level, importable: child.importable) }
            if child.level.rawValue > level.rawValue { level = child.level }
            importable = importable && child.importable
            effects += child.effects
            dependencies += child.dependencies
        }
    }

    private func assess(_ command: ConfigCommand, document: ConfigDocument, location: ConfigImportLocation,
                        files: [String: ConfigImportFile], graph: Graph, stack: [String], budget: inout Int,
                        detectCycles: Bool = true, previewStoredRoot: Bool = false) -> Assessment {
        var result = Assessment()
        guard budget > 0, stack.count <= maximumDepth else { result.add(.limit, level: .blocked, importable: false); return result }
        budget -= 1
        let tokens = command.tokens, dialect = document.dialect
        if document.ignoredSuffixRange != nil || command.splitAtLimit || command.tokenizationFailure != nil || tokens.contains(where: { !$0.closed }) {
            result.add(.malformed, level: .blocked, importable: false)
        }
        guard let first = tokens.first else { result.importable = false; return result }
        let name = first.text.lowercased()
        let assignment = ConfigImportScript.assignment(command, document: document, catalog: catalog, knownNames: Set(graph.values.keys))
        if let assignment {
            assessSetting(assignment.name, value: assignment.value, dialect: dialect,
                          archived: graph.archivedNames.contains(assignment.name), into: &result)
            if ["sets", "setu"].contains(name) { result.add(.network) }
            if graph.referenced.contains(assignment.name) || ConfigImportScript.looksExecutable(assignment.value, dialect: dialect) {
                result.add(.script)
                // A root definition previews a hypothetical vstr of this value.
                // Nested setters store text for later presses; inspecting that
                // text must not create an immediate execution edge or loop.
                let inspectExecution = detectCycles && previewStoredRoot
                expand(assignment.value, dialect: dialect, location: location, label: "variable:" + assignment.name,
                       files: files, graph: graph, stack: inspectExecution ? ["variable:" + assignment.name] : stack,
                       budget: &budget, into: &result, detectCycles: inspectExecution)
            }
        } else if command.kind == .setting {
            result.add(.malformed, level: .blocked, importable: false)
        } else {
            switch name {
            case "bind":
                guard tokens.count >= 3, let key = ConfigDocument.keyNumber(tokens[1].text),
                      let binding = ConfigImportScript.binding(command, document: document) else {
                    result.add(.malformed, level: .blocked, importable: false); return result
                }
                if ConfigImportScript.bindingWasTruncated(command) { result.add(.limit, level: .blocked, importable: false) }
                if !Self.preservesSavedValue(binding, command: "bind", name: ConfigDocument.keySpelling(key), escapeQuotes: true, dialect: dialect) {
                    result.add(.malformed, level: .blocked, importable: false)
                }
                if binding.first == 43 {
                    if binding.contains(59) { result.add(.keyRelease) }
                    // CL_KeyEvent appends key/time to the whole + binding on
                    // both press and release (Com_sprintf caps it at 1023).
                    // The timestamp's value cannot change this command name.
                    let event = Data((binding + Data(" \(key) 1\n".utf8)).prefix(1023))
                    let parsed = ConfigDocument(data: event, dialect: dialect)
                    for generated in parsed.commands where generated.tokens.first.map({ $0.range.lowerBound >= binding.count }) == true {
                        result.add(.unknownForwardedCommand, importable: false)
                        result.effects.append(.init(location: .init(file: location.file, context: location.context + ["key event (illustrative time)"], range: generated.range),
                                                    command: generated, classification: .needsOK, reasons: [.unknownForwardedCommand],
                                                    details: ConfigDocument.text(event.subdata(in: generated.range))))
                    }
                }
                expand(binding, dialect: dialect, location: location, label: "bind:" + tokens[1].text,
                       files: files, graph: graph, stack: stack, budget: &budget, into: &result,
                       detectCycles: detectCycles && previewStoredRoot)
            case "unbind":
                if tokens.count != 2 || ConfigDocument.keyNumber(tokens[1].text, folding: false) == nil { result.add(.malformed, importable: false) }
            case "unbindall": result.add(.unbindAll)
            case "exec":
                // References are never executable import payloads. I2 must apply
                // reviewed dependency commands, or copy a fully reviewed chain
                // under launcher ownership and rewrite its reference explicitly.
                result.importable = false
                result.add(.script)
                guard tokens.count == 2, let path = ConfigImportScript.execPath(tokens[1].value) else {
                    result.add(.unresolvedExec); return result
                }
                let target = files[path]
                result.dependencies.append(.init(kind: .exec, name: path, resolved: target != nil))
                if let target {
                    let key = "file:" + path
                    if stack.contains(key) {
                        if detectCycles { result.add(.cycle, level: .blocked, importable: false) }
                    }
                    else {
                        expand(target.document.serialized(), dialect: dialect,
                               location: .init(file: path, context: [], range: 0..<0), label: nil,
                               files: files, graph: graph, stack: stack + [key], budget: &budget, into: &result, detectCycles: detectCycles)
                    }
                } else { result.add(.unresolvedExec) }
            case "vstr":
                guard tokens.count == 2 else { result.add(.malformed, importable: false); return result }
                let variable = tokens[1].text.lowercased(), key = "variable:" + variable
                let values = graph.values[variable] ?? []
                result.dependencies.append(.init(kind: .vstr, name: variable, resolved: !values.isEmpty, locations: graph.origins[variable] ?? []))
                if stack.contains(key) {
                    if detectCycles { result.add(.cycle, level: .blocked, importable: false) }
                }
                else if values.isEmpty { result.add(.unresolvedVariable, importable: false) }
                else {
                    for value in values.sorted(by: { $0.lexicographicallyPrecedes($1) }) {
                        // Cmd_Vstr_f adds LF through the production 1024-byte
                        // va buffer. Longer helpers error before any leaf runs.
                        guard value.count <= 1022 else {
                            result.add(.limit, level: .blocked, importable: false); continue
                        }
                        expand(value, dialect: dialect, location: location, label: key,
                               files: files, graph: graph, stack: stack + [key], budget: &budget, into: &result, detectCycles: detectCycles)
                    }
                }
            case "toggle", "togglep", "reset", "increase", "decrease", "setfromdvar":
                if ["increase", "decrease"].contains(name), catalog.command(named: name, dialect: dialect) == nil {
                    result.add(.unknownForwardedCommand, importable: false); return result
                }
                if tokens.count < 2 { result.add(.malformed, importable: false) }
                else {
                    let values = tokens.count > 2 ? Array(tokens.dropFirst(2)).map(\.value) : [Data()]
                    for value in values {
                        assessSetting(tokens[1].text.lowercased(), value: value, dialect: dialect,
                                      archived: graph.archivedNames.contains(tokens[1].text.lowercased()), into: &result)
                        if name != "setfromdvar" && ConfigImportScript.looksExecutable(value, dialect: dialect) {
                            result.add(.script)
                            expand(value, dialect: dialect, location: location, label: "value:" + tokens[1].text.lowercased(),
                                   files: files, graph: graph, stack: stack, budget: &budget, into: &result, detectCycles: false)
                        }
                    }
                    if name == "setfromdvar" {
                        if tokens.count != 3 { result.add(.malformed, importable: false) }
                        else {
                            let copied = graph.values[tokens[2].text.lowercased()] ?? []
                            if copied.isEmpty { result.add(.unresolvedVariable, importable: false) }
                            for value in copied.sorted(by: { $0.lexicographicallyPrecedes($1) }) {
                                assessSetting(tokens[1].text.lowercased(), value: value, dialect: dialect,
                                              archived: graph.archivedNames.contains(tokens[1].text.lowercased()), into: &result)
                                if ConfigImportScript.looksExecutable(value, dialect: dialect) || graph.referenced.contains(tokens[1].text.lowercased()) {
                                    result.add(.script)
                                    expand(value, dialect: dialect, location: location, label: "copied:" + tokens[2].text.lowercased(),
                                           files: files, graph: graph, stack: stack, budget: &budget, into: &result, detectCycles: false)
                                }
                            }
                            if Self.credentialName(tokens[2].text.lowercased()) { result.add(.credential, level: .blocked, importable: false) }
                        }
                    }
                }
            case "wait":
                if tokens.count == 2 && tokens[1].text.trimmingCharacters(in: .whitespaces).hasPrefix("-") {
                    result.add(.negativeWait, level: .blocked, importable: false)
                } else { result.add(.sideEffect) }
            case "connect", "reconnect", "disconnect": result.add(.connect)
            case "quit": result.add(.quit)
            case "rcon":
                if tokens.dropFirst().contains(where: { Self.credentialName($0.text.lowercased()) || $0.text.lowercased().split(separator: " ").contains("login") }) {
                    result.add(.credential, level: .blocked, importable: false)
                }
                else { result.add(.remoteConsole) }
            case "openscriptmenu", "match", "mr":
                if tokens.dropFirst().contains(where: { $0.text.lowercased().contains("login") || $0.text.lowercased().contains("password") }) {
                    result.add(.credential, level: .blocked, importable: false)
                } else { result.add(.chat) }
            case "name": result.add(.identity)
            case "say", "say_team", "tell", "cmd", "callvote", "vote", "voicechat", "voiceteamchat", "mp_quickmessage": result.add(.chat)
            case "writeconfig", "writedefaults", "condump", "record", "screenshot", "screenshotjpeg":
                result.add(.fileWrite)
                if tokens.dropFirst().contains(where: { Self.pathValue($0.text) }) { result.add(.filesystemPath, level: .blocked, importable: false) }
            default:
                if Self.credentialName(name) || name.contains("login") { result.add(.credential, level: .blocked, importable: false) }
                else if Self.pathName(name) { result.add(.filesystemPath, level: .blocked, importable: false) }
                else if Self.inertName(name) { result.add(.inertName, importable: false); result.add(.unknownForwardedCommand, importable: false) }
                else if let info = catalog.command(named: name, dialect: dialect) {
                    if !info.bindable || !Self.safeAction(name) { result.add(.sideEffect) }
                } else if catalog.setting(named: name, dialect: dialect) != nil {
                    // A lone dvar prints its value; do not turn it into a write.
                    result.importable = false
                } else { result.add(.unknownForwardedCommand, importable: false) }
            }
        }
        return result
    }

    private func expand(_ data: Data, dialect: ConfigDialect, location: ConfigImportLocation, label: String?,
                        files: [String: ConfigImportFile], graph: Graph, stack: [String], budget: inout Int, into result: inout Assessment,
                        detectCycles: Bool = true) {
        let document = ConfigDocument(data: data, dialect: dialect)
        let context = label.map { location.context + [$0] } ?? []
        for command in document.commands where !command.tokens.isEmpty || command.tokenizationFailure != nil || command.splitAtLimit {
            guard budget > 0 else { result.add(.limit, level: .blocked, importable: false); break }
            let childLocation = ConfigImportLocation(file: location.file, context: context, range: command.range)
            var child = assess(command, document: document, location: childLocation, files: files,
                               graph: graph, stack: stack + (label?.hasPrefix("bind:") == true ? [label!] : []),
                               budget: &budget, detectCycles: detectCycles)
            let details = child.reasons.contains(.credential) ? "[credential-bearing command removed]" : ConfigDocument.text(data.subdata(in: command.range))
            child.effects.insert(.init(location: childLocation, command: command, classification: child.level,
                                       reasons: child.reasons, details: details), at: 0)
            result.merge(child)
        }
    }

    /// Key_WriteBindings escapes only quotes; Dvar_WriteVariables writes the
    /// archived value verbatim. Cbuf counts every quote, including escaped
    /// quotes, before tokenization, so compare the actual saved command shape.
    private static func preservesSavedValue(_ value: Data, command: String, name: String,
                                           escapeQuotes: Bool, dialect: ConfigDialect) -> Bool {
        var saved = Data("\(command) \(name) \"".utf8)
        for byte in value {
            if escapeQuotes && byte == 34 { saved.append(92) }
            saved.append(byte)
        }
        saved.append(contentsOf: [34, 10])
        let parsed = ConfigDocument(data: saved, dialect: dialect)
        guard parsed.ignoredSuffixRange == nil, parsed.commands.count == 1,
              let reloaded = parsed.commands.first, !reloaded.splitAtLimit,
              reloaded.tokenizationFailure == nil, reloaded.tokens.allSatisfy(\.closed) else { return false }
        return reloaded.tokens.map(\.value) == [Data(command.utf8), Data(name.utf8), value]
    }

    private func assessSetting(_ name: String, value: Data, dialect: ConfigDialect, archived: Bool = false, into result: inout Assessment) {
        if archived || catalog.setting(named: name, dialect: dialect)?.archived == true {
            if !Self.preservesSavedValue(value, command: "seta", name: name, escapeQuotes: false, dialect: dialect) {
                result.add(.malformed, level: .blocked, importable: false)
            }
        }
        if Self.credentialName(name) { result.add(.credential, level: .blocked, importable: false) }
        if let url = URLComponents(string: ConfigDocument.text(value)),
           ["http", "https"].contains(url.scheme?.lowercased() ?? ""),
           url.user != nil || url.password != nil || url.queryItems?.contains(where: { Self.credentialURLParameter($0.name) }) == true {
            result.add(.credential, level: .blocked, importable: false)
        }
        if Self.pathName(name) || Self.pathValue(ConfigDocument.text(value)) { result.add(.filesystemPath, level: .blocked, importable: false) }
        if name.hasPrefix("cg_fov") { result.add(.protectedSetting, level: .blocked, importable: false) }
        if let info = catalog.setting(named: name, dialect: dialect) {
            if info.inert { result.add(.inertName) }
            if info.cheatProtected || info.readOnly || info.writeProtected { result.add(.protectedSetting, level: .blocked, importable: false) }
            else if !info.inert && !info.archived { result.add(.sideEffect) }
        } else if Self.inertName(name) { result.add(.inertName) }
        else { result.add(.unknownSetting) }
        if name == "name" || name == "cl_anonymous" { result.add(.identity) }
        if name.contains("download") || name.contains("redirect") || name.contains("upload") || name.contains("www")
            || name.hasPrefix("net_") || name.hasPrefix("server") || name.hasPrefix("cl_master") { result.add(.network) }
        if ["activeaction", "com_writeconfig", "cl_avidemo", "developer", "developer_script", "logfile", "g_log", "r_logfile"].contains(name) { result.add(.sideEffect) }
    }

    private static func credentialName(_ name: String) -> Bool {
        name.contains("password") || name.contains("passwd") || name.contains("cdkey") || name.contains("cd_key")
            || name.contains("token") || name.contains("secret") || name.hasPrefix("cl_hwid")
            || ["match_login", "codkey", "auth", "apikey", "api_key"].contains(name)
    }
    static func credentialURLParameter(_ name: String) -> Bool {
        let name = name.lowercased()
        return credentialName(name) || ["key", "signature", "sig", "access_key"].contains(name)
    }
    private static func pathName(_ name: String) -> Bool {
        name.hasPrefix("fs_") || name.hasPrefix("sys_") || name.contains("path") || name.contains("directory")
            || ["com_playerprofile", "g_log"].contains(name)
    }
    private static func pathValue(_ value: String) -> Bool {
        let lower = value.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if lower.hasPrefix("https://") || lower.hasPrefix("http://") { return false }
        if lower.hasPrefix("/") || lower.hasPrefix("~") || lower.hasPrefix("\\") || lower.hasPrefix("file:") { return true }
        if lower.contains("../") || lower.contains("..\\") || lower.contains("/users/") || lower.contains("/volumes/") { return true }
        let bytes = Array(lower.utf8)
        return lower.contains("/") || lower.contains("\\")
            || (bytes.count >= 3 && (97...122).contains(bytes[0]) && bytes[1] == 58 && [47, 92].contains(bytes[2]))
    }
    private static func safeAction(_ name: String) -> Bool {
        if name.hasPrefix("+") || name.hasPrefix("-") { return true }
        return ["weapnext", "weapprev", "weaponslot", "toggleads", "togglescores", "gocrouch", "goprone", "gostand",
                "togglecrouch", "toggleprone", "centerview", "togglemenu", "openmenu", "closemenu", "toggleconsole",
                "chatmodepublic", "chatmodeteam", "clear"].contains(name)
    }
    private static func inertName(_ name: String) -> Bool {
        ["cl_timenudge", "r_vsync", "cl_run", "pmove_fixed", "cvarlist", "r_primitives", "r_smp", "handicap", "head",
         "cg_stereoseparation", "cg_laserlight", "r_dof_enable", "ragdoll_enable"].contains(name)
            || ["cg_sprintmeter", "cg_scoresping_", "compass", "r_texfilter", "b_"].contains(where: { name.hasPrefix($0) })
    }

    private struct Graph {
        let catalog: any ConfigImportCatalog
        var values: [String: Set<Data>] = [:]
        var origins: [String: [ConfigImportLocation]] = [:]
        var referenced = Set<String>()
        var archivedNames = Set<String>()
        var copies: [(String, String)] = []
        var visited = Set<Data>()
        var count = 0
        var limited = false
        mutating func collect(_ document: ConfigDocument, file: String, context: [String], depth: Int) {
            guard depth <= 16, count < 8192 else { limited = true; return }
            guard visited.insert(document.serialized()).inserted else { return }
            for command in document.commands where command.kind == .setting && command.tokens.count >= 3 {
                values[command.tokens[1].text.lowercased(), default: []].formUnion([])
                if command.tokens[0].text.lowercased() == "seta" { archivedNames.insert(command.tokens[1].text.lowercased()) }
            }
            for command in document.commands {
                count += 1
                guard count < 8192 else { limited = true; return }
                let tokens = command.tokens
                let location = ConfigImportLocation(file: file, context: context, range: command.range)
                if let assignment = ConfigImportScript.assignment(command, document: document, catalog: catalog, knownNames: Set(values.keys)) {
                    values[assignment.name, default: []].insert(assignment.value)
                    if !origins[assignment.name, default: []].contains(location) { origins[assignment.name, default: []].append(location) }
                    if ConfigImportScript.looksExecutable(assignment.value, dialect: document.dialect) {
                        collect(.init(data: assignment.value, dialect: document.dialect), file: file, context: context + ["variable:" + assignment.name], depth: depth + 1)
                    }
                } else if command.kind == .bind, let binding = ConfigImportScript.binding(command, document: document) {
                    collect(.init(data: binding, dialect: document.dialect), file: file, context: context + ["bind:" + tokens[1].text], depth: depth + 1)
                }
                if tokens.count == 2 && command.kind == .vstr { referenced.insert(tokens[1].text.lowercased()) }
                if tokens.count == 3 && tokens[0].text.lowercased() == "setfromdvar" {
                    copies.append((tokens[1].text.lowercased(), tokens[2].text.lowercased()))
                    origins[tokens[1].text.lowercased(), default: []].append(location)
                }
                if tokens.count > 2 && ["toggle", "togglep"].contains(tokens[0].text.lowercased()) {
                    for token in tokens.dropFirst(2) { values[tokens[1].text.lowercased(), default: []].insert(token.value) }
                }
            }
        }
        mutating func resolveCopies() {
            for _ in 0..<32 {
                var changed = false
                for (dest, source) in copies {
                    let before = values[dest, default: []].count
                    values[dest, default: []].formUnion(values[source] ?? [])
                    changed = changed || values[dest, default: []].count != before
                }
                if !changed { return }
            }
            limited = true
        }
    }
}
