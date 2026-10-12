import Foundation

private final class GameProbe: @unchecked Sendable {
    private let lock = NSLock()
    private var pids: Set<Int32> = []
    func set(_ pids: Set<Int32>) { lock.lock(); self.pids = pids; lock.unlock() }
    func get() -> Set<Int32> { lock.lock(); defer { lock.unlock() }; return pids }
}
private enum InjectedFailure: Error { case write }

@main struct ConfigLaunchTests {
    static func require(_ condition: Bool, _ message: String = "config launch contract", line: Int = #line) {
        if !condition { fatalError("FAIL: \(message) (line \(line))") }
    }
    @MainActor static func main() throws {
        let scratch = URL(fileURLWithPath: ProcessInfo.processInfo.environment["TMPDIR"]!)
        let home = scratch.appendingPathComponent("home")
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let original = Data("// keep\r\nseta sensitivity \"2\"\r\n".utf8)
        try PrivateFile.write(original, to: home.appendingPathComponent("main/config_mp.cfg"))
        var settings = GameSettings()
        settings.advanced = "SENSITIVITY 7\nfs_game mods/test\nnet_port 29971\nr_gamma 1.2"
        let session = try ConfigStudioSession(home: home, dialect: .cod2x)
        try session.initialize(settings: settings)
        require(session.current != nil && !session.launchReserved && !session.isReadOnly)
        let migrated = try session.prepareLaunch(settings: settings)
        require(migrated != nil && session.migrationError == nil)
        let id = try store.activeSelection().profileID!
        let main = try store.read(id: id)
        require(main.document.effectiveSetting(named: "sensitivity") == "7")
        require(main.document.effectiveSetting(named: "r_gamma") == "1.2")
        require(main.metadata.dpi == 800 && main.document.effectiveSetting(named: "dpi") == nil)
        try session.cancelLaunch()
        var personal = main.document
        try personal.setSetting(named: "sensitivity", value: "3")
        _ = try store.update(id: id, document: personal, expected: main.version, summary: "Aim")
        settings.sensitivity = 9
        let ready = try session.prepareLaunch(settings: settings)!
        let plan = try GameLaunchPlan(settings: settings, dataPath: "/Game", homePath: home.path, configStudio: ready)
        require(!plan.arguments.contains("sensitivity") && !plan.arguments.contains("SENSITIVITY"))
        require(!plan.arguments.contains("r_gamma") && !plan.arguments.contains("com_maxfps"))
        for dvar in ["r_mode", "r_fullscreen", "r_borderless", "r_swapInterval", "logfile", "developer", "com_introPlayed", "fs_game", "net_port"] {
            require(plan.arguments.contains(dvar), "missing machine option \(dvar)")
        }
        require(try store.read(id: id).document.effectiveSetting(named: "sensitivity") == "3")
        require(!plan.arguments.contains("+exec"))
        try session.cancelLaunch()
        let other = try store.create(id: "match.one-two", displayName: "Match", document: personal)
        try store.select(id: other.profile.id, expectedActive: store.activeSelection().originalBytes)
        let chain = try ConfigStudioLaunch.chainBasename(profileID: other.profile.id)
        require(chain.range(of: "^cs_[A-Za-z0-9_]+\\.cfg$", options: .regularExpression) != nil)
        require(chain != (try ConfigStudioLaunch.chainBasename(profileID: "match_one_two")))
        try PrivateFile.write(Data("vstr test\n".utf8), to: home.appendingPathComponent("main/" + chain))
        let chained = try session.prepareLaunch(settings: settings)!
        let chainPlan = try GameLaunchPlan(settings: settings, dataPath: "/Game", homePath: home.path, configStudio: chained)
        require(chainPlan.arguments.suffix(2) == ["+exec", chain])
        require((try? ConfigStudioLaunch.chainBasename(profileID: "x+quit")) == nil)
        require((try? ConfigStudioLaunch.chainBasename(profileID: "x\n")) == nil)
        try session.cancelLaunch()
        print("PASS: migration, personal launch ownership, machine/advanced options, safe optional chains and DPI metadata")

        // Failed migration must retain every old command and preserve originals.
        let failedHome = scratch.appendingPathComponent("failed")
        try PrivateFile.write(original, to: failedHome.appendingPathComponent("main/config_mp.cfg"))
        let invalid = settings
        try PrivateFile.write(Data("{}".utf8), to: failedHome.appendingPathComponent("main/players/.studio-migration/migration.json"))
        let failed = try ConfigStudioSession(home: failedHome, dialect: .cod2x)
        let fallback = try failed.prepareLaunch(settings: invalid)
        require(fallback == nil && failed.migrationError != nil)
        let legacy = try GameLaunchPlan(settings: invalid, dataPath: "/Game", homePath: failedHome.path, configStudio: fallback)
        require(legacy.arguments == (try GameLaunchPlan(settings: invalid, dataPath: "/Game", homePath: failedHome.path)).arguments)
        require(legacy.arguments.contains("sensitivity"))
        require(try Data(contentsOf: failedHome.appendingPathComponent("main/config_mp.cfg")) == original)
        try failed.cancelLaunch()
        print("PASS: migration failure keeps the complete legacy launch and original config")

        // A launched engine and another attached engine share a single session.
        _ = try session.prepareLaunch(settings: settings)
        session.gameLaunched(pid: 101)
        try session.reconcileGames([101, 102])
        try session.refresh()
        var draft = session.current!.document
        try draft.setSetting(named: "sensitivity", value: "4")
        session.setDraft(draft)
        for operation in [ { try session.saveDraft(summary: "Aim") }, { try session.selectProfile(id) }, { _ = try session.createProfile(id: "blocked", displayName: "Blocked") } ] {
            do { try operation(); fatalError("accepted running mutation") }
            catch ConfigProfileError.gameRunning {}
        }
        let path = home.appendingPathComponent("main/players/\(other.profile.id)/config_mp.cfg")
        let historyBefore = try store.history(id: other.profile.id).count
        try PrivateFile.write(Data("seta sensitivity \"6\"\n".utf8), to: path)
        try session.reconcileGames([102])
        let stillRunningHistory = try store.history(id: other.profile.id).count
        require(session.isReadOnly && stillRunningHistory == historyBefore)
        try session.reconcileGames([])
        try session.reconcileGames([])
        let history = try store.history(id: other.profile.id)
        require(history.count == historyBefore + 1 && history.first?.source == .gameSession)
        require(history.first?.beforeData == personal.serialized())
        require(session.draft?.effectiveSetting(named: "sensitivity") == "4")
        print("PASS: launched/attached read-only guard, preserved draft, one snapshot after last exit")

        // Idle external conflicts leave the unsaved draft available for explicit recovery.
        try session.discardDraft()
        var next = session.current!.document
        try next.setSetting(named: "sensitivity", value: "8")
        session.setDraft(next)
        try PrivateFile.write(Data("seta sensitivity \"external\"\n".utf8), to: path)
        do { try session.saveDraft(summary: "Aim"); fatalError("overwrote external config") }
        catch ConfigProfileError.conflict {}
        try session.refresh()
        require(session.draft?.effectiveSetting(named: "sensitivity") == "8")
        require(session.current?.document.effectiveSetting(named: "sensitivity") == "external")
        print("PASS: external conflict refresh keeps the user's draft and reads the disk version")

        let probe = GameProbe()
        let observed = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: { probe.get() })
        probe.set([103])
        require(observed.isReadOnly)
        do { _ = try observed.createProfile(id: "blocked", displayName: "Blocked"); fatalError("missed discovered process") }
        catch ConfigProfileError.gameRunning {}
        try observed.reconcileGames([103])
        probe.set([])
        try observed.reconcileGames([])
        print("PASS: discovery guard refuses writes before the attachment timer runs")

