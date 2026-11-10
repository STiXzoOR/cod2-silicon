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
        precondition(settings.fullscreen == "borderless")
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
        settings.advanced = "dpi 1600"
        precondition((try? settings.arguments()) == nil, "DPI is metadata, never an engine startup command")
        var startup = GameSettings()
        startup.advanced = "fs_cdpath /Game\nfs_copyfiles 0\nfs_restrict 0\nloc_language 1\ndedicated 0\ncom_hunkMegs 512"
        let startupNames = try startup.launchDvars(migrated: true).map { $0.0.lowercased() }
        for name in ["fs_cdpath", "fs_copyfiles", "fs_restrict", "loc_language", "dedicated", "com_hunkmegs"] {
            if !startupNames.contains(name) { fatalError("FAIL: migrated launch dropped startup option " + name) }
        }
        precondition((try? LaunchLink("cod2x://connect/127.0.0.1:28960"))?.address == "127.0.0.1:28960")
        precondition((try? LaunchLink("cod2x://connect/localhost%3Bquit")) == nil)
        let injection = try LaunchLink("cod2x://connect%20localhost%20password%20%22p%2Bquit%20%22")
        let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game Data", homePath: "/Private Home", link: injection)
        precondition(!plan.arguments.contains("password") && !plan.arguments.joined().contains("quit"))
        precondition(plan.link?.password == "p+quit ")
        precondition(plan.arguments.contains("\"/Game Data\""))
        precondition((try? EngineCommandLine.validate(Array(repeating: "+set x 1", count: 32))) == nil)
        precondition((try? EngineCommandLine.validate(Array(repeating: "+set x 1", count: 31))) != nil)
        try engines(server: server, scratch: scratch)
        try cod2xFiles(scratch: scratch)
        try crashReporting(scratch: scratch)
        try presentation(fixtures)
        print("PASS: crash reports: CoD2x's own (log with its pending report) never opened or sent, ours shortened, command lines dropped, a CD key refuses, one envelope to a stand-in Sentry on 127.0.0.1, nothing when off or without a receiver, never twice; CoD2x's switch reaches only a CoD2x game")
        print("PASS: CoD2x files: the archive taken from a CoD2x mss32.dll by the engine's rule (none or two refused, a stock DLL named), the release refused at any other size or hash with only the archive kept, present for CoD2x and absent for Original 1.3, the game data folder never written")
        print("PASS: game version: CoD2x by default and kept across saves, both engines found by executable, Original 1.3 asks for CoD2x only on CoD2x servers, links and passwords never in argv, crash and freeze reports from either engine")
        print("PASS: real master/status fixtures, malformed packets, colours, native key CRC/private storage, config and URL injection, frame cap 0–1000 (250 by default)")
        print("PASS: cm/360, advanced dvar forms, settings migration, release notes, maps, server facts, media, key tag, player name and scrims")
    }

    /// Settings › Game version: which engine starts, what it can join, and what it leaves behind.
    static func engines(server stock: GameServer, scratch: URL) throws {
        precondition(GameSettings().engine == .cod2x)
        let older = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"fps":125,"vsync":true}"#.utf8))
        precondition(older.engine == .cod2x && older.fps == 125 && older.vsync, "settings saved before the choice existed play CoD2x")
        var original = GameSettings(); original.engine = .original; original.fps = 333
        let reread = try JSONDecoder().decode(GameSettings.self, from: JSONEncoder().encode(original))
        precondition(reread == original && reread.engine == .original)
        let newer = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"fps":200,"engine":"metal"}"#.utf8))
        precondition(newer.engine == .cod2x && newer.fps == 200, "an engine this version doesn't know falls back without losing the rest")
        var same = original; same.engine = .cod2x
        let written = try original.config(), unchanged = try same.config()
        precondition(written == unchanged, "the game version is not a dvar")

        // The app's engines, by executable. A build without the original carries CoD2x only.
        let game = scratch.appendingPathComponent("CoD2 Game.app"), macos = game.appendingPathComponent("Contents/MacOS")
        try FileManager.default.createDirectory(at: macos, withIntermediateDirectories: true)
        precondition(GameEngine.installed(in: game).isEmpty)
        FileManager.default.createFile(atPath: macos.appendingPathComponent("cod2_macos").path, contents: Data(), attributes: [.posixPermissions: 0o755])
        FileManager.default.createFile(atPath: macos.appendingPathComponent("cod2_macos_stock").path, contents: Data(), attributes: [.posixPermissions: 0o644])
        precondition(GameEngine.installed(in: game) == [.cod2x], "a stock engine that can't run doesn't count")
        try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: macos.appendingPathComponent("cod2_macos_stock").path)
        precondition(GameEngine.installed(in: game) == [.cod2x, .original])

        // Original 1.3 joins stock 1.3 servers only; CoD2x joins both.
        var codx = stock; codx.fields["shortversion"] = "1.4.6.8"
        precondition(GameEngine.original.canJoin(stock) && !GameEngine.original.canJoin(codx))
        precondition(GameEngine.cod2x.canJoin(stock) && GameEngine.cod2x.canJoin(codx))
        let link = try LaunchLink.direct("203.0.113.24:28960"), secret = try LaunchLink.direct("203.0.113.24:28960", password: "hunter2")
        precondition(GameEngine.cod2x.check(server: codx, lookedUp: false) == .go)
        precondition(GameEngine.original.check(server: stock, lookedUp: false) == .go)
        precondition(GameEngine.original.check(server: codx, lookedUp: false) == .needsCoD2x)
        precondition(GameEngine.original.check(server: nil, lookedUp: false) == .lookUp)
        precondition(GameEngine.original.check(server: nil, lookedUp: true) == .go, "an unanswered server is the game's to try")
        let request = EngineRequest(link: link)
        precondition(request.title == "This server needs CoD2x")
        precondition(request.message == "This server runs CoD2x 1.4. Original 1.3 can only join Stock 1.3 servers. CoD2x stays on for later games; you can change this in Settings.")

        // Both engines take their server as an Apple event once running: never +connect or a password in argv.
        let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: "/Home", link: secret)
        precondition(plan.link?.address == "203.0.113.24:28960" && plan.link?.password == "hunter2")
        precondition(!plan.arguments.contains("+connect") && !plan.arguments.joined().contains("hunter2"))

        // Crash and freeze reports, which either engine writes into the app home: the newest since the game started.
        let reports = scratch.appendingPathComponent("reports")
        try FileManager.default.createDirectory(at: reports, withIntermediateDirectories: true)
        let started = Date()
        func report(_ name: String, _ age: TimeInterval) throws {
            let url = reports.appendingPathComponent(name)
            try Data("report\n".utf8).write(to: url)
            try FileManager.default.setAttributes([.modificationDate: started.addingTimeInterval(age)], ofItemAtPath: url.path)
        }
        try report("cod2_crash_101.txt", -60)
        precondition(CrashReports.latest(in: reports, since: started) == nil, "a report from an earlier game is not this one's")
        try report("cod2_freeze_202.txt", 5)
        try report("notes.txt", 9); try report("cod2_crash_303.log", 9)
        precondition(CrashReports.latest(in: reports, since: started)?.lastPathComponent == "cod2_freeze_202.txt")
        try report("cod2_crash_202.txt", 7)
        precondition(CrashReports.latest(in: reports, since: started)?.lastPathComponent == "cod2_crash_202.txt")
    }

    /// Settings › Privacy › crash reports, against a stand-in receiver on 127.0.0.1 (no other host).
    static func crashReporting(scratch: URL) throws {
        let fm = FileManager.default
        // CoD2x's switch reaches the engine only for a CoD2x game with the switch on; an inherited value never leaks through.
        for (engine, on, expected) in [(GameEngine.cod2x, true, "1"), (.cod2x, false, nil), (.original, true, nil), (.original, false, nil)] as [(GameEngine, Bool, String?)] {
            var environment = ["COD2_SILICON_COD2X_REPORTS": "1", "HOME": "/h"]
            CrashReporting.applyConsent(to: &environment, engine: engine, cod2xReports: on)
            precondition(environment["COD2_SILICON_COD2X_REPORTS"] == expected && environment["HOME"] == "/h", "\(engine) \(on)")
        }
        let off = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"fps":125}"#.utf8))
        precondition(!off.sendCrashReports && !off.sendCoD2xReports, "both switches are off until turned on")
        var on = GameSettings(); on.sendCrashReports = true; on.sendCoD2xReports = true
        let reread = try JSONDecoder().decode(GameSettings.self, from: JSONEncoder().encode(on)), plain = try GameSettings().config(), withSwitches = try on.config()
        precondition(reread == on && withSwitches == plain, "kept across saves, and not a dvar")

        // The receiver: HTTPS, or plain HTTP only to this Mac.
        precondition(SentryDSN("https://abc123@o9.ingest.example.com/4242")?.envelopeURL.absoluteString == "https://o9.ingest.example.com/api/4242/envelope/")
        precondition(SentryDSN("https://k@sentry.example.com:8443/relay/7")?.envelopeURL.absoluteString == "https://sentry.example.com:8443/relay/api/7/envelope/")
        for bad in ["", "http://k@example.com/1", "https://example.com/1", "https://k@example.com/", "https://k@example.com/x", "ftp://k@127.0.0.1/1", "https://k@h/1?x=1"] {
            precondition(SentryDSN(bad) == nil, bad)
        }
        precondition(SentryDSN("https://pub:sec@h.example/3")?.authorization(client: "c/1") == "Sentry sentry_version=7, sentry_client=c/1, sentry_key=pub, sentry_secret=sec")

        // Shortened before it leaves: home folder names, command lines; a CD key (by the game's checksum) stops it.
        let original = """
            ================ CoD2 Silicon CRASH REPORT ================

            crash      : SIGSEGV (Segmentation fault) at 0x0
            executable : /Users/jane.doe/Applications/CoD2 Silicon.app/Contents/Helpers/CoD2 Game.app/Contents/MacOS/cod2_macos_stock
            command    : +set fs_homepath "/Users/jane.doe/Library/Application Support/CoD2 Silicon" +set name Jane
              stack@rsp: 0x0000000102345678 0000000000000000 000000016fdff000
            X0 =0x0000000000000001  X1 =0x000000016fdfe9c8
            0x00000001a2b3c4d5 /Users/jane.doe/Library/Frameworks/SDL3.framework/SDL3
            """
        let shortened = try CrashReporting.redact(original, home: "/Users/jane.doe")
        precondition(!shortened.contains("jane") && !shortened.contains("Jane") && !shortened.contains("command") && !shortened.contains("fs_homepath"), shortened)
        precondition(shortened.contains("executable : ~/Applications/CoD2 Silicon.app") && shortened.contains("0x00000001a2b3c4d5 ~/Library/Frameworks/SDL3.framework/SDL3"))
        let users = try CrashReporting.redact("dyld /Users/sam/a\n/Users/sam", home: "/Elsewhere/sam")
        let elsewhere = try CrashReporting.redact("in /Elsewhere/sam/game", home: "/Elsewhere/sam")
        precondition(users == "dyld ~/a\n~" && elsewhere == "in ~/game", "any /Users name, and a home outside /Users")
        for key in ["0000-0000-0000-0000-86D3", "0000 0000 0000 0000 86d3", "key 000000000000000086D3.", "codkey=whatever"] {
            do { _ = try CrashReporting.redact("crash : x\n" + key, home: "/Users/a"); preconditionFailure(key) } catch { precondition(error == .key, key) }
        }
        precondition(!CrashReporting.containsKey("0000-0000-0000-0000-86D4 000000000000000086D3A 0x000000000000000086D3 ") && !CrashReporting.containsKey(original))
        precondition(CrashReporting.reason(shortened, name: "cod2_crash_1.txt") == "SIGSEGV (Segmentation fault) at 0x0")
        precondition(CrashReporting.reason("\n=== COD2X CRASH ===\nVersion:      1.4.6.8\nSignal:       11 SIGSEGV, code 2\n", name: "cod2x_crash.x.1.log") == "11 SIGSEGV, code 2")
        precondition(CrashReporting.reason("=== COD2X FREEZE ===\nReason:       main thread stalled\n", name: "cod2x_freeze.x.1.log") == "main thread stalled")
        precondition(CrashReporting.reason("nothing", name: "cod2_freeze_9.txt") == "The game stopped responding")

        // The envelope: its header, a fatal event without a user, and the shortened log as an attachment.
        let dsn = try requireValue(SentryDSN("https://pub@o1.example.com/42"))
        let facts = CrashReporting.Facts(release: "0.3.0", macOS: "26.1.0", model: "Mac15,8")
        let envelope = CrashReporting.envelope(log: "cod2_crash_77.txt", text: "crash : boom\n", occurred: Date(timeIntervalSince1970: 1_800_000_000),
                                               facts: facts, dsn: dsn, eventID: "0123456789abcdef0123456789abcdef", sentAt: Date(timeIntervalSince1970: 1_800_000_060))
        let event = #"{"contexts":{"device":{"model":"Mac15,8","type":"device"},"os":{"name":"macOS","type":"os","version":"26.1.0"}},"event_id":"0123456789abcdef0123456789abcdef","level":"fatal","logentry":{"formatted":"boom"},"platform":"native","release":"0.3.0","sdk":{"name":"cod2-silicon.launcher","settings":{"infer_ip":"never"},"version":"0.3.0"},"tags":{"engine":"original","mac_model":"Mac15,8","macos":"26.1.0"},"timestamp":1800000000}"#
        let expected = #"{"dsn":"https://pub@o1.example.com/42","event_id":"0123456789abcdef0123456789abcdef","sent_at":"2027-01-15T08:01:00Z"}"# + "\n"
            + #"{"length":\#(event.utf8.count),"type":"event"}"# + "\n" + event + "\n"
            + #"{"content_type":"text/plain","filename":"cod2_crash_77.txt","length":13,"type":"attachment"}"# + "\ncrash : boom\n\n"
        precondition(envelope == Data(expected.utf8), String(decoding: envelope, as: UTF8.self))

        // After a game: CoD2x's own log (its pending report beside it) is never opened; ours goes once.
        let home = scratch.appendingPathComponent("report home"), started = Date().addingTimeInterval(-1)
        try fm.createDirectory(at: home, withIntermediateDirectories: true)
        let theirs = home.appendingPathComponent("cod2x_crash.20261110_120000.501.log"), ours = home.appendingPathComponent("cod2x_freeze.20261110_120005.501.log")
        try Data("=== COD2X CRASH ===\nSignal: 11 SIGSEGV\nCOD2X-ONLY\n".utf8).write(to: theirs)
        try Data("{}".utf8).write(to: home.appendingPathComponent(theirs.lastPathComponent + ".pending.json"))
        try Data("=== COD2X FREEZE ===\nReason: main thread heartbeat stopped\n/Users/jane.doe/Library/x\ncommand : +set name Jane\n".utf8).write(to: ours)
        try fm.setAttributes([.posixPermissions: 0], ofItemAtPath: theirs.path)  // opening it would fail the send
        defer { try? fm.setAttributes([.posixPermissions: 0o600], ofItemAtPath: theirs.path) }
        let logs = CrashReports.all(in: home, since: started)
        precondition(logs.map(\.lastPathComponent).sorted() == [theirs.lastPathComponent, ours.lastPathComponent], "\(logs)")
        precondition(CrashReporting.owner(of: theirs) == .cod2x && CrashReporting.owner(of: ours) == .ours)
        precondition(CrashReporting.owner(of: home.appendingPathComponent("cod2_crash_5.txt")) == .ours)
        let standIn = try StandIn(statuses: [503, 200])
        let local = try requireValue(SentryDSN("http://pub@127.0.0.1:\(standIn.port)/42"))
        func run(_ sendOurs: Bool, _ cod2x: Bool, _ dsn: SentryDSN?, _ only: [URL]? = nil) -> CrashReporting.Status? {
            let logs = only ?? logs
            return wait { await CrashReporting.report(logs: logs, home: home, sendOurs: sendOurs, cod2xReports: cod2x, dsn: dsn, facts: facts) }
        }
        precondition(run(false, false, local) == nil && run(true, false, nil) == nil && run(false, true, local) == .cod2x && standIn.requests.isEmpty,
                     "nothing leaves with sending off or without a receiver")
        precondition(run(true, true, local, [theirs]) == .cod2x && standIn.requests.isEmpty, "CoD2x's own is never ours to send")
        precondition(run(true, false, local) == .notSent(.refused) && standIn.requests.count == 1 && CrashReporting.sentDigests(home: home).isEmpty)
        precondition(run(true, false, local) == .sent && standIn.requests.count == 2 && CrashReporting.sentDigests(home: home).count == 1)
        precondition(run(true, false, local) == nil && run(true, true, local) == nil && standIn.requests.count == 2, "never sent twice")
        let raw = standIn.requests[1], split = try requireValue(raw.range(of: Data("\r\n\r\n".utf8)))
        let head = String(decoding: raw[..<split.lowerBound], as: UTF8.self).components(separatedBy: "\r\n")
        let body = Data(raw[split.upperBound...])
        precondition(head[0] == "POST /api/42/envelope/ HTTP/1.1", head[0])
        precondition(head.contains("X-Sentry-Auth: Sentry sentry_version=7, sentry_client=cod2-silicon-launcher/0.3.0, sentry_key=pub"), "\(head)")
        precondition(head.contains("Content-Type: application/x-sentry-envelope") && head.contains("User-Agent: CoD2-Silicon-Launcher"), "\(head)")
        print("Sentry stand-in received: " + head.joined(separator: " | ") + " | body \(body.count) bytes")
        let lines = body.split(separator: UInt8(ascii: "\n"), omittingEmptySubsequences: false).map { Data($0) }
        let header = try requireValue(JSONSerialization.jsonObject(with: lines[0]) as? [String: Any])
        let item = try requireValue(JSONSerialization.jsonObject(with: lines[2]) as? [String: Any])
        precondition(header["dsn"] as? String == local.text && (header["event_id"] as? String)?.count == 32 && header["sent_at"] != nil)
        precondition(item["level"] as? String == "fatal" && item["user"] == nil && (item["logentry"] as? [String: String])?["formatted"] == "main thread heartbeat stopped")
        precondition((item["tags"] as? [String: String])?["engine"] == "cod2x" && item["release"] as? String == "0.3.0")
        let attachment = String(decoding: body, as: UTF8.self)
        precondition(attachment.contains("\n=== COD2X FREEZE ===\nReason: main thread heartbeat stopped\n~/Library/x\n\n") && !attachment.contains("Jane") && !attachment.contains("COD2X-ONLY"))
        precondition(body.last == UInt8(ascii: "\n"))

        // A report that might hold a CD key stays on this Mac and is not recorded as sent.
        let keyed = home.appendingPathComponent("cod2_crash_88.txt")
        try Data("crash : x\nname 0000-0000-0000-0000-86D3\n".utf8).write(to: keyed)
        precondition(run(true, false, local, [keyed]) == .notSent(.key) && standIn.requests.count == 2)
        precondition(CrashReporting.Status.sent.message == "Report sent." && CrashReporting.Status.notSent(.key).message == "Report not sent: it might contain your CD key, so it stayed on this Mac.")
    }

    /// Settings › Game › CoD2x files, on synthetic DLLs and releases (no network): CoD2x's archive found by the
    /// engine's rule, the release pinned by size and hash, and the app home's copy kept for CoD2x only.
    static func cod2xFiles(scratch: URL) throws {
        let fm = FileManager.default
        let iwd = zip([("ui_mp/main.menu", Data("{ menuDef }".utf8), true), ("materials/radar_player_arrow", Data(repeating: 7, count: 130), false),
                       ("images/radar_player_arrow.iwi", Data(repeating: 1, count: 4096), true)])
        let other = zip([("readme.txt", Data("not CoD2x".utf8), false)])
        let cod2xDLL = dll([other, iwd])
        precondition((try? CoD2xFiles.archive(fromDLL: cod2xDLL)) == iwd, "the embedded archive byte for byte; an unrelated ZIP beside it doesn't count")
        let shouted = zip([("UI_MP\\Main.menu", Data("x".utf8), false), ("Materials\\Radar_Player_Arrow", Data("y".utf8), false)])
        precondition((try? CoD2xFiles.archive(fromDLL: dll([shouted]))) == shouted, "names compare as the engine compares them")
        for (bad, why) in [(dll([]), "a stock Miles DLL holds no archive"), (dll([other]), "an archive without CoD2x's menu and arrow"),
                           (dll([iwd, iwd]), "two archives"), (iwd, "not a Windows DLL"), (Data(count: 63), "too short")] {
            do { _ = try CoD2xFiles.archive(fromDLL: bad); preconditionFailure(why) } catch { precondition(error as? CoD2xFiles.Failure == .noCoD2x, why) }
        }

        // The release: mss32.dll (not mss32_original.dll) once the size and SHA-256 match; anything else is refused.
        let release = zip([("CoD2x Installation and uninstallation manual.txt", Data("read me".utf8), true), ("mss32.dll", cod2xDLL, true),
                           ("mss32_original.dll", dll([]), true)])
        let size = release.count, hash = CoD2xFiles.sha256(release)
        precondition((try? CoD2xFiles.dll(fromRelease: release, size: size, sha256: hash)) == cod2xDLL)
        var flipped = release; flipped[flipped.count / 2] ^= 0xff
        for (bad, size, hash, why) in [(release, CoD2xFiles.releaseSize, CoD2xFiles.releaseSHA256, "not the pinned release"), (release + Data([0]), size, hash, "another size"),
                                       (flipped, size, hash, "another hash"), (Data(count: CoD2xFiles.releaseSize), CoD2xFiles.releaseSize, CoD2xFiles.releaseSHA256, "the pinned size, another hash")] {
            do { _ = try CoD2xFiles.dll(fromRelease: bad, size: size, sha256: hash); preconditionFailure(why) } catch { precondition(error as? CoD2xFiles.Failure == .mismatch, why) }
        }
        precondition(CoD2xFiles.releaseSize == 2_577_745 && CoD2xFiles.releaseURL.absoluteString.hasSuffix("/v1.4.6.8/CoD2x_1.4.6.8_windows.zip") && CoD2xFiles.version == "1.4.6.8")
        let refused = scratch.appendingPathComponent("refused home"), downloaded = scratch.appendingPathComponent("release home")
        do { try CoD2xFiles.install(release: release, home: refused); preconditionFailure("unpinned") } catch {
            precondition(CoD2xFiles.message(error) == "The download didn’t match CoD2x 1.4.6.8, so it wasn’t used." && !fm.fileExists(atPath: refused.path))
        }
        try CoD2xFiles.install(release: release, home: downloaded, size: size, sha256: hash)
        precondition((try? fm.subpathsOfDirectory(atPath: downloaded.path))?.sorted() == ["cod2x", "cod2x/iw_CoD2x_01.iwd"],
                     "only the archive is kept: no release, no DLL")
        precondition((try? Data(contentsOf: CoD2xFiles.stored(home: downloaded))) == iwd)

        // Choose Folder: a Windows game folder's mss32.dll, read and never changed.
        let windows = scratch.appendingPathComponent("Windows CoD2"), home = scratch.appendingPathComponent("app home")
        try fm.createDirectory(at: windows.appendingPathComponent("main"), withIntermediateDirectories: true)
        do { try CoD2xFiles.install(folder: windows, home: home); preconditionFailure("no DLL") } catch { precondition(error as? CoD2xFiles.Failure == .noCoD2x) }
        try dll([]).write(to: windows.appendingPathComponent("mss32.dll"))
        do { try CoD2xFiles.install(folder: windows, home: home); preconditionFailure("stock DLL") } catch {
            precondition(CoD2xFiles.message(error) == "This folder doesn’t have CoD2x installed.", "a stock Miles DLL says the folder has no CoD2x")
        }
        try cod2xDLL.write(to: windows.appendingPathComponent("mss32.dll"))
        let windowsBefore = tree(windows)
        precondition(CoD2xFiles.status(home: home, dataPath: "") == (false, CoD2xFiles.Message.notInstalled))
        try CoD2xFiles.install(folder: windows, home: home)
        precondition((try? Data(contentsOf: CoD2xFiles.stored(home: home))) == iwd && tree(windows) == windowsBefore)
        precondition(CoD2xFiles.status(home: home, dataPath: "") == (true, "Installed · from your CoD2x folder"), "only the pinned archive is called 1.4.6.8")
        precondition(CoD2xFiles.Message.installed == "Installed · 1.4.6.8")

        // Before a game: present in the app home's main for CoD2x, absent for Original 1.3; the game data folder is never written.
        let data = scratch.appendingPathComponent("Game Data"), fresh = scratch.appendingPathComponent("fresh home"), placed = CoD2xFiles.placed(home: home)
        try fm.createDirectory(at: data.appendingPathComponent("main"), withIntermediateDirectories: true)
        try Data("stock".utf8).write(to: data.appendingPathComponent("main/iw_00.iwd"))
        let game = tree(data)
        precondition(CoD2xFiles.prepare(for: .cod2x, home: fresh, dataPath: data.path) == .none && !fm.fileExists(atPath: fresh.path), "no files: the game still starts")
        precondition(CoD2xFiles.prepare(for: .cod2x, home: home, dataPath: data.path) == .placed && (try? Data(contentsOf: placed)) == iwd)
        precondition(CoD2xFiles.prepare(for: .cod2x, home: home, dataPath: data.path) == .inPlace)
        try Data("older".utf8).write(to: placed)
        precondition(CoD2xFiles.prepare(for: .cod2x, home: home, dataPath: data.path) == .placed && (try? Data(contentsOf: placed)) == iwd, "a different copy is replaced")
        precondition(CoD2xFiles.prepare(for: .original, home: home, dataPath: data.path) == .removed && !fm.fileExists(atPath: placed.path))
        precondition(CoD2xFiles.prepare(for: .original, home: home, dataPath: data.path) == .none)
        try Data("the player's own".utf8).write(to: placed)
        precondition(CoD2xFiles.prepare(for: .original, home: home, dataPath: data.path) == .none && fm.fileExists(atPath: placed.path), "only a copy identical to the kept one goes")
        try fm.removeItem(at: placed)
        precondition(tree(data) == game, "the game data folder is never written")
        // A Windows install with CoD2x has its own copy in the game data folder, which stays in charge for both engines.
        _ = CoD2xFiles.prepare(for: .cod2x, home: home, dataPath: data.path)
        try iwd.write(to: data.appendingPathComponent("main/iw_CoD2x_01.iwd"))
        let withCoD2x = tree(data)
        for engine in [GameEngine.cod2x, .original] {
            precondition(CoD2xFiles.prepare(for: engine, home: home, dataPath: data.path) == .gameFolder && !fm.fileExists(atPath: placed.path))
        }
        precondition(tree(data) == withCoD2x, "the game data folder is never written")
        precondition(CoD2xFiles.status(home: fresh, dataPath: data.path) == (false, "Found in your game folder"))
        try fm.removeItem(at: data.appendingPathComponent("main/iw_CoD2x_01.iwd"))
        let gameAgain = tree(data)

        // Remove: the kept archive and its identical copy only.
        precondition(CoD2xFiles.prepare(for: .cod2x, home: home, dataPath: data.path) == .placed)
        try CoD2xFiles.remove(home: home)
        precondition(!fm.fileExists(atPath: CoD2xFiles.stored(home: home).path) && !fm.fileExists(atPath: placed.path))
        precondition(CoD2xFiles.status(home: home, dataPath: data.path) == (false, "Not installed") && tree(data) == gameAgain)
    }
    /// A small ZIP, each entry stored or raw-deflated (CRCs are left 0; neither reader checks them).
    static func zip(_ entries: [(name: String, data: Data, deflate: Bool)]) -> Data {
        func le16(_ v: Int) -> [UInt8] { [UInt8(v & 0xff), UInt8(v >> 8 & 0xff)] }
        func le32(_ v: Int) -> [UInt8] { le16(v & 0xffff) + le16(v >> 16 & 0xffff) }
        var body: [UInt8] = [], directory: [UInt8] = []
        for entry in entries {
            let payload = entry.deflate ? [UInt8]((try! (entry.data as NSData).compressed(using: .zlib)) as Data) : [UInt8](entry.data)
            let name = Array(entry.name.utf8), method = entry.deflate ? 8 : 0, offset = body.count
            let sizes = [le32(0), le32(payload.count), le32(entry.data.count), le16(name.count)].flatMap { $0 }
            body += [le32(0x04034b50), le16(20), le16(0), le16(method), le16(0), le16(0), sizes, le16(0), name, payload].flatMap { $0 }
            directory += [le32(0x02014b50), le16(20), le16(20), le16(0), le16(method), le16(0), le16(0), sizes, le16(0), le16(0),
                          le16(0), le16(0), le32(0), le32(offset), name].flatMap { $0 }
        }
        return Data([le32(0x06054b50), le16(0), le16(0), le16(entries.count), le16(entries.count), le32(directory.count), le32(body.count), le16(0)]
            .reduce(body + directory, +))
    }
    /// A Windows DLL as the engine checks one (MZ, PE at the offset in the header), padding, then each ZIP.
    static func dll(_ zips: [Data]) -> Data {
        var bytes = [UInt8](repeating: 0xcc, count: 128)
        bytes[0] = 0x4d; bytes[1] = 0x5a; bytes[60] = 64; bytes[61] = 0; bytes[62] = 0; bytes[63] = 0
        bytes.replaceSubrange(64..<68, with: [0x50, 0x45, 0, 0])
        for zip in zips { bytes += [UInt8](zip) + [UInt8](repeating: 0x90, count: 333) }
        return Data(bytes)
    }
    /// Every file and folder under a directory with its size, modification time and SHA-256, so any write shows.
    static func tree(_ root: URL) -> [String: String] {
        let keys: [URLResourceKey] = [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey]
        func entry(_ url: URL) -> String {
            guard let values = try? url.resourceValues(forKeys: Set(keys)) else { return "unreadable" }
            let stamp = values.contentModificationDate?.timeIntervalSince1970 ?? 0
            guard values.isDirectory != true else { return "folder \(stamp)" }
            return "\(values.fileSize ?? -1) \(stamp) " + ((try? Data(contentsOf: url)).map(CoD2xFiles.sha256) ?? "unreadable")
        }
        var result = [root.path: entry(root)]
        guard let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: keys) else { return result }
        for case let url as URL in walk { result[url.path] = entry(url) }
        return result
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

