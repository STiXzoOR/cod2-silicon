import Foundation
import Darwin
import CryptoKit

struct LauncherError: LocalizedError, Sendable {
    var message: String
    var errorDescription: String? { message }
}

struct ColorRun: Equatable, Sendable { var text: String; var code: Int }
enum QuakeColors {
    static func runs(_ name: String) -> [ColorRun] {
        let chars = Array(name.prefix(1024))
        var result: [ColorRun] = [], text = "", code = 7, i = 0
        while i < chars.count {
            if chars[i] == "^", i + 1 < chars.count, let digit = chars[i + 1].wholeNumberValue, digit < 10 {
                if !text.isEmpty { result.append(ColorRun(text: text, code: code)); text = "" }
                code = digit; i += 2
            } else { text.append(chars[i]); i += 1 }
        }
        if !text.isEmpty { result.append(ColorRun(text: text, code: code)) }
        return result
    }
    static func plain(_ name: String) -> String { runs(name).map(\.text).joined() }
}

struct ServerPlayer: Codable, Sendable, Identifiable {
    var id: Int; var score: Int; var ping: Int; var name: String
}
struct GameServer: Codable, Sendable, Identifiable {
    var address: String
    var fields: [String: String]
    var ping: Int
    var players: [ServerPlayer]
    var observedAt: Date? = nil
    var queryConflicts: [String]? = nil
    var id: String { address }
    var name: String { fields["sv_hostname"] ?? fields["hostname"] ?? address }
    var map: String { fields["mapname"] ?? "Unknown" }
    var gametype: String { fields["g_gametype"] ?? fields["gametype"] ?? "—" }
    var maxPlayers: Int { Int(fields["sv_maxclients"] ?? fields["sv_maxClients"] ?? "0") ?? 0 }
    var playerCount: Int { Int(fields["clients"] ?? "") ?? players.count }
    var version: String { fields["shortversion"] ?? (fields["protocol"] == "120" ? "1.4.x" : "1.3") }
    var mod: String { fields["fs_game"] ?? "" }
    var password: Bool { fields["pswrd"] == "1" || fields["g_password"] == "1" }
    var fps: String { fields["sv_maxfps"] ?? fields["com_maxfps"] ?? fields["g_maxfps"] ?? "Server default" }
    mutating func mergeInfo(_ info: GameServer) {
        var conflicts = Set(queryConflicts ?? [])
        for (key, value) in info.fields {
            let name = key.lowercased()
            if fields.contains(where: { $0.key.lowercased() == name && $0.value != value }) { conflicts.insert(name) }
            fields[key] = value
        }
        queryConflicts = conflicts.isEmpty ? nil : conflicts.sorted()
    }
}

enum WireParser {
    static let marker = Data([255, 255, 255, 255])
    static func master(_ packet: Data) -> [String] {
        let header = marker + Data("getserversResponse".utf8)
        guard packet.count <= 65535, packet.starts(with: header) else { return [] }
        let b = Array(packet)
        var i = header.count, result: [String] = []
        while i < b.count && [0, 10, 13].contains(b[i]) { i += 1 }
        while i + 7 <= b.count && b[i] == 92 {
            if Array(b[(i + 1)..<(i + 4)]) == Array("EOT".utf8) { break }
            let port = Int(b[i + 5]) * 256 + Int(b[i + 6])
            if port > 0 && b[i + 1] > 0 && b[i + 1] < 224 {
                result.append("\(b[i + 1]).\(b[i + 2]).\(b[i + 3]).\(b[i + 4]):\(port)")
            }
            i += 7
        }
        return Array(Set(result)).sorted()
    }
    static func status(_ packet: Data, address: String, ping: Int) throws -> GameServer {
        guard packet.count <= 65535, packet.starts(with: marker) else { throw LauncherError(message: "Invalid server response.") }
        let bytes = packet.dropFirst(4)
        let text = String(data: Data(bytes), encoding: .utf8) ?? String(data: Data(bytes), encoding: .isoLatin1) ?? ""
        let lines = text.components(separatedBy: "\n")
        guard lines.count >= 2, ["statusResponse", "infoResponse"].contains(lines[0].trimmingCharacters(in: .whitespacesAndNewlines)) else {
            throw LauncherError(message: "Invalid server response header.")
        }
        let tokens = lines[1].components(separatedBy: "\\")
        guard tokens.first == "", tokens.count % 2 == 1, tokens.count <= 1025 else { throw LauncherError(message: "Invalid server info fields.") }
        var fields: [String: String] = [:]
        for i in stride(from: 1, to: tokens.count - 1, by: 2) {
            guard tokens[i].count <= 128, tokens[i + 1].count <= 2048 else { throw LauncherError(message: "Oversized server field.") }
            if let previous = fields[tokens[i]], previous != tokens[i + 1] { throw LauncherError(message: "Conflicting server info fields.") }
            fields[tokens[i]] = tokens[i + 1]
        }
        var players: [ServerPlayer] = []
        for line in lines.dropFirst(2).prefix(256) {
            guard let quote = line.firstIndex(of: "\""), let end = line.lastIndex(of: "\""), quote < end else { continue }
            let numbers = line[..<quote].split(separator: " ")
            guard numbers.count == 2, let score = Int(numbers[0]), let ping = Int(numbers[1]) else { continue }
            players.append(ServerPlayer(id: players.count, score: score, ping: ping, name: String(line[line.index(after: quote)..<end].prefix(128))))
        }
        let statusObservation = lines[0].trimmingCharacters(in: .whitespacesAndNewlines) == "statusResponse"
        return GameServer(address: address, fields: fields, ping: ping, players: players, observedAt: statusObservation ? Date() : nil)
    }
}

struct LaunchLink: Sendable {
    var address: String
    var password: String
    var url: String
    init(_ url: String) throws {
        guard let parsed = LauncherParseLink(url) else { throw LauncherError(message: "This link is invalid. Only connect and password are accepted.") }
        self.url = url; address = parsed["address"] ?? ""; password = parsed["password"] ?? ""
    }
    static func direct(_ address: String, password: String = "") throws -> LaunchLink {
        let command = "connect \(address) password \"\(password)\""
        let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-._~"))
        guard let encoded = command.addingPercentEncoding(withAllowedCharacters: allowed) else { throw LauncherError(message: "Invalid address.") }
        return try LaunchLink("cod2x://" + encoded)
    }
    static func canonicalAddress(_ address: String) throws -> String {
        let parsed = try direct(address.trimmingCharacters(in: .whitespacesAndNewlines)).address.lowercased()
        return parsed.contains(":") ? parsed : parsed + ":28960"
    }
}