        try session.discardDraft()
        _ = try session.prepareLaunch(settings: settings)
        session.gameLaunched(pid: 104)
        try PrivateFile.write(Data("seta sensitivity \"restart\"\n".utf8), to: path)
        let restarted = try ConfigStudioSession(home: home, dialect: .cod2x)
        try restarted.reconcileGames([104])
        try restarted.reconcileGames([])
        try restarted.reconcileGames([])
        let restartedHistory = try store.history(id: other.profile.id)
        require(restartedHistory.filter { $0.source == .gameSession }.count == 2)
        require(restartedHistory.first?.beforeData == Data("seta sensitivity \"external\"\n".utf8))
        print("PASS: persistent pre-session archive survives launcher restart and records once")

        let conflictHome = scratch.appendingPathComponent("conflict")
        let marker = scratch.appendingPathComponent("fail-write")
        let interrupted = try ConfigProfileStore(homeURL: conflictHome, dialect: .cod2x, writeFile: { data, url in
            if url.lastPathComponent == "studio.json" && FileManager.default.fileExists(atPath: marker.path) { throw InjectedFailure.write }
            try PrivateFile.write(data, to: url)
        })
        let first = try interrupted.create(id: "daily", displayName: "Daily", document: ConfigDocument(data: original, dialect: .cod2x))
        try interrupted.select(id: "daily", expectedActive: nil)
        let recoverySession = try ConfigStudioSession(home: conflictHome, dialect: .cod2x)
        try recoverySession.refresh()
        var intent = first.document; try intent.setSetting(named: "sensitivity", value: "11")
        recoverySession.setDraft(intent)
        var metadata = first.metadata; metadata.notes["sensitivity"] = "Draft note"
        try PrivateFile.write(Data(), to: marker)
        do { _ = try interrupted.update(id: "daily", document: intent, expected: first.version, metadata: metadata, summary: "Aim"); fatalError("missing injected failure") }
        catch InjectedFailure.write {}
        let conflictPath = conflictHome.appendingPathComponent("main/players/daily/config_mp.cfg")
        let external = Data("seta sensitivity \"outside\"\n".utf8)
        try PrivateFile.write(external, to: conflictPath)
        do { try recoverySession.refresh(); fatalError("ignored conflicting journal") }
        catch ConfigProfileError.conflict {}
        try recoverySession.inspectRecovery(profileID: "daily")
        require(recoverySession.recovery?.current.config == external)
        try recoverySession.keepDiskVersion()
        require(recoverySession.current?.data == external && recoverySession.draft?.effectiveSetting(named: "sensitivity") == "11")
        require(try interrupted.history(id: "daily").count == 1)
        let archivedIntent = try FileManager.default.contentsOfDirectory(at: conflictHome.appendingPathComponent("main/players/daily/recovery"), includingPropertiesForKeys: nil)
        require(archivedIntent.count == 1)
        do { try recoverySession.saveDraft(summary: "Aim"); fatalError("silently rebased draft") }
        catch ConfigProfileError.conflict {}
        try recoverySession.rebaseDraft(on: recoverySession.current!.version)
        try recoverySession.saveDraft(summary: "Reapplied reviewed draft")
        require(recoverySession.current?.document.effectiveSetting(named: "sensitivity") == "11")
        require(try interrupted.history(id: "daily").first?.beforeData == external)
        print("PASS: explicit journal recovery preserves external bytes, archived intent and stale draft")