/// Runs async work to completion from the synchronous test.
func wait<T: Sendable>(_ work: @escaping @Sendable () async -> T) -> T {
    let box = AsyncBox<T>(), done = DispatchSemaphore(value: 0)
    Task.detached { box.value = await work(); done.signal() }
    done.wait()
    return box.value!
}
final class AsyncBox<T>: @unchecked Sendable { var value: T? }

/// A stand-in receiver on 127.0.0.1: records each request and answers with the next status (then 200).
final class StandIn: @unchecked Sendable {
    let port: Int
    private let lock = NSLock()
    private var statuses: [Int]
    private var received: [Data] = []
    var requests: [Data] { lock.lock(); defer { lock.unlock() }; return received }
    init(statuses: [Int]) throws {
        self.statuses = statuses
        let fd = socket(AF_INET, SOCK_STREAM, 0)
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size); address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1"); address.sin_port = 0
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        let bound = withUnsafeMutablePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { bind(fd, $0, length) == 0 && listen(fd, 8) == 0 && getsockname(fd, $0, &length) == 0 }
        }
        guard fd >= 0, bound else { throw LauncherError(message: "stand-in receiver") }
        port = Int(UInt16(bigEndian: address.sin_port))
        Thread.detachNewThread { [self] in
            while true {
                let client = accept(fd, nil, nil)
                if client < 0 { return }
                var data = Data(), buffer = [UInt8](repeating: 0, count: 65536)
                while true {
                    if let end = data.range(of: Data("\r\n\r\n".utf8)) {
                        let head = String(decoding: data[..<end.lowerBound], as: UTF8.self).lowercased().components(separatedBy: "\r\n")
                        let length = head.first { $0.hasPrefix("content-length:") }.flatMap { Int($0.dropFirst(15).trimmingCharacters(in: .whitespaces)) } ?? 0
                        if data.count >= end.upperBound + length { break }
                    }
                    let count = read(client, &buffer, buffer.count)
                    if count <= 0 { break }
                    data.append(contentsOf: buffer[0..<count])
                }
                lock.lock(); received.append(data); let status = self.statuses.isEmpty ? 200 : self.statuses.removeFirst(); lock.unlock()
                let reply = "HTTP/1.1 \(status) Stand-in\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}"
                _ = reply.withCString { write(client, $0, strlen($0)) }
                close(client)
            }
        }
    }
}