enum EngineCommandLine {
    static func validate(_ arguments: [String]) throws -> [String] {
        let text = arguments.joined(separator: " ")
        guard text.utf8.count < 3900 else { throw LauncherError(message: "Launch arguments exceed the game's byte limit.") }
        guard text.filter({ $0 == "+" }).count <= 31 else { throw LauncherError(message: "Too many startup commands. Reduce the number of advanced dvars before playing.") }
        return arguments
    }
}

struct ConfigStudioLaunch: Sendable {
    let profileID: String
    let chain: String?
    var connectionIsolated = false
    static func validateProfileID(_ id: String) throws {
        guard id.range(of: "\\A[A-Za-z0-9][A-Za-z0-9_.-]{0,40}\\z", options: .regularExpression) != nil else {
            throw LauncherError(message: "The loadout identifier is invalid.")
        }
    }
    static func chainBasename(profileID: String) throws -> String {
        try validateProfileID(profileID)
        // A fixed ASCII basename fits MAX_QPATH even for a 41-byte profile ID.
        // Hash every ID so punctuation substitution cannot alias another profile.
        let digest = SHA256.hash(data: Data(profileID.lowercased().utf8)).prefix(20).map { String(format: "%02x", $0) }.joined()
        return "cs_" + digest + ".cfg"
    }
}

struct ConfigConnectionLaunch: Equatable, Sendable {
    let address: String
    let values: [String: String]
    static let allowedSettings: Set<String> = ["com_maxfps", "rate", "snaps", "cl_maxpackets", "m_filter", "cl_mouseaccel", "r_picmip", "r_picmip_manual"]
    func arguments(for link: LaunchLink?) throws -> [String] {
        guard let link, try LaunchLink.canonicalAddress(link.address) == LaunchLink.canonicalAddress(address),
              Set(values.keys.map { $0.lowercased() }).count == values.count else {
            throw LauncherError(message: "Temporary settings belong to a different connection.")
        }
        return try values.sorted { $0.key < $1.key }.flatMap { name, value in
            guard Self.allowedSettings.contains(name.lowercased()),
                  value.range(of: "^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)$", options: .regularExpression) != nil,
                  Double(value)?.isFinite == true, GameSettings.safeValue(value) else {
                throw LauncherError(message: "This temporary setting is not an editable, numeric client option.")
            }
            return ["+set", name.lowercased(), "\"\(value)\""]
        }
    }
}

/// Which engine a game starts with. CoD2x is compiled into the engine, so the app carries two:
/// `cod2_macos` with CoD2x and `cod2_macos_stock`, the original 1.3 from the same sources.
enum GameEngine: String, Codable, Sendable, CaseIterable {
    case cod2x, original
    var executableName: String { self == .cod2x ? "cod2_macos" : "cod2_macos_stock" }
    /// The engines a game app carries, by their executables.
    static func installed(in gameBundle: URL) -> Set<GameEngine> {
        Set(allCases.filter { FileManager.default.isExecutableFile(atPath: $0.executable(in: gameBundle).path) })
    }
    func executable(in gameBundle: URL) -> URL { gameBundle.appendingPathComponent("Contents/MacOS/" + executableName) }
    /// Whether this engine can play on the server: CoD2x joins both kinds, the original only stock 1.3.
    func canJoin(_ server: GameServer) -> Bool { self == .cod2x || !ServerFacts(server).isCoD2x }
}

struct GameLaunchPlan {
    var arguments: [String]
    var link: LaunchLink?
    /// loadout: the value the game's profile config gives a dvar; with it, what that config leaves
    /// unset is supplied from GameSettings.loadoutDefaults.
    init(settings: GameSettings, dataPath: String, homePath: String, link: LaunchLink? = nil, demo: String? = nil, forwarded: [String] = [], configStudio: ConfigStudioLaunch? = nil, connection: ConfigConnectionLaunch? = nil,
         loadout: ((String) -> String?)? = nil) throws {
        guard GameSettings.safeValue(dataPath), GameSettings.safeValue(homePath) else { throw LauncherError(message: "Game paths contain unsupported characters.") }
        var args = try settings.launchDvars(migrated: configStudio != nil).flatMap { ["+set", $0.0, "\"\($0.1)\""] }
        if let loadout {
            args += try settings.loadoutDefaults(migrated: configStudio != nil, loadout: loadout).flatMap { ["+seta", $0.0, "\"\($0.1)\""] }
        }
        args += ["+set", "fs_basepath", "\"\(dataPath)\"", "+set", "fs_homepath", "\"\(homePath)\""]
        if let configStudio {
            try ConfigStudioLaunch.validateProfileID(configStudio.profileID)
            if let chain = configStudio.chain {
                guard chain == (try ConfigStudioLaunch.chainBasename(profileID: configStudio.profileID)) else {
                    throw LauncherError(message: "The loadout's advanced chain filename is invalid.")
                }
                let path = URL(fileURLWithPath: homePath).appendingPathComponent("main/" + chain)
                let values = try? path.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey])
                if values?.isRegularFile == true && values?.isSymbolicLink != true { args += ["+exec", chain] }
            }
        }
        if let demo {
            guard demo.range(of: "^[A-Za-z0-9_.-]+$", options: .regularExpression) != nil else { throw LauncherError(message: "Rename this demo using letters, numbers, dots, underscores or hyphens before playback.") }
            args += ["+demo", demo]
        }
        if connection != nil && (configStudio == nil || demo != nil) {
            throw LauncherError(message: "Temporary server settings require a saved loadout and a server connection.")
        }
        if connection?.values.isEmpty == false && configStudio?.connectionIsolated != true {
            throw LauncherError(message: "Temporary server settings require an isolated connection write owner.")
        }
        arguments = try EngineCommandLine.validate(args + forwarded + (try connection?.arguments(for: link) ?? []))
        // Links are delivered as Apple events after startup, never serialized into argv (both engines).
        self.link = link
    }
}