        for dialect in [ConfigDialect.stock, .cod2x] {
            let freshHome = scratch.appendingPathComponent("fresh-" + dialect.rawValue)
            let fresh = try ConfigStudioSession(home: freshHome, dialect: dialect)
            let freshLaunch = try fresh.prepareLaunch(settings: GameSettings())
            require(freshLaunch?.profileID == "Main")
            try fresh.cancelLaunch()
            let freshStore = try ConfigProfileStore(homeURL: freshHome, dialect: dialect)
            require(try freshStore.read(id: "Main").metadata.origin.source == "New installation")
            let profileHome = scratch.appendingPathComponent("profile-" + dialect.rawValue)
            let profileStore = try ConfigProfileStore(homeURL: profileHome, dialect: dialect)
            _ = try profileStore.create(id: "old", displayName: "Old", document: ConfigDocument(data: original, dialect: dialect))
            try profileStore.select(id: "old", expectedActive: nil)
            let fromProfile = try ConfigStudioSession(home: profileHome, dialect: dialect)
            _ = try fromProfile.prepareLaunch(settings: GameSettings())
            try fromProfile.cancelLaunch()
            require(try profileStore.read(id: "Main").metadata.origin.source == "players/old/config_mp.cfg")
            require(try profileStore.read(id: "old").data == original)
        }
        let stockMod = try ConfigStudioSession(home: scratch.appendingPathComponent("stock-mod"), dialect: .stock)
        require(try stockMod.prepareLaunch(settings: GameSettings(), forwarded: ["+set", "fs_game", "mods/test"]) == nil)
        require(stockMod.migrationError != nil)
        try stockMod.cancelLaunch()
        print("PASS: explicit fresh/profile migration in both dialects; stock mod launch retains legacy ownership")

