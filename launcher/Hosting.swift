import Foundation

// Servers › Hosting: a server the person runs on this Mac, or writes out for another machine. This
// file holds what can be decided without running anything: the saved form, the server.cfg it becomes,
// the launch arguments and the export. The process, downloads and windows are in HostingModel.swift.

struct HostedServer: Codable, Identifiable, Equatable, Sendable {
    /// Who can join: people on the local network (the engine's `dedicated 1`, no heartbeats), or
    /// anyone, with the server listed on the public masters (`dedicated 2`).
    enum Reach: String, Codable, CaseIterable, Sendable {
        case network, listed
        var title: String { self == .network ? "My Network" : "The Internet" }
        var detail: String {
            self == .network ? "Macs and PCs on the same network." : "Listed publicly once your router lets players in."
        }
    }
    enum Rules: String, Codable, CaseIterable, Sendable {
        case open = "public", zpam
        var title: String { self == .open ? "Public" : "Competitive · zPAM" }
        var detail: String { self == .open ? "The game’s own rules, for casual play." : "Match rules, ready-up and strict settings for every player." }
    }
    var id: String
    var name: String
    var engine: GameEngine
    var reach: Reach
    var players: Int
    var joinPassword: String
    var adminPassword: String
    var rules: Rules
    var gameType: String
    var friendlyFire: Bool
    /// zPAM's own name for the format (`pam_mode`), with the game type it belongs to.
    var matchMode: String
    var maps: [String]
    var port: Int
    static let defaultPort = 28960
    static func new(id: String = UUID().uuidString.lowercased(), name: String = "My Server", engine: GameEngine = .cod2x) -> HostedServer {
        HostedServer(id: id, name: name, engine: engine, reach: .network, players: 12, joinPassword: "", adminPassword: Self.password(),
                     rules: .open, gameType: "tdm", friendlyFire: false, matchMode: "sd:comp", maps: ["mp_toujane", "mp_carentan", "mp_burgundy"], port: defaultPort)
    }
    /// Sixteen letters and digits from the system's generator, without the ones that read alike.
    static func password() -> String {
        let alphabet = Array("abcdefghjkmnpqrstuvwxyz23456789")
        var generator = SystemRandomNumberGenerator()
        return String((0..<16).map { _ in alphabet.randomElement(using: &generator)! })
    }
    /// zPAM's version for this server's game: 4.x needs CoD2x, 3.36 is the last for the original 1.3.
    var zpamLine: ZPAMLine { engine == .cod2x ? .four : .three }
}

extension HostedServer {
    /// The ports the game's own local-network list asks (the engine's CL_LocalServers_f): the original
    /// 1.3 looks at four, CoD2x at thirty-two. A server on any other port is joined by its address.
    var listedPorts: ClosedRange<Int> { engine == .cod2x ? 28960...28991 : 28960...28963 }
    var inLocalList: Bool { listedPorts.contains(port) }
}

/// The stock game types, and zPAM's match formats as its server.cfg names them.
enum HostingCatalog {
    static let gameTypes: [(id: String, title: String)] = [("tdm", "Team Deathmatch"), ("dm", "Deathmatch"), ("sd", "Search & Destroy"),
                                                           ("ctf", "Capture the Flag"), ("hq", "Headquarters")]
    static func gameTypeTitle(_ id: String) -> String { gameTypes.first { $0.id == id }?.title ?? id }
    /// `gametype:pam_mode`, in the order the menu shows them.
    static let matchModes: [(id: String, title: String)] = [
        ("sd:comp", "Search & Destroy · match"), ("sd:comp_mr15", "Search & Destroy · first to 16"), ("sd:comp_mr10", "Search & Destroy · first to 11"),
        ("sd:comp_2v2", "Search & Destroy · 2 v 2"), ("sd:comp_rifle", "Search & Destroy · rifles only"), ("sd:comp_lan", "Search & Destroy · LAN"),
        ("tdm:comp", "Team Deathmatch · match"), ("dm:comp", "Deathmatch · match"), ("ctf:comp", "Capture the Flag · match"), ("hq:comp", "Headquarters · match"),
        ("sd:pub", "Search & Destroy · public rules")]
    static func matchModeTitle(_ id: String) -> String { matchModes.first { $0.id == id }?.title ?? id }
    /// The server guide's part about letting players in from the internet.
    static let portGuide = URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/docs/server.md#letting-players-in-from-the-internet")!
    /// The server guide's part about Wi-Fi pauses.
    static let wifiGuide = URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/docs/server.md#wi-fi-players-who-freeze-and-jump")!
    /// zPAM plays its own corrected copies of stock maps, named like `mp_toujane_fix`; this is the stock name.
    static func stockMap(_ name: String) -> String { name.hasSuffix("_fix") ? String(name.dropLast(4)) : name }
    /// The maps both versions ship with.
    static let maps = ["mp_breakout", "mp_brecourt", "mp_burgundy", "mp_carentan", "mp_dawnville", "mp_decoy", "mp_downtown", "mp_farmhouse",
                       "mp_harbor", "mp_leningrad", "mp_matmata", "mp_railyard", "mp_rhine", "mp_toujane", "mp_trainstation"]
}

