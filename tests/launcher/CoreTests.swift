import Foundation

@main struct CoreTests {
    static func main() throws {
        let fixtures = URL(fileURLWithPath: CommandLine.arguments[1])
        let master = try Data(contentsOf: fixtures.appendingPathComponent("master.cod2x.me-120.bin"))
        let servers = WireParser.master(master)
        precondition(servers.count > 50 && servers.contains("151.80.40.237:28962"))
        precondition(WireParser.master(Data(master.dropLast(4))).count <= servers.count)
        precondition(WireParser.master(Data("bad".utf8)).isEmpty)
        let status = try Data(contentsOf: fixtures.appendingPathComponent("status.bin"))
        let server = try WireParser.status(status, address: "127.0.0.1:28960", ping: 31)
        precondition(server.map == "mp_trainstation" && server.maxPlayers == 33)
        precondition(server.version == "1.3" && server.mod == "kingbot" && !server.players.isEmpty)
        precondition(server.players[0].name == "Pypson")
        precondition((try? WireParser.status(Data("bad".utf8), address: "x", ping: 0)) == nil)
        let runs = QuakeColors.runs("^1Red ^7white ^^00Sinister ^xliteral")
        precondition(runs.map(\.text).joined() == "Red white ^0Sinister ^xliteral")
        precondition(runs.first?.code == 1)
        precondition(KeyStore.valid("0000-0000-0000-0000-86D3"))
        precondition(!KeyStore.valid("000000000000000086D4"))
        precondition(!KeyStore.valid(""))
        let scratch = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: scratch) }
        try FileManager.default.createDirectory(at: scratch, withIntermediateDirectories: true)
        let preferences = scratch.appendingPathComponent("preferences")
        try "name=Player\ncodkey=invalid\n".write(to: preferences, atomically: true, encoding: .utf8)
        try KeyStore.save("000000000000000086D3", at: preferences)
        precondition(KeyStore.hasValidKey(at: preferences))
        let saved = try String(contentsOf: preferences, encoding: .utf8)
        precondition(saved.contains("name=Player"))
        let attrs = try FileManager.default.attributesOfItem(atPath: preferences.path)
        precondition((attrs[.posixPermissions] as? NSNumber)?.intValue == 0o600)
        var settings = GameSettings()
        settings.advanced = "cg_fov=90\nname=Player One"
        let config = try settings.config()
        precondition(GameSettings().fps == 250 && config.contains("seta com_maxfps \"250\""))
        precondition(config.contains("seta name \"Player One\""))
        // The frame cap is free from 0 (no cap) to 1000, the engine dvar's range.
        for (fps, valid) in [(0, true), (1000, true), (1001, false), (-1, false), (-250, false)] {
            var capped = GameSettings(); capped.fps = fps
            precondition(((try? capped.config()) != nil) == valid, "frame cap \(fps)")
            if valid { let written = try capped.config(); precondition(written.contains("seta com_maxfps \"\(fps)\"")) }
        }
        precondition(FrameCap.chip(250) == ("250", "fps cap") && FrameCap.chip(0) == (nil, "No fps cap"))
        precondition(FrameCap.spoken(0) == "No frame cap" && FrameCap.customCaption(0) == "Unlimited" && FrameCap.customCaption(90) == "Custom value")
        precondition(FrameCap.presets.map(\.value) == [125, 250, 333, 1000] && FrameCap.isPreset(250) && !FrameCap.isPreset(0) && !FrameCap.isPreset(144))
        settings.advanced = "name=bad;quit"
        precondition((try? settings.config()) == nil)
        settings.advanced = "fs_basepath=/tmp"
        precondition((try? settings.config()) == nil)
        precondition((try? LaunchLink("cod2x://connect/127.0.0.1:28960"))?.address == "127.0.0.1:28960")
        precondition((try? LaunchLink("cod2x://connect/localhost%3Bquit")) == nil)
        let injection = try LaunchLink("cod2x://connect%20localhost%20password%20%22p%2Bquit%20%22")
        let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game Data", homePath: "/Private Home", link: injection)
        precondition(!plan.arguments.contains("password") && !plan.arguments.joined().contains("quit"))
        precondition(plan.link?.password == "p+quit ")
        precondition(plan.arguments.contains("\"/Game Data\""))
        precondition((try? EngineCommandLine.validate(Array(repeating: "+set x 1", count: 32))) == nil)
        precondition((try? EngineCommandLine.validate(Array(repeating: "+set x 1", count: 31))) != nil)
        try presentation(fixtures)
        print("PASS: real master/status fixtures, malformed packets, colours, native key CRC/private storage, config and URL injection, frame cap 0–1000 (250 by default)")
        print("PASS: cm/360, advanced dvar forms, settings migration, release notes, maps, server facts, media, key tag, player name and scrims")
    }

    static func presentation(_ fixtures: URL) throws {
        // cm/360: 360 / (0.022 × 5) counts per turn at 800 DPI is 4.0909 in = 10.39 cm.
        let cm = try requireValue(MouseMath.centimetresPer360(dpi: 800, sensitivity: 5))
        precondition(abs(cm - 10.3909) < 0.001 && MouseMath.label(cm) == "10.4")
        let doubled = try requireValue(MouseMath.centimetresPer360(dpi: 1600, sensitivity: 2.5))
        let customYaw = try requireValue(MouseMath.centimetresPer360(dpi: 400, sensitivity: 2, yaw: 0.0165))
        precondition(abs(doubled - cm) < 1e-9 && abs(customYaw - 69.2727) < 0.001)
        for (dpi, sensitivity) in [(0.0, 5.0), (800, 0), (-800, 5), (.nan, 5), (800, .infinity)] {
            precondition(MouseMath.centimetresPer360(dpi: dpi, sensitivity: sensitivity) == nil)
        }
        precondition(MouseMath.label(nil) == "—")

        // Console-style and equals-style advanced lines produce the same dvars; renderer reservations stay.
        var settings = GameSettings()
        settings.advanced = "cg_fov 80\nr_gamma=1.1\n  name   Player One  \nsnaps = 30"
        let pairs = try settings.dvars()
        for expected in [("cg_fov", "80"), ("r_gamma", "1.1"), ("name", "Player One"), ("snaps", "30"), ("r_anisotropy", "8")] {
            precondition(pairs.contains { $0.0 == expected.0 && $0.1 == expected.1 }, "missing \(expected)")
        }
        for bad in ["cg_fov", "r_renderer metal", "fs_homepath /tmp", "cl_password x", "name a;quit", "9bad 1"] {
            settings.advanced = bad
            precondition((try? settings.dvars()) == nil, bad)
        }
        settings.advanced = ""; settings.anisotropy = 3
        precondition((try? settings.dvars()) == nil)
        let legacy = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"resolution":"2560x1440","fps":250,"advanced":"cg_fov=90"}"#.utf8))
        precondition(legacy.resolution == "2560x1440" && legacy.fps == 250 && legacy.anisotropy == 8 && legacy.rawMouse && legacy.dpi == 800)
        let library = try JSONDecoder().decode(LauncherLibrary.self, from: Data(#"{"favorites":["192.0.2.1:28960"],"recent":[],"cache":[]}"#.utf8))
        precondition(library.favorites.count == 1 && library.dispatches.isEmpty)

        // Release notes: drafts hidden, Markdown reduced to a sentence, only github.com links kept.
        var utc = Calendar(identifier: .gregorian); utc.timeZone = TimeZone(identifier: "UTC")!
        let feed = try ReleaseNotes.parse(try Data(contentsOf: fixtures.appendingPathComponent("releases.json")), calendar: utc)
        precondition(feed.dispatches.count == 3 && feed.latestVersion == "0.1.1")
        precondition(!feed.dispatches.contains { $0.summary.contains("Draft") })
        precondition(feed.dispatches[0] == Dispatch(stamp: "02 NOV", title: "CoD2 Silicon 0.2 beta",
            summary: "Launcher redesign with Liquid Glass navigation. Server browser favourites.",
            link: "https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.2.0-beta.1"))
        precondition(feed.dispatches[1].title == "CoD2 Silicon 0.1.1" && feed.dispatches[1].link == nil && feed.dispatches[1].stamp == "20 OCT")
        precondition(feed.dispatches[1].summary.hasSuffix("…") && feed.dispatches[1].summary.count <= 161 && feed.dispatches[1].summary.hasPrefix("Fixes r_mode on 6K"))
        precondition(feed.dispatches[2].summary == "Call of Duty 2 multiplayer, native on Apple silicon. CoD2x 1.4 compatible.")
        precondition(ReleaseNotes.isNewer("0.1.10", than: "0.1.9") && !ReleaseNotes.isNewer("0.1.0", than: "0.1.0"))
        precondition((try? ReleaseNotes.parse(Data("{}".utf8))) == nil && (try? ReleaseNotes.parse(Data(count: 1_000_001))) == nil)
        let empty = try ReleaseNotes.parse(Data("[]".utf8))
        precondition(empty.dispatches.isEmpty && empty.latestVersion == nil)

        // Maps: stock regions and scenery, safe names for untrusted metadata, stable custom scenes.
        precondition(MapCatalog.displayName("mp_toujane") == "Toujane" && MapCatalog.region("MP_TOUJANE") == "NORTH AFRICA")
        precondition(MapCatalog.displayName("mp_brecourt") == "Brécourt" && MapCatalog.scenery("mp_railyard") == .winter)
        precondition(MapCatalog.displayName("mp_su_crossroads_v2") == "Su Crossroads V2" && MapCatalog.region("mp_su_crossroads_v2") == nil)
        precondition(MapCatalog.key("../mp_toujane") == nil && MapCatalog.displayName("mp_x;quit") == "Unknown map" && MapCatalog.key("mp_") == nil)
        precondition(MapCatalog.scenery("mp_custom") == MapCatalog.scenery("MP_CUSTOM"))
        precondition(GameModes.long("tdm") == "Team Deathmatch" && GameModes.long("SD") == "Search & Destroy" && GameModes.long("zom") == "ZOM")

        // Server facts drive badges, ping bars and the details grid.
        func server(_ fields: [String: String], ping: Int = 18) -> GameServer { GameServer(address: "192.0.2.1:28960", fields: fields, ping: ping, players: []) }
        let codx = ServerFacts(server(["shortversion": "1.4.6.8", "clients": "24", "sv_maxclients": "24", "sv_maxfps": "250", "pswrd": "1"]))
        precondition(codx.isCoD2x && codx.versionLabel == "CoD2x 1.4.6.8" && codx.occupancy == .full && codx.frameCap == "250 fps" && codx.access == "Password")
        let stock = ServerFacts(server(["protocol": "118", "clients": "0", "sv_maxclients": "20"], ping: 88))
        precondition(!stock.isCoD2x && stock.versionLabel == "Stock 1.3" && stock.occupancy == .empty && stock.frameCap == "Not set" && stock.pingLevel == 1)
        precondition(ServerFacts(server(["protocol": "120"])).versionLabel == "CoD2x 1.4")
        precondition([18, 39, 40, 59, 60, 79, 80].map(ServerFacts.pingLevel) == [4, 4, 3, 3, 2, 2, 1])

        // Library rows: map hints from file names, sizes and stamped dates.
        let date = try requireValue(ISO8601DateFormatter().date(from: "2026-10-04T21:14:00Z"))
        let demo = MediaFacts(name: "toujane_tdm_1004_2114.dm_1", date: date, bytes: 3_100_000, calendar: utc)
        precondition(demo.map == "toujane" && demo.size == "3.1 MB" && demo.stamp == "04 OCT 21:14")
        precondition(MediaFacts.mapHint("shot0001.jpg") == nil && MediaFacts.sizeLabel(512) == "512 B" && MediaFacts.sizeLabel(48_200) == "48 KB")

        // CD key display: grouped, uppercased, bounded, stamped onto the tag without exposing extra input.
        precondition(KeyFormat.display("ab12-cd34 ef56gh78ij90kl12mn") == "AB12 CD34 EF56 GH78 IJ90")
        precondition(KeyFormat.tag("7q4m2kx9lp3r") == ["7Q4M 2KX9", "LP3R ····", "····", "COD2 · 1.3"])
        precondition(KeyFormat.tag("") == ["···· ····", "···· ····", "····", "COD2 · 1.3"] && KeyFormat.tag("ab")[0] == "AB·· ····")

        // Scrims: computed opacity restores 4.5:1 for the extreme pixel in either appearance,
        // judged after compositing in gamma-encoded sRGB as SwiftUI does.
        let brass = ScrimMath.luminance(0xe2, 0xbd, 0x72), ink = ScrimMath.luminance(0x6b, 0x4a, 0x12), paper = ScrimMath.luminance(0xef, 0xe8, 0xd8)
        precondition(abs(ScrimMath.decode(ScrimMath.encode(0.37)) - 0.37) < 1e-9)
        for high in [0.1, 0.2, 0.5, 0.8] {
            let alpha = ScrimMath.darkScrim(high: high, text: brass)
            let shown = ScrimMath.decode(ScrimMath.encode(high) * (1 - alpha))
            precondition(ScrimMath.contrast(brass, shown) >= 4.5, "dark scrim \(high)")
        }
        precondition(ScrimMath.darkScrim(high: 0.01, text: brass) == 0.35 && ScrimMath.darkScrim(high: 1, text: 0.1) == 0.92)
        for low in [0.0, 0.05, 0.15, 0.3] {
            let alpha = ScrimMath.lightWash(low: low, text: ink, paper: paper)
            let shown = ScrimMath.decode(ScrimMath.encode(low) * (1 - alpha) + ScrimMath.encode(paper) * alpha)
            precondition(ScrimMath.contrast(ink, shown) >= 4.5, "light wash \(low)")
        }
        var pixels = Data(count: 10 * 10 * 4)
        for i in 0..<100 { let v = UInt8(i * 255 / 99); pixels[i * 4] = v; pixels[i * 4 + 1] = v; pixels[i * 4 + 2] = v; pixels[i * 4 + 3] = 255 }
        let spread = try requireValue(ScrimMath.percentiles(rgba: pixels, width: 10, height: 10, region: (0, 0, 1, 1)))
        precondition(spread.low < 0.02 && spread.high > 0.7 && ScrimMath.percentiles(rgba: pixels, width: 10, height: 9, region: (0, 0, 1, 1)) == nil)
        precondition(ScrimMath.percentiles(rgba: pixels, width: 10, height: 10, region: (0.95, 0.95, 0.01, 0.01)) == nil)

        // Player name comes from the engine's config, last assignment wins, colour codes kept for display.
        precondition(PlayerProfile.name(config: "seta name \"Old\"\nseta cg_fov \"80\"\nseta name \"^1Red^7Fox\"\n") == "^1Red^7Fox")
        precondition(PlayerProfile.name(config: "seta name \"^7\"\n") == nil && PlayerProfile.name(config: "") == nil)
        // The engine's active profile (players/active.txt) picks players/<profile>/config_mp.cfg; never invent a name.
        precondition(PlayerProfile.activeProfile("default") == "default" && PlayerProfile.activeProfile("  default\n") == "default")
        precondition(PlayerProfile.activeProfile("\"My Squad\"\n") == "My Squad" && PlayerProfile.activeProfile("alpha bravo") == "alpha")
        for unsafe in ["", "..", ".", "../etc", "a/b", "a\\b", "c:d", "\"\""] { precondition(PlayerProfile.activeProfile(unsafe) == nil, unsafe) }
        precondition(PlayerProfile.activeProfile(String(repeating: "x", count: 65)) == nil && PlayerProfile.unnamed == "Player")
    }

    static func requireValue<T>(_ value: T?) throws -> T {
        guard let value else { throw LauncherError(message: "Missing expected value") }
        return value
    }
}