        // The original in-game profile menu can create/delete profiles too.
        let changedHome = scratch.appendingPathComponent("game-profiles")
        let changedStore = try ConfigProfileStore(homeURL: changedHome, dialect: .cod2x)
        _ = try changedStore.create(id: "old", displayName: "Old", document: ConfigDocument(data: original, dialect: .cod2x))
        try changedStore.select(id: "old", expectedActive: nil)
        let changedSession = try ConfigStudioSession(home: changedHome, dialect: .cod2x)
        _ = try changedSession.prepareLaunch(settings: GameSettings())
        changedSession.gameLaunched(pid: 105)
        try FileManager.default.removeItem(at: changedHome.appendingPathComponent("main/players/old"))
        let createdInGame = Data("seta name \"In game\"\n".utf8)
        try PrivateFile.write(createdInGame, to: changedHome.appendingPathComponent("main/players/new/config_mp.cfg"))
        try PrivateFile.write(Data("new".utf8), to: changedHome.appendingPathComponent("main/players/active.txt"))
        do { try changedSession.reconcileGames([]) }
        catch { fatalError("FAIL: in-game profile deletion blocked session exit: \(error)") }
        let createdHistory = try changedStore.history(id: "new")
        require(createdHistory.count == 1 && createdHistory[0].source == .gameSession)
        require(createdHistory[0].beforeData.isEmpty && createdHistory[0].data == createdInGame)
        require(try changedSession.prepareLaunch(settings: GameSettings())?.profileID == "new")
        try changedSession.cancelLaunch()
        print("PASS: in-game profile creation gets history and profile deletion does not block the next launch")

        // Viewing another loadout never changes the game's active selection.
        let viewHome = scratch.appendingPathComponent("view")
        let viewStore = try ConfigProfileStore(homeURL: viewHome, dialect: .cod2x)
        _ = try viewStore.create(id: "daily", displayName: "Daily", document: ConfigDocument(data: original, dialect: .cod2x))
        let scrim = try viewStore.create(id: "scrim", displayName: "Scrim", document: ConfigDocument(data: Data("seta sensitivity \"3\"\n".utf8), dialect: .cod2x))
        try viewStore.select(id: "daily", expectedActive: nil)
        let viewer = try ConfigStudioSession(home: viewHome, dialect: .cod2x)
        _ = try viewer.prepareLaunch(settings: GameSettings())
        try viewer.cancelLaunch()
        // Migration made a new Main from daily and selected it.
        let activeID = try viewStore.activeSelection().profileID!
        let activeBytes = try Data(contentsOf: viewHome.appendingPathComponent("main/players/active.txt"))
        require(activeID == "Main" && viewer.current?.profile.id == activeID)
        try viewer.focus("scrim")
        require(viewer.focusedProfileID == "scrim" && viewer.current?.profile.id == "scrim" && viewer.current?.profile.isActive == false)
        require(try viewer.readProfile(activeID).profile.isActive && !(try viewer.readProfile("daily").profile.isActive))
        require(try viewer.readActive()?.profile.id == activeID)
        require(viewer.current?.profile.id == "scrim")
        var scrimEdit = viewer.current!.document
        try scrimEdit.setSetting(named: "sensitivity", value: "2.5")
        try viewer.update(document: scrimEdit, summary: "Changed Mouse speed")
        require(try viewStore.read(id: "scrim").document.effectiveSetting(named: "sensitivity") == "2.5")
        require(try viewStore.history(id: "scrim").first?.beforeData == scrim.data)
        require(viewer.history.first?.summary == "Changed Mouse speed")
        require(try Data(contentsOf: viewHome.appendingPathComponent("main/players/active.txt")) == activeBytes)
        require(try viewer.prepareLaunch(settings: GameSettings())?.profileID == activeID && viewer.current?.profile.id == "scrim")
        try viewer.cancelLaunch()
        // A temporary per-connection copy is made from the active loadout, not the one on screen.
        let activeRead = try viewStore.read(id: activeID)
        let isolated = try viewer.prepareLaunch(settings: GameSettings(), connection: ConfigConnectionLaunch(address: "203.0.113.24:28960", values: ["snaps": "40"]),
                                                connectionExpectedVersion: activeRead.version, connectionChainBytes: nil)
        require(isolated?.profileID == activeID && isolated?.connectionIsolated == true && viewer.current?.profile.id == "scrim")
        require(!viewer.profiles.contains { $0.id.hasPrefix("csconn") })
        try viewer.cancelLaunch()
        require(try Data(contentsOf: viewHome.appendingPathComponent("main/players/active.txt")) == activeBytes)
        require(try viewStore.list().allSatisfy { !$0.id.hasPrefix("csconn") })
        try FileManager.default.removeItem(at: viewHome.appendingPathComponent("main/players/scrim"))
        try viewer.refresh()
        require(viewer.focusedProfileID == nil && viewer.current?.profile.id == activeID)
        do { try viewer.focus("missing"); fatalError("focused a missing loadout") }
        catch ConfigProfileError.missingProfile {}
        require(viewer.current?.profile.id == activeID)
        print("PASS: a viewed loadout is edited and read without changing the active selection or launch profile")