enum PrivateFile {
    static func write(_ data: Data, to target: URL) throws {
        try FileManager.default.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let temporary = target.appendingPathExtension(UUID().uuidString)
        let fd = open(temporary.path, O_CREAT | O_EXCL | O_WRONLY, 0o600)
        guard fd >= 0 else { throw LauncherError(message: "Cannot create private settings file.") }
        var succeeded = false
        defer { if !succeeded { unlink(temporary.path) } }
        let written = data.withUnsafeBytes { raw -> Bool in
            var count = 0
            while count < raw.count {
                let n = Darwin.write(fd, raw.baseAddress!.advanced(by: count), raw.count - count)
                if n < 0 && errno == EINTR { continue }
                if n <= 0 { return false }; count += n
            }
            return true
        }
        let synced = fsync(fd) == 0
        let closed = close(fd) == 0
        guard written && synced && closed && rename(temporary.path, target.path) == 0 else { throw LauncherError(message: "Cannot save private settings file.") }
        succeeded = true
    }
}
enum KeyStore {
    static func normalize(_ key: String) -> String { key.replacingOccurrences(of: "-", with: "").replacingOccurrences(of: " ", with: "").uppercased() }
    static func valid(_ key: String) -> Bool { LauncherKeyValid(normalize(key)) }
    static func hasValidKey(at path: URL) -> Bool {
        guard let text = try? String(contentsOf: path, encoding: .utf8) else { return false }
        let found = text.components(separatedBy: "\n").contains { $0.hasPrefix("codkey=") && valid(String($0.dropFirst(7))) }
        if found { chmod(path.path, 0o600) }
        return found
    }
    static func save(_ key: String, at path: URL) throws {
        guard valid(key) else { throw LauncherError(message: "Enter all 20 characters from your licensed copy. The checksum does not match.") }
        let previous = (try? String(contentsOf: path, encoding: .utf8)) ?? ""
        var lines = previous.components(separatedBy: "\n").filter { !$0.isEmpty && !$0.hasPrefix("codkey=") }
        lines.append("codkey=" + normalize(key))
        try PrivateFile.write(Data((lines.joined(separator: "\n") + "\n").utf8), to: path)
    }
}

extension GameEngine {
    enum JoinCheck: Equatable { case go, needsCoD2x, lookUp }
    /// Before a game starts on a server. Original 1.3 joins only stock 1.3 servers; a server not seen
    /// yet is looked up once, and if it doesn't answer the game tries (and reports a version mismatch itself).
    func check(server: GameServer?, lookedUp: Bool) -> JoinCheck {
        guard self == .original else { return .go }
        if let server { return canJoin(server) ? .go : .needsCoD2x }
        return lookedUp ? .go : .lookUp
    }
}

/// A CoD2x server joined while Original 1.3 is chosen; the launcher asks whether to turn CoD2x on and join.
struct EngineRequest: Identifiable, Sendable {
    let link: LaunchLink
    var id: String { link.address }
    var title: String { "This server needs CoD2x" }
    var message: String {
        "This server runs CoD2x 1.4. Original 1.3 can only join Stock 1.3 servers. CoD2x stays on for later games; you can change this in Settings."
    }
}

/// The reports either engine writes into the app home (its fs_homepath) when it crashes
/// (`cod2_crash_<pid>.txt`) or its main thread stalls (`cod2_freeze_<pid>.txt`).
enum CrashReports {
    /// The newest report written since `since`.
    static func latest(in home: URL, since: Date) -> URL? {
        func modified(_ url: URL) -> Date { (try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast }
        return ((try? FileManager.default.contentsOfDirectory(at: home, includingPropertiesForKeys: [.contentModificationDateKey])) ?? [])
            .filter { url in
                // The original engine writes cod2_crash_<pid>.txt; CoD2x builds use CoD2x's names, cod2x_crash.<time>.<pid>.log.
                let name = url.lastPathComponent
                let original = ["cod2_crash_", "cod2_freeze_"].contains(where: name.hasPrefix) && url.pathExtension == "txt"
                let cod2x = ["cod2x_crash.", "cod2x_freeze."].contains(where: name.hasPrefix) && url.pathExtension == "log"
                return (original || cod2x) && modified(url) >= since
            }
            .max { modified($0) < modified($1) }
    }
    /// Every report written since `since`, oldest first.
    static func all(in home: URL, since: Date) -> [URL] {
        func modified(_ url: URL) -> Date { (try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast }
        let all = ((try? FileManager.default.contentsOfDirectory(at: home, includingPropertiesForKeys: [.contentModificationDateKey])) ?? [])
            .filter { url in
                let name = url.lastPathComponent
                return ((["cod2_crash_", "cod2_freeze_"].contains(where: name.hasPrefix) && url.pathExtension == "txt")
                    || (["cod2x_crash.", "cod2x_freeze."].contains(where: name.hasPrefix) && url.pathExtension == "log")) && modified(url) >= since
            }
        return all.sorted { (modified($0), $0.lastPathComponent) < (modified($1), $1.lastPathComponent) }
    }
}

/// A Sentry project's address (its DSN), a value the release build writes into the launcher's
/// Info.plist as `CoD2SentryDSN`. HTTPS only, except a stand-in on this Mac for tests.
struct SentryDSN: Equatable, Sendable {
    let text: String
    let publicKey: String
    let secretKey: String?
    let envelopeURL: URL
    init?(_ text: String) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let parts = URLComponents(string: trimmed), let scheme = parts.scheme?.lowercased(), let host = parts.host?.lowercased(), !host.isEmpty,
              let key = parts.user, key.range(of: "\\A[A-Za-z0-9]{1,64}\\z", options: .regularExpression) != nil,
              parts.query == nil, parts.fragment == nil else { return nil }
        let loopback = ["127.0.0.1", "localhost", "::1"].contains(host)
        guard scheme == "https" || (scheme == "http" && loopback) else { return nil }
        var path = parts.path.split(separator: "/").map(String.init)
        guard let project = path.popLast(), project.range(of: "\\A[0-9]{1,20}\\z", options: .regularExpression) != nil else { return nil }
        var endpoint = URLComponents()
        endpoint.scheme = scheme; endpoint.host = parts.host; endpoint.port = parts.port
        endpoint.path = "/" + (path + ["api", project, "envelope"]).joined(separator: "/") + "/"
        guard let url = endpoint.url else { return nil }
        self.text = trimmed; publicKey = key; envelopeURL = url
        secretKey = parts.password.flatMap { $0.isEmpty ? nil : $0 }
    }
    /// The X-Sentry-Auth header (Sentry's transport authentication, protocol version 7).
    func authorization(client: String) -> String {
        "Sentry sentry_version=7, sentry_client=\(client), sentry_key=\(publicKey)" + (secretKey.map { ", sentry_secret=" + $0 } ?? "")
    }
}

/// Crash reports leave the Mac only when the player turns sending on in Settings › Privacy.
/// A fault inside CoD2x's own code is CoD2x's: the CoD2x engine writes `<log>.pending.json`
/// beside its log and, only when COD2_SILICON_COD2X_REPORTS is 1, sends it to the CoD2x project
/// at its next start (cod2x_report.c). Every other report is ours: the launcher sends it once to the
/// project's Sentry, shortened and checked first, and never to CoD2x.
enum CrashReporting {
    static let consentVariable = "COD2_SILICON_COD2X_REPORTS"
    static let infoKey = "CoD2SentryDSN"
    static let sizeLimit = 1 << 20
    static let ledgerName = "crash-reports-sent.json"

    /// The engine's switch for CoD2x's own reports: on only for a CoD2x game the player agreed for.
    static func applyConsent(to environment: inout [String: String], engine: GameEngine, cod2xReports: Bool) {
        if engine == .cod2x && cod2xReports { environment[consentVariable] = "1" } else { environment.removeValue(forKey: consentVariable) }
    }

