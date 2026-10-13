import Foundation

@main struct ConfigReadyCheckTests {
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: " + message) }
    }
    static func document(_ text: String, dialect: ConfigDialect = .cod2x) -> ConfigDocument {
        ConfigDocument(data: Data(text.utf8), dialect: dialect)
    }
    static func server(_ fields: [String: String], address: String = "127.0.0.1:29979") throws -> GameServer {
        let text = fields.sorted { $0.key < $1.key }.map { "\\\($0.key)\\\($0.value)" }.joined()
        return try WireParser.status(WireParser.marker + Data(("statusResponse\n" + text + "\n").utf8), address: address, ping: 12)
    }
    @MainActor static func main() throws {
        let resources = URL(fileURLWithPath: CommandLine.arguments[1])
        let rules = try ConfigRules(data: Data(contentsOf: resources.appendingPathComponent("ConfigRulesets.json")))
        let catalog = try ConfigCatalog(data: Data(contentsOf: resources.appendingPathComponent("ConfigCatalog.json")))
        let checker = ConfigReadyCheck(catalog: catalog, rules: rules)
        let home = URL(fileURLWithPath: ProcessInfo.processInfo.environment["COD2_READY_TEST_SCRATCH"]!).appendingPathComponent("home")
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let session = try ConfigStudioSession(home: home, dialect: .cod2x)
        try session.initialize(settings: GameSettings())
        let active = try store.activeSelection().profileID!
        let main = try store.read(id: active)
        let text = """
        // exact bytes must survive a temporary fix
        seta com_maxfps "333"
        seta rate "25000"
        seta snaps "40"
        seta cl_maxpackets "125"
        seta sc_enable "0"
        seta fx_sort "1"
        seta mss_q3fs "1"
        seta m_filter "0"
        seta cl_mouseAccel "0"
        seta r_picmip "1"
        seta r_picmip_manual "1"
        """ + "\n"
        _ = try store.update(id: active, document: document(text), expected: main.version, summary: "Ready test")
        try session.refresh()
        let profile = try store.read(id: active)
        let competitive = try server(["protocol": "120", "fs_game": "", "g_competitive": "1", "sv_fps": "20", "sv_maxRate": "25000", "pswrd": "1"])
        let report = checker.evaluate(server: competitive, profile: profile, mode: .cod2x)
        require(report.rulesetIDs == ["cod2x-competitive"] && report.status == .needsChanges, "competitive mapping or frame violation lost")
        require(report.fixes == ["com_maxfps": "250"], "only the editable frame clamp may be fixed")
        require(report.details.contains { $0.reason.contains("333") && $0.reason.contains("250") }, "333 unavailable must be explained")
        require(report.facts["sv_fps"] == "20" && report.facts["sv_maxrate"] == "25000", "advertised facts lost")
        require(report.limitations.contains { $0.contains("Not measured") }, "status RTT cannot certify performance")
        print("PASS: competitive rules, cited limits, server-owned values and absent measurement")

        let fixtures = URL(fileURLWithPath: CommandLine.arguments[2])
        let fixture = try WireParser.status(Data(contentsOf: fixtures.appendingPathComponent("status.bin")), address: competitive.address, ping: 7)
        for unknown in [fixture, try server([:]), try server(["protocol": "120", "fs_game": ""]),
                        try server(["protocol": "118", "fs_game": "pam"]),
                        try server(["protocol": "120", "fs_game": "zpam", "_zpam": "4.09", "g_competitive": "1"]),
                        try server(["protocol": "118", "fs_game": "", "sv_ruleset": "invented"]),
                        try server(["protocol": "120", "fs_game": "", "g_competitive": "1", "sv_maxRate": "NaN"])] {
            let review = checker.evaluate(server: unknown, profile: profile, mode: .cod2x)
            require(review.status == .unknown && review.fixes.isEmpty, "unknown rules advertised an all-clear or fix")
        }
        require(checker.evaluate(server: nil, profile: profile, mode: .cod2x).status == .unknown, "missing query became positive")
        for (fields, mode, expected) in [
            (["protocol": "118", "fs_game": ""], ConfigCatalogMode.stock, "stock-1.3"),
            (["protocol": "120", "fs_game": "", "g_competitive": "0"], .cod2x, "public-cod2x"),
            (["protocol": "118", "fs_game": "zpam336"], .stock, "zpam-3.36"),
            (["protocol": "120", "fs_game": "zpam408", "g_competitive": "1"], .cod2x, "zpam-4.08"),
            (["protocol": "120", "fs_game": "zpam", "_zpam": "4.08", "g_competitive": "1"], .cod2x, "zpam-4.08")
        ] {
            require(checker.evaluate(server: try server(fields), profile: profile, mode: mode).rulesetIDs == [expected], "verified identity did not map")
        }
        for bad in [try server(["protocol": "120", "fs_game": "zpam408", "g_competitive": "0"]),
                    try server(["protocol": "120", "fs_game": "zpam408", "_zpam": "3.36"]),
                    try server(["protocol": "999", "fs_game": "", "g_competitive": "1"]),
                    try server(["protocol": "120", "fs_game": "", "g_competitive": "1", "G_COMPETITIVE": "0"])] {
            require(checker.evaluate(server: bad, profile: profile, mode: .cod2x).status == .unknown, "contradictory identity became positive")
        }
        require(checker.evaluate(server: competitive, profile: profile, mode: .stock).status == .unknown, "client mismatch became positive")
        print("PASS: status.bin, missing/unknown mods and policies, exact zPAM versions and contradictory advertisements")

        var legal = profile.document
        try legal.setSetting(named: "com_maxfps", value: "250")
        let match = try store.create(id: "Match", displayName: "Match", document: legal)
        require(checker.evaluate(server: competitive, profile: match, mode: .cod2x).status == .matchesKnownRules, "bounded cited rules failed")
        let unobserved = try JSONDecoder().decode(GameServer.self, from: Data("""
        {"address":"127.0.0.1:29979","fields":{"protocol":"120","fs_game":"","g_competitive":"1"},"ping":12,"players":[]}
        """.utf8))
        require(checker.evaluate(server: unobserved, profile: match, mode: .cod2x).status == .unknown, "unobserved cached status became a positive judgment")
        require(checker.evaluate(server: try server(["protocol": "120", "fs_game": "", "g_competitive": "0", "com_maxfps_limit": "1"]), profile: profile, mode: .cod2x).status == .unknown,
                "unverified legacy FPS restriction became a positive judgment")
        var expired = competitive; expired.observedAt = Date(timeIntervalSinceNow: -61)
        var future = competitive; future.observedAt = Date(timeIntervalSinceNow: 61)
        require(checker.evaluate(server: expired, profile: match, mode: .cod2x).status == .unknown, "stale status became positive")
        require(checker.evaluate(server: future, profile: match, mode: .cod2x).status == .unknown, "future status became positive")
        var conflicting = competitive
        conflicting.mergeInfo(try server(["protocol": "120", "g_competitive": "0"]))
        require(checker.evaluate(server: conflicting, profile: match, mode: .cod2x).status == .unknown, "getinfo silently replaced contradictory status")
        require(checker.evaluate(server: try server(["protocol": "120", "fs_game": "", "g_competitive": "1", "cl_maxpackets": "100"]), profile: match, mode: .cod2x).status == .unknown,
                "contradictory advertised client rule became positive")
        let incomplete = try store.create(id: "Empty", displayName: "Empty", document: document("seta com_maxfps 250\n"))
        require(checker.evaluate(server: competitive, profile: incomplete, mode: .cod2x).status == .unknown, "absent direct values became positive")
        var wait = legal
        try wait.setBind(key: "MOUSE5", binding: "+attack; wait; -attack")
        let waiting = try store.create(id: "Wait", displayName: "Wait", document: wait)
        let warning = checker.evaluate(server: competitive, profile: waiting, mode: .cod2x)
        require(warning.status == .needsChanges && warning.details.contains { $0.setting == "wait" && $0.status == .violation }, "disabled wait bind hidden")
        require(warning.fixes.isEmpty, "wait bind should not be rewritten automatically")
        try wait.setBind(key: "MOUSE5", binding: "+attack; -attack")
        try wait.setSetting(named: "com_maxfps", value: "333")
        try wait.setSetting(named: "rate", value: "1000")
        var metadata = ConfigStudioMetadata(displayName: "Locked"); metadata.locks = ["COM_MAXFPS"]
        let locked = try store.create(id: "Locked", displayName: "Locked", document: wait, metadata: metadata)
        require(checker.evaluate(server: competitive, profile: locked, mode: .cod2x).fixes.isEmpty, "lock or server ownership bypassed")
        try wait.setSetting(named: "script", value: "wait; +attack")
        try wait.setBind(key: "MOUSE5", binding: "vstr script")
        let scripted = try store.create(id: "Script", displayName: "Script", document: wait)
        require(checker.evaluate(server: competitive, profile: scripted, mode: .cod2x).status == .unknown, "dynamic bind became all-clear")
        print("PASS: bounded match, missing values, effective wait binds, scripts, locks and ownership")

        let chained = try store.create(id: "Chained", displayName: "Chained", document: legal)
        let hudCycle = ConfigChain(id: "hud", key: "h", behavior: .commandCycle(steps: [["set cg_drawFPS Off"], ["set cg_drawFPS Simple"]]))
        _ = try ConfigChains.apply(try ConfigChains.prepare(chains: [hudCycle], read: chained), to: store)
        let knownChain = try store.read(id: "Chained")
        let companion = ConfigDocument(data: knownChain.version.chain!, dialect: .cod2x)
        require(checker.evaluate(server: competitive, profile: knownChain, mode: .cod2x, advancedChain: companion).status == .matchesKnownRules,
                "verified HUD cycle does not make unrelated cited rules unknown")
        let capCycle = ConfigChain(id: "cap", key: "f", behavior: .commandCycle(steps: [["set com_maxfps 125"], ["set com_maxfps 333", "wait"]]))
        _ = try ConfigChains.apply(try ConfigChains.prepare(chains: [capCycle], read: knownChain), to: store)
        let changingCap = try store.read(id: "Chained")
        let changingReport = checker.evaluate(server: competitive, profile: changingCap, mode: .cod2x)
        require(changingReport.status == .unknown && changingReport.fixes.isEmpty && changingReport.details.contains { $0.setting == "wait" && $0.status == .violation },
                "all reachable chain leaves are reviewed; a changing rule value cannot receive a temporary fix")
        print("PASS: verified B2 companion review and dynamic rule-value refusal")

        let connection = ConfigServerConnection()
        try connection.select(address: competitive.address)
        try connection.fix(report: report)
        let link = try LaunchLink.direct("127.0.0.1:29979", password: "synthetic password")
        let overrides = try connection.launch(for: link, profile: profile, report: report)
        require(overrides?.values == ["com_maxfps": "250"], "reviewed fix not passed to launch")
        let launch = try session.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version)!
        let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: home.path, link: link, configStudio: launch, connection: overrides)
        require(plan.arguments.suffix(3) == ["+set", "com_maxfps", "\"250\""], "connection override not last")
        require(!plan.arguments.joined().contains("synthetic password") && plan.link?.password == "synthetic password", "password leaked into argv")
        require(try store.read(id: active).version == profile.version, "temporary fix persisted profile or metadata")
        try session.cancelLaunch()
        let deep = try LaunchLink("cod2x://connect%20127.0.0.1%3A29979%20password%20%22synthetic%20password%22")
        require(try connection.launch(for: deep, profile: profile, report: report)?.values["com_maxfps"] == "250", "deep link lost same-server fix")
        require(try connection.launch(for: LaunchLink.direct("127.0.0.1:29980"), profile: profile, report: report) == nil, "fix leaked to another address")
        try connection.select(address: "127.0.0.1:29980")
        try connection.select(address: competitive.address)
        require(connection.overrides.isEmpty && connection.profileID == nil, "changing selection retained connection choice")
        try connection.select(address: "LOCALHOST")
        try connection.select(address: "localhost:28960")
        require(connection.address == "localhost:28960", "default port/case identity not canonical")
        print("PASS: password, direct and deep-link joins, byte preservation, server isolation and selection reset")

        try connection.select(address: competitive.address)
        try connection.fix(report: report)
        do { try connection.fix(report: checker.evaluate(server: fixture, profile: profile, mode: .cod2x)); fatalError("fixed unknown policy") }
        catch ConfigReadyCheckError.noVerifiedFix {}
        var external = profile.document; try external.setSetting(named: "sensitivity", value: "4")
        let changed = ConfigProfileRead(profile: profile.profile, document: external, metadata: profile.metadata,
                                      version: ConfigProfileVersion(config: external.serialized(), metadata: profile.version.metadata))
        do { _ = try connection.launch(for: link, profile: changed, report: report); fatalError("launched stale review") }
        catch ConfigReadyCheckError.staleReview {}
        for values in [["fs_homepath": "/bad"], ["cg_fov": "90"], ["com_maxfps": "250;+quit"], ["COM_MAXFPS": "250", "com_maxfps": "125"]] {
            do { _ = try ConfigConnectionLaunch(address: competitive.address, values: values).arguments(for: link)
                _ = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: home.path, link: link,
                configStudio: launch, connection: ConfigConnectionLaunch(address: competitive.address, values: values)); fatalError("unsafe overrides accepted") }
            catch is LauncherError {}
        }
        do { _ = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: home.path,
            configStudio: launch, connection: overrides); fatalError("menu consumed server override") }
        catch is LauncherError {}
        print("PASS: stale review and unsafe/credential/protected launch boundaries")

        try connection.useProfile(match.profile.id)
        require(connection.overrides.isEmpty, "loadout choice retained old-profile fix")
        try PrivateFile.write(Data((active + "\r\n// retained selection\r\n").utf8), to: home.appendingPathComponent("main/players/active.txt"))
        let selectionBefore = try store.activeSelection().originalBytes
        let versions = try store.list().map { try store.read(id: $0.id).version }
        let temporary = try session.prepareLaunch(settings: GameSettings(), connectionProfileID: match.profile.id)!
        require(try temporary.profileID == "Match" && store.activeSelection().profileID == "Match", "alternate profile not the launch owner")
        require(try store.list().map { try store.read(id: $0.id).version } == versions, "alternate launch rewrote profile bytes")
        do { _ = try session.prepareLaunch(settings: GameSettings(), connectionProfileID: active); fatalError("changed reserved launch") }
        catch ConfigProfileError.gameRunning {}
        try session.cancelLaunch()
        require(try store.activeSelection().originalBytes == selectionBefore, "cancel did not restore exact active selection")
        _ = try session.prepareLaunch(settings: GameSettings(), connectionProfileID: match.profile.id)
        session.gameLaunched(pid: 12345)
        let restarted = try ConfigStudioSession(home: home, dialect: .cod2x)
        try restarted.reconcileGames([12345])
        do { _ = try restarted.prepareLaunch(settings: GameSettings(), connectionProfileID: active); fatalError("attached game changed loadout") }
        catch ConfigProfileError.gameRunning {}
        try restarted.reconcileGames([])
        require(try store.activeSelection().originalBytes == selectionBefore, "exit/restart lost temporary selection restoration")
        require(try store.read(id: active).version == profile.version, "alternate connection rewrote Main")
        print("PASS: one-connection loadout, exact selection restore, restart recovery and F3a read-only reservation")

        // Archived dvars are written by the game, even when startup uses +set.
        // A connection fix must give those writes a disposable owner.
        let fixedSession = try ConfigStudioSession(home: home, dialect: .cod2x)
        let fixed = try fixedSession.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version)!
        _ = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: home.path, link: link,
                               configStudio: fixed, connection: overrides)
        fixedSession.gameLaunched(pid: 12346)
        let engineOwner = try store.activeSelection().profileID!
        require(engineOwner != active && fixed.profileID == active, "fixed launch did not isolate the game's write owner")
        require(fixedSession.current?.profile.id == active && !fixedSession.profiles.contains { $0.id == engineOwner }, "disposable profile appeared as a saved loadout")
        var engineConfig = try store.read(id: engineOwner).document
        try engineConfig.setSetting(named: "com_maxfps", value: "250")
        try PrivateFile.write(engineConfig.serialized(), to: home.appendingPathComponent("main/players/\(engineOwner)/config_mp.cfg"))
        let fixedRestarted = try ConfigStudioSession(home: home, dialect: .cod2x)
        try fixedRestarted.reconcileGames([12346])
        require(try store.read(id: active).version == profile.version, "game write changed Main while running")
        try fixedRestarted.reconcileGames([])
        require(try store.read(id: active).version == profile.version, "engine archive write persisted connection override into Main")
        require(try store.activeSelection().originalBytes == selectionBefore, "fixed connection retained temporary selection")
        require(!FileManager.default.fileExists(atPath: home.appendingPathComponent("main/players/\(engineOwner)").path) || engineOwner == active,
                "disposable connection profile survived exit")
        print("PASS: engine config serialization cannot persist connection fixes into saved loadouts")
        let selectedElsewhere = try ConfigStudioSession(home: home, dialect: .cod2x)
        _ = try selectedElsewhere.prepareLaunch(settings: GameSettings(), connectionProfileID: "Match")
        try store.select(id: "Wait", expectedActive: store.activeSelection().originalBytes)
        try selectedElsewhere.cancelLaunch()
        require(try store.activeSelection().profileID == "Wait", "restoration overwrote a later game/user selection")
        try store.select(id: active, expectedActive: store.activeSelection().originalBytes)
        let cancelled = try ConfigStudioSession(home: home, dialect: .cod2x)
        _ = try cancelled.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version)
        let cancelledOwner = try store.activeSelection().profileID!
        try cancelled.cancelLaunch()
        require(try store.activeSelection().profileID == active, "failed launch did not restore selection")
        require(!FileManager.default.fileExists(atPath: home.appendingPathComponent("main/players/" + cancelledOwner).path), "failed launch leaked disposable write owner")
        print("PASS: later game/user selection wins and failed launch removes temporary write owner")
        let deletedHome = home.deletingLastPathComponent().appendingPathComponent("deleted-selection")
        let deletedStore = try ConfigProfileStore(homeURL: deletedHome, dialect: .cod2x)
        let deletedSession = try ConfigStudioSession(home: deletedHome, dialect: .cod2x)
        try deletedSession.initialize(settings: GameSettings())
        _ = try deletedSession.createProfile(id: "Before", displayName: "Before", document: legal)
        try deletedSession.selectProfile("Before")
        _ = try deletedSession.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: deletedStore.read(id: "Before").version)
        let deletedOwner = try deletedStore.activeSelection().profileID!
        deletedSession.gameLaunched(pid: 12347)
        try FileManager.default.removeItem(at: deletedHome.appendingPathComponent("main/players/Before"))
        do { try deletedSession.reconcileGames([]) }
        catch { fatalError("FAIL: deleted prior selection blocked connection cleanup: \(error)") }
        require(!FileManager.default.fileExists(atPath: deletedHome.appendingPathComponent("main/players/" + deletedOwner).path), "deleted prior selection leaked disposable profile")
        require(!FileManager.default.fileExists(atPath: deletedHome.appendingPathComponent("main/players/.studio-game-session.json").path), "deleted prior selection retained a blocking archive")
        _ = try deletedSession.prepareLaunch(settings: GameSettings())
        try deletedSession.cancelLaunch()
        print("PASS: deleted prior profile cannot block cleanup or a later launch")

        let limits = try ConfigStudioSession(home: home, dialect: .cod2x)
        let limitedLaunch = try limits.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version)!
        let limitedOwner = try store.activeSelection().profileID!
        do { _ = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Game", homePath: home.path, link: link,
            forwarded: (0..<32).flatMap { _ in ["+echo", "fixture"] }, configStudio: limitedLaunch, connection: overrides); fatalError("startup command limit bypassed") }
        catch is LauncherError {}
        try limits.cancelLaunch()
        require(!FileManager.default.fileExists(atPath: home.appendingPathComponent("main/players/" + limitedOwner).path), "rejected launch leaked isolated write owner")
        let laterFixed = try ConfigStudioSession(home: home, dialect: .cod2x)
        _ = try laterFixed.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version)
        let laterOwner = try store.activeSelection().profileID!
        laterFixed.gameLaunched(pid: 12348)
        try store.select(id: "Wait", expectedActive: store.activeSelection().originalBytes)
        try laterFixed.reconcileGames([])
        require(try store.activeSelection().profileID == "Wait", "fixed session overwrote a later real selection")
        require(!FileManager.default.fileExists(atPath: home.appendingPathComponent("main/players/" + laterOwner).path), "later real selection leaked disposable owner")
        try store.select(id: active, expectedActive: store.activeSelection().originalBytes)
        print("PASS: command-limit rejection cleans up and a later real selection wins after fixed play")

        let chainName = try ConfigStudioLaunch.chainBasename(profileID: active)
        let chainPath = home.appendingPathComponent("main/" + chainName)
        let chainConnection = ConfigServerConnection()
        try chainConnection.select(address: competitive.address)
        try chainConnection.fix(report: report)
        try PrivateFile.write(Data("// harmless new chain\n".utf8), to: chainPath)
        let changedChainReport = checker.evaluate(server: competitive, profile: profile, mode: .cod2x,
            advancedChain: try cancelled.readAdvancedChain(profileID: active))
        require(changedChainReport.fixes == ["com_maxfps": "250"], "comment-only chain should permit a bounded review")
        do { _ = try chainConnection.launch(for: link, profile: profile, report: changedChainReport); fatalError("changed chain retained old fix") }
        catch ConfigReadyCheckError.staleReview {}
        try PrivateFile.write(Data("wait; vstr script\n".utf8), to: chainPath)
        let chain = try cancelled.readAdvancedChain(profileID: active)
        require(checker.evaluate(server: competitive, profile: profile, mode: .cod2x, advancedChain: chain).status == .unknown, "executable source chain escaped readiness")
        do { _ = try chainConnection.launch(for: link, profile: profile, report: checker.evaluate(server: competitive, profile: profile, mode: .cod2x, advancedChain: chain)); fatalError("executable chain retained old fix") }
        catch ConfigReadyCheckError.staleReview {}
        do { _ = try cancelled.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version); fatalError("F3 ignored changed chain bytes") }
        catch ConfigProfileError.conflict {}
        try cancelled.cancelLaunch()
        try FileManager.default.removeItem(at: chainPath)
        _ = try store.update(id: active, document: external, expected: profile.version, summary: "External edit before launch")
        do { _ = try cancelled.prepareLaunch(settings: GameSettings(), connection: overrides, connectionExpectedVersion: profile.version); fatalError("F3 copied a version newer than the reviewed one") }
        catch ConfigProfileError.conflict {}
        try cancelled.cancelLaunch()
        print("PASS: source chain review/staleness and source revision validation before isolated publication")
    }
}