/// The two lines of zPAM, each with the release asset that carries it.
enum ZPAMLine: String, Codable, Sendable {
    case four, three
    /// `zpam412.zip`, `zpam336.zip`: the digits are the version.
    var assetPattern: String { self == .four ? #"^zpam4\d{2}\.zip$"# : #"^zpam3\d{2}\.zip$"# }
    var game: String { self == .four ? "CoD2x" : "the original 1.3" }
    /// “zpam412.zip” → “4.12”.
    static func version(asset: String) -> String? {
        let digits = asset.filter(\.isNumber)
        guard asset.lowercased().hasPrefix("zpam"), digits.count == 3 else { return nil }
        return "\(digits.prefix(1)).\(digits.dropFirst())"
    }
}

/// One downloadable zPAM, as GitHub's release listing describes it.
struct ZPAMRelease: Equatable, Sendable {
    let version: String
    let asset: String
    let url: URL
    let bytes: Int
    /// Lower-case hex SHA-256 from the release's own `digest`.
    let sha256: String

    static let releasesURL = URL(string: "https://api.github.com/repos/eyza-cod2/zpam3/releases?per_page=30")!
    static let projectURL = URL(string: "https://github.com/eyza-cod2/zpam3")!
    /// The newest release that carries this line's archive with a published checksum. Anything
    /// without a checksum, or served from somewhere other than GitHub's release downloads, is skipped.
    static func newest(_ line: ZPAMLine, in listing: Data) -> ZPAMRelease? {
        guard let releases = try? JSONSerialization.jsonObject(with: listing) as? [[String: Any]] else { return nil }
        for release in releases where release["draft"] as? Bool != true && release["prerelease"] as? Bool != true {
            for asset in release["assets"] as? [[String: Any]] ?? [] {
                guard let name = asset["name"] as? String, name.range(of: line.assetPattern, options: .regularExpression) != nil,
                      let version = ZPAMLine.version(asset: name),
                      let link = (asset["browser_download_url"] as? String).flatMap(URL.init(string:)),
                      link.scheme == "https", link.host == "github.com", link.path.hasPrefix("/eyza-cod2/zpam3/releases/download/"),
                      let bytes = asset["size"] as? Int, (1...1_073_741_824).contains(bytes),
                      let digest = asset["digest"] as? String, digest.hasPrefix("sha256:") else { continue }
                let hex = String(digest.dropFirst(7)).lowercased()
                guard hex.count == 64, hex.allSatisfy({ $0.isHexDigit }) else { continue }
                return ZPAMRelease(version: version, asset: name, url: link, bytes: bytes, sha256: hex)
            }
        }
        return nil
    }
}

enum HostingConfig {
    /// Another port for this server: the first free one the game's local list asks, so friends still
    /// see it there; when none is free, the first free one above its own.
    static func otherPort(for server: HostedServer, free: (Int) -> Bool) -> Int {
        server.listedPorts.first { $0 != server.port && free($0) } ?? (server.port + 1...server.port + 50).first(where: free) ?? server.port + 1
    }
    /// A value as it can stand between quotes in a config: no quotes, semicolons, line breaks or
    /// control characters, and nothing that would start a comment.
    static func safe(_ value: String, limit: Int = 128) -> String {
        var text = String(value.unicodeScalars.filter { $0.value >= 32 && $0.value != 127 && $0 != "\"" && $0 != ";" && $0.value < 256 })
        while text.contains("//") { text = text.replacingOccurrences(of: "//", with: "/") }
        return String(text.trimmingCharacters(in: .whitespaces).prefix(limit))
    }
    /// A password as the game can take it on one line: also without spaces, which end a console word.
    static func password(_ value: String) -> String { safe(value, limit: 32).filter { $0 != " " } }
    static func validMap(_ name: String) -> Bool {
        !name.isEmpty && name.count <= 64 && name.allSatisfy { $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "_") }
    }
    static func maps(_ server: HostedServer) -> [String] {
        let maps = server.maps.filter(validMap)
        return maps.isEmpty ? ["mp_toujane"] : maps
    }
    static func gameType(_ server: HostedServer) -> String {
        let type = server.rules == .zpam ? String(server.matchMode.split(separator: ":").first ?? "sd") : server.gameType
        return HostingCatalog.gameTypes.contains { $0.id == type } ? type : "tdm"
    }
    static func pamMode(_ server: HostedServer) -> String {
        let mode = server.matchMode.split(separator: ":").dropFirst().first.map(String.init) ?? "comp"
        return mode.allSatisfy { $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "_") } && !mode.isEmpty ? mode : "comp"
    }

    /// The lines this server decides, each with the comment that explains it. `passwords` false
    /// writes CHANGE_ME where a password goes, for a file that will be handed to someone else.
    static func lines(_ server: HostedServer, passwords: Bool = true) -> [(name: String, value: String, comment: String)] {
        let type = gameType(server), maps = maps(server)
        let join = passwords ? password(server.joinPassword) : (password(server.joinPassword).isEmpty ? "" : "CHANGE_ME")
        let admin = passwords ? password(server.adminPassword) : "CHANGE_ME"
        var lines: [(String, String, String)] = [
            ("sv_hostname", safe(server.name, limit: 64), "The name players see in the server list"),
            ("sv_maxclients", String(min(64, max(2, server.players))), "How many players can be on the server at once"),
            ("g_password", join, "Password to join; empty for an open server"),
            ("rcon_password", admin, "Admin password, for controlling the server from inside the game; empty turns that off"),
            ("g_gametype", type, "Game type: \(HostingCatalog.gameTypeTitle(type))"),
        ]
        if server.rules == .zpam {
            lines.append(("pam_mode", pamMode(server), "zPAM’s match format: \(HostingCatalog.matchModeTitle(server.matchMode))"))
            lines.append(("sv_wwwDownload", "1", "Players’ games fetch zPAM and its maps over the web"))
            lines.append(("sv_wwwBaseURL", "http://cod2x.me/zpam", "Where they fetch them from; zPAM’s own default"))
        } else {
            lines.append(("scr_friendlyfire", server.friendlyFire ? "1" : "0", "Friendly fire: 1 on, 0 off"))
            lines.append(("sv_pure", "1", "Players must have the same game files as the server"))
            lines.append(("sv_allowDownload", "0", "The server doesn’t send files to players"))
        }
        lines.append(("sv_maprotation", maps.map { server.rules == .zpam ? "map \($0)" : "gametype \(type) map \($0)" }.joined(separator: " "), "Maps, in order"))
        lines.append(("g_log", "games_mp.log", "The game’s own log of joins, kills and rounds"))
        return lines
    }

    /// A complete server.cfg for a server with public rules.
    static func serverConfig(_ server: HostedServer, passwords: Bool = true) -> String {
        var text = "// \(safe(server.name, limit: 64)): written by CoD2 Silicon. Lines starting with // are comments.\n"
        for line in lines(server, passwords: passwords) { text += "// \(line.comment)\nset \(line.name) \"\(line.value)\"\n" }
        // Starting through the rotation, so Next Map goes on to the second map instead of repeating the first.
        text += "// Start on the first map of the rotation\nmap_rotate\n"
        return text
    }

    /// zPAM's own server.cfg with this server's values put on the lines that set them. Everything
    /// else in the template (its rules for public mode, weapons, ready-up) stays as zPAM ships it.
    static func serverConfig(_ server: HostedServer, zpamTemplate: String, passwords: Bool = true) -> String {
        var pending = Dictionary(lines(server, passwords: passwords).map { ($0.name.lowercased(), $0) }, uniquingKeysWith: { first, _ in first })
        var output: [String] = [], startedMap = false
        for raw in zpamTemplate.components(separatedBy: "\n") {
            let line = raw.hasSuffix("\r") ? String(raw.dropLast()) : raw
            let words = line.split(whereSeparator: { $0 == " " || $0 == "\t" })
            if words.count >= 2, ["set", "seta", "sets"].contains(words[0].lowercased()), let mine = pending.removeValue(forKey: words[1].lowercased()) {
                output.append("\(words[0]) \(mine.name) \"\(mine.value)\"  // \(mine.comment)")
            } else if words.count == 2, words[0].lowercased() == "map" {
                // The template ends by starting a map; start on this server's rotation instead.
                if !startedMap { output.append("map_rotate"); startedMap = true }
            } else {
                output.append(line)
            }
        }
        // Anything the template didn't set goes before the map starts, or at the end when it never starts one.
        let extra = lines(server, passwords: passwords).filter { pending[$0.name.lowercased()] != nil }.map { "set \($0.name) \"\($0.value)\"  // \($0.comment)" }
        if !extra.isEmpty {
            if let index = output.lastIndex(of: "map_rotate") { output.insert(contentsOf: extra, at: index) } else { output.append(contentsOf: extra) }
        }
        if !startedMap { output.append("map_rotate") }
        return output.joined(separator: "\n") + (output.last == "" ? "" : "\n")
    }

    /// What the dedicated server is started with. Paths are quoted for the engine's own parser, which
    /// can't take quotes, plus signs or semicolons inside them.
    static func arguments(_ server: HostedServer, dataPath: String, home: String, bind: String = "0.0.0.0") throws -> [String] {
        for path in [dataPath, home] where path.contains(where: { "\"+;\n\r".contains($0) }) || !path.hasPrefix("/") {
            throw LauncherError(message: "The game or server folder has a quote, plus sign or semicolon in its path, which the game can’t read. Move it and try again.")
        }
        // The revision is chosen on the command line only: the engine registers it once, before any config runs.
        return ["+set", "fs_basepath", "\"\(dataPath)\"", "+set", "fs_homepath", "\"\(home)\"", "+set", "net_ip", bind,
                "+set", "net_port", String(server.port), "+set", "dedicated", server.reach == .listed ? "2" : "1"]
            + ["+exec", "server.cfg"]
    }

    // MARK: Export for another machine

    static func fileName(_ server: HostedServer) -> String {
        let base = String(server.name.lowercased().map { $0.isASCII && ($0.isLetter || $0.isNumber) ? $0 : "-" })
            .split(separator: "-").joined(separator: "-")
        return base.isEmpty ? "cod2-server" : String(base.prefix(48))
    }
    /// The files of an export, by name. Game files and zPAM's archives are never among them.
    static func export(_ server: HostedServer, zpamTemplate: String?, zpam: ZPAMRelease?, passwords: Bool, matchSettings: Bool) -> [String: String] {
        var server = server
        if server.rules == .zpam && !matchSettings {
            // The same game type, with the game's own rules.
            let type = gameType(server)
            server.rules = .open; server.gameType = type
        }
        let config = server.rules == .zpam && zpamTemplate != nil ? serverConfig(server, zpamTemplate: zpamTemplate!, passwords: passwords)
                                                                 : serverConfig(server, passwords: passwords)
        let mode = server.reach == .listed ? "2" : "1"
        let options = "+set net_port \(server.port) +set dedicated \(mode) +exec server.cfg"
        let linux = "./cod2_lnxded"
        // The Linux server keeps `dedicated` at 2 whatever it is given (it answers "dedicated is read only"),
        // so it always reports to the public master. Checked on the original 1.3 Linux server.
        let linuxNote = server.reach == .listed ? "" : "# The Linux server lists itself on the public master servers whatever \"dedicated\" says. README.txt has the way round it.\n"
        var files = ["server.cfg": config,
                     "start-linux.sh": "#!/bin/sh\n# Run from the game folder, beside main/. server.cfg goes into main/.\n\(linuxNote)\(linux) \(options)\n",
                     "start-windows.bat": "@echo off\r\nrem Run from the game folder, beside main\\. server.cfg goes into main\\.\r\nCoD2MP_s.exe \(options)\r\n"]
        var readme = """
        \(safe(server.name, limit: 64)): server files written by CoD2 Silicon

        1. Copy server.cfg into the main folder of a Call of Duty 2 install on the server, patched to 1.3\(server.engine == .cod2x ? " with CoD2x installed (https://github.com/callofduty2x/CoD2x)" : "").
        2. Start the server with start-linux.sh or start-windows.bat from the game folder.
        3. Open UDP port \(server.port) to the server.

        Game files are not included: use your own licensed copy.

        """
        if server.engine == .cod2x { readme += "Who can join is decided by the CoD2x on that machine: a CoD2x server takes players on its own version or a newer one.\n\n" }
        if !passwords { readme += "Passwords are not included. Replace every CHANGE_ME in server.cfg before starting.\n\n" }
        if server.rules == .zpam {
            readme += "This server uses zPAM\(zpam.map { " \($0.version)" } ?? ""), which is not included. Download \(zpam?.asset ?? "its archive") from \(ZPAMRelease.projectURL.absoluteString)/releases and copy its .iwd files into main/ beside server.cfg.\n"
                + "A zPAM server asks CoD2x’s update service for newer versions, and players’ games fetch zPAM’s maps from cod2x.me.\n\n"
        }
        if server.reach == .listed {
            readme += "The server lists itself on the public master servers. On Windows, change dedicated to 1 in start-windows.bat to keep it unlisted. A Linux server always lists itself: see below.\n\n"
        } else {
            readme += "On Windows this server stays unlisted: nothing is announced.\n\n"
        }
        readme += """
        On Linux the game's server program ignores the dedicated setting and always reports to the public master server, so the server shows in everyone's server list. To keep a Linux server unlisted, add this line to /etc/hosts on that machine before you start it:

        127.0.0.1 cod2master.activision.com

        \(server.engine == .cod2x ? "A CoD2x server also reports to CoD2x's own master, master.cod2x.me.\n" : "")
        """
        files["README.txt"] = readme
        return files
    }
}