    enum Owner: Equatable, Sendable { case cod2x, ours }
    /// A CoD2x log with its pending report beside it belongs to CoD2x; everything else is ours.
    static func owner(of log: URL) -> Owner {
        let pending = log.deletingLastPathComponent().appendingPathComponent(log.lastPathComponent + ".pending.json")
        return log.lastPathComponent.hasPrefix("cod2x_") && FileManager.default.fileExists(atPath: pending.path) ? .cod2x : .ours
    }

    enum Refusal: Error, Equatable, Sendable { case unreadable, tooLarge, key, offline, refused }
    enum Status: Equatable, Sendable {
        case sent, notSent(Refusal), cod2x
        var message: String {
            switch self {
            case .sent: "Report sent."
            case .cod2x: "The CoD2x project gets this report the next time you play CoD2x."
            case .notSent(let refusal):
                "Report not sent: " + {
                    switch refusal {
                    case .unreadable: "it couldn’t be read."
                    case .tooLarge: "it was too large."
                    case .key: "it might contain your CD key, so it stayed on this Mac."
                    case .offline: "couldn’t reach the report service. Check your connection."
                    case .refused: "the report service didn’t accept it."
                    }
                }()
            }
        }
    }
    static let sending = "Sending report…"

    /// What goes with a report besides the log.
    struct Facts: Sendable {
        var release: String
        var macOS: String
        var model: String
        static func current(release: String) -> Facts {
            let os = ProcessInfo.processInfo.operatingSystemVersion
            var size = 0
            var model = "Mac"
            if sysctlbyname("hw.model", nil, &size, nil, 0) == 0, size > 1 {
                var bytes = [CChar](repeating: 0, count: size)
                if sysctlbyname("hw.model", &bytes, &size, nil, 0) == 0 { model = String(decoding: bytes.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) }
            }
            return Facts(release: release, macOS: "\(os.majorVersion).\(os.minorVersion).\(os.patchVersion)", model: model)
        }
    }

    /// The report as it may leave the Mac: home folder names become `~`, lines that carry the game's
    /// command line are dropped, and anything that looks like a CD key stops it. The preferences
    /// file that holds the key is never read; a key is recognised by its checksum.
    static func redact(_ text: String, home: String = NSHomeDirectory()) throws(Refusal) -> String {
        guard !containsKey(text) else { throw .key }
        var result = text
        if !home.isEmpty && home != "/" { result = result.replacingOccurrences(of: home, with: "~") }
        result = result.replacingOccurrences(of: "/Users/[^/\\s\"'<>:]+", with: "~", options: [.regularExpression, .caseInsensitive])
        let commandKey = try? NSRegularExpression(pattern: "^\\s*(command|command ?line|cmdline|arguments|argv|args)\\s*[:=]", options: .caseInsensitive)
        let commandText = try? NSRegularExpression(pattern: "\\+(set|seta|exec|connect|password|demo)\\b", options: .caseInsensitive)
        result = result.components(separatedBy: "\n").filter { line in
            let range = NSRange(line.startIndex..., in: line)
            return commandKey?.firstMatch(in: line, range: range) == nil && commandText?.firstMatch(in: line, range: range) == nil
        }.joined(separator: "\n")
        guard !containsKey(result) else { throw .key }
        return result
    }
    /// Twenty key characters, together or in the game's groups of four, that pass the game's key checksum.
    static func containsKey(_ text: String) -> Bool {
        if text.range(of: "codkey", options: .caseInsensitive) != nil { return true }
        guard let pattern = try? NSRegularExpression(pattern: "(?<![A-Za-z0-9])(?=((?:[A-Za-z0-9]{4}[- ]?){4}[A-Fa-f0-9]{4})(?![A-Za-z0-9]))") else { return true }
        return pattern.matches(in: text, range: NSRange(text.startIndex..., in: text)).contains { match in
            Range(match.range(at: 1), in: text).map { KeyStore.valid(String(text[$0])) } ?? false
        }
    }
    /// The report's first reason line: `crash :` in the original engine's report, `Reason:` or `Signal:` in CoD2x's.
    static func reason(_ text: String, name: String) -> String {
        for line in text.components(separatedBy: "\n") {
            let parts = line.split(separator: ":", maxSplits: 1).map { $0.trimmingCharacters(in: .whitespaces) }
            if parts.count == 2, ["crash", "reason", "signal"].contains(parts[0].lowercased()), !parts[1].isEmpty { return String(parts[1].prefix(200)) }
        }
        return name.contains("freeze") ? "The game stopped responding" : "The game crashed"
    }

    /// One Sentry envelope: a fatal event and the shortened log as its attachment.
    static func envelope(log name: String, text: String, occurred: Date, facts: Facts, dsn: SentryDSN, eventID: String, sentAt: Date) -> Data {
        let engine = name.hasPrefix("cod2x_") ? "cod2x" : "original"
        let event: [String: Any] = [
            "event_id": eventID, "timestamp": Int(occurred.timeIntervalSince1970), "platform": "native", "level": "fatal",
            "logentry": ["formatted": reason(text, name: name)], "release": facts.release,
            "tags": ["engine": engine, "macos": facts.macOS, "mac_model": facts.model],
            "contexts": ["os": ["type": "os", "name": "macOS", "version": facts.macOS], "device": ["type": "device", "model": facts.model]],
            // No user, and Sentry is told never to take the address the report came from.
            "sdk": ["name": "cod2-silicon.launcher", "version": facts.release, "settings": ["infer_ip": "never"]],
        ]
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime]
        func json(_ value: [String: Any]) -> Data {
            (try? JSONSerialization.data(withJSONObject: value, options: [.sortedKeys, .withoutEscapingSlashes])) ?? Data("{}".utf8)
        }
        let body = json(event), attachment = Data(text.utf8)
        var envelope = json(["event_id": eventID, "dsn": dsn.text, "sent_at": formatter.string(from: sentAt)])
        envelope += Data("\n".utf8) + json(["type": "event", "length": body.count]) + Data("\n".utf8) + body + Data("\n".utf8)
        envelope += json(["type": "attachment", "length": attachment.count, "filename": name, "content_type": "text/plain"]) + Data("\n".utf8) + attachment + Data("\n".utf8)
        return envelope
    }
    static func request(_ envelope: Data, dsn: SentryDSN, release: String) -> URLRequest {
        var request = URLRequest(url: dsn.envelopeURL)
        request.httpMethod = "POST"; request.httpBody = envelope; request.timeoutInterval = 15
        request.setValue("application/x-sentry-envelope", forHTTPHeaderField: "Content-Type")
        request.setValue(dsn.authorization(client: "cod2-silicon-launcher/" + release), forHTTPHeaderField: "X-Sentry-Auth")
        request.setValue("CoD2-Silicon-Launcher", forHTTPHeaderField: "User-Agent")
        request.setValue("en", forHTTPHeaderField: "Accept-Language")  // not the Mac's own languages
        return request
    }