        // Polling notices external same-size edits, including an editor that keeps mtime.
        let pollHome = scratch.appendingPathComponent("poll")
        let pollStore = try ConfigProfileStore(homeURL: pollHome, dialect: .cod2x)
        let pollData = Data("seta sensitivity 2\n".utf8)
        _ = try pollStore.create(id: "daily", displayName: "Daily", document: ConfigDocument(data: pollData, dialect: .cod2x))
        try pollStore.select(id: "daily", expectedActive: nil)
        let polling = try ConfigStudioSession(home: pollHome, dialect: .cod2x)
        try polling.reconcileGames([])
        let pollPath = pollHome.appendingPathComponent("main/players/daily/config_mp.cfg")
        let mtime = try FileManager.default.attributesOfItem(atPath: pollPath.path)[.modificationDate]!
        try Data("seta sensitivity 3\n".utf8).write(to: pollPath)
        try FileManager.default.setAttributes([.modificationDate: mtime], ofItemAtPath: pollPath.path)
        try polling.reconcileGames([])
        require(polling.current?.document.effectiveSetting(named: "sensitivity") == "3")
        var pollMetadata = polling.current!.metadata; pollMetadata.displayName = "Renamed"
        try PrivateFile.write(JSONEncoder().encode(pollMetadata), to: pollHome.appendingPathComponent("main/players/daily/studio.json"))
        try polling.reconcileGames([])
        require(polling.current?.metadata.displayName == "Renamed" && polling.profiles.first?.displayName == "Renamed")
        let row = try pollStore.history(id: "daily")[0]
        let replacement = ConfigHistoryEntry(source: .you, summary: "External history edit", before: ConfigProfileVersion(config: pollData, metadata: nil), after: ConfigProfileVersion(config: pollData, metadata: nil), id: row.id)
        try PrivateFile.write(JSONEncoder().encode(replacement), to: pollHome.appendingPathComponent("main/players/daily/history/" + row.id + ".json"))
        try polling.reconcileGames([])
        require(polling.history.first?.summary == "External history edit")
        let companion = pollHome.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "daily")))
        try PrivateFile.write(Data("// external companion\n".utf8), to: companion)
        try polling.reconcileGames([])
        require(polling.current?.version.chain == Data("// external companion\n".utf8))
        _ = try pollStore.create(id: "other", displayName: "Other", document: ConfigDocument(data: pollData, dialect: .cod2x))
        try pollStore.select(id: "other", expectedActive: pollStore.activeSelection().originalBytes)
        try polling.reconcileGames([])
        require(polling.current?.profile.id == "other" && polling.profiles.count == 2)
        try FileManager.default.removeItem(at: pollHome.appendingPathComponent("main/players/daily"))
        try polling.reconcileGames([])
        require(polling.profiles.map(\.id) == ["other"])
        print("PASS: polling detects config, metadata, history, companion, selection and profile changes")
    }
}
