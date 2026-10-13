import Foundation

// Release acceptance checks for Config Studio: History restores that change only the
// advanced-bind companion, Take Theirs against server-owned settings, Check-up's local
// match summaries, and the rules summary. Private temporary homes, exact bytes on disk.

enum ConfigStudioAcceptanceTests {
    static func require(_ condition: Bool, _ message: String = "config studio acceptance", line: Int = #line) {
        if !condition { fatalError("FAIL: \(message) (line \(line))") }
    }
    static func bytes(_ text: String) -> Data { Data(text.utf8) }
    static func value<T>(_ value: T?, line: Int = #line) -> T {
        guard let value else { fatalError("FAIL: missing value (line \(line))") }
        return value
    }

    @MainActor static func run(scratch: URL, catalog: ConfigCatalog, resources: URL) throws {
        let rules = try ConfigRules(data: Data(contentsOf: resources.appendingPathComponent("ConfigRulesets.json")))
        let optimizer = try ConfigOptimizer(data: Data(contentsOf: resources.appendingPathComponent("ConfigOptimizer.json")))
        try history(scratch: scratch, catalog: catalog)
        try ordinaryControls(scratch: scratch, catalog: catalog)
        try ordinaryRetries(scratch: scratch, catalog: catalog)
        try takeTheirs(scratch: scratch, catalog: catalog)
        try takeTheirsRetries(scratch: scratch, catalog: catalog)
        try matches(scratch: scratch, catalog: catalog, rules: rules, optimizer: optimizer)
        standing()
    }

    @MainActor static func studio(_ home: URL, config: String, catalog: ConfigCatalog) throws -> (ConfigStudioModel, ConfigStudioSession, ConfigProfileStore) {
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        _ = try store.create(id: "Main", displayName: "Main", document: ConfigDocument(data: bytes(config), dialect: .cod2x))
        try store.select(id: "Main", expectedActive: nil)
        let session = try ConfigStudioSession(home: home, dialect: .cod2x)
        try session.refresh()
        let model = ConfigStudioModel(session: session, catalog: { catalog })
        model.announce = { _ in }
        model.loadCatalogNow()
        return (model, session, store)
    }