    /// Reports of ours already sent, by the SHA-256 of the log, so none is ever sent twice.
    static func sentDigests(home: URL) -> Set<String> {
        guard let data = try? Data(contentsOf: home.appendingPathComponent(ledgerName)), let list = try? JSONDecoder().decode([String].self, from: data) else { return [] }
        return Set(list)
    }
    private static func recordSent(_ digest: String, home: URL) {
        let url = home.appendingPathComponent(ledgerName)
        var list = (try? JSONDecoder().decode([String].self, from: Data(contentsOf: url))) ?? []
        list.append(digest)
        if let data = try? JSONEncoder().encode(Array(list.suffix(500))) { try? PrivateFile.write(data, to: url) }
    }

    /// After a game: CoD2x's own reports are left to the CoD2x engine; ours go to Sentry once each, and
    /// only with sending on and a DSN in this build. nil when there is nothing to say about sending.
    /// `owners`, when given, is the classification made as the game ended.
    static func report(logs: [URL], owners: [Owner]? = nil, home: URL, sendOurs: Bool, cod2xReports: Bool, dsn: SentryDSN?, facts: Facts,
                       session: URLSession = URLSession(configuration: .ephemeral)) async -> Status? {
        defer { session.finishTasksAndInvalidate() }
        let owners = owners ?? logs.map(owner)
        let ours = zip(logs, owners).filter { $0.1 == .ours }.map(\.0)
        guard sendOurs, let dsn, !ours.isEmpty else { return ours.count < logs.count && cod2xReports ? .cod2x : nil }
        var status: Status?
        for log in ours {
            let result = await send(log, home: home, dsn: dsn, facts: facts, session: session)
            if status == nil || status == .sent { status = result }
        }
        return status
    }
    private static func send(_ log: URL, home: URL, dsn: SentryDSN, facts: Facts, session: URLSession) async -> Status? {
        guard let values = try? log.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey, .contentModificationDateKey]),
              values.isRegularFile == true, values.isSymbolicLink != true, let raw = try? Data(contentsOf: log) else { return .notSent(.unreadable) }
        guard raw.count <= sizeLimit else { return .notSent(.tooLarge) }
        let digest = CoD2xFiles.sha256(raw)
        if sentDigests(home: home).contains(digest) { return nil }
        let text: String
        do { text = try redact(String(decoding: raw, as: UTF8.self)) } catch { return .notSent(error) }
        let eventID = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
        let body = envelope(log: log.lastPathComponent, text: text, occurred: values.contentModificationDate ?? Date(), facts: facts, dsn: dsn, eventID: eventID, sentAt: Date())
        let response: URLResponse
        do { (_, response) = try await session.data(for: request(body, dsn: dsn, release: facts.release)) } catch { return .notSent(.offline) }
        guard let code = (response as? HTTPURLResponse)?.statusCode, (200..<300).contains(code) else { return .notSent(.refused) }
        recordSent(digest, home: home)
        return .sent
    }
}

/// CoD2x's own menus and icons: the archive `iw_CoD2x_01.iwd` that CoD2x carries inside its Windows
/// `mss32.dll`. CoD2x has no licence, so the app ships neither. Only when the player asks, the launcher
/// downloads the pinned official release, or reads the `mss32.dll` of a Windows game folder with CoD2x,
/// and keeps the archive alone in `<app home>/cod2x`. Before each game it goes into `<app home>/main`
/// (the engines' fs_homepath) for CoD2x and is taken away for the original 1.3. The game data folder
/// is only ever read.
enum CoD2xFiles {
    /// The one release this build uses, pinned by size and SHA-256 next to its version. Never "latest".
    static let version = "1.4.6.8"
    static let releaseURL = URL(string: "https://github.com/callofduty2x/CoD2x/releases/download/v1.4.6.8/CoD2x_1.4.6.8_windows.zip")!
    static let releaseSize = 2_577_745
    static let releaseSHA256 = "c33f52037e67eaa948746a93967d27d8c70ab58623abab48d07803600a6a8555"
    /// The archive inside that release's mss32.dll, which tells it apart from a copy of another version.
    static let archiveSHA256 = "fcb20f972cea3ed57eb2479976626fbe34bc1a58cea2d626135e379b51c2fa16"
    static let archiveName = "iw_CoD2x_01.iwd"
    /// The engine's limits (cod2x_features.c): the mss32.dll it reads, and the archive and its contents.
    static let dllLimit = 256 * 1024 * 1024
    static let archiveLimit = 64 * 1024 * 1024

    enum Message {
        static let notInstalled = "Not installed"
        static let installed = "Installed · \(CoD2xFiles.version)"
        static let fromFolder = "Installed · from your CoD2x folder"
        static let gameFolder = "Found in your game folder"
        static let downloading = "Downloading…"
        static let copying = "Copying…"
    }
    enum Failure: LocalizedError, Equatable {
        case download, mismatch, noCoD2x, unreadable, save, remove
        var errorDescription: String? {
            switch self {
            case .download: "Couldn’t download. Check your connection and try again."
            case .mismatch: "The download didn’t match CoD2x \(CoD2xFiles.version), so it wasn’t used."
            case .noCoD2x: "This folder doesn’t have CoD2x installed."
            case .unreadable: "Couldn’t read that folder."
            case .save: "Couldn’t save the files on this Mac."
            case .remove: "Couldn’t remove the files."
            }
        }
    }
    static func message(_ error: Error) -> String { ((error as? Failure) ?? .save).errorDescription ?? "" }

    static func stored(home: URL) -> URL { home.appendingPathComponent("cod2x").appendingPathComponent(archiveName) }
    static func placed(home: URL) -> URL { home.appendingPathComponent("main").appendingPathComponent(archiveName) }
    /// A Windows install with CoD2x already has the archive in the game data folder's main.
    static func inGameFolder(dataPath: String) -> Bool {
        !dataPath.isEmpty && FileManager.default.fileExists(atPath: URL(fileURLWithPath: dataPath).appendingPathComponent("main/" + archiveName).path)
    }
    /// Settings: whether an archive is kept, and what to say about it.
    static func status(home: URL, dataPath: String) -> (installed: Bool, message: String) {
        if let kept = regularFile(stored(home: home)) { return (true, sha256(kept) == archiveSHA256 ? Message.installed : Message.fromFolder) }
        return (false, inGameFolder(dataPath: dataPath) ? Message.gameFolder : Message.notInstalled)
    }