/// What the engine's `getstatus` answer says about a hosted server.
struct HostedStatus: Equatable, Sendable {
    struct Player: Equatable, Sendable, Identifiable { var id: Int; var name: String; var score: Int; var ping: Int }
    var map: String
    var gameType: String
    var maxPlayers: Int
    var players: [Player]
    /// zPAM's own line about the match (“Round 0 | MR15 Ready-up”, or “Ready-up” from zPAM 3), when the server runs it.
    var matchInfo: String? = nil

    /// Parses `statusResponse`: a line of `\key\value` pairs, then one `score ping "name"` line per player.
    static func parse(_ response: String) -> HostedStatus? {
        let lines = response.components(separatedBy: "\n")
        guard lines.count >= 2, lines[0].hasSuffix("statusResponse") else { return nil }
        let parts = lines[1].split(separator: "\\", omittingEmptySubsequences: false).dropFirst().map(String.init)
        var fields: [String: String] = [:]
        var index = 0
        while index + 1 < parts.count { fields[parts[index].lowercased()] = parts[index + 1]; index += 2 }
        var players: [Player] = []
        for (slot, line) in lines.dropFirst(2).enumerated() where !line.isEmpty {
            let words = line.split(separator: " ", maxSplits: 2).map(String.init)
            guard words.count == 3, let score = Int(words[0]), let ping = Int(words[1]) else { continue }
            players.append(Player(id: slot, name: words[2].trimmingCharacters(in: CharacterSet(charactersIn: "\"")), score: score, ping: ping))
        }
        // zPAM 4 calls it _match_info, zPAM 3 _match_round.
        let match = (fields["_match_info"] ?? fields["_match_round"]).map { safeText($0) }.flatMap { $0.isEmpty ? nil : $0 }
        return HostedStatus(map: fields["mapname"] ?? "", gameType: fields["g_gametype"] ?? "", maxPlayers: Int(fields["sv_maxclients"] ?? "") ?? 0, players: players, matchInfo: match)
    }
    /// Text from a server, for display: without the game's colour codes or control characters, and not longer than a line.
    private static func safeText(_ value: String) -> String {
        let plain = value.replacingOccurrences(of: #"\^[0-9]"#, with: "", options: .regularExpression)
        return String(String(plain.unicodeScalars.filter { $0.value >= 32 && $0.value != 127 }).trimmingCharacters(in: .whitespaces).prefix(60))
    }
}