    /// Ordinary controls keep Undo and Redo inside the complete revision they saved.
    @MainActor static func ordinaryControls(scratch: URL, catalog: ConfigCatalog) throws {
        let (immediate, immediateSession, _) = try studio(scratch.appendingPathComponent("controls-metadata-immediate"),
                                                         config: "seta sensitivity \"2.5\"\n", catalog: catalog)
        let metadataUndo = UndoManager(); metadataUndo.groupsByEvent = false
        let original = value(immediateSession.current?.version)
        metadataUndo.beginUndoGrouping(); immediate.rename("New Main", undo: metadataUndo); metadataUndo.endUndoGrouping()
        let written = value(immediateSession.current?.version)
        metadataUndo.undo()
        require(immediateSession.current?.version == original && metadataUndo.canRedo, "immediate metadata Undo restores exact bytes")
        metadataUndo.redo()
        require(immediateSession.current?.version == written && immediate.failure == nil, "immediate metadata Redo restores exact bytes")
        for kind in ["setting", "metadata"] {
            for action in ["undo", "redo"] {
                for change in ["loadout", "session", "config", "metadata", "companion"] {
                    let label = "\(kind)-\(action)-\(change)"
                    let (model, session, store) = try studio(scratch.appendingPathComponent("controls-" + label),
                                                             config: "seta sensitivity \"2.5\"\n", catalog: catalog)
                    let home = session.home, speed = value(model.spec(named: "sensitivity"))
                    _ = try store.create(id: "Other", displayName: "Other", document: ConfigDocument(data: bytes("seta sensitivity \"7\"\n"), dialect: .cod2x))
                    let undo = UndoManager(); undo.groupsByEvent = false
                    let original = value(session.current?.version)
                    undo.beginUndoGrouping()
                    if kind == "setting" { model.set(speed, to: "3.5", undo: undo) } else { model.rename("New Main", undo: undo) }
                    undo.endUndoGrouping()
                    require(undo.canUndo && session.current?.version != original, "\(label): the change registers Undo")
                    if action == "redo" {
                        undo.undo()
                        require(undo.canRedo && session.current?.version == original, "\(label): immediate Undo restores the complete revision")
                    }
                    switch change {
                    case "loadout": model.select("Other")
                    case "session":
                        let replacement = try ConfigStudioSession(home: home, dialect: .cod2x)
                        try replacement.refresh(); model.attach(replacement)
                    case "config":
                        try PrivateFile.write(bytes("seta sensitivity \"8\"\n"), to: home.appendingPathComponent("main/players/Main/config_mp.cfg"))
                        try session.refresh()
                    case "metadata":
                        var metadata = value(session.current?.metadata)
                        metadata.notes["sensitivity"] = "Keep this newer note"
                        try PrivateFile.write(try JSONEncoder().encode(metadata), to: home.appendingPathComponent("main/players/Main/studio.json"))
                        try session.refresh()
                    default:
                        try PrivateFile.write(Data(), to: home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "Main"))))
                        try session.refresh()
                    }
                    let before = try savedBytes(home)
                    if action == "redo" { undo.redo() } else { undo.undo() }
                    require(try savedBytes(home) == before, "\(label): a stale action preserves every config, sidecar, companion and History byte")
                    require(model.failure?.title == "Nothing to undo", "\(label): a stale action explains why it was refused")
                }
            }
        }
        print("PASS: ordinary setting and metadata Undo/Redo stay within their session, loadout and complete saved revision")
    }

    /// Retry is an explicit reapply on the latest version, inside the original loadout.
    @MainActor static func ordinaryRetries(scratch: URL, catalog: ConfigCatalog) throws {
        for kind in ["setting", "metadata"] {
            for change in ["loadout", "session", "latest"] {
                let label = "\(kind)-retry-\(change)"
                let (model, session, store) = try studio(scratch.appendingPathComponent("controls-" + label),
                                                         config: "seta sensitivity \"2.5\"\n", catalog: catalog)
                let home = session.home, speed = value(model.spec(named: "sensitivity"))
                _ = try store.create(id: "Other", displayName: "Other", document: ConfigDocument(data: bytes("seta sensitivity \"7\"\n"), dialect: .cod2x))
                let file = home.appendingPathComponent("main/players/Main/config_mp.cfg")
                let latest = bytes("seta sensitivity \"2.5\"\nseta r_gamma \"1.4\"\n")
                try PrivateFile.write(latest, to: file)
                if kind == "setting" { model.set(speed, to: "3.5") } else { model.rename("New Main") }
                require(model.failure?.retry != nil && session.draft != nil, "\(label): a conflict retains a retry")
                if change == "loadout" { model.select("Other") }
                else if change == "session" {
                    let replacement = try ConfigStudioSession(home: home, dialect: .cod2x)
                    try replacement.refresh(); model.attach(replacement)
                } else {
                    var metadata = try store.read(id: "Main").metadata
                    metadata.notes["sensitivity"] = "Keep this newer note"
                    metadata.locks.insert("r_gamma")
                    try PrivateFile.write(try JSONEncoder().encode(metadata), to: home.appendingPathComponent("main/players/Main/studio.json"))
                }
                let before = try savedBytes(home)
                model.retryFailure()
                if change == "latest" {
                    let read = try store.read(id: "Main")
                    require(model.failure == nil && session.draft == nil, "\(label): an explicit retry succeeds on the latest version")
                    require(read.document.effectiveSetting(named: "r_gamma") == "1.4" && read.metadata.notes["sensitivity"] == "Keep this newer note"
                            && read.metadata.locks.contains("r_gamma"), "\(label): retry preserves newer unrelated config and metadata")
                    require(kind == "setting" ? read.document.effectiveSetting(named: "sensitivity") == "3.5" : read.metadata.displayName == "New Main")
                } else {
                    require(try savedBytes(home) == before, "\(label): retry never writes to another loadout or session")
                    require(model.failure != nil && model.failure?.retry == nil, "\(label): a stale retry cannot be retried again")
                }
            }
        }
        print("PASS: ordinary retries preserve their target and reapply only the intended change to the latest version")
    }

    /// Includes exact filenames and bytes of History as well as all synthetic profile files.
    static func savedBytes(_ home: URL) throws -> [String: Data] {
        let files = FileManager.default.enumerator(at: home, includingPropertiesForKeys: [.isRegularFileKey])!
        var result: [String: Data] = [:]
        for case let file as URL in files {
            if try file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile == true {
                result[String(file.path.dropFirst(home.path.count + 1))] = try Data(contentsOf: file)
            }
        }
        return result
    }

    /// Restore eligibility comes from the whole revision, companion included; the since-now
    /// cache tells an absent companion from an empty one.
    @MainActor static func history(scratch: URL, catalog: ConfigCatalog) throws {
        let (model, session, store) = try studio(scratch.appendingPathComponent("acceptance-history"), config: "seta sensitivity \"2.5\"\n", catalog: catalog)
        let home = session.home
        let config = home.appendingPathComponent("main/players/Main/config_mp.cfg")
        let sidecar = home.appendingPathComponent("main/players/Main/studio.json")
        let companion = home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "Main")))

        // Absent and empty companions are different revisions.
        model.saveSnapshot()
        let plain = value(session.history.first)
        require(plain.chain == nil && model.changesSince(plain).isEmpty && !model.canRestore(plain, revision: .after))
        try PrivateFile.write(Data(), to: companion)
        try session.refresh()
        require(session.current?.version.chain == Data(), "an empty companion reads as empty, not absent")
        require(model.changesSince(plain).map(\.id) == ["chain"], "the since-now cache doesn't mistake an empty companion for none")
        require(model.canRestore(plain, revision: .after), "restoring the absent companion is a real change")
        model.restore(plain)
        require(!FileManager.default.fileExists(atPath: companion.path) && session.current?.version.chain == nil, "restore removes the empty companion")
        require(model.changesSince(plain).isEmpty && !model.canRestore(plain, revision: .after))

        // A History row that changed only the companion (as a game session can).
        let chains = [ConfigChain(id: "greetings", key: "n", behavior: .commandCycle(steps: [["say gl"], ["say hf"]]))]
        try ConfigChains.apply(try ConfigChains.prepare(chains: chains, read: value(session.current)), to: session)
        let first = value(session.current?.version)
        let firstChain = value(first.chain)
        let secondChain = firstChain + bytes("// edited while you played\n")
        try PrivateFile.write(secondChain, to: companion)
        try store.recordGameSession(id: "Main", before: first)
        try session.refresh()
        let row = value(session.history.first)
        require(row.source == .gameSession && row.beforeData == row.data && row.beforeMetadata == row.metadata && row.beforeChain == firstChain && row.chain == secondChain)
        require(model.changes(in: row).map(\.id) == ["chain"], "the main chain-diff row is kept")
        require(model.canRestore(row, revision: .before), "a companion-only change can be restored to its state before")
        require(!model.canRestore(row, revision: .after) && model.changesSince(row).isEmpty, "its own state is now")

        let before = (config: try Data(contentsOf: config), sidecar: try Data(contentsOf: sidecar)), count = session.history.count
        let undo = UndoManager(); undo.groupsByEvent = false
        undo.beginUndoGrouping(); model.restore(row, revision: .before, undo: undo); undo.endUndoGrouping()
        require(try Data(contentsOf: companion) == firstChain, "restoring before writes the earlier companion bytes")
        require(try Data(contentsOf: config) == before.config && Data(contentsOf: sidecar) == before.sidecar, "config and metadata stay byte-identical")
        let restored = value(session.history.first)
        require(session.history.count == count + 1 && restored.source == .restore && restored.beforeChain == secondChain && restored.chain == firstChain
                && restored.beforeData == restored.data && restored.beforeMetadata == restored.metadata, "one restore row records exactly the companion change")
        require(model.canRestore(row, revision: .after) && model.changesSince(row).map(\.id) == ["chain"])
        undo.undo()
        require(try Data(contentsOf: companion) == secondChain && Data(contentsOf: config) == before.config && Data(contentsOf: sidecar) == before.sidecar,
                "Undo puts back the companion it replaced")
        require(session.history.count == count + 2 && session.history.first?.source == .restore && session.history.contains { $0.id == row.id },
                "Undo is another restore; earlier History stays")
        // Command-Z must not replace an external edit observed after the restore.
        let historyDirectory = home.appendingPathComponent("main/players/Main/history")
        func disk() throws -> [Data?] {
            [try Data(contentsOf: config), try Data(contentsOf: sidecar), try? Data(contentsOf: companion)]
        }
        func historyBytes() throws -> [String: Data] {
            try Dictionary(uniqueKeysWithValues: FileManager.default.contentsOfDirectory(at: historyDirectory, includingPropertiesForKeys: nil)
                .map { ($0.lastPathComponent, try Data(contentsOf: $0)) })
        }
        func rejectedUndo(_ entry: ConfigHistoryEntry, revision: ConfigSnapshotRevision, name: String, edit: () throws -> Void) throws {
            let undo = UndoManager(); undo.groupsByEvent = false
            undo.beginUndoGrouping(); model.restore(entry, revision: revision, undo: undo); undo.endUndoGrouping()
            require(undo.canUndo && session.history.first?.source == .restore, "\(name): the restore registers Undo")
            try edit()
            try session.refresh()
            let before = try disk(), historyBefore = try historyBytes(), rowsBefore = session.history
            undo.undo()
            require(try disk() == before, "\(name): refusing stale Undo preserves every profile byte")
            require(try historyBytes() == historyBefore && session.history == rowsBefore, "\(name): refusing stale Undo leaves History byte-identical")
            require(model.failure?.title == "Nothing to undo", "\(name): stale Undo explains that the loadout changed")
        }
        try rejectedUndo(row, revision: .before, name: "changed companion") {
            try PrivateFile.write(firstChain + bytes("// edited after the restore\n"), to: companion)
        }
        try rejectedUndo(row, revision: .before, name: "changed metadata") {
            var metadata = value(session.current).metadata
            metadata.notes["sensitivity"] = "Keep this newer note"
            try PrivateFile.write(try JSONEncoder().encode(metadata), to: sidecar)
        }
        try rejectedUndo(plain, revision: .after, name: "empty companion after an absent one") {
            try PrivateFile.write(Data(), to: companion)
        }
        try session.reconcileGames([4401])
        require(!model.canRestore(row, revision: .before), "nothing restores while the game runs")
        try session.reconcileGames([])
        print("PASS: History restores companion-only changes from the full revision; absent and empty companions stay distinct")
    }

    /// One shared rule for the button and the action: settings the chosen rules, the catalog,
    /// a lock or the launcher own keep the person's value; markers still show theirs.
    @MainActor static func takeTheirs(scratch: URL, catalog: ConfigCatalog) throws {
        let config = "seta cg_viewsize \"80\"\nseta cg_weaponCycleDelay \"0\"\nseta sensitivity \"2.5\"\n"
        let (model, session, store) = try studio(scratch.appendingPathComponent("acceptance-compare"), config: config, catalog: catalog)
        let home = session.home
        _ = try store.create(id: "Scrim", displayName: "Scrim", document: ConfigDocument(data: bytes("seta cg_viewsize \"100\"\nseta cg_weaponCycleDelay \"150\"\nseta sensitivity \"1.9\"\n"), dialect: .cod2x))
        try session.refresh()
        let files = ["main/players/Main/config_mp.cfg", "main/players/Main/studio.json", "main/" + (try ConfigStudioLaunch.chainBasename(profileID: "Main"))]
        func disk() -> [Data?] { files.map { try? Data(contentsOf: home.appendingPathComponent($0)) } }
        let checkup = model.checkup
        checkup.load()
        require(checkup.rules != nil, "Check-up's rules load beside the test executable")
        checkup.rulesetIDs = ["zpam-4.08"]
        let view = value(model.spec(named: "cg_viewsize")), delay = value(model.spec(named: "cg_weaponCycleDelay")), speed = value(model.spec(named: "sensitivity"))
        let catalogOwned = value(model.specs.first { $0.setting.variant(for: .cod2x)?.serverControlled == true && !$0.launcherOwned && $0.isNumeric && $0.defaultText != nil })
        let launcher = value(model.specs.first { $0.launcherOwned && $0.isNumeric && $0.defaultText != nil })

        func guarded(_ spec: ConfigSettingSpec, _ source: String, _ reason: String, line: Int = #line) {
            require(model.theirs(spec) != nil, "\(spec.name): the comparison marker stays (\(source))", line: line)
            let why = model.takeTheirsBlock(spec)
            require(why?.contains(reason) == true, "\(spec.name): Take Theirs is disabled with its reason (\(why ?? "none"))", line: line)
            let before = disk(), count = session.history.count
            model.takeTheirs(spec)
            require(disk() == before && session.history.count == count, "\(spec.name): the guarded action writes nothing (\(source))", line: line)
        }

        // A player from Discover.
        model.compare(with: ConfigPlayerComparison(entryID: "player-1", name: "[Player 1]", provenance: "Players catalog",
                                                   values: ["cg_viewsize": "100", "cg_weaponcycledelay": "150", "sensitivity": "3.2",
                                                            catalogOwned.name.lowercased(): alternative(catalogOwned, model: model),
                                                            launcher.name.lowercased(): alternative(launcher, model: model)]))
        guarded(view, "player", "server")
        guarded(delay, "player", "server")
        guarded(catalogOwned, "player", "server")
        guarded(launcher, "player", "CoD2 Silicon sets")
        require(model.takeTheirsBlock(speed) == nil)
        model.takeTheirs(speed)
        require(model.state(speed).raw == "3.2" && session.history.first?.summary == "Took Mouse speed from [Player 1]", "ordinary settings stay writable")
        model.setLock(speed, true)
        model.compare(with: ConfigPlayerComparison(entryID: "player-1", name: "[Player 1]", provenance: "Players catalog", values: ["sensitivity": "2.2"]))
        guarded(speed, "player", "Locked")
        model.setLock(speed, false)

        // Another of the person's own loadouts.
        model.compareID = "Scrim"
        guarded(view, "loadout", "server")
        guarded(delay, "loadout", "server")
        try session.reconcileGames([4402])
        guarded(speed, "loadout", "running")
        try session.reconcileGames([])
        require(model.takeTheirsBlock(speed) == nil)
        model.takeTheirs(speed)
        require(model.state(speed).raw == "1.9" && session.history.first?.summary == "Took Mouse speed from Scrim")

        // Ownership follows the rules chosen in Check-up.
        checkup.rulesetIDs = []
        require(model.takeTheirsBlock(view) == nil && model.takeTheirsBlock(delay) == nil, "without those rules the person may take them")
        model.takeTheirs(view)
        require(model.state(view).raw == "100" && session.history.first?.summary == "Took \(view.title) from Scrim")
        model.stopComparing()
        print("PASS: Take Theirs refuses server-owned (rules and catalog), locked, launcher-owned and read-only settings with a reason; markers stay")
    }

    /// A failed comparison keeps its value and source, but retries recheck current
    /// ownership and stay tied to the session and loadout the person meant to change.
    @MainActor static func takeTheirsRetries(scratch: URL, catalog: ConfigCatalog) throws {
        for change in ["rules", "lock", "game", "loadout", "session", "comparison"] {
            let (model, session, store) = try studio(scratch.appendingPathComponent("acceptance-compare-retry-" + change),
                                                     config: "seta cg_viewsize \"80\"\n", catalog: catalog)
            let home = session.home
            for (id, size) in [("Scrim", "100"), ("Other", "70")] {
                _ = try store.create(id: id, displayName: id, document: ConfigDocument(data: bytes("seta cg_viewsize \"\(size)\"\n"), dialect: .cod2x))
            }
            let companion = home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "Main")))
            try PrivateFile.write(bytes("// keep these companion bytes\n"), to: companion)
            try session.refresh()
            let checkup = model.checkup
            checkup.load(); checkup.rulesetIDs = []
            require(checkup.rules != nil)
            model.compareID = "Scrim"
            let view = value(model.spec(named: "cg_viewsize"))
            require(model.takeTheirsBlock(view) == nil && model.theirs(view)?.value == "100")
            let config = home.appendingPathComponent("main/players/Main/config_mp.cfg")
            let sidecar = home.appendingPathComponent("main/players/Main/studio.json")
            let latest = bytes("seta cg_viewsize \"80\"\nseta sensitivity \"4\"\n")
            try PrivateFile.write(latest, to: config)
            let count = session.history.count
            model.takeTheirs(view)
            require(model.failure?.retry != nil && session.draft != nil && model.state(view).raw == "100", "\(change): a stale comparison keeps its retry and draft")
            require(try Data(contentsOf: config) == latest && session.history.count == count, "\(change): the conflict changes no disk bytes or History")

            let reason: String
            switch change {
            case "rules":
                checkup.rulesetIDs = ["zpam-4.08"]; reason = "server"
                require(model.takeTheirsBlock(view)?.contains(reason) == true)
            case "lock":
                var metadata = value(session.current?.metadata)
                metadata.locks.insert(view.name)
                try PrivateFile.write(try JSONEncoder().encode(metadata), to: sidecar)
                require(model.takeTheirsBlock(view) == nil, "the external lock is learned during retry refresh")
                reason = "Locked"
            case "game":
                try session.reconcileGames([4410]); reason = "running"
            case "loadout":
                model.select("Other"); reason = "loadout"
                require(model.current?.profile.id == "Other" && session.draft != nil)
            case "session":
                let replacement = try ConfigStudioSession(home: home, dialect: .cod2x)
                try replacement.refresh(); model.attach(replacement); reason = "loadout"
            default:
                model.compare(with: ConfigPlayerComparison(entryID: "other-player", name: "[Other player]", provenance: "Players catalog", values: ["cg_viewsize": "90"]))
                reason = ""
            }
            let ids = ["Main", "Scrim", "Other"]
            let paths = try ids.flatMap { id in
                ["main/players/\(id)/config_mp.cfg", "main/players/\(id)/studio.json", "main/" + (try ConfigStudioLaunch.chainBasename(profileID: id))]
            }
            func disk() -> [Data?] { paths.map { try? Data(contentsOf: home.appendingPathComponent($0)) } }
            func historyBytes() throws -> [[String: Data]] {
                try ids.map { id in
                    let directory = home.appendingPathComponent("main/players/\(id)/history")
                    let files = try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)
                    return try Dictionary(uniqueKeysWithValues: files.map { ($0.lastPathComponent, try Data(contentsOf: $0)) })
                }
            }
            let before = disk(), history = try ids.map { try store.history(id: $0).count }, savedHistory = try historyBytes()
            model.retryFailure()
            if change == "comparison" {
                require(model.failure == nil && session.draft == nil && model.state(view).raw == "100", "retry keeps the original comparison value after selecting a different comparison")
                require(session.history.count == count + 1 && session.history.first?.summary == "Took \(view.title) from Scrim", "retry records one transaction with the original comparison source")
                require(try Data(contentsOf: config) == bytes("seta cg_viewsize \"100\"\nseta sensitivity \"4\"\n"), "allowed retry preserves the newer unrelated setting")
                require(Array(disk().dropFirst()) == Array(before.dropFirst()), "allowed retry preserves metadata, companion and other loadouts")
            } else {
                let afterHistory = try ids.map { try store.history(id: $0).count }, afterHistoryBytes = try historyBytes()
                require(disk() == before && afterHistory == history && afterHistoryBytes == savedHistory,
                        "\(change): blocked retry preserves latest config, metadata, companion and every History filename/byte")
                require(model.failure?.message.contains(reason) == true && model.failure?.retry == nil, "\(change): retry gives the current blocking reason")
                require(model.session?.draft == nil, "\(change): no new failed draft is created by a blocked retry")
                if change == "rules" || change == "lock" {
                    model.set(view, to: "90")
                    require(model.failure == nil && model.state(view).raw == "90", "\(change): ownership and locks still permit the person's own edit")
                }
            }
        }
        print("PASS: comparison retries recheck ownership and target identity, preserve original value/source and leave ordinary edits writable")
    }

    /// A different valid value for a setting, so a comparison marker shows.
    @MainActor static func alternative(_ spec: ConfigSettingSpec, model: ConfigStudioModel) -> String {
        let mine = model.state(spec).number ?? 0
        if let max = spec.maximum, mine + 1 <= max { return ConfigValues.text(number: mine + 1, spec: spec) }
        if let min = spec.minimum, mine - 1 >= min { return ConfigValues.text(number: mine - 1, spec: spec) }
        return ConfigValues.text(number: mine + 1, spec: spec)
    }

    static let row = #"{"schemaVersion":1,"endedAt":"2026-10-11T19:42:00Z","endReason":"map_change","map":"mp_toujane","mode":"sd","durationSeconds":1260,"serverAddress":"192.0.2.1:28960","ruleHints":{"g_competitive":"1"},"frameCount":100,"frameTimeP50MS":3,"frameTimeP99MS":7,"frameTimeMaxMS":20,"frameTimeHistogramLimitMS":250,"framesUnderCap":98,"capSampleFrames":100,"capFPS":333,"pingAverageMS":40,"pingMaximumMS":50,"pingSamples":3,"packetsReceived":3,"packetsLost":2,"packetLossPercent":40,"snapshotsReceived":4}"#

    /// Check-up reads the game's summaries only when asked (after a game, or Refresh), shows the
    /// latest with its match, and keeps every missing value “Not measured”.
    @MainActor static func matches(scratch: URL, catalog: ConfigCatalog, rules: ConfigRules, optimizer: ConfigOptimizer) throws {
        let (model, session, _) = try studio(scratch.appendingPathComponent("acceptance-matches"),
                                             config: "seta com_maxfps \"333\"\nseta sensitivity \"2.6\"\n", catalog: catalog)
        let checkup = ConfigCheckupModel(studio: model, loadRules: { rules }, loadOptimizer: { optimizer })
        checkup.load()
        var reads = 0
        checkup.readMatches = { home in reads += 1; return try MatchSummaries.read(homePath: home) }
        let stats = session.home.appendingPathComponent("main/stats")
        try FileManager.default.createDirectory(at: stats, withIntermediateDirectories: true)
        let file = stats.appendingPathComponent("matches.jsonl")

        // Not read yet: nothing measured, nothing read.
        require(checkup.matchState == .notRead && checkup.gauges(value(checkup.report)).map(\.status).prefix(3) == ["From settings", "Not measured", "Not measured"])
        require(reads == 0)
        // No file yet, summaries on: an honest empty state.
        checkup.refreshMatches(home: session.home)
        require(checkup.matchState == .read([]) && reads == 1)
        let empty = value(checkup.matchPresentation)
        require(empty.title == "No matches recorded yet" && empty.tone == .unknown && empty.lines.isEmpty)
        checkup.matchSummariesEnabled = false
        let off = value(checkup.matchPresentation)
        require(off.title == "Match summaries are off" && off.detail.contains("Settings › Privacy"))
        checkup.matchSummariesEnabled = true

        // Two matches: the latest is shown, with its match, and reaches the report.
        let older = row.replacingOccurrences(of: "mp_toujane", with: "mp_carentan").replacingOccurrences(of: "\"pingAverageMS\":40", with: "\"pingAverageMS\":45")
        try (older + "\n" + row + "\n").write(to: file, atomically: true, encoding: .utf8)
        let unmeasured = value(checkup.report)
        require(unmeasured.dials[0].value == nil && unmeasured.limitations.contains("Packet loss: Not measured."))
        checkup.refreshMatches()
        require(reads == 2 && checkup.latestMatch?.map == "mp_toujane" && checkup.matchCount == 2)
        let report = value(checkup.report)
        require(report.dials[0].value == 7 && report.dials[2].value == 40 && !report.limitations.contains("Packet loss: Not measured."),
                "Refresh invalidates the cached report when the measurement changes")
        for _ in 0..<5 { _ = checkup.report; _ = checkup.gauges(report); _ = checkup.matchPresentation }
        require(reads == 2, "reading the view never rereads the file")
        let measured = value(checkup.matchPresentation)
        require(measured.title == "Your last match: Toujane, Search & Destroy" && measured.tone == .information, measured.title)
        require(measured.detail.contains("may have used a different loadout") && !measured.detail.contains("Main"), "a match isn't claimed for the selected loadout")
        require(measured.lines.contains("2 matches recorded on this Mac") && measured.lines.contains { $0.hasPrefix("Ended ") && $0.contains("21 min") })
        let gauges = checkup.gauges(report)
        require(gauges[0].value == "7 MS" && gauges[0].status == "Measured" && gauges[0].tone == .information)
        require(gauges[0].detail.contains("p50 3 ms · p99 7 ms · max 20 ms"), gauges[0].detail)
        // The cap can change during a match: the counts aren't attributed to the cap recorded at the end.
        require(gauges[0].detail.contains("98 of 100 capped frames (98%) within the cap in force") && gauges[0].detail.contains("cap at the end of the match 333 fps")
                && !gauges[0].detail.contains("within the 333 fps cap"), gauges[0].detail)
        require(gauges[1].status == "Not measured" || gauges[1].status == "From settings", "mouse polling is never invented")
        require(!gauges[1].detail.contains("Hz") && gauges[1].detail.contains("Polling rate isn’t measured"))
        require(gauges[2].value == "40 MS" && gauges[2].detail.contains("average 40 ms, highest 50 ms over 3 samples"), gauges[2].detail)
        require(gauges[2].detail.contains("Missing server updates: 40% (2 of 5), inferred from gaps in the update sequence; not a precise packet-loss measurement."), gauges[2].detail)
        require(gauges.allSatisfy { !$0.detail.lowercased().contains("score") && !$0.value.lowercased().contains("score") })

        // Overflow: the p99 is beyond the exact range, so it is an upper bound only.
        let slow = row.replacingOccurrences(of: "\"frameTimeP99MS\":7,\"frameTimeMaxMS\":20,\"frameTimeHistogramLimitMS\":250",
                                            with: "\"frameTimeP99MS\":300,\"frameTimeMaxMS\":300,\"frameTimeHistogramLimitMS\":250")
        try (slow + "\n").write(to: file, atomically: true, encoding: .utf8)
        checkup.refreshMatches()
        require(checkup.latestMatch?.frameQuantilesAreExact == false && checkup.report?.dials[0].value == nil, "an upper bound isn't passed on as a measured p99")
        let bound = checkup.gauges(value(checkup.report))[0]
        require(bound.value == "≤ 300 MS" && bound.status == "Upper bound" && bound.detail.contains("p99 at least 250 ms (at most 300 ms)") && !bound.detail.contains("p99 300 ms"), bound.detail)
        // A p99 sample at exactly the limit is in the overflow bucket: at least the limit, not longer than it.
        let edge = row.replacingOccurrences(of: "\"frameTimeP99MS\":7,\"frameTimeMaxMS\":20,\"frameTimeHistogramLimitMS\":250",
                                            with: "\"frameTimeP99MS\":250,\"frameTimeMaxMS\":250,\"frameTimeHistogramLimitMS\":250")
        try (edge + "\n").write(to: file, atomically: true, encoding: .utf8)
        checkup.refreshMatches()
        let atLimit = checkup.gauges(value(checkup.report))[0]
        require(checkup.latestMatch?.frameQuantilesAreExact == false && atLimit.value == "≤ 250 MS" && atLimit.detail.contains("p99 at least 250 ms")
                && !atLimit.detail.contains("longer than"), atLimit.detail)
        // A cap that ended the match off still keeps the frames counted under earlier caps.
        let uncapped = row.replacingOccurrences(of: "\"capFPS\":333", with: "\"capFPS\":0")
        try (uncapped + "\n").write(to: file, atomically: true, encoding: .utf8)
        checkup.refreshMatches()
        let capOff = checkup.gauges(value(checkup.report))[0].detail
        require(capOff.contains("98 of 100 capped frames (98%) within the cap in force") && capOff.contains("no cap at the end of the match"), capOff)

        // The largest values the reader accepts format without trapping: a duration of UInt64.max
        // milliseconds and received plus lost updates summing to exactly UInt64.max.
        let huge = row.replacingOccurrences(of: "\"durationSeconds\":1260", with: "\"durationSeconds\":18446744073709548")
            .replacingOccurrences(of: "\"pingSamples\":3,\"packetsReceived\":3,\"packetsLost\":2,\"packetLossPercent\":40,\"snapshotsReceived\":4",
                                  with: "\"pingSamples\":3,\"packetsReceived\":18446744073709551613,\"packetsLost\":2,\"packetLossPercent\":0,\"snapshotsReceived\":18446744073709551615")
        try (huge + "\n").write(to: file, atomically: true, encoding: .utf8)
        checkup.refreshMatches()
        require(checkup.latestMatch?.packetsReceived == UInt64.max - 2, "the reader accepts the largest packet sum")
        require(value(checkup.matchPresentation).lines.contains { $0.hasPrefix("Ended ") && $0.hasSuffix(" min") })
        let largest = checkup.gauges(value(checkup.report))[2].detail
        require(largest.contains("(2 of 18446744073709551615)"), largest)

        // Values a match didn't record stay Not measured.
        let quiet = row.replacingOccurrences(of: "\"pingAverageMS\":40,\"pingMaximumMS\":50,\"pingSamples\":3,\"packetsReceived\":3,\"packetsLost\":2,\"packetLossPercent\":40",
                                             with: "\"pingSamples\":0,\"packetsReceived\":0,\"packetsLost\":0")
        try (quiet + "\n").write(to: file, atomically: true, encoding: .utf8)
        checkup.refreshMatches()
        let connection = checkup.gauges(value(checkup.report))[2]
        require(connection.value == "—" && connection.status == "Not measured" && connection.detail.contains("Missing server updates: Not measured"), connection.detail)
        require(checkup.report?.limitations.contains("Packet loss: Not measured.") == true)

        // A read failure is said plainly and measures nothing.
        checkup.readMatches = { _ in throw CocoaError(.fileReadNoPermission) }
        checkup.refreshMatches()
        guard case .failed(let message) = checkup.matchState else { fatalError("FAIL: a read failure is kept as a failure") }
        require(!message.isEmpty && checkup.matchPresentation?.title == "Couldn’t read your match summaries" && checkup.matchPresentation?.tone == .negative)
        require(checkup.report?.dials[0].value == nil && checkup.gauges(value(checkup.report))[2].status == "Not measured")
        print("PASS: Check-up reads match summaries only when asked and shows the latest match's measurements, bounds and unknowns")
    }

    static func evaluation(_ setting: String, _ status: ConfigRuleStatus) -> ConfigRuleEvaluation {
        ConfigRuleEvaluation(setting: setting, value: "1", status: status, reason: "", serverOwned: false, sources: [])
    }

    /// The rules summary and the empty-suggestions message never call a partly assessed
    /// loadout one that follows the rules.
    static func standing() {
        func standing(_ statuses: [ConfigRuleStatus], rules: [String] = ["zpam-4.08"]) -> ConfigCheckupStanding {
            ConfigCheckupStanding(policy: statuses.enumerated().map { evaluation("s\($0.offset)", $0.element) }, rulesetIDs: rules,
                                  selectionTitle: "zPAM 4.08", tuneFor: .competitive)
        }
        require(standing([.ok]).status?.title == "Follows zPAM 4.08" && standing([.ok]).status?.tone == .positive)
        require(standing([.ok, .ok]).empty.title == "NOTHING TO SUGGEST" && standing([.ok]).empty.message.contains("already match"))
        let partial = standing([.ok, .unknown, .ok])
        require(partial.status?.title == "1 of 3 settings not assessed" && partial.status?.tone == .unknown, "an ok row can't speak for an unknown one")
        require(partial.empty.title == "NO SUGGESTIONS" && !partial.empty.message.contains("already match"), "withheld advice isn't a success")
        let advisory = standing([.ok, .warning])
        require(advisory.status?.title == "Within the limits, 1 advisory" && advisory.status?.tone == .caution && !advisory.empty.message.contains("already match"))
        require(standing([.ok, .unknown, .violation, .conflict]).status?.title == "1 setting breaks these rules" && standing([.violation]).status?.tone == .negative)
        require(standing([.ok, .unknown, .conflict]).status?.title == "The chosen rules disagree" && standing([.conflict]).status?.tone == .caution)
        require(standing([.unknown]).status?.title == "Not assessed" && standing([]).status?.title == "Not assessed")
        require(standing([.ok], rules: []).status == nil && standing([], rules: []).empty.title == "NO RULES")
        // Rules chosen but none of them covers a setting here: nothing was checked, so nothing “already matches”.
        let uncovered = standing([])
        require(uncovered.empty.title == "NO SUGGESTIONS" && !uncovered.empty.message.contains("already match"), "an empty policy isn't a match: \(uncovered.empty.message)")
        print("PASS: the rules summary keeps violations and conflicts first and never says Follows with rows unassessed")
    }
}