    /// The release, fetched only when the player asks: a plain request in a private session (no cookies,
    /// cache or headers of ours), its size checked before it is read. The temporary file is deleted.
    static func download() async throws -> Data {
        let session = URLSession(configuration: .ephemeral)
        defer { session.finishTasksAndInvalidate() }
        let result: (URL, URLResponse)
        do { result = try await session.download(from: releaseURL) } catch { throw Failure.download }
        defer { try? FileManager.default.removeItem(at: result.0) }
        guard (result.1 as? HTTPURLResponse)?.statusCode == 200 else { throw Failure.download }
        guard (try? result.0.resourceValues(forKeys: [.fileSizeKey]).fileSize) == releaseSize else { throw Failure.mismatch }
        do { return try Data(contentsOf: result.0) } catch { throw Failure.download }
    }
    /// Keeps the archive from the release. The release and its mss32.dll stay in memory only.
    static func install(release zip: Data, home: URL, size: Int = releaseSize, sha256 expected: String = releaseSHA256) throws {
        let archive: Data
        do { archive = try Self.archive(fromDLL: try dll(fromRelease: zip, size: size, sha256: expected)) } catch { throw Failure.mismatch }
        try keep(archive, home: home)
    }
    /// Reads (never changes) a Windows game folder's mss32.dll and keeps its CoD2x archive.
    static func install(folder: URL, home: URL) throws {
        let dll = folder.appendingPathComponent("mss32.dll").resolvingSymlinksInPath()
        guard let values = try? dll.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]), values.isRegularFile == true,
              (values.fileSize ?? 0) <= dllLimit else { throw Failure.noCoD2x }
        let data: Data
        do { data = try Data(contentsOf: dll) } catch { throw Failure.unreadable }
        try keep(try archive(fromDLL: data), home: home)
    }
    private static func keep(_ archive: Data, home: URL) throws {
        do { try PrivateFile.write(archive, to: stored(home: home)) } catch { throw Failure.save }
    }
    /// Removes the kept archive, and the copy in the app home's main when it is that archive.
    static func remove(home: URL) throws {
        let kept = stored(home: home), target = placed(home: home)
        if let data = regularFile(kept), regularFile(target) == data, (try? FileManager.default.removeItem(at: target)) == nil { throw Failure.remove }
        if FileManager.default.fileExists(atPath: kept.path), (try? FileManager.default.removeItem(at: kept)) == nil { throw Failure.remove }
    }

    enum Placement: Equatable { case gameFolder, placed, inPlace, removed, none, failed }
    /// Before a game: CoD2x finds the kept archive in `<home>/main`, the original 1.3 doesn't. A game data
    /// folder with CoD2x's own copy keeps it in charge, so only the app's copy is taken away (fs_homepath
    /// comes after fs_basepath in the search order, so it would win). Only a copy identical to the kept
    /// archive is ever removed, nothing is written outside the app home, and nothing stops a game.
    static func prepare(for engine: GameEngine, home: URL, dataPath: String) -> Placement {
        let main = home.appendingPathComponent("main").resolvingSymlinksInPath().standardizedFileURL
        guard !dataPath.isEmpty,
              main.path != URL(fileURLWithPath: dataPath).appendingPathComponent("main").resolvingSymlinksInPath().standardizedFileURL.path else { return .none }
        let kept = regularFile(stored(home: home)), target = placed(home: home), current = regularFile(target)
        let ownCopy = inGameFolder(dataPath: dataPath)
        if engine == .original || ownCopy {
            guard let kept, current == kept else { return ownCopy ? .gameFolder : .none }
            guard (try? FileManager.default.removeItem(at: target)) != nil else { return .failed }
            return ownCopy ? .gameFolder : .removed
        }
        guard let kept else { return .none }
        if current == kept { return .inPlace }
        let temporary = main.appendingPathComponent(".\(archiveName).\(UUID().uuidString)")
        do {
            try FileManager.default.createDirectory(at: main, withIntermediateDirectories: true)
            try FileManager.default.copyItem(at: stored(home: home), to: temporary)  // a clone on APFS
            guard rename(temporary.path, target.path) == 0 else { throw Failure.save }
            return .placed
        } catch {
            try? FileManager.default.removeItem(at: temporary)
            return .failed
        }
    }

    /// The release's mss32.dll, once the release matches its pinned size and SHA-256.
    static func dll(fromRelease zip: Data, size: Int = releaseSize, sha256 expected: String = releaseSHA256) throws -> Data {
        guard zip.count == size, sha256(zip) == expected else { throw Failure.mismatch }
        let b = [UInt8](zip)
        var eocd = b.count - 22
        while eocd >= max(0, b.count - 22 - 65_535) && le32(b, eocd) != 0x06054b50 { eocd -= 1 }
        guard eocd >= 0, eocd >= b.count - 22 - 65_535 else { throw Failure.mismatch }
        let count = le16(b, eocd + 10), directory = le32(b, eocd + 16)
        guard directory + le32(b, eocd + 12) <= eocd else { throw Failure.mismatch }
        var position = directory
        var entry: (method: Int, flags: Int, compressed: Int, expanded: Int, offset: Int)?
        for _ in 0..<count {
            guard position + 46 <= eocd, le32(b, position) == 0x02014b50 else { throw Failure.mismatch }
            let nameLength = le16(b, position + 28)
            guard position + 46 + nameLength <= eocd else { throw Failure.mismatch }
            let name = String(decoding: b[(position + 46)..<(position + 46 + nameLength)], as: UTF8.self)
            if name.split(whereSeparator: { $0 == "/" || $0 == "\\" }).last?.lowercased() == "mss32.dll" {
                guard entry == nil else { throw Failure.mismatch }
                entry = (le16(b, position + 10), le16(b, position + 8), le32(b, position + 20), le32(b, position + 24), le32(b, position + 42))
            }
            position += 46 + nameLength + le16(b, position + 30) + le16(b, position + 32)
        }
        guard let entry, entry.flags & 1 == 0, entry.expanded <= dllLimit, entry.offset + 30 <= directory,
              le32(b, entry.offset) == 0x04034b50 else { throw Failure.mismatch }
        let start = entry.offset + 30 + le16(b, entry.offset + 26) + le16(b, entry.offset + 28)
        guard start + entry.compressed <= directory else { throw Failure.mismatch }
        let payload = Data(b[start..<(start + entry.compressed)])
        let dll: Data
        switch entry.method {
        case 0: dll = payload
        case 8:  // raw DEFLATE, which is what the Compression framework calls zlib
            guard let expanded = try? (payload as NSData).decompressed(using: .zlib) as Data else { throw Failure.mismatch }
            dll = expanded
        default: throw Failure.mismatch
        }
        guard dll.count == entry.expanded else { throw Failure.mismatch }
        return dll
    }

    /// The engine's rule (Cod2x_IwdFromDll in cod2x_features.c): a Windows executable holding exactly one
    /// ZIP, found from its end-of-central-directory record, whose directory names CoD2x's main menu and
    /// radar arrow and expands to 64 MB at most. A stock Miles mss32.dll holds none.
    static func archive(fromDLL data: Data) throws -> Data {
        let b = [UInt8](data)
        guard b.count >= 64, b.count <= dllLimit, b[0] == 0x4d, b[1] == 0x5a, le32(b, 60) <= b.count - 4,
              b[le32(b, 60)..<(le32(b, 60) + 4)].elementsEqual([0x50, 0x45, 0, 0]) else { throw Failure.noCoD2x }
        var found: Range<Int>?
        var position = 0
        while position + 22 <= b.count {
            defer { position += 1 }
            guard b[position] == 0x50, le32(b, position) == 0x06054b50 else { continue }
            let directorySize = le32(b, position + 12), directory = le32(b, position + 16), end = position + 22 + le16(b, position + 20)
            guard le16(b, position + 4) == 0, le16(b, position + 6) == 0, le16(b, position + 8) == le16(b, position + 10),
                  directorySize + directory <= position, end <= b.count else { continue }
            let begin = position - directorySize - directory
            guard le32(b, begin) == 0x04034b50, end - begin <= archiveLimit,
                  isArchive(b, begin: begin, end: end, directory: directory, directorySize: directorySize, count: le16(b, position + 10)) else { continue }
            guard found == nil else { throw Failure.noCoD2x }  // two: not a CoD2x release
            found = begin..<end
        }
        guard let found else { throw Failure.noCoD2x }
        return Data(b[found])
    }
    /// Cod2x_IwdArchive: names compared lowercased with forward slashes, up to a NUL as C compares them.
    private static func isArchive(_ b: [UInt8], begin: Int, end: Int, directory: Int, directorySize: Int, count: Int) -> Bool {
        let last = directory + directorySize
        var position = directory, expanded = 0, menu = false, arrow = false
        for _ in 0..<count {
            guard position + 46 <= last, last <= end - begin, le32(b, begin + position) == 0x02014b50 else { return false }
            expanded += le32(b, begin + position + 24)
            let nameLength = le16(b, begin + position + 28)
            guard position + 46 + nameLength <= last else { return false }
            if nameLength < 256 {
                let bytes = b[(begin + position + 46)..<(begin + position + 46 + nameLength)].prefix { $0 != 0 }
                let name = String(decoding: bytes.map { (c: UInt8) -> UInt8 in c == 0x5c ? 0x2f : (c >= 0x41 && c <= 0x5a ? c + 32 : c) }, as: UTF8.self)
                menu = menu || name == "ui_mp/main.menu"
                arrow = arrow || name == "materials/radar_player_arrow"
            }
            position += 46 + nameLength + le16(b, begin + position + 30) + le16(b, begin + position + 32)
        }
        return menu && arrow && expanded <= archiveLimit
    }
    private static func le16(_ b: [UInt8], _ i: Int) -> Int { Int(b[i]) | Int(b[i + 1]) << 8 }
    private static func le32(_ b: [UInt8], _ i: Int) -> Int { le16(b, i) | le16(b, i + 2) << 16 }
    static func sha256(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
    private static func regularFile(_ url: URL) -> Data? {
        guard let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]),
              values.isRegularFile == true, values.isSymbolicLink != true, (values.fileSize ?? 0) <= archiveLimit else { return nil }
        return try? Data(contentsOf: url)
    }
}

