import Foundation
import AppKit
import Network

// Stand-ins for the two types Hosting takes from the launcher's core.
enum GameEngine: String, Codable, Sendable, CaseIterable { case cod2x, original }
struct LauncherError: Error { var message: String }

@main struct HostingTests {
    /// Run before a failing test exits, so no server it started is left behind.
    nonisolated(unsafe) static var cleanup: () -> Void = {}
    static func require(_ condition: @autoclosure () -> Bool, _ message: String) {
        if !condition() { FileHandle.standardError.write(Data("FAIL: \(message)\n".utf8)); cleanup(); exit(1) }
    }
    static func main() async throws {
        // MARK: server.cfg for public rules
        var server = HostedServer.new(id: "one", name: "Friday \"Rifles\"; quit // night")
        server.joinPassword = "let me in"; server.adminPassword = "admin\";quit"
        server.maps = ["mp_toujane", "../etc", "mp_carentan; quit", "mp_burgundy"]
        server.players = 500; server.friendlyFire = true
        let config = HostingConfig.serverConfig(server)
        require(config.contains("set sv_hostname \"Friday Rifles quit / night\"\n"), "the name loses quotes, semicolons and comment starts: \(config)")
        require(config.contains("set g_password \"letmein\"\n") && config.contains("set rcon_password \"adminquit\"\n"), "passwords lose spaces and quotes")
        require(config.contains("set sv_maxclients \"64\"\n"), "players are capped at 64")
        require(config.contains("set sv_maprotation \"gametype tdm map mp_toujane gametype tdm map mp_burgundy\"\n"), "only real map names reach the rotation")
        require(config.contains("set scr_friendlyfire \"1\"\n") && config.hasSuffix("map_rotate\n"), "friendly fire, and the rotation starts the first map")
        require(config.split(separator: "\n").allSatisfy { $0.hasPrefix("//") || $0.hasPrefix("set ") || $0 == "map_rotate" }, "every line is a comment, a set or the start of the rotation")
        require(config.split(separator: "\n").filter { !$0.hasPrefix("//") }.allSatisfy { $0.filter { $0 == "\"" }.count % 2 == 0 && !$0.contains(";") },
                "no line can carry a second command")
        let handed = HostingConfig.serverConfig(server, passwords: false)
        require(handed.contains("set g_password \"CHANGE_ME\"") && handed.contains("set rcon_password \"CHANGE_ME\"") && !handed.contains("letmein") && !handed.contains("adminquit"),
                "a file for someone else holds no password")
        server.joinPassword = ""
        require(HostingConfig.serverConfig(server, passwords: false).contains("set g_password \"\"\n"), "an open server stays open in the export")

        // MARK: zPAM's own template, with this server's values on its lines
        let template = ["// zPAM", "set g_gametype \"sd\"  // sd, dm", "set pam_mode \"comp\"", "set sv_hostname \"Awesome zPAM server\"", "set g_password \"\"",
                        "set rcon_password \"\"", "seta sv_wwwBaseURL \"http://cod2x.me/zpam\"  // enter a valid URL", "seta sv_wwwDownload \"1\"",
                        "set scr_sd_timelimit 0", "set sv_maprotation \"map mp_toujane map mp_burgundy\"", "", "map mp_toujane_fix", ""].joined(separator: "\r\n")
        var match = HostedServer.new(id: "two", name: "Scrim Room")
        match.rules = .zpam; match.matchMode = "sd:comp_mr15"; match.maps = ["mp_dawnville", "mp_matmata"]; match.adminPassword = "secretword"
        let merged = HostingConfig.serverConfig(match, zpamTemplate: template)
        let lines = merged.components(separatedBy: "\n")
        require(lines.contains { $0.hasPrefix("set pam_mode \"comp_mr15\"") } && lines.contains { $0.hasPrefix("set g_gametype \"sd\"") }, "match mode and game type: \(merged)")
        require(lines.contains { $0.hasPrefix("set sv_hostname \"Scrim Room\"") } && lines.contains { $0.hasPrefix("set rcon_password \"secretword\"") }, "name and admin password")
        require(lines.contains { $0.hasPrefix("seta sv_wwwBaseURL \"http://cod2x.me/zpam\"") } && lines.contains("set scr_sd_timelimit 0"), "zPAM's own lines stay")
        require(lines.contains { $0.hasPrefix("set sv_maprotation \"map mp_dawnville map mp_matmata\"") }, "the rotation is this server's")
        require(lines.filter { $0.hasPrefix("map") } == ["map_rotate"], "the template's starting map is replaced by the start of the rotation")
        let maxIndex = lines.firstIndex { $0.hasPrefix("set sv_maxclients") }, mapIndex = lines.firstIndex(of: "map_rotate")
        require(maxIndex != nil && mapIndex != nil && maxIndex! < mapIndex!, "settings the template lacks are set before the map starts")
        require(!merged.contains("scr_friendlyfire") && !merged.contains("sv_pure"), "public-only settings stay out of a zPAM config")

        // MARK: Launch arguments
        let arguments = try HostingConfig.arguments(server, dataPath: "/Games/Call of Duty 2", home: "/Users/sample/Library/Application Support/CoD2 Silicon/hosting/servers/one")
        require(arguments == ["+set", "fs_basepath", "\"/Games/Call of Duty 2\"", "+set", "fs_homepath", "\"/Users/sample/Library/Application Support/CoD2 Silicon/hosting/servers/one\"",
                              "+set", "net_ip", "0.0.0.0", "+set", "net_port", "28960", "+set", "dedicated", "1", "+exec", "server.cfg"], "arguments: \(arguments)")
        // Released CoD2x players: the engine's revision is set on the command line, before the config runs, and only for CoD2x.
        server.releasedCoD2x = true
        let released = try HostingConfig.arguments(server, dataPath: "/Games/CoD2", home: "/tmp/one")
        require(Array(released.suffix(5)) == ["+set", "sv_cod2xRevision", "6", "+exec", "server.cfg"], "a server for released CoD2x players starts as revision 6: \(released)")
        var original = server; original.engine = .original
        let originalArguments = try HostingConfig.arguments(original, dataPath: "/Games/CoD2", home: "/tmp/one")
        require(!originalArguments.contains("sv_cod2xRevision") && !original.runsAsReleasedCoD2x,
                "the original 1.3 never gets a CoD2x revision")
        let releasedExport = HostingConfig.export(server, zpamTemplate: nil, zpam: nil, passwords: false, matchSettings: false)
        require(!releasedExport.values.contains { $0.contains("sv_cod2xRevision") }
                    && releasedExport["README.txt"]!.contains("takes players on its own version or a newer one"),
                "another machine's CoD2x decides who can join; the export only says so")
        require(!HostingConfig.export(original, zpamTemplate: nil, zpam: nil, passwords: false, matchSettings: false)["README.txt"]!.contains("its own version"),
                "the original 1.3 export says nothing about CoD2x versions")
        server.releasedCoD2x = false
        // A list saved by 0.3.0 has none of the later keys: every server loads, with the new options off.
        let saved030 = Data(#"[{"adminPassword":"abcdefgh23456789","engine":"cod2x","friendlyFire":false,"gameType":"tdm","id":"one","joinPassword":"","maps":["mp_toujane"],"matchMode":"sd:comp","name":"Old Server","players":12,"port":28960,"reach":"network","rules":"public"},{"adminPassword":"x","engine":"original","friendlyFire":true,"gameType":"dm","id":"two","joinPassword":"pw","maps":["mp_carentan"],"matchMode":"sd:comp","name":"Second","players":8,"port":28961,"reach":"listed","rules":"zpam"}]"#.utf8)
        let old = try JSONDecoder().decode([HostedServer].self, from: saved030)
        require(old.count == 2 && old[0].name == "Old Server" && old[1].reach == .listed && old[1].rules == .zpam && old.allSatisfy { !$0.releasedCoD2x },
                "a list saved by 0.3.0 loads whole, with released-CoD2x off")
        var kept = old[0]; kept.releasedCoD2x = true
        let reloaded = try JSONDecoder().decode(HostedServer.self, from: JSONEncoder().encode(kept))
        require(reloaded == kept && reloaded.releasedCoD2x, "the new option survives saving")
        // MARK: Ports the game's own local list asks, and Wi-Fi
        var ported = HostedServer.new(id: "p", name: "Ports")
        require(ported.listedPorts == 28960...28991 && ported.inLocalList, "CoD2x's local list asks 28960 to 28991")
        ported.engine = .original
        require(ported.listedPorts == 28960...28963, "the original 1.3's local list asks 28960 to 28963")
        require(HostingConfig.otherPort(for: ported) { $0 != 28961 } == 28962, "a busy port is replaced by the next free one the game lists")
        require(HostingConfig.otherPort(for: ported) { $0 > 28963 } == 28964, "with every listed port busy, the first free port above")
        ported.port = 28963
        require(HostingConfig.otherPort(for: ported) { _ in true } == 28960, "a listed port below is preferred to an unlisted one above")
        ported.port = 28970
        require(!ported.inLocalList && HostingConfig.otherPort(for: ported) { _ in false } == 28971, "nothing free still names a port")
        ported.engine = .cod2x
        require(ported.inLocalList, "28970 is in CoD2x's local list")
        require(HostingNetwork.isWiFi(satisfied: true, preferred: .wifi), "Wi-Fi in front is Wi-Fi")
        for other in [NWInterface.InterfaceType.wiredEthernet, .loopback, .cellular, .other] {
            require(!HostingNetwork.isWiFi(satisfied: true, preferred: other), "\(other) in front is not Wi-Fi")
        }
        require(!HostingNetwork.isWiFi(satisfied: false, preferred: .wifi) && !HostingNetwork.isWiFi(satisfied: true, preferred: nil), "no working path, or no interface, is not Wi-Fi")
        server.reach = .listed
        let listed = try HostingConfig.arguments(server, dataPath: "/Games/CoD2", home: "/tmp/one")
        require(listed.contains("2") && !listed.contains("1"), "a listed server is dedicated 2")
        for bad in ["/Games/Co+D2", "/Games/\"x\"", "/Games/a;b", "relative/path"] {
            require((try? HostingConfig.arguments(server, dataPath: bad, home: "/tmp/one")) == nil, "a path the engine can't read is refused: \(bad)")
        }

        // MARK: zPAM's release listing
        let digest = String(repeating: "ab", count: 32)
        func asset(_ name: String, _ url: String, digest: String?, size: Int = 1000) -> [String: Any] {
            var asset: [String: Any] = ["name": name, "browser_download_url": url, "size": size]
            if let digest { asset["digest"] = digest }
            return asset
        }
        let base = "https://github.com/eyza-cod2/zpam3/releases/download"
        let listing = try JSONSerialization.data(withJSONObject: [
            ["draft": true, "assets": [asset("zpam499.zip", "\(base)/4.99/zpam499.zip", digest: "sha256:\(digest)")]],
            ["prerelease": true, "assets": [asset("zpam498.zip", "\(base)/4.98/zpam498.zip", digest: "sha256:\(digest)")]],
            ["assets": [asset("zpam413.zip", "https://example.com/zpam413.zip", digest: "sha256:\(digest)"), asset("zpam413_test.zip", "\(base)/4.13/zpam413_test.zip", digest: "sha256:\(digest)")]],
            ["assets": [asset("zpam412.zip", "\(base)/4.12/zpam412.zip", digest: nil)]],
            ["assets": [asset("source.zip", "\(base)/4.11/source.zip", digest: "sha256:\(digest)"), asset("zpam411.zip", "\(base)/4.11/zpam411.zip", digest: "sha256:\(digest.uppercased())", size: 183_607_173)]],
            ["assets": [asset("zpam336.zip", "\(base)/3.36/zpam336.zip", digest: "sha256:\(digest)", size: 171_561_731)]],
        ])
        let four = ZPAMRelease.newest(.four, in: listing), three = ZPAMRelease.newest(.three, in: listing)
        require(four?.version == "4.11" && four?.asset == "zpam411.zip" && four?.bytes == 183_607_173 && four?.sha256 == digest,
                "drafts, pre-releases, other hosts and downloads without a checksum are skipped: \(String(describing: four))")
        require(three?.version == "3.36" && three?.bytes == 171_561_731, "the 1.3 line finds 3.36")
        require(ZPAMRelease.newest(.four, in: Data("not json".utf8)) == nil, "a broken listing finds nothing")
        require(ZPAMLine.version(asset: "zpam412.zip") == "4.12" && ZPAMLine.version(asset: "other.zip") == nil, "version from the file name")

        // MARK: What the server answers
        let status = HostedStatus.parse("statusResponse\n\\g_gametype\\tdm\\mapname\\mp_toujane\\sv_maxclients\\12\\sv_hostname\\Friday\n14 4 \"Sample Player\"\n9 21 \"Second \"Sample\"\n")
        require(status == HostedStatus(map: "mp_toujane", gameType: "tdm", maxPlayers: 12,
                                       players: [.init(id: 0, name: "Sample Player", score: 14, ping: 4), .init(id: 1, name: "Second \"Sample", score: 9, ping: 21)]),
                "status: \(String(describing: status))")
        require(HostedStatus.parse("statusResponse\n\\_match_info\\Round 0 | MR15 Ready-up\\mapname\\mp_toujane_fix\n")?.matchInfo == "Round 0 | MR15 Ready-up", "zPAM's match line")
        require(HostedStatus.parse("statusResponse\n\\_match_round\\^2Ready-up\\mapname\\mp_toujane_fix\n")?.matchInfo == "Ready-up", "zPAM 3's match line, without colour codes")
        require(HostedStatus.parse("print\nBad rcon") == nil && HostedStatus.parse("") == nil, "other answers are not a status")

        // MARK: Export
        match.joinPassword = "scrimnight"
        let export = HostingConfig.export(match, zpamTemplate: template, zpam: four, passwords: false, matchSettings: true)
        require(Set(export.keys) == ["server.cfg", "start-linux.sh", "start-windows.bat", "README.txt"], "the export's files: \(export.keys.sorted())")
        require(export.values.allSatisfy { !$0.contains("scrimnight") && !$0.contains("secretword") }, "no password anywhere in an export without passwords")
        require(export["server.cfg"]!.contains("set pam_mode \"comp_mr15\"") && export["README.txt"]!.contains("zpam411.zip") && export["README.txt"]!.contains("CHANGE_ME"),
                "the zPAM export names the mode, the download and the passwords to set")
        require(export["start-linux.sh"]!.contains("+set net_port 28960 +set dedicated 1 +exec server.cfg") && export["start-windows.bat"]!.contains("CoD2MP_s.exe"), "start commands")
        // The Linux server lists itself whatever it is told: a My Network export says so and how to stop it.
        require(export["README.txt"]!.contains("127.0.0.1 cod2master.activision.com") && export["README.txt"]!.contains("On Windows this server stays unlisted")
                && export["start-linux.sh"]!.contains("lists itself on the public master servers") && !export["start-windows.bat"]!.contains("lists itself"),
                "a My Network export warns that a Linux server is listed anyway")
        var listedMatch = match; listedMatch.reach = .listed
        let listedExport = HostingConfig.export(listedMatch, zpamTemplate: template, zpam: four, passwords: false, matchSettings: true)
        require(listedExport["README.txt"]!.contains("change dedicated to 1 in start-windows.bat") && !listedExport["start-linux.sh"]!.contains("#  The Linux")
                && listedExport["start-linux.sh"]!.contains("+set dedicated 2") && !listedExport["start-linux.sh"]!.contains("whatever"),
                "a listed export only tells Windows hosts how to unlist")
        let plain = HostingConfig.export(match, zpamTemplate: template, zpam: four, passwords: true, matchSettings: false)
        require(!plain["server.cfg"]!.contains("pam_mode") && plain["server.cfg"]!.contains("set g_gametype \"sd\"") && plain["server.cfg"]!.contains("scrimnight")
                && !plain["README.txt"]!.contains("zPAM"), "without match settings the export is a public server of the same game type")
        require(HostingConfig.fileName(match) == "scrim-room" && HostingConfig.fileName(HostedServer.new(name: "…")) == "cod2-server", "folder names")

        // MARK: The model: saved servers, and the files it writes
        let scratch = URL(fileURLWithPath: CommandLine.arguments[1])
        try await MainActor.run {
            let model = HostingModel(folder: scratch.appendingPathComponent("hosting"), gameBundle: scratch.appendingPathComponent("Game.app"))
            model.newServer(engine: .cod2x)
            require(model.showingDraft && model.servers.isEmpty, "a new server is a draft until it is kept")
            model.draft?.name = "  "; model.draft?.adminPassword = ""
            let saved = model.saveDraft()
            require(saved?.name == "My Server" && saved?.adminPassword.count == 16 && model.selection == saved?.id, "an empty name and admin password get defaults")
            var edited = saved!
            edited.name = "Edited"; model.update(edited)
            let list = scratch.appendingPathComponent("hosting/servers.json")
            let mode = (try FileManager.default.attributesOfItem(atPath: list.path)[.posixPermissions] as? NSNumber)?.intValue
            require(mode == 0o600, "the saved list, which holds passwords, is readable by its owner only")
            let again = HostingModel(folder: scratch.appendingPathComponent("hosting"), gameBundle: scratch.appendingPathComponent("Game.app"))
            require(again.servers.map(\.name) == ["Edited"], "servers are read back")
            model.newServer(engine: .original)
            require(model.draft?.port == 28961, "a second server gets the next port")
            model.cancelDraft()
            model.start(edited)
            if case .failed(let message) = model.phase(edited.id) { require(message.contains("doesn’t include"), "a missing server program is said plainly: \(message)") }
            else { require(false, "starting without the server program fails") }
            require(!FileManager.default.fileExists(atPath: model.home(edited.id).path), "a start that fails early writes nothing")
            model.delete(edited)
            require(model.servers.isEmpty, "deleted")
        }
        require(HostingNetwork.portFree(1) == false && HostingNetwork.exchange(port: 9, command: "getstatus", timeout: 1) == nil, "ports below 1025 are refused; no answer is nil")
        print("PASS: hosted servers: server.cfg writing and its escaping, zPAM template merge, launch arguments, release listing checks, status parsing, export, saved list")

        // MARK: A real server, when one is given: COD2_HOSTING_LIVE="<game app>|<game data>"
        guard let live = ProcessInfo.processInfo.environment["COD2_HOSTING_LIVE"]?.split(separator: "|").map(String.init), live.count == 2 else { return }
        for engine in GameEngine.allCases {
            let model = await MainActor.run { () -> HostingModel in
                let model = HostingModel(folder: scratch.appendingPathComponent("live-\(engine.rawValue)"), gameBundle: URL(fileURLWithPath: live[0]))
                model.dataPath = { live[1] }
                model.newServer(engine: engine)
                model.draft?.port = engine == .cod2x ? 28991 : 28992
                model.draft?.name = "Hosting test"
                model.start(model.saveDraft()!)
                return model
            }
            let server = await MainActor.run { model.servers[0] }
            let port = server.port
            cleanup = {
                // Ask the test's own server, by its port, to quit; it is this process's only child there.
                usleep(700_000)
                _ = HostingNetwork.exchange(port: port, command: "rcon \(server.adminPassword) quit")
                usleep(500_000)
            }
            var status: HostedStatus?
            for _ in 0..<60 {
                try await Task.sleep(for: .milliseconds(500))
                status = await MainActor.run { model.statuses[server.id] }
                if status != nil { break }
                if await MainActor.run(body: { !model.isRunning(server.id) }) { break }
            }
            let phase = await MainActor.run { model.phase(server.id) }
            let log = (try? String(contentsOf: await MainActor.run { model.logURL(server.id) }, encoding: .utf8)) ?? ""
            require(status?.map == "mp_toujane" && status?.gameType == "tdm" && status?.maxPlayers == 12, "\(engine): the server answers with its map: \(String(describing: status)) \(phase)\n\(log.suffix(1500))")
            if case .running = phase {} else { require(false, "\(engine): running, not \(phase)") }
            let hostname = HostingNetwork.exchange(port: server.port, command: "getinfo x") ?? ""
            require(hostname.contains("Hosting test"), "\(engine): the name from server.cfg is in use: \(hostname)")
            await MainActor.run { model.nextMap(server) }
            var rotated = false
            for _ in 0..<40 where !rotated {
                try await Task.sleep(for: .milliseconds(500))
                rotated = await MainActor.run { model.statuses[server.id]?.map == "mp_carentan" }
            }
            require(rotated, "\(engine): Next Map reaches the server through its admin channel")
            try await Task.sleep(for: .milliseconds(700))
            let refused = HostingNetwork.exchange(port: server.port, command: "rcon wrong-password status") ?? ""
            require(refused.contains("Invalid password"), "\(engine): a wrong admin password is refused: \(refused)")
            await MainActor.run { model.stop(server) }
            var stopped = false
            for _ in 0..<40 where !stopped {
                try await Task.sleep(for: .milliseconds(250))
                stopped = await MainActor.run { model.phase(server.id) == .stopped }
            }
            require(stopped, "\(engine): Stop Server ends it cleanly: \(await MainActor.run { model.phase(server.id) })")
            require(HostingNetwork.portFree(server.port), "\(engine): the port is free again")
            let home = await MainActor.run { model.home(server.id) }
            let written = try FileManager.default.subpathsOfDirectory(atPath: home.path).sorted()
            cleanup = {}
            print("\(engine) wrote:", written.joined(separator: ", "))
        }
        print("PASS: both dedicated servers start from the launcher's own files, answer status, take Next Map, refuse a wrong admin password and stop cleanly")

        // MARK: zPAM from its real releases, when asked for: COD2_HOSTING_ZPAM=1 (two downloads of about 175 MB)
        guard ProcessInfo.processInfo.environment["COD2_HOSTING_ZPAM"] == "1" else { return }
        for engine in GameEngine.allCases {
            let model = await MainActor.run { () -> HostingModel in
                let model = HostingModel(folder: scratch.appendingPathComponent("zpam-\(engine.rawValue)"), gameBundle: URL(fileURLWithPath: live[0]))
                model.dataPath = { live[1] }
                return model
            }
            let line: ZPAMLine = engine == .cod2x ? .four : .three
            await model.lookUpZPAM(line)
            guard case .missing(let release) = await MainActor.run(body: { model.zpam[line] }) else {
                require(false, "\(engine): the release listing names a download: \(String(describing: await MainActor.run { model.zpam[line] }))"); return
            }
            print("\(engine): zPAM \(release.version), \(release.asset), \(release.bytes) bytes, sha256 \(release.sha256)")
            await MainActor.run { model.getZPAM(line) }
            var state = await MainActor.run { model.zpam[line] }
            for _ in 0..<1200 {
                state = await MainActor.run { model.zpam[line] }
                if case .getting = state { try await Task.sleep(for: .milliseconds(500)) } else { break }
            }
            require(state == .ready(version: release.version), "\(engine): zPAM downloads, matches its checksum and unpacks: \(String(describing: state))")
            let kept = try FileManager.default.subpathsOfDirectory(atPath: scratch.appendingPathComponent("zpam-\(engine.rawValue)/zpam").path).sorted()
            print("\(engine) kept:", kept.joined(separator: ", "))
            let server = await MainActor.run { () -> HostedServer in
                model.newServer(engine: engine)
                model.draft?.port = engine == .cod2x ? 28993 : 28994
                model.draft?.name = "zPAM test"; model.draft?.rules = .zpam; model.draft?.matchMode = "sd:comp_mr15"; model.draft?.maps = ["mp_dawnville", "mp_matmata"]
                let server = model.saveDraft()!
                model.start(server)
                return server
            }
            let port = server.port
            cleanup = { usleep(700_000); _ = HostingNetwork.exchange(port: port, command: "rcon \(server.adminPassword) quit"); usleep(500_000) }
            var status: HostedStatus?
            for _ in 0..<80 {
                try await Task.sleep(for: .milliseconds(500))
                status = await MainActor.run { model.statuses[server.id] }
                if status != nil { break }
                if await MainActor.run(body: { !model.isRunning(server.id) }) { break }
            }
            let log = (try? String(contentsOf: await MainActor.run { model.logURL(server.id) }, encoding: .isoLatin1)) ?? ""
            // zPAM plays its own corrected copy of a stock map, named with _fix.
            require(status.map { HostingCatalog.stockMap($0.map) } == "mp_dawnville" && status?.gameType == "sd", "\(engine): the zPAM server runs the first map of its rotation: \(String(describing: status))\n\(log.suffix(2500))")
            let info = HostingNetwork.exchange(port: port, command: "getstatus") ?? ""
            print("\(engine) zPAM status:", info.split(separator: "\n").dropFirst().first.map { String($0.prefix(700)) } ?? "")
            let match = await MainActor.run { model.statuses[server.id]?.matchInfo } ?? ""
            require(info.contains("\\_zpam\\\(release.version)") && match.contains("Ready-up") && (line == .three || match.contains("MR15")),
                    "\(engine): zPAM \(release.version) runs the match and waits for players to ready up: \(match)")
            await MainActor.run { model.stop(server) }
            var stopped = false
            for _ in 0..<40 where !stopped {
                try await Task.sleep(for: .milliseconds(250))
                stopped = await MainActor.run { model.phase(server.id) == .stopped }
            }
            require(stopped, "\(engine): the zPAM server stops cleanly")
            cleanup = {}
        }
        print("PASS: zPAM downloads from its releases, matches its checksum, and a match server runs with it on both engines")
    }
}
