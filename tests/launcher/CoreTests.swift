import Foundation

@main struct CoreTests {
    static func main() throws {
        try displayResolutions()
        let fixtures = URL(fileURLWithPath: CommandLine.arguments[1])
        precondition(Dispatch.bundled.first?.title == "CoD2 Unbound 0.4.0", "bundled news must describe the shipping release")
        precondition(!Dispatch.bundled.contains { $0.summary.contains("Planned for 0.3") }, "future work has no promised version")
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
        precondition(FrameCap.customCaption(0) == "Unlimited" && FrameCap.customCaption(90) == "Custom value")
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
        try sessionLogs(scratch: scratch)
        try engines(server: server, scratch: scratch)
        try cod2xFiles(scratch: scratch)
        try crashReporting(scratch: scratch)
        try appUpdate(scratch: scratch)
        print("PASS: install update: the release's archive and SHA256SUMS from this project only, one checksum line, any size or SHA-256 mismatch refused, refusals while a game runs or where the app can't replace itself, synthetic bundles checked for identifier, version, signature and Team ID, and the swap on a copy with the old app kept until the new one is in place")
        try presentation(fixtures)
        // The launcher names the CoD2x its engine really is: the engine header's own release string.
        let header = try String(contentsOf: fixtures.appendingPathComponent("../../../src/PC/qcommon/cod2x.h").standardizedFileURL, encoding: .utf8)
        let define = header.components(separatedBy: "\n").first { $0.hasPrefix("#define COD2X_RELEASE ") } ?? ""
        precondition(define == "#define COD2X_RELEASE \"\(CoD2xFiles.engineRelease)\"", "CoD2xFiles.engineRelease must be the engine's COD2X_RELEASE: \(define)")
        precondition(CoD2xFiles.engineTitle == "CoD2x " + CoD2xFiles.engineRelease && CoD2xFiles.engineRelease == CoD2xFiles.version, "the engine runs as the release whose files are downloaded")
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
        // Each engine draws with Metal when the app carries that build, and with OpenGL when it doesn't.
        precondition(GameSettings().renderer == .metal)
        precondition(GameEngine.cod2x.renderer(.metal, in: game) == .openGL && GameEngine.cod2x.executable(in: game).lastPathComponent == "cod2_macos")
        FileManager.default.createFile(atPath: macos.appendingPathComponent("cod2_macos_metal").path, contents: Data(), attributes: [.posixPermissions: 0o755])
        precondition(GameEngine.cod2x.renderer(.metal, in: game) == .metal && GameEngine.cod2x.renderer(.openGL, in: game) == .openGL)
        precondition(GameEngine.original.renderer(.metal, in: game) == .openGL, "the original 1.3 has no Metal build here")
        precondition(GameEngine.original.executable(in: game, renderer: .metal).lastPathComponent == "cod2_macos_stock_metal")
        let olderSettings = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"fps":200,"renderer":"vulkan"}"#.utf8))
        precondition(olderSettings.renderer == .metal && olderSettings.fps == 200, "a renderer this version doesn't know falls back without losing the rest")
        // Render scale: a Metal-only percentage, passed only below full size and only to the Metal game.
        precondition(GameSettings().renderScale == 100 && olderSettings.renderScale == 100, "settings without the render scale read as full size")
        let oddScale = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"fps":200,"renderScale":60}"#.utf8))
        precondition(oddScale.renderScale == 100 && oddScale.fps == 200, "a render scale not on the list reads as full size")
        let listedScale = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"renderScale":67}"#.utf8))
        precondition(listedScale.renderScale == 67)
        let flat: ([(String, String)]) -> [String] = { $0.map { "\($0.0) \($0.1)" } }
        let fullDvars = flat(try GameSettings().launchDvars(migrated: false))
        let fullMetal = flat(try GameSettings().launchDvars(migrated: false, metal: true))
        precondition(fullMetal == fullDvars, "Full adds nothing")
        var scaled = GameSettings(); scaled.renderScale = 75
        let scaledMetal = flat(try scaled.launchDvars(migrated: false, metal: true))
        precondition(scaledMetal == fullDvars + ["r_renderScale 75"], "75% on Metal adds exactly one r_renderScale 75")
        let scaledMigrated = flat(try scaled.launchDvars(migrated: true, metal: true)), fullMigrated = flat(try GameSettings().launchDvars(migrated: true))
        precondition(scaledMigrated == fullMigrated + ["r_renderScale 75"], "with loadouts too")
        let scaledGL = flat(try scaled.launchDvars(migrated: false, metal: false))
        precondition(scaledGL == fullDvars, "OpenGL adds nothing")
        let scaledConfig = try scaled.launchConfig(migrated: false)
        precondition(!scaledConfig.lowercased().contains("r_renderscale"), "launcher.cfg never carries the render scale")
        let scaledPlan = try GameLaunchPlan(settings: scaled, dataPath: "/Game", homePath: "/Home", metal: true)
        precondition(scaledPlan.arguments.joined(separator: " ").contains("+set r_renderScale \"75\""))
        let scaledGLPlan = try GameLaunchPlan(settings: scaled, dataPath: "/Game", homePath: "/Home")
        let fullPlan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: "/Home", metal: true)
        precondition(scaledGLPlan.arguments == fullPlan.arguments, "Full or OpenGL: the command line is unchanged")

        // Original 1.3 joins stock 1.3 servers only; CoD2x joins both.
        var codx = stock; codx.fields["shortversion"] = "1.4.6.8"
        precondition(GameEngine.original.canJoin(stock) && !GameEngine.original.canJoin(codx))
        precondition(GameEngine.cod2x.canJoin(stock) && GameEngine.cod2x.canJoin(codx))
        let secret = try LaunchLink.direct("203.0.113.24:28960", password: "hunter2")
        precondition(GameEngine.cod2x.check(server: codx, lookedUp: false) == .go)
        precondition(GameEngine.original.check(server: stock, lookedUp: false) == .go)
        precondition(GameEngine.original.check(server: codx, lookedUp: false) == .needsCoD2x)
        precondition(GameEngine.original.check(server: nil, lookedUp: false) == .lookUp)
        precondition(GameEngine.original.check(server: nil, lookedUp: true) == .go, "an unanswered server is the game's to try")

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
            var environment = ["COD2_UNBOUND_COD2X_REPORTS": "1", "HOME": "/h"]
            CrashReporting.applyConsent(to: &environment, engine: engine, cod2xReports: on)
            precondition(environment["COD2_UNBOUND_COD2X_REPORTS"] == expected && environment["HOME"] == "/h", "\(engine) \(on)")
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
            ================ CoD2 Unbound CRASH REPORT ================

            crash      : SIGSEGV (Segmentation fault) at 0x0
            executable : /Users/jane.doe/Applications/CoD2 Unbound.app/Contents/Helpers/CoD2 Game.app/Contents/MacOS/cod2_macos_stock
            command    : +set fs_homepath "/Users/jane.doe/Library/Application Support/CoD2 Unbound" +set name Jane
              stack@rsp: 0x0000000102345678 0000000000000000 000000016fdff000
            X0 =0x0000000000000001  X1 =0x000000016fdfe9c8
            0x00000001a2b3c4d5 /Users/jane.doe/Library/Frameworks/SDL3.framework/SDL3
            """
        let shortened = try CrashReporting.redact(original, home: "/Users/jane.doe")
        precondition(!shortened.contains("jane") && !shortened.contains("Jane") && !shortened.contains("command") && !shortened.contains("fs_homepath"), shortened)
        precondition(shortened.contains("executable : ~/Applications/CoD2 Unbound.app") && shortened.contains("0x00000001a2b3c4d5 ~/Library/Frameworks/SDL3.framework/SDL3"))
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
        let event = #"{"contexts":{"device":{"model":"Mac15,8","type":"device"},"os":{"name":"macOS","type":"os","version":"26.1.0"}},"event_id":"0123456789abcdef0123456789abcdef","level":"fatal","logentry":{"formatted":"boom"},"platform":"native","release":"0.3.0","sdk":{"name":"cod2-unbound.launcher","settings":{"infer_ip":"never"},"version":"0.3.0"},"tags":{"engine":"original","mac_model":"Mac15,8","macos":"26.1.0"},"timestamp":1800000000}"#
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
        precondition(head.contains("X-Sentry-Auth: Sentry sentry_version=7, sentry_client=cod2-unbound-launcher/0.3.0, sentry_key=pub"), "\(head)")
        precondition(head.contains("Content-Type: application/x-sentry-envelope") && head.contains("User-Agent: CoD2-Unbound-Launcher"), "\(head)")
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

    /// Install Update, without the network: what is offered, the checksum and size checks, the refusals,
    /// synthetic app bundles signed here ad hoc, and the swap on copies in a scratch folder.
    static func appUpdate(scratch: URL) throws {
        let fm = FileManager.default, root = scratch.appendingPathComponent("update")
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        func expect(_ failure: AppUpdate.Failure, _ why: String, _ work: () throws -> Void) {
            do { try work(); preconditionFailure(why) } catch { precondition(error as? AppUpdate.Failure == failure, "\(why): \(error)") }
        }
        // What the update check offers: the newest non-prerelease release's archive for this Mac and its SHA256SUMS.
        let base = "https://github.com/cod2unbound/cod2-unbound/releases/download/v0.4.0/"
        let feed = try ReleaseNotes.parse(Data(#"""
            [{"tag_name":"v0.5.0-beta.1","prerelease":true,"assets":[{"name":"CoD2-Unbound-0.5.0-macos-arm64.zip","size":9,"browser_download_url":"https://github.com/x"}]},
             {"tag_name":"v0.4.0","prerelease":false,"assets":[{"name":"CoD2-Unbound-0.4.0-macos-arm64.zip","size":123456,"browser_download_url":"\#(base)CoD2-Unbound-0.4.0-macos-arm64.zip"},
              {"name":"SHA256SUMS","size":100,"browser_download_url":"\#(base)SHA256SUMS"}]}]
            """#.utf8))
        precondition(feed.latestVersion == "0.4.0" && feed.latestAssets.count == 2)
        let offer = try requireValue(AppUpdate.offer(version: "0.4.0", assets: feed.latestAssets))
        precondition(offer.archiveSize == 123456 && offer.archive.lastPathComponent == "CoD2-Unbound-0.4.0-macos-arm64.zip" && offer.sums.lastPathComponent == "SHA256SUMS")
        let zipAsset = feed.latestAssets[0], sumsAsset = feed.latestAssets[1]
        var foreign = zipAsset; foreign.url = URL(string: "https://example.com/cod2unbound/cod2-unbound/releases/download/v0.4.0/CoD2-Unbound-0.4.0-macos-arm64.zip")!
        var other = zipAsset; other.url = URL(string: "https://github.com/someone/else/releases/download/v0.4.0/CoD2-Unbound-0.4.0-macos-arm64.zip")!
        var huge = zipAsset; huge.size = AppUpdate.archiveLimit + 1
        // The same repository at the addresses it had before is still this project.
        for root in ["https://github.com/STiXzoOR/cod2-silicon", "https://github.com/cod2unbound/cod2-silicon"] {
            var zip = zipAsset, sums = sumsAsset
            zip.url = URL(string: root + "/releases/download/v0.4.0/CoD2-Unbound-0.4.0-macos-arm64.zip")!
            sums.url = URL(string: root + "/releases/download/v0.4.0/SHA256SUMS")!
            precondition(AppUpdate.offer(version: "0.4.0", assets: [zip, sums])?.archive == zip.url, root)
        }
        for (assets, why) in [([zipAsset], "no SHA256SUMS"), ([sumsAsset], "no archive"), ([foreign, sumsAsset], "another host"), ([other, sumsAsset], "another project"),
                              ([zipAsset, zipAsset, sumsAsset], "two archives"), ([huge, sumsAsset], "too large")] as [([ReleaseAsset], String)] {
            precondition(AppUpdate.offer(version: "0.4.0", assets: assets) == nil, why)
        }
        precondition(AppUpdate.offer(version: "0.4", assets: feed.latestAssets) == nil && AppUpdate.offer(version: "0.4.1", assets: feed.latestAssets) == nil)

        // SHA256SUMS: exactly one line for the archive, as shasum writes it.
        let name = offer.archiveName, hex = String(repeating: "ab", count: 32)
        let plain = try AppUpdate.checksum(Data("\(hex)  \(name)\n".utf8), name: name)
        let binary = try AppUpdate.checksum(Data("\(String(repeating: "0", count: 64))  other.zip\r\n\(hex.uppercased()) *\(name)\r\n".utf8), name: name)
        precondition(plain == hex && binary == hex)
        for (text, why) in [("", "empty"), ("\(hex)  other.zip\n", "missing"), ("\(hex)  \(name)\n\(hex)  \(name)\n", "twice"), ("\(hex.dropLast())  \(name)\n", "short"),
                            ("\(hex.dropLast())g  \(name)\n", "not hex"), ("\(hex) \(name)\n", "one space"), ("\(hex)  \(name).sig\n", "another name")] {
            expect(.mismatch, why) { _ = try AppUpdate.checksum(Data(text.utf8), name: name) }
        }
        expect(.mismatch, "too large") { _ = try AppUpdate.checksum(Data(repeating: 0x20, count: AppUpdate.sumsLimit + 1), name: name) }

        // The archive: the listed size and the listed SHA-256, both.
        let archive = root.appendingPathComponent(name), bytes = Data("synthetic archive".utf8)
        try bytes.write(to: archive)
        let digest = CoD2xFiles.sha256(bytes)
        try AppUpdate.verify(archive, size: bytes.count, sha256: digest.uppercased())
        expect(.mismatch, "size") { try AppUpdate.verify(archive, size: bytes.count + 1, sha256: digest) }
        expect(.mismatch, "hash") { try AppUpdate.verify(archive, size: bytes.count, sha256: String(digest.reversed())) }
        expect(.mismatch, "missing") { try AppUpdate.verify(root.appendingPathComponent("none.zip"), size: 0, sha256: digest) }

        // Refusals: a running game, a translocated app, a folder that can't be written.
        let folder = root.appendingPathComponent("Applications"), app = folder.appendingPathComponent("CoD2 Unbound.app")
        func makeApp(_ url: URL, id: String = "com.cod2unbound.app.test", version: String = "0.4.0", marker: String = "new") throws {
            let macos = url.appendingPathComponent("Contents/MacOS")
            try fm.createDirectory(at: macos, withIntermediateDirectories: true)
            try fm.copyItem(at: URL(fileURLWithPath: "/usr/bin/true"), to: macos.appendingPathComponent("CoD2Launcher"))
            let info: [String: Any] = ["CFBundleExecutable": "CoD2Launcher", "CFBundleIdentifier": id, "CFBundleShortVersionString": version, "CFBundlePackageType": "APPL"]
            try PropertyListSerialization.data(fromPropertyList: info, format: .xml, options: 0).write(to: url.appendingPathComponent("Contents/Info.plist"))
            try fm.createDirectory(at: url.appendingPathComponent("Contents/Resources"), withIntermediateDirectories: true)
            try Data(marker.utf8).write(to: url.appendingPathComponent("Contents/Resources/marker"))
            precondition(AppUpdate.run("/usr/bin/codesign", ["--force", "--sign", "-", url.path]) == 0, "ad hoc signing")
        }
        try makeApp(app, version: "0.3.0", marker: "old")
        precondition(AppUpdate.refusal(app: app, gameRunning: false) == nil)
        precondition(AppUpdate.refusal(app: app, gameRunning: true) == .gameRunning)
        // A hosted server would be stopped by the restart: the update waits until the servers are stopped.
        precondition(AppUpdate.refusal(app: app, gameRunning: false, serversRunning: true) == .serversRunning, "an update is refused while hosted servers run")
        precondition(AppUpdate.Failure.serversRunning.errorDescription?.contains("Servers › Hosting") == true && !AppUpdate.Failure.serversRunning.opensReleases)
        precondition(AppUpdate.refusal(app: URL(fileURLWithPath: "/private/var/folders/xy/T/AppTranslocation/1234-5678/d/CoD2 Unbound.app"), gameRunning: false) == .readOnly)
        try fm.setAttributes([.posixPermissions: 0o555], ofItemAtPath: folder.path)
        precondition(AppUpdate.refusal(app: app, gameRunning: false) == .notWritable)
        try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: folder.path)
        precondition(AppUpdate.Failure.readOnly.opensReleases && AppUpdate.Failure.notWritable.opensReleases && !AppUpdate.Failure.mismatch.opensReleases)

        // The unpacked app: one app inside, this app's identifier, the offered version, a valid signature, the same Team ID.
        let staging = root.appendingPathComponent("staging"), built = root.appendingPathComponent("built")
        try fm.createDirectory(at: staging, withIntermediateDirectories: true)
        try makeApp(built.appendingPathComponent("CoD2 Unbound.app"))
        let release = staging.appendingPathComponent(name)
        precondition(AppUpdate.run("/usr/bin/ditto", ["-c", "-k", "--keepParent", built.appendingPathComponent("CoD2 Unbound.app").path, release.path]) == 0)
        let candidate = try AppUpdate.unpack(release, in: staging)
        precondition(candidate.lastPathComponent == "CoD2 Unbound.app")
        try AppUpdate.check(candidate, bundleID: "com.cod2unbound.app.test", version: "0.4.0", team: nil)
        expect(.notThisApp, "another app") { try AppUpdate.check(candidate, bundleID: "io.github.someone.else", version: "0.4.0", team: nil) }
        expect(.notThisApp, "another version") { try AppUpdate.check(candidate, bundleID: "com.cod2unbound.app.test", version: "0.4.1", team: nil) }
        expect(.developer, "ad hoc, where this app has a Team ID") { try AppUpdate.check(candidate, bundleID: "com.cod2unbound.app.test", version: "0.4.0", team: "ABCDE12345") }
        precondition(AppUpdate.teamIdentifier(of: candidate) == nil)
        let tampered = root.appendingPathComponent("tampered/CoD2 Unbound.app")
        try makeApp(tampered)
        try Data("changed".utf8).write(to: tampered.appendingPathComponent("Contents/Resources/marker"))
        expect(.signature, "changed after signing") { try AppUpdate.check(tampered, bundleID: "com.cod2unbound.app.test", version: "0.4.0", team: nil) }
        let two = root.appendingPathComponent("two"), twoZip = root.appendingPathComponent("two.zip"), twoStaging = root.appendingPathComponent("two-staging")
        try makeApp(two.appendingPathComponent("A.app")); try makeApp(two.appendingPathComponent("B.app"))
        try fm.createDirectory(at: twoStaging, withIntermediateDirectories: true)
        precondition(AppUpdate.run("/usr/bin/ditto", ["-c", "-k", two.path, twoZip.path]) == 0)
        expect(.notThisApp, "two apps") { _ = try AppUpdate.unpack(twoZip, in: twoStaging) }
        try Data("not a zip".utf8).write(to: twoZip)
        expect(.mismatch, "not an archive") { _ = try AppUpdate.unpack(twoZip, in: root.appendingPathComponent("bad-staging")) }

        // The swap, on copies: the new app takes the old one's place and the old one is kept whole until then.
        var discarded: [URL] = []
        try AppUpdate.swap(app, with: candidate) { old in
            precondition((try? String(contentsOf: app.appendingPathComponent("Contents/Resources/marker"), encoding: .utf8)) == "new", "the new app is in place first")
            precondition((try? String(contentsOf: old.appendingPathComponent("Contents/Resources/marker"), encoding: .utf8)) == "old")
            discarded.append(old)
        }
        precondition(discarded.count == 1 && AppUpdate.run("/usr/bin/codesign", ["--verify", "--deep", "--strict", app.path]) == 0)
        try AppUpdate.check(app, bundleID: "com.cod2unbound.app.test", version: "0.4.0", team: nil)
        expect(.install, "nothing to put in place") { try AppUpdate.swap(app, with: staging.appendingPathComponent("missing.app")) { _ in preconditionFailure("discarded") } }
        precondition((try? String(contentsOf: app.appendingPathComponent("Contents/Resources/marker"), encoding: .utf8)) == "new", "a failed swap leaves the app as it was")
        precondition(AppUpdate.message(AppUpdate.Failure.mismatch) == "The download didn’t match the release, so it wasn’t installed.")
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
        precondition(feed.dispatches[0].date != nil && feed.dispatches[0].version == "0.2.0-beta.1")
        var first = feed.dispatches[0]; first.date = nil; first.version = nil
        precondition(first == Dispatch(stamp: "02 NOV", title: "CoD2 Unbound 0.2 beta",
            summary: "Launcher redesign with Liquid Glass navigation. Server browser favourites.",
            link: "https://github.com/cod2unbound/cod2-unbound/releases/tag/v0.2.0-beta.1"))
        precondition(feed.dispatches[1].title == "CoD2 Unbound 0.1.1" && feed.dispatches[1].link == nil && feed.dispatches[1].stamp == "20 OCT")
        precondition(feed.dispatches[1].summary.hasSuffix("…") && feed.dispatches[1].summary.count <= 161 && feed.dispatches[1].summary.hasPrefix("Fixes r_mode on 6K"))
        precondition(feed.dispatches[2].summary == "Call of Duty 2 multiplayer, native on Apple silicon. CoD2x 1.4 compatible.")
        precondition(ReleaseNotes.isNewer("0.1.10", than: "0.1.9") && !ReleaseNotes.isNewer("0.1.0", than: "0.1.0"))
        precondition((try? ReleaseNotes.parse(Data("{}".utf8))) == nil && (try? ReleaseNotes.parse(Data(count: 1_000_001))) == nil)
        let empty = try ReleaseNotes.parse(Data("[]".utf8))
        precondition(empty.dispatches.isEmpty && empty.latestVersion == nil)

        // The news file: plain text only, github.com links or a known place, future days hidden, newest first.
        let noon = utc.date(from: DateComponents(year: 2026, month: 11, day: 22, hour: 12))!
        let news = try NewsFeed.parse(Data(#"""
        {"items":[
          {"date":"2026-11-18","title":"Older note","summary":"First."},
          {"date":"2026-11-21","title":"  14 more\nplayer\u202e setups ","summary":"New in Configs.","open":"configs","link":"https://example.com/x"},
          {"date":"2026-11-23","title":"Tomorrow","summary":"Not yet."},
          {"date":"2026-02-31","title":"No such day"},
          {"date":"21 Nov","title":"Bad date"},
          {"date":"2026-11-20","title":"","summary":"No title"},
          {"date":"2026-11-19","title":"Linked","link":"https://github.com/cod2unbound/cod2-unbound/discussions/1","open":"terminal"}],
         "next":{"title":"A Metal renderer","summary":"In the works.","link":"https://github.com/cod2unbound/cod2-unbound/blob/main/docs/ROADMAP.md"}}
        """#.utf8), now: noon, calendar: utc)
        precondition(news.items.map(\.title) == ["14 more player setups", "Linked", "Older note"], "\(news.items.map(\.title))")
        precondition(news.items[0].stamp == "21 NOV" && news.items[0].open == "configs" && news.items[0].link == nil && news.items[0].version == nil)
        precondition(news.items[1].link == "https://github.com/cod2unbound/cod2-unbound/discussions/1" && news.items[1].open == nil && news.items[1].summary.isEmpty)
        precondition(news.next?.stamp == "NEXT" && news.next?.title == "A Metal renderer" && news.next?.date == nil)
        precondition((try? NewsFeed.parse(Data(count: 65_537))) == nil && (try? NewsFeed.parse(Data("[]".utf8))) == nil)
        let noNews = try NewsFeed.parse(Data("{}".utf8))
        precondition(noNews == NewsFeed.Feed())
        let longNews = try NewsFeed.parse(Data(("{\"items\":[{\"date\":\"2026-11-01\",\"title\":\"" + String(repeating: "word ", count: 60) + "\"}]}").utf8), now: noon, calendar: utc)
        precondition(longNews.items[0].title.count == 80 && longNews.items[0].title.hasSuffix("…"))
        let shipped = try NewsFeed.parse(try Data(contentsOf: URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("../../docs/news.json")))
        precondition(shipped.next != nil, "the news file in this repository must parse")

        // What’s New on Home: this version’s line is always there, newest first, what comes next last.
        let builtIn = HomeDispatches.builtIn(version: "0.3.0")
        precondition(HomeDispatches.list(version: "0.3.0", releases: []) == [builtIn] && builtIn.stamp == "NOW" && builtIn.date == nil)
        func dated(_ title: String, _ day: Int, version: String? = nil) -> Dispatch {
            Dispatch(stamp: "\(day) NOV", title: title, summary: "", link: nil, date: utc.date(from: DateComponents(year: 2026, month: 11, day: day)), version: version)
        }
        let mine = dated("CoD2 Unbound 0.3.0", 18, version: "0.3.0"), newer = dated("CoD2 Unbound 0.3.1", 25, version: "0.3.1")
        let note = dated("A note", 21), late = dated("A later note", 27)
        precondition(HomeDispatches.list(version: "0.3.0", releases: [mine], news: [note], next: news.next).map(\.title) == ["A note", "CoD2 Unbound 0.3.0", "A Metal renderer"])
        precondition(HomeDispatches.list(version: "0.3.0", releases: [newer, mine], news: [late, note], next: news.next).map(\.title) == ["A later note", "CoD2 Unbound 0.3.0", "A Metal renderer"])
        precondition(HomeDispatches.list(version: "0.3.0", releases: [newer, mine], news: [late, note]).map(\.title) == ["A later note", "CoD2 Unbound 0.3.1", "CoD2 Unbound 0.3.0"])
        precondition(HomeDispatches.list(version: "0.3.9", releases: [mine], news: [note]).map(\.stamp) == ["21 NOV", "18 NOV", "NOW"])
        let stored = try JSONDecoder().decode(LauncherLibrary.self, from: Data(#"{"dispatches":[{"stamp":"02 NOV","title":"CoD2 Unbound 0.3.0","summary":"Old."}],"news":"damaged"}"#.utf8))
        precondition(stored.news.isEmpty && stored.newsNext == nil && stored.dispatches[0].date == nil)
        precondition(HomeDispatches.list(version: "0.3.0", releases: stored.dispatches).map(\.stamp) == ["02 NOV"], "a release an older build stored still stands for this version")

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
        precondition(codx.isCoD2x && codx.versionLabel == "CoD2x 1.4.6.8" && codx.occupancy == .full && codx.access == "Password")
        let stock = ServerFacts(server(["protocol": "118", "clients": "0", "sv_maxclients": "20"], ping: 88))
        precondition(!stock.isCoD2x && stock.versionLabel == "Original 1.3" && stock.occupancy == .empty && stock.pingLevel == 1)
        precondition(ServerFacts(server(["protocol": "120"])).versionLabel == "CoD2x 1.4")
        precondition([18, 39, 40, 59, 60, 79, 80].map(ServerFacts.pingLevel) == [4, 4, 3, 3, 2, 2, 1])

        // Library rows: map hints from file names, sizes and stamped dates.
        let date = try requireValue(ISO8601DateFormatter().date(from: "2026-10-04T21:14:00Z"))
        let demo = MediaFacts(name: "toujane_tdm_1004_2114.dm_1", date: date, bytes: 3_100_000, calendar: utc)
        precondition(demo.map == "toujane" && demo.size == "3.1 MB" && demo.stamp == "04 OCT 21:14")
        precondition(MediaFacts.mapHint("shot0001.jpg") == nil && MediaFacts.sizeLabel(512) == "512 B" && MediaFacts.sizeLabel(48_200) == "48 KB")
        // A recording saved as “round (1).dm_1” or “Best round.dm_1” joins the demos folder under a name the game can play.
        precondition(!DemoName.playable("round (1)") && !DemoName.playable("Best round") && DemoName.playable("toujane_tdm-1004.2114"))
        precondition(DemoName.playableName("round (1)") == "round__1_" && DemoName.playableName("Best round") == "Best_round"
                     && DemoName.playableName("café") == "caf_" && DemoName.playableName(".hidden") == "demo.hidden" && DemoName.playableName("") == "demo")
        for name in ["round (1)", "Best round", "café", ".hidden", "a+b;c\"d"] {
            let copy = DemoName.playableName(name)
            precondition(DemoName.playable(copy), "copy name \(copy) can't be played")
            let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: "/Home", demo: copy)
            precondition(plan.arguments.suffix(2) == ["+demo", copy])
        }
        precondition((try? GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: "/Home", demo: "round (1)")) == nil, "a demo name the game can't take was passed")

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
        try settingsChecks()  // ship-settings
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

// MARK: - ship-settings
extension CoreTests {
    /// Settings wording, What's New, Report a Problem and the extra launch options' +set lines.
    static func settingsChecks() throws {
        var utc = Calendar(identifier: .gregorian); utc.timeZone = TimeZone(identifier: "UTC")!
        let english = Locale(identifier: "en_GB")
        let now = try requireValue(utc.date(from: DateComponents(year: 2026, month: 11, day: 11, hour: 15)))
        func at(_ day: Int, _ hour: Int, _ minute: Int, year: Int = 2026) throws -> Date { try requireValue(utc.date(from: DateComponents(year: year, month: 11, day: day, hour: hour, minute: minute))) }
        let today = try at(11, 14, 2), yesterday = try at(10, 21, 30), earlier = try at(8, 21, 30), lastYear = try at(8, 21, 30, year: 2025)
        precondition(SettingsText.when(today, now: now, calendar: utc, locale: english) == "today at 14:02")
        precondition(SettingsText.when(yesterday, now: now, calendar: utc, locale: english) == "yesterday at 21:30")
        precondition(SettingsText.when(earlier, now: now, calendar: utc, locale: english) == "on 8 Nov at 21:30")
        precondition(SettingsText.when(lastYear, now: now, calendar: utc, locale: english) == "on 8 Nov 2025 at 21:30")
        precondition(SettingsText.resolution("2560x1440") == "2560 × 1440" && SettingsText.size("2560x1440")! == (2560, 1440) && SettingsText.size("wide") == nil)
        precondition(SettingsText.aspect(width: 2560, height: 1440) == "16:9" && SettingsText.aspect(width: 3456, height: 2234) == "1.55:1")
        precondition(SettingsText.aspect(width: 1680, height: 1050) == "16:10" && SettingsText.aspect(width: 1024, height: 768) == "4:3")
        precondition(SettingsText.maskedKey(lastFour: "7k2d") == "•••• •••• •••• •••• 7K2D")

        let notes = """
        ## CoD2 Unbound 0.3.1

        ### Added
        - **Connect to an address.** Join a server by address or `cod2x://` link,
          with ⌘L.

        ### Fixed
        - Map art is no longer stretched. See [the issue](https://example.invalid/1).
        - Setup remembers your CD key

        ### Removed
        - Old thing.
        """
        let items = ReleaseNotes.items(notes)
        precondition(items == [ReleaseNoteItem(tag: "New", title: "Connect to an address", detail: "Join a server by address or cod2x:// link, with ⌘L."),
                               ReleaseNoteItem(tag: "Fixed", title: "Map art is no longer stretched", detail: "See the issue."),
                               ReleaseNoteItem(tag: "Fixed", title: "Setup remembers your CD key", detail: "")], "\(items)")
        precondition(ReleaseNotes.items(notes, limit: 1).count == 1 && ReleaseNotes.items("Plain text, no sections.").isEmpty)
        let long = ReleaseNotes.items("### Fixed\n- Title. " + String(repeating: "word ", count: 80), length: 40)
        precondition(long.first.map { $0.detail.count <= 41 && $0.detail.hasSuffix("…") } == true)

        let report = ReportDetails(version: "0.3.0", build: "412", engines: ["CoD2x 1.4.6.8", "Original 1.3"], macOS: "27.0",
                                   model: "MacBook Pro, Apple M4 Pro", shaders: "original", display: "2560 × 1440 borderless")
        precondition(report.lines == ["CoD2 Unbound 0.3.0 (412)", "CoD2x 1.4.6.8 · Original 1.3", "macOS 27.0 · MacBook Pro, Apple M4 Pro",
                                      "Shaders: original · Display: 2560 × 1440 borderless"])
        let issue = try requireValue(report.issueURL().flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false) })
        precondition(issue.host == "github.com" && issue.path == "/cod2unbound/cod2-unbound/issues/new")
        let fields = Dictionary(uniqueKeysWithValues: (issue.queryItems ?? []).map { ($0.name, $0.value ?? "") })
        precondition(fields["template"] == "bug_report.yml" && fields["version"] == "0.3.0 (412)" && fields["macos"] == "27.0")
        precondition(!report.text.lowercased().contains("key") && ReportDetails(version: "0.3.0", build: "", engines: [], macOS: "", model: "", shaders: "", display: "").lines[0] == "CoD2 Unbound 0.3.0")

        for line in ["+set com_hunkMegs 512", "+seta com_hunkMegs 512", "seta com_hunkMegs 512", "set com_hunkMegs=512", "com_hunkMegs 512"] {
            precondition(GameSettings.advancedPair(line).map { $0.0 == "com_hunkMegs" && $0.1 == "512" } == true, line)
        }
        var typed = GameSettings(); typed.advanced = "+set net_port 29980"
        let typedConfig = try typed.launchConfig(migrated: true)
        precondition(typedConfig.contains("seta net_port \"29980\""))
        typed.advanced = "+set cl_cdkey ABCD"
        precondition((try? typed.launchConfig(migrated: false)) == nil, "a +set prefix must not get past the refused names")
        var legacy = Data(#"{"resolution":"1920x1080"}"#.utf8)
        var decoded = try JSONDecoder().decode(GameSettings.self, from: legacy)
        precondition(decoded.hideWhilePlaying && decoded.appearance == .system && !decoded.refreshServersOnOpen && decoded.checkUpdatesAutomatically && decoded.dismissedWarnings.isEmpty)
        legacy = Data(#"{"appearance":"sepia","dismissedWarnings":["switch-version"]}"#.utf8)
        decoded = try JSONDecoder().decode(GameSettings.self, from: legacy)
        precondition(decoded.appearance == .system && decoded.dismissedWarnings == [DismissibleWarning.switchVersion])
    }
}

// MARK: - ship-play
extension CoreTests {
    static func displayResolutions() throws {
        let expected = [
            (DisplayResolution(5120, 2880), "640x480 800x600 1024x768 1280x720 1920x1080 2560x1440 3008x1692 3840x2160 5120x2880", "2560x1440"),
            (DisplayResolution(6016, 3384), "640x480 800x600 1024x768 1280x720 1920x1080 2560x1440 3008x1692 3840x2160 5120x2880 6016x3384", "3008x1692"),
            (DisplayResolution(1920, 1080), "640x480 800x600 1024x768 1280x720 1920x1080", "1920x1080"),
            (DisplayResolution(3024, 1964), "640x480 800x600 1024x768 1280x720 1920x1080 2560x1440 3008x1692 3024x1964", "3024x1964")
        ]
        for (native, values, initial) in expected {
            let list = DisplayResolutions.offered(native: native, modes: [])
            precondition(list.map(\.value).joined(separator: " ") == values)
            precondition(DisplayResolutions.initial(native: native) == initial)
            let modes = [native, native, DisplayResolution(6016,3384), DisplayResolution(320,240),
                         DisplayResolution(640,480), DisplayResolution(1600,1200), DisplayResolution(1600,900)]
            let union = DisplayResolutions.offered(native: native, modes: modes)
            precondition(union == Array(Set(union)).sorted() && union.allSatisfy { $0.fits(native) })
            precondition(union.contains(DisplayResolution(1600,900)))
            precondition(union.contains(DisplayResolution(1600,1200)) == (native.height >= 1200))
            precondition(DisplayResolutions.fitting("6016x3384", in: list) == native.value)
            precondition(DisplayResolutions.fitting("1280x720", in: list) == "1280x720", "A player's supported choice stays")
            print("PASS: display \(native.value): \(values); first run \(initial)")
        }
        let scaled = DisplayResolutions.offered(native: .init(5120,2880), modes: [.init(3008,1692), .init(6016,3384)])
        precondition(!scaled.contains(.init(6016,3384)) && scaled.last?.value == "5120x2880", "Scaled backing cannot raise the native limit")
        let narrow = DisplayResolutions.offered(native: .init(1920,1080), modes: [.init(1600,1200)])
        precondition(!narrow.contains(.init(1600,1200)), "Both dimensions must fit")
        var older = try JSONDecoder().decode(GameSettings.self, from: Data(#"{"resolution":"3024x1964"}"#.utf8))
        let notebookConfig = try older.config()
        precondition(notebookConfig.contains("3024x1964"), "Native notebook sizes are valid settings")
        older.resolution = "1920x1080;quit"
        precondition((try? older.config()) == nil, "Resolution validation still blocks commands")
    }
}

// MARK: - ship-play
extension CoreTests {
    static func sessionLogs(scratch: URL) throws {
        let home = scratch.appendingPathComponent("session-logs")
        let log = try GameSessionLog(home: home, password: "fixture-secret")
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/bin/sh")
        process.arguments = ["-c", "printf 'Starting game\\n'; printf 'CoD2 slow frame: 373 ms\\n' >&2; printf 'pass'; printf 'word fixture-secret\\n'; printf '0000-0000-0000-0000-86D3\\n' >&2; printf 'unlabelled fixture-secret\\n'; printf 'final line' >&2; exit 7"]
        process.standardOutput = log.pipe; process.standardError = log.pipe
        try process.run(); log.closeParentWriter(); process.waitUntilExit(); log.finish()
        let text = try String(contentsOf: log.url, encoding: .utf8)
        precondition(process.terminationStatus == 7 && text.contains("Starting game") && text.contains("373 ms") && text.contains("final line"))
        precondition(!text.contains("fixture-secret") && !text.contains("86D3") && !text.contains("0000"), "Filter before disk, across writes and streams")
        for file in [log.url, home.appendingPathComponent("logs")] {
            let attrs = try FileManager.default.attributesOfItem(atPath: file.path)
            precondition((attrs[.posixPermissions] as? NSNumber)?.intValue == (file == log.url ? 0o600 : 0o700))
        }
        precondition(GameSessionLog.filtered("\\password\\secret").contains("omitted"))
        precondition(GameSessionLog.filtered("OOB Print: getKeyAuthorize 0 ABCDEFGHIJKLMNOP PB digest").contains("omitted"))
        precondition(GameSessionLog.filtered("OOB Print: getchallenge 0 key-digest").contains("omitted"))
        precondition(!GameSessionLog.filtered("+set password secret").contains("secret"))
        for index in 1...12 {
            let session = try GameSessionLog(home: home, now: Date().addingTimeInterval(Double(index)))
            try session.pipe.fileHandleForWriting.write(contentsOf: Data("session \(index)\n".utf8))
            session.finish()
        }
        precondition(GameSessionLog.files(home: home).count == 10 && !FileManager.default.fileExists(atPath: log.url.path))
        let bounded = try GameSessionLog(home: home, now: Date().addingTimeInterval(20))
        let chunk = Data((String(repeating: "x", count: 1023) + "\n").utf8)
        for _ in 0..<5000 { try bounded.pipe.fileHandleForWriting.write(contentsOf: chunk) }
        bounded.finish()
        let size = try bounded.url.resourceValues(forKeys: [.fileSizeKey]).fileSize!
        precondition(size <= GameSessionLog.fileLimit)
        let contents = try String(contentsOf: bounded.url, encoding: .utf8)
        precondition(contents.contains("limit; later output omitted"))
        // Old sessions exceeding the total allowance are removed, even if there are fewer than ten.
        for url in GameSessionLog.files(home: home).prefix(6) { try Data(repeating: 65, count: 4 * 1024 * 1024).write(to: url) }
        let rotated = try GameSessionLog(home: home, now: Date().addingTimeInterval(30)); rotated.finish()
        let bytes = try GameSessionLog.files(home: home).reduce(0) { try $0 + $1.resourceValues(forKeys: [.fileSizeKey]).fileSize! }
        precondition(bytes <= GameSessionLog.totalLimit - GameSessionLog.fileLimit)
        // A hosted server's logs: their own folder and prefix, both passwords kept out, pruned on their own.
        let serverFolder = scratch.appendingPathComponent("server-logs/logs")
        let gameLogs = GameSessionLog.files(home: home).map(\.lastPathComponent)
        let first = try GameSessionLog(folder: serverFolder, prefix: "server", secrets: ["joinword", "", "adminword"])
        try first.pipe.fileHandleForWriting.write(contentsOf: Data("Server: mp_toujane\nsays joinword\nsays adminword\nPlayer connected\n".utf8))
        first.finish()
        let serverText = try String(contentsOf: first.url, encoding: .utf8)
        precondition(first.url.lastPathComponent.hasPrefix("server-") && serverText.contains("Server: mp_toujane") && serverText.contains("Player connected")
                     && !serverText.contains("joinword") && !serverText.contains("adminword"), "a server log keeps both passwords out: \(serverText)")
        precondition(GameSessionLog.latest(folder: serverFolder, prefix: "server")?.lastPathComponent == first.url.lastPathComponent
                     && GameSessionLog.latest(folder: serverFolder, prefix: "game") == nil
                     && GameSessionLog.files(home: home).map(\.lastPathComponent) == gameLogs, "server logs and game logs do not count against each other")
        try Data("old".utf8).write(to: serverFolder.appendingPathComponent("console.log"))
        for index in 1...12 { try GameSessionLog(folder: serverFolder, prefix: "server", secrets: [], now: Date().addingTimeInterval(Double(index))).finish() }
        precondition(GameSessionLog.files(folder: serverFolder, prefix: "server").count == 10
                     && FileManager.default.fileExists(atPath: serverFolder.appendingPathComponent("console.log").path), "ten starts are kept; 0.3.0's single log is left alone")
        let unsafeHome = scratch.appendingPathComponent("unsafe-logs")
        try FileManager.default.createDirectory(at: unsafeHome, withIntermediateDirectories: true)
        try FileManager.default.createSymbolicLink(at: unsafeHome.appendingPathComponent("logs"), withDestinationURL: home)
        precondition((try? GameSessionLog(home: unsafeHome)) == nil, "Never follow a logs-folder symlink")
        let disconnected = try GameSessionLog(home: scratch.appendingPathComponent("disconnected-log"))
        let child = Process(); child.executableURL = URL(fileURLWithPath: "/bin/sh")
        child.arguments = ["-c", "sleep 0.2; printf after; exit 7"]
        child.standardOutput = disconnected.pipe; child.standardError = disconnected.pipe
        try child.run(); disconnected.closeParentWriter(); disconnected.finish(); child.waitUntilExit()
        precondition(child.terminationReason == .exit && child.terminationStatus == 7, "Closing the log reader must not kill the game with SIGPIPE")
        print("PASS: session stdout/stderr and failed exit retained; split credentials filtered before disk; owner-only files; ten-session and byte limits; symlinks refused")
    }
}
