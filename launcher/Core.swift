import Foundation
import Darwin

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
            fields[tokens[i]] = tokens[i + 1]
        }
        var players: [ServerPlayer] = []
        for line in lines.dropFirst(2).prefix(256) {
            guard let quote = line.firstIndex(of: "\""), let end = line.lastIndex(of: "\""), quote < end else { continue }
            let numbers = line[..<quote].split(separator: " ")
            guard numbers.count == 2, let score = Int(numbers[0]), let ping = Int(numbers[1]) else { continue }
            players.append(ServerPlayer(id: players.count, score: score, ping: ping, name: String(line[line.index(after: quote)..<end].prefix(128))))
        }
        return GameServer(address: address, fields: fields, ping: ping, players: players)
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
}

enum EngineCommandLine {
    static func validate(_ arguments: [String]) throws -> [String] {
        let text = arguments.joined(separator: " ")
        guard text.utf8.count < 3900 else { throw LauncherError(message: "Launch arguments exceed the game's byte limit.") }
        guard text.filter({ $0 == "+" }).count <= 31 else { throw LauncherError(message: "Too many startup commands. Reduce the number of advanced dvars before playing.") }
        return arguments
    }
}

struct GameLaunchPlan {
    var arguments: [String]
    var link: LaunchLink?
    init(settings: GameSettings, dataPath: String, homePath: String, link: LaunchLink? = nil, demo: String? = nil, forwarded: [String] = []) throws {
        guard GameSettings.safeValue(dataPath), GameSettings.safeValue(homePath) else { throw LauncherError(message: "Game paths contain unsupported characters.") }
        var args = try settings.arguments()
        args += ["+set", "fs_basepath", "\"\(dataPath)\"", "+set", "fs_homepath", "\"\(homePath)\""]
        if let demo {
            guard demo.range(of: "^[A-Za-z0-9_.-]+$", options: .regularExpression) != nil else { throw LauncherError(message: "Rename this demo using letters, numbers, dots, underscores or hyphens before playback.") }
            args += ["+demo", demo]
        }
        arguments = try EngineCommandLine.validate(args + forwarded)
        // Links are delivered as Apple events after startup, never serialized into argv.
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

struct GameSettings: Codable, Sendable {
    var resolution = "1920x1080"
    var fullscreen = "exclusive"
    var fps = 333
    var vsync = false
    var rawMouse = true
    var sensitivity = 5.0
    var dpi = 800.0
    var volume = 0.8
    var anisotropy = 8
    var advanced = ""
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
              (1...1000).contains(fps), sensitivity.isFinite, (0.01...100).contains(sensitivity), volume.isFinite, (0...1).contains(volume),
              Self.anisotropyLevels.contains(anisotropy) else {
            throw LauncherError(message: "Choose a valid resolution, frame cap (1–1000), sensitivity, volume and filtering level.")
        }
        var pairs = [("r_mode", resolution), ("r_fullscreen", fullscreen == "windowed" ? "0" : "1"),
            ("r_borderless", fullscreen == "borderless" ? "1" : "0"), ("com_maxfps", String(fps)),
            ("r_swapInterval", vsync ? "1" : "0"), ("in_rawmouse", rawMouse ? "1" : "0"),
            ("sensitivity", String(sensitivity)), ("snd_volume", String(volume)), ("r_anisotropy", String(anisotropy)), ("m_filter", "0"), ("cl_mouseAccel", "0"),
            ("logfile", "0"), ("developer", "0"), ("com_introPlayed", "1")]
        for line in advanced.components(separatedBy: "\n") where !line.trimmingCharacters(in: .whitespaces).isEmpty {
            let pair = Self.advancedPair(line)
            let name = pair?.0 ?? ""
            guard let pair, name.range(of: "^[A-Za-z_][A-Za-z0-9_]{0,63}$", options: .regularExpression) != nil,
                  !["fs_basepath", "fs_homepath", "password", "cl_cdkey", "cdkey", "r_renderer", "r_renderscale", "r_metalfx", "r_hdr"].contains(name.lowercased()),
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
    func config() throws -> String { try dvars().map { "seta \($0.0) \"\($0.1)\"" }.joined(separator: "\n") + "\n" }
    func arguments() throws -> [String] { try dvars().flatMap { ["+set", $0.0, "\"\($0.1)\""] } }
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