struct GameSettings: Codable, Sendable, Equatable {
    static let machineDvarNames: Set<String> = ["r_mode", "r_fullscreen", "r_borderless", "r_swapinterval", "r_renderer", "r_renderscale", "r_metalfx", "r_hdr", "com_introplayed", "logfile", "developer", "fs_basepath", "fs_homepath", "fs_game", "fs_cdpath", "fs_copyfiles", "fs_restrict", "loc_language", "dedicated", "com_hunkmegs", "net_port", "cl_matchsummaries"]
    var resolution = "1920x1080"
    var fullscreen = "borderless"
    /// com_maxfps: 0 (no cap) to 1000, the engine dvar's range.
    var fps = 250
    var vsync = false
    var rawMouse = true
    var sensitivity = 5.0
    var dpi = 800.0
    var volume = 0.8
    var anisotropy = 8
    var advanced = ""
    var matchSummariesEnabled = true
    /// CoD2x unless the player chose the original 1.3 in Settings.
    var engine = GameEngine.cod2x
    /// Settings › Privacy: crash reports to this project's Sentry, and CoD2x's own to CoD2x. Off until turned on.
    var sendCrashReports = false
    var sendCoD2xReports = false
    static let resolutions = ["1280x720", "1920x1080", "2560x1440", "3008x1692", "3840x2160", "5120x2880", "6016x3384"]
    static let anisotropyLevels = [2, 4, 8, 16]
    var cm360: Double? { MouseMath.centimetresPer360(dpi: dpi, sensitivity: sensitivity) }
    init() {}
    // Settings saved by older launchers lack newer keys; keep their values and default the rest.
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        let d = GameSettings()
        resolution = try c.decodeIfPresent(String.self, forKey: .resolution) ?? d.resolution
        fullscreen = try c.decodeIfPresent(String.self, forKey: .fullscreen) ?? d.fullscreen
        fps = try c.decodeIfPresent(Int.self, forKey: .fps) ?? d.fps
        vsync = try c.decodeIfPresent(Bool.self, forKey: .vsync) ?? d.vsync
        rawMouse = try c.decodeIfPresent(Bool.self, forKey: .rawMouse) ?? d.rawMouse
        sensitivity = try c.decodeIfPresent(Double.self, forKey: .sensitivity) ?? d.sensitivity
        dpi = try c.decodeIfPresent(Double.self, forKey: .dpi) ?? d.dpi
        volume = try c.decodeIfPresent(Double.self, forKey: .volume) ?? d.volume
        anisotropy = try c.decodeIfPresent(Int.self, forKey: .anisotropy) ?? d.anisotropy
        advanced = try c.decodeIfPresent(String.self, forKey: .advanced) ?? d.advanced
        matchSummariesEnabled = try c.decodeIfPresent(Bool.self, forKey: .matchSummariesEnabled) ?? d.matchSummariesEnabled
        // An engine this version doesn't know (from a newer launcher) falls back without losing the rest.
        engine = (try? c.decodeIfPresent(GameEngine.self, forKey: .engine)) ?? d.engine
        sendCrashReports = (try? c.decodeIfPresent(Bool.self, forKey: .sendCrashReports)) ?? d.sendCrashReports
        sendCoD2xReports = (try? c.decodeIfPresent(Bool.self, forKey: .sendCoD2xReports)) ?? d.sendCoD2xReports
    }
    /// One advanced line as (name, value): `dvar value` as typed in the console, or `dvar=value`.
    static func advancedPair(_ line: String) -> (String, String)? {
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        let equals = trimmed.firstIndex(of: "="), space = trimmed.firstIndex(where: { $0 == " " || $0 == "\t" })
        guard let split = [equals, space].compactMap({ $0 }).min() else { return nil }
        let name = String(trimmed[..<split]).trimmingCharacters(in: .whitespaces)
        var value = String(trimmed[trimmed.index(after: split)...]).trimmingCharacters(in: .whitespaces)
        if split == space, value.hasPrefix("=") { value = String(value.dropFirst()).trimmingCharacters(in: .whitespaces) }
        return (name, value)
    }
    static func safeValue(_ value: String) -> Bool {
        value.utf8.count <= 512 && !value.unicodeScalars.contains { $0.value < 32 || $0.value == 127 || "\";\\+".unicodeScalars.contains($0) }
    }
    func dvars() throws -> [(String, String)] {
        guard Self.resolutions.contains(resolution), ["exclusive", "borderless", "spaces", "windowed"].contains(fullscreen),
              (0...1000).contains(fps), sensitivity.isFinite, (0.01...100).contains(sensitivity), volume.isFinite, (0...1).contains(volume),
              Self.anisotropyLevels.contains(anisotropy) else {
            throw LauncherError(message: "Choose a valid resolution, frame cap (0–1000, where 0 is no cap), sensitivity, volume and filtering level.")
        }
        var pairs = [("r_mode", resolution), ("r_fullscreen", fullscreen == "windowed" ? "0" : "1"),
            ("r_borderless", fullscreen == "borderless" ? "1" : "0"), ("com_maxfps", String(fps)),
            ("r_swapInterval", vsync ? "1" : "0"), ("in_rawmouse", rawMouse ? "1" : "0"), ("m_rinput", rawMouse ? "1" : "0"),
            ("sensitivity", String(sensitivity)), ("snd_volume", String(volume)), ("r_anisotropy", String(anisotropy)), ("m_filter", "0"), ("cl_mouseAccel", "0"),
            ("logfile", "0"), ("developer", "0"), ("com_introPlayed", "1"), ("cl_matchSummaries", matchSummariesEnabled ? "1" : "0")]
        for line in advanced.components(separatedBy: "\n") where !line.trimmingCharacters(in: .whitespaces).isEmpty {
            let pair = Self.advancedPair(line)
            let name = pair?.0 ?? ""
            guard let pair, name.range(of: "^[A-Za-z_][A-Za-z0-9_]{0,63}$", options: .regularExpression) != nil,
                  !["fs_basepath", "fs_homepath", "password", "cl_cdkey", "cdkey", "dpi", "r_renderer", "r_renderscale", "r_metalfx", "r_hdr", "cl_matchsummaries"].contains(name.lowercased()),
                  !name.lowercased().contains("password"), !name.lowercased().contains("codkey") else {
                throw LauncherError(message: "Advanced options use one “dvar value” per line. Paths, credentials and upcoming renderer options are managed separately.")
            }
            let value = pair.1
            guard Self.safeValue(value) else { throw LauncherError(message: "Dvar values cannot contain quotes, separators or control characters.") }
            pairs.removeAll { $0.0 == name }; pairs.append((name, value))
        }
        guard pairs.reduce(0, { $0 + $1.0.utf8.count + $1.1.utf8.count + 12 }) < 2500 else { throw LauncherError(message: "Advanced options exceed the engine's startup limit.") }
        return pairs
    }
    /// What the stock first run wrote from the game's recommended settings (configure_mp.csv)
    /// on this Mac. CoD2x builds never apply recommended settings, so the launcher supplies
    /// these while the loadout does not set them, and the first picture stays the same.
    static let firstRunPicture: [(String, String)] = [("r_glow", "0"), ("r_distortion", "0"), ("r_rendererPreference", "none"),
        ("r_zfeather", "0"), ("sc_enable", "0"), ("r_dlightLimit", "4"), ("fx_sort", "1"), ("r_depthPrepassModels", "0")]
    /// Startup values for what neither these settings nor the loadout set: the frame cap and raw
    /// mouse, which CoD2x registers as 85 and 0, and the stock first-run picture. They are archived
    /// (seta), as the recommended settings were, so the loadout keeps them from then on.
    func loadoutDefaults(migrated: Bool, loadout: (String) -> String?) throws -> [(String, String)] {
        let launched = Set(try launchDvars(migrated: migrated).map { $0.0.lowercased() })
        var values = Self.firstRunPicture
        if migrated {
            let own = Dictionary(try dvars().map { ($0.0.lowercased(), $0.1) }, uniquingKeysWith: { $1 })
            let raw = loadout("in_rawmouse").map { $0 == "0" ? "0" : "1" } ?? own["m_rinput"] ?? "1"
            values = [("com_maxfps", own["com_maxfps"] ?? String(fps)), ("m_rinput", raw)] + values
        }
        return values.filter { !launched.contains($0.0.lowercased()) && loadout($0.0) == nil }
    }
    func config() throws -> String { try dvars().map { "seta \($0.0) \"\($0.1)\"" }.joined(separator: "\n") + "\n" }
    func arguments() throws -> [String] { try dvars().flatMap { ["+set", $0.0, "\"\($0.1)\""] } }
    func launchDvars(migrated: Bool) throws -> [(String, String)] {
        let values = try dvars()
        return migrated ? values.filter { Self.machineDvarNames.contains($0.0.lowercased()) } : values
    }
    func launchConfig(migrated: Bool) throws -> String {
        try launchDvars(migrated: migrated).map { "seta \($0.0) \"\($0.1)\"" }.joined(separator: "\n") + "\n"
    }
}

struct LauncherLibrary: Codable {
    var favorites: Set<String> = []
    var recent: [String] = []
    var cache: [GameServer] = []
    var cachedAt: Date? = nil
    var dispatches: [Dispatch] = []
    var dispatchesCheckedAt: Date? = nil
    init() {}
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        favorites = try c.decodeIfPresent(Set<String>.self, forKey: .favorites) ?? []
        recent = try c.decodeIfPresent([String].self, forKey: .recent) ?? []
        cache = try c.decodeIfPresent([GameServer].self, forKey: .cache) ?? []
        cachedAt = try c.decodeIfPresent(Date.self, forKey: .cachedAt)
        dispatches = Array((try c.decodeIfPresent([Dispatch].self, forKey: .dispatches) ?? []).prefix(5))
        dispatchesCheckedAt = try c.decodeIfPresent(Date.self, forKey: .dispatchesCheckedAt)
    }
    mutating func joined(_ address: String) { recent.removeAll { $0 == address }; recent.insert(address, at: 0); recent = Array(recent.prefix(30)) }
}
