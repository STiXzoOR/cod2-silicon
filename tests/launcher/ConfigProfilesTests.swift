import Foundation

private enum TestFailure: Error { case injected }

private final class WriteGate: @unchecked Sendable {
    private let lock = NSLock()
    private var failure: String?
    private var remainingMatches = 0
    private var running = false
    func failOnce(_ filename: String, afterMatches: Int = 0) {
        lock.lock(); failure = filename; remainingMatches = afterMatches; lock.unlock()
    }
    func setRunning(_ value: Bool) { lock.lock(); running = value; lock.unlock() }
    func isRunning() -> Bool { lock.lock(); defer { lock.unlock() }; return running }
    func write(_ data: Data, _ url: URL) throws {
        lock.lock()
        let matches = failure == url.lastPathComponent || (failure == "history" && url.deletingLastPathComponent().lastPathComponent == "history")
        let fail = matches && remainingMatches == 0
        if matches && remainingMatches > 0 { remainingMatches -= 1 }
        if fail { failure = nil }
        lock.unlock()
        if fail { throw TestFailure.injected }
        try PrivateFile.write(data, to: url)
    }
}

@main struct ConfigProfilesTests {
    static func bytes(_ text: String) -> Data { Data(text.utf8) }
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: \(message)") }
    }
    static func rejects(_ expected: ConfigProfileError, _ operation: () throws -> Void) {
        do { try operation(); fatalError("FAIL: expected \(expected)") }
        catch let error as ConfigProfileError { require(error == expected, "expected \(expected), got \(error)") }
        catch { fatalError("FAIL: unexpected error \(error)") }
    }
    static func temporaryHome() throws -> URL {
        let base = ProcessInfo.processInfo.environment["TMPDIR"].map { URL(fileURLWithPath: $0, isDirectory: true) } ?? FileManager.default.temporaryDirectory
        let root = base.appendingPathComponent("cod2-profiles-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        return root
    }
    static func document(_ data: Data) -> ConfigDocument { ConfigDocument(data: data, dialect: .cod2x) }

    static func profilesAndTransactions() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let original = bytes("// untouched\r\nseta sensitivity \"5\"\r\nseta name \"") + Data([233, 255]) + bytes("\"\r\nmystery x; vstr chain\n")
        let first = try store.create(id: "daily", displayName: "Daily", document: document(original))
        require(first.data == original, "creation preserves every original byte")
        let second = try store.duplicate(id: "daily", as: "match", displayName: "Match", expected: first.version)
        require(second.data == original && second.metadata.origin.kind == .copied, "duplicate copies bytes and provenance")
        require(second.metadata.origin.source == "daily", "copy identifies its source profile")
        let selection = try store.activeSelection()
        try store.select(id: "match", expectedActive: selection.originalBytes)
        require(try Data(contentsOf: home.appendingPathComponent("main/players/active.txt")) == bytes("match"), "selection uses the engine active file")
        let profiles = try store.list()
        require(profiles.count == 2 && profiles.first(where: { $0.id == "match" })?.isActive == true, "list has real profiles and active state")
        var edited = second.document
        try edited.setSetting(named: "sensitivity", value: "3")
        var metadata = second.metadata
        metadata.locks = ["sensitivity"]
        metadata.notes = ["sensitivity": "Feels steady"]
        metadata.dpi = 1600
        metadata.lastImportedValues = ["sensitivity": "5"]
        let applied = try store.update(id: "match", document: edited, expected: second.version, metadata: metadata, source: .you, summary: "Changed aim speed")
        require(applied.beforeData == original && applied.data == edited.serialized(), "history retains both sides of the gesture")
        let changed = try store.read(id: "match")
        require(changed.data == original.replacing(bytes("\"5\""), with: bytes("\"3\"")), "only selected setting changes on disk")
        require(changed.metadata == metadata, "locks, notes, origin, DPI and comparison values persist")
        let restored = try store.restore(id: "match", snapshotID: applied.id, expected: changed.version, revision: .before)
        require(restored.source == .restore && restored.beforeData == changed.data && restored.data == original, "restore is a new reversible transaction")
        require(try store.read(id: "match").metadata == second.metadata, "restore includes matching metadata")
        require(try store.history(id: "match").count == 3, "copy, edit and restore each have one snapshot")
        let undone = try store.read(id: "match")
        _ = try store.restore(id: "match", snapshotID: applied.id, expected: undone.version)
        let restoredResult = try store.read(id: "match")
        require(restoredResult.data == changed.data && restoredResult.metadata == metadata, "ordinary restore selects the snapshot result")
        require(try store.history(id: "match").count == 4, "restoring either revision appends a new transaction")
        let permissions = try FileManager.default.attributesOfItem(atPath: home.appendingPathComponent("main/players/match/config_mp.cfg").path)[.posixPermissions] as? NSNumber
        require(permissions?.intValue == 0o600, "profile writes are private")
    }

    static func conflictsAndReadOnly() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, gameIsRunning: { gate.isRunning() })
        let saved = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n")))
        var next = saved.document; try next.setSetting(named: "x", value: "2")
        let path = home.appendingPathComponent("main/players/daily/config_mp.cfg")
        try PrivateFile.write(bytes("seta x \"external\"\n"), to: path)
        rejects(.conflict) { _ = try store.update(id: "daily", document: next, expected: saved.version, summary: "Change") }
        require(try Data(contentsOf: path) == bytes("seta x \"external\"\n"), "stale edit cannot overwrite external bytes")
        require(try store.history(id: "daily").count == 1, "conflict writes no snapshot")
        let current = try store.read(id: "daily")
        gate.setRunning(true)
        rejects(.gameRunning) { _ = try store.update(id: "daily", document: next, expected: current.version, summary: "Change") }
        rejects(.gameRunning) { _ = try store.create(id: "blocked", displayName: "Blocked", document: next) }
        rejects(.gameRunning) { _ = try store.duplicate(id: "daily", as: "blocked", displayName: "Blocked", expected: current.version) }
        rejects(.gameRunning) { try store.select(id: "daily", expectedActive: nil) }
        rejects(.gameRunning) { _ = try store.restore(id: "daily", snapshotID: "missing", expected: current.version) }
        require(try store.read(id: "daily").data == current.data, "running profile remains readable")
        require(!FileManager.default.fileExists(atPath: home.appendingPathComponent("main/players/blocked").path), "running create leaves no profile")
        gate.setRunning(false)
        try store.select(id: "daily", expectedActive: nil)
        try PrivateFile.write(bytes("other"), to: home.appendingPathComponent("main/players/active.txt"))
        rejects(.conflict) { try store.select(id: "daily", expectedActive: bytes("daily")) }
        var changedMetadata = current.metadata; changedMetadata.notes["x"] = "External note"
        try PrivateFile.write(JSONEncoder().encode(changedMetadata), to: home.appendingPathComponent("main/players/daily/studio.json"))
        rejects(.conflict) { _ = try store.update(id: "daily", document: next, expected: current.version, summary: "Change") }
        for invalid in ["../escape", "bad name", "x+quit", ".hidden", String(repeating: "a", count: 42)] {
            rejects(.invalidIdentifier) { _ = try store.create(id: invalid, displayName: "Unsafe", document: next) }
        }
        rejects(.alreadyExists) { _ = try store.create(id: "DAILY", displayName: "Collision", document: next) }
        try FileManager.default.createSymbolicLink(at: home.appendingPathComponent("main/players/link"), withDestinationURL: home)
        rejects(.unsafePath) { _ = try store.read(id: "link") }
        try store.recordGameSession(id: "daily", before: current.version)
        require(try store.history(id: "daily").first?.source == .gameSession, "exit records external game changes")
        let exited = try store.read(id: "daily")
        try store.recordGameSession(id: "daily", before: exited.version)
        require(try store.history(id: "daily").count == 2, "unchanged game session adds no snapshot")
    }

    static func interruptedTransactionAndRetention() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
        let first = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"0\"\n")))
        var next = first.document; try next.setSetting(named: "x", value: "1")
        var metadata = first.metadata; metadata.notes["x"] = "One"
        gate.failOnce("studio.json")
        do { _ = try store.update(id: "daily", document: next, expected: first.version, metadata: metadata, summary: "One"); fatalError("FAIL: injected failure ignored") }
        catch TestFailure.injected {}
        let recovered = try ConfigProfileStore(homeURL: home, dialect: .cod2x).read(id: "daily")
        require(recovered.data == bytes("seta x \"1\"\n") && recovered.metadata == metadata, "new store completes interrupted transaction")
        require(try store.history(id: "daily").count == 2, "recovery does not duplicate snapshots")
        for value in 2...55 {
            let before = try store.read(id: "daily")
            var doc = before.document; try doc.setSetting(named: "x", value: String(value))
            _ = try store.update(id: "daily", document: doc, expected: before.version, summary: "Changed value")
        }
        let history = try store.history(id: "daily")
        require(history.count == 50 && history.first?.data == bytes("seta x \"55\"\n"), "retention keeps newest fifty")
        require(history.last?.data == bytes("seta x \"6\"\n"), "retention removes oldest transactions")
        require(history.allSatisfy { $0.summary == "Changed value" }, "history preserves gesture summaries")
    }

    static func migration() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
        let original = bytes("// legacy\r\nseta sensitivity \"2\"\r\nseta fs_basepath \"licensed-data\"\r\n")
        try PrivateFile.write(original, to: home.appendingPathComponent("main/config_mp.cfg"))
        let unrelated = try store.create(id: "Main", displayName: "Existing Main", document: document(bytes("seta x \"unrelated\"\n")))
        let values = try ConfigPersonalValues(effectiveLauncherDvars: [("com_maxfps", "250"), ("sensitivity", "5.0"), ("SENSITIVITY", "7.0"), ("in_rawmouse", "1"), ("snd_volume", "0.8"), ("m_filter", "0"), ("cl_mouseAccel", "0"), ("r_mode", "1920x1080"), ("r_swapInterval", "1"), ("com_introPlayed", "1"), ("fs_basepath", "wrong")], dpi: 800)
        let input = ConfigMigrationInput(source: .flattenedLegacy, expectedOriginal: original, personalValues: values)
        gate.failOnce("studio.json")
        do { _ = try ConfigMigration.run(store: store, input: input); fatalError("FAIL: migration failure ignored") }
        catch TestFailure.injected {}
        require(try ConfigMigration.originalBackup(store: store) == original, "original snapshot precedes failed migration")
        require(try store.read(id: "Main").version == unrelated.version, "failed migration preserves unrelated Main")
        require(try Data(contentsOf: home.appendingPathComponent("main/config_mp.cfg")) == original, "flattened source is preserved")
        let result = try ConfigMigration.run(store: store, input: input)
        require(result.profileID == "Main_2", "unrelated Main gets a distinct migration target")
        let main = try store.read(id: result.profileID)
        require(main.profile.displayName == "Main" && main.metadata.dpi == 800, "migration creates Main with DPI metadata")
        require(main.document.effectiveSetting(named: "sensitivity") == "7.0", "effective case-insensitive last launcher override wins")
        require(main.document.effectiveSetting(named: "fs_basepath") == "licensed-data", "migration never applies fs_basepath")
        require(main.document.effectiveSetting(named: "r_mode") == nil && main.document.effectiveSetting(named: "com_introPlayed") == nil, "machine and housekeeping values stay outside loadout")
        require(try store.activeSelection().profileID == "Main_2", "migration selects target only after applying it")
        let repeatResult = try ConfigMigration.run(store: store, input: input)
        require(repeatResult.alreadyCompleted, "migration is idempotent")
        require(try store.history(id: "Main_2").count == 1 && store.read(id: "Main_2").data == main.data, "repeat migration adds no writes or snapshots")
        let initialSnapshot = try store.history(id: "Main_2")[0]
        _ = try store.restore(id: "Main_2", snapshotID: initialSnapshot.id, expected: main.version, revision: .before)
        let restoredOriginal = try store.read(id: "Main_2")
        require(restoredOriginal.data == original && restoredOriginal.version.metadata == nil, "original migration restore recovers exact bytes and absent sidecar")
        require(try ConfigMigration.originalBackup(store: store) == original, "restoring history cannot remove the independent original backup")
        let sourceHome = try temporaryHome(); defer { try? FileManager.default.removeItem(at: sourceHome) }
        let sourceStore = try ConfigProfileStore(homeURL: sourceHome, dialect: .stock)
        let source = try sourceStore.create(id: "default", displayName: "Default", document: ConfigDocument(data: original, dialect: .stock))
        let profileInput = ConfigMigrationInput(source: .profile("default"), expectedOriginal: source.data, personalValues: values)
        _ = try ConfigMigration.run(store: sourceStore, input: profileInput)
        require(try sourceStore.read(id: "default").version == source.version, "normal profile migration preserves source profile and metadata")
        require(try ConfigMigration.originalBackup(store: sourceStore) == original, "profile source snapshot is byte identical")
    }

    static func transactionFailureBoundaries() throws {
        for filename in [".studio-transaction.json", "history", "config_mp.cfg", "studio.json"] {
            let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
            let gate = WriteGate()
            let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
            let original = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"0\"\n")))
            var next = original.document; try next.setSetting(named: "x", value: "1")
            var meta = original.metadata; meta.notes["x"] = "One"
            gate.failOnce(filename)
            do { _ = try store.update(id: "daily", document: next, expected: original.version, metadata: meta, summary: "One"); fatalError("FAIL: transaction boundary ignored") }
            catch TestFailure.injected {}
            let reopened = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
            var actual = try reopened.read(id: "daily")
            if filename == ".studio-transaction.json" {
                require(actual.version == original.version, "journal failure cannot start a transaction")
                _ = try reopened.update(id: "daily", document: next, expected: original.version, metadata: meta, summary: "One")
                actual = try reopened.read(id: "daily")
            }
            require(actual.data == bytes("seta x \"1\"\n") && actual.metadata == meta, "interrupted transaction converges at \(filename)")
            require(try reopened.history(id: "daily").count == 2, "replay keeps exactly one new snapshot at \(filename)")
        }
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
        let original = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"0\"\n")))
        var next = original.document; try next.setSetting(named: "x", value: "1")
        gate.failOnce("config_mp.cfg")
        do { _ = try store.update(id: "daily", document: next, expected: original.version, summary: "One"); fatalError("FAIL: transaction failure ignored") }
        catch TestFailure.injected {}
        let configURL = home.appendingPathComponent("main/players/daily/config_mp.cfg")
        try PrivateFile.write(bytes("seta x \"external\"\n"), to: configURL)
        let reopened = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        rejects(.conflict) { _ = try reopened.read(id: "daily") }
        require(try Data(contentsOf: configURL) == bytes("seta x \"external\"\n"), "recovery refuses to overwrite later external edits")
    }

    static func migrationFailureBoundaries() throws {
        for (filename, skips) in [("original.cfg", 0), ("migration.json", 0), ("active.txt", 0), ("migration.json", 1)] {
            let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
            let gate = WriteGate()
            let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
            let source = bytes("seta sensitivity \"2\"\nseta cmdcur \"vstr tgl1\"\nseta tgl1 \"name Player; set cmdcur vstr tgl2\"\n")
            let sourceURL = home.appendingPathComponent("main/config_mp.cfg")
            try PrivateFile.write(source, to: sourceURL)
            let values = try ConfigPersonalValues(effectiveLauncherDvars: [("sensitivity", "5")], dpi: 800)
            let input = ConfigMigrationInput(source: .flattenedLegacy, expectedOriginal: source, personalValues: values)
            gate.failOnce(filename, afterMatches: skips)
            do { _ = try ConfigMigration.run(store: store, input: input); fatalError("FAIL: migration boundary ignored") }
            catch TestFailure.injected {}
            require(try Data(contentsOf: sourceURL) == source, "failed migration never alters its source at \(filename)")
            if filename != "original.cfg" { require(try ConfigMigration.originalBackup(store: store) == source, "failure retains original snapshot at \(filename)") }
            let reopened = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
            let result = try ConfigMigration.run(store: reopened, input: input)
            let migrated = try reopened.read(id: result.profileID)
            require(migrated.data == bytes("seta sensitivity \"5\"\nseta cmdcur \"vstr tgl1\"\nseta tgl1 \"name Player; set cmdcur vstr tgl2\"\n"), "resumed migration preserves archived chains")
            require(try reopened.history(id: result.profileID).count == 1, "resumed migration has one original snapshot")
            require(try ConfigMigration.run(store: reopened, input: input).alreadyCompleted, "resumed migration marks success once")
        }
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let store = try ConfigProfileStore(homeURL: home, dialect: .stock)
        let source = bytes("seta sensitivity one /* cannot discard */ two\n")
        try PrivateFile.write(source, to: home.appendingPathComponent("main/config_mp.cfg"))
        let values = try ConfigPersonalValues(effectiveLauncherDvars: [("sensitivity", "5")], dpi: 800)
        let input = ConfigMigrationInput(source: .flattenedLegacy, expectedOriginal: source, personalValues: values)
        do { _ = try ConfigMigration.run(store: store, input: input); fatalError("FAIL: lossy migration accepted") }
        catch ConfigEditError.lossyEdit {}
        require(try ConfigMigration.originalBackup(store: store) == source, "parser failure still preserves original first")
        require(try store.list().isEmpty && store.activeSelection().originalBytes == nil, "failed parser migration never claims completion or selection")
    }

    static func emptyHomeMigration() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let values = try ConfigPersonalValues(effectiveLauncherDvars: [("sensitivity", "4"), ("com_maxfps", "250")], dpi: 800)
        let input = ConfigMigrationInput(source: .emptyHome, expectedOriginal: Data(), personalValues: values)
        let result: ConfigMigrationResult
        do { result = try ConfigMigration.run(store: store, input: input) }
        catch { FileHandle.standardError.write(Data("FAIL: fresh-empty migration: \(error)\n".utf8)); throw error }
        require(result.profileID == "Main", "fresh onboarding creates Main")
        require(try store.read(id: "Main").document.effectiveSetting(named: "sensitivity") == "4", "fresh Main inherits launcher feel")
        require(try ConfigMigration.originalBackup(store: store) == Data(), "explicit absence has an empty original backup")
        require(try store.history(id: "Main").first?.beforeData == Data(), "fresh migration records its original absence")

        let appearedHome = try temporaryHome(); defer { try? FileManager.default.removeItem(at: appearedHome) }
        let appearedStore = try ConfigProfileStore(homeURL: appearedHome, dialect: .cod2x)
        let appearedURL = appearedHome.appendingPathComponent("main/config_mp.cfg")
        try PrivateFile.write(bytes("seta sensitivity \"external\"\n"), to: appearedURL)
        rejects(.conflict) { _ = try ConfigMigration.run(store: appearedStore, input: input) }
        require(try Data(contentsOf: appearedURL) == bytes("seta sensitivity \"external\"\n"), "empty-home claim cannot ignore an existing source")

        let racedHome = try temporaryHome(); defer { try? FileManager.default.removeItem(at: racedHome) }
        let racedURL = racedHome.appendingPathComponent("main/config_mp.cfg")
        let racedStore = try ConfigProfileStore(homeURL: racedHome, dialect: .cod2x, writeFile: { data, url in
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == "original.cfg" { try PrivateFile.write(Data("seta x \"raced\"\n".utf8), to: racedURL) }
        })
        rejects(.conflict) { _ = try ConfigMigration.run(store: racedStore, input: input) }
        require(try Data(contentsOf: racedURL) == bytes("seta x \"raced\"\n"), "raced-in source remains intact")
        require(try racedStore.list().isEmpty && racedStore.activeSelection().originalBytes == nil, "raced-in source prevents publication and selection")

        let reservedHome = try temporaryHome(); defer { try? FileManager.default.removeItem(at: reservedHome) }
        try FileManager.default.createDirectory(at: reservedHome.appendingPathComponent("main/players/Main"), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let reservedStore = try ConfigProfileStore(homeURL: reservedHome, dialect: .cod2x)
        let reservedResult: ConfigMigrationResult
        do { reservedResult = try ConfigMigration.run(store: reservedStore, input: input) }
        catch { FileHandle.standardError.write(Data("FAIL: reserved-empty migration: \(error)\n".utf8)); throw error }
        require(reservedResult.profileID == "Main_2", "reserved Main directory is preserved on fresh migration")
    }

    static func creationPublicationRace() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let destination = home.appendingPathComponent("main/players/daily", isDirectory: true)
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { data, url in
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == "studio.json" {
                try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
            }
        })
        rejects(.alreadyExists) { _ = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n"))) }
        require(try FileManager.default.contentsOfDirectory(atPath: destination.path).isEmpty, "publication cannot replace a raced-in empty profile directory")
    }

    static func runningTransitionDuringWrites() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, gameIsRunning: { gate.isRunning() }, writeFile: { data, url in
            require(!gate.isRunning(), "no additional file write after the game starts")
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == "config_mp.cfg" { gate.setRunning(true) }
        })
        rejects(.gameRunning) { _ = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n"))) }
        let contents = try FileManager.default.contentsOfDirectory(atPath: home.appendingPathComponent("main/players").path)
        require(contents == [".studio.lock"], "running transition removes unpublished staging data")

        let migrationHome = try temporaryHome(); defer { try? FileManager.default.removeItem(at: migrationHome) }
        gate.setRunning(false)
        let migrationStore = try ConfigProfileStore(homeURL: migrationHome, dialect: .cod2x, gameIsRunning: { gate.isRunning() }, writeFile: { data, url in
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == "active.txt" { gate.setRunning(true) }
        })
        let values = try ConfigPersonalValues(effectiveLauncherDvars: [("sensitivity", "5")], dpi: 800)
        let input = ConfigMigrationInput(source: .emptyHome, expectedOriginal: Data(), personalValues: values)
        rejects(.gameRunning) { _ = try ConfigMigration.run(store: migrationStore, input: input) }
        gate.setRunning(false)
        let resumed = try ConfigMigration.run(store: migrationStore, input: input)
        require(!resumed.alreadyCompleted, "migration interrupted at selection was not marked complete while running")
        require(try migrationStore.history(id: resumed.profileID).count == 1, "running migration retry retains one initial snapshot")
    }

    static func metadataEditBetweenPublicationSteps() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let originalStore = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let original = try originalStore.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n")))
        var next = original.document; try next.setSetting(named: "x", value: "2")
        var metadata = original.metadata; metadata.notes["x"] = "My draft"
        var externalMetadata = original.metadata; externalMetadata.notes["x"] = "External edit"
        let external = try JSONEncoder().encode(externalMetadata)
        let metadataURL = home.appendingPathComponent("main/players/daily/studio.json")
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { data, url in
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == "config_mp.cfg" { try PrivateFile.write(external, to: metadataURL) }
        })
        rejects(.conflict) { _ = try store.update(id: "daily", document: next, expected: original.version, metadata: metadata, summary: "Change") }
        require(try Data(contentsOf: metadataURL) == external, "config publication cannot overwrite a later external sidecar edit")
        rejects(.conflict) { _ = try originalStore.read(id: "daily") }
    }

    static func linkedHomeAncestor() throws {
        let root = try temporaryHome(); defer { try? FileManager.default.removeItem(at: root) }
        let licensed = root.appendingPathComponent("licensed", isDirectory: true)
        try FileManager.default.createDirectory(at: licensed, withIntermediateDirectories: false)
        let link = root.appendingPathComponent("alias", isDirectory: true)
        try FileManager.default.createSymbolicLink(at: link, withDestinationURL: licensed)
        rejects(.unsafePath) {
            let store = try ConfigProfileStore(homeURL: link.appendingPathComponent("home"), dialect: .stock)
            _ = try store.create(id: "daily", displayName: "Daily", document: ConfigDocument(data: Data(), dialect: .stock))
        }
        require(try FileManager.default.contentsOfDirectory(atPath: licensed.path).isEmpty, "a linked ancestor cannot redirect home writes into licensed data")
        for (alias, physical) in [("/var", "/private/var"), ("/tmp", "/private/tmp")] {
            let suffix = "/cod2-profiles-\(UUID().uuidString)"
            let store = try ConfigProfileStore(homeURL: URL(fileURLWithPath: alias + suffix), dialect: .stock)
            require(store.homeURL.path == physical + suffix, "macOS system aliases remain usable without following arbitrary links")
        }
        _ = try ConfigProfileStore(homeURL: FileManager.default.temporaryDirectory, dialect: .stock)
        for path in ["/", "/private/..", "/tmp/../.."] {
            rejects(.invalidHome) { _ = try ConfigProfileStore(homeURL: URL(fileURLWithPath: path), dialect: .stock) }
        }
    }

    static func invalidExistingHistory() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let original = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n")))
        let first = try store.history(id: "daily")[0]
        let directory = home.appendingPathComponent("main/players/daily")
        let snapshot = directory.appendingPathComponent("history/" + first.id + ".json")
        var invalid = try JSONSerialization.jsonObject(with: Data(contentsOf: snapshot)) as! [String: Any]
        invalid["id"] = "invalid"
        let invalidBytes = try JSONSerialization.data(withJSONObject: invalid)
        try PrivateFile.write(invalidBytes, to: snapshot)
        var next = original.document; try next.setSetting(named: "x", value: "2")
        rejects(.invalidHistory) { _ = try store.update(id: "daily", document: next, expected: original.version, summary: "Change") }
        require(try Data(contentsOf: directory.appendingPathComponent("config_mp.cfg")) == original.data, "invalid history is rejected before committing config bytes")
        require(!FileManager.default.fileExists(atPath: directory.appendingPathComponent(".studio-transaction.json").path), "invalid history does not strand a new transaction")
        require(try store.read(id: "daily").version == original.version, "invalid history keeps the original profile readable")
        require(try Data(contentsOf: snapshot) == invalidBytes, "invalid history evidence is retained")
        try PrivateFile.write(bytes("{"), to: snapshot)
        rejects(.invalidHistory) { _ = try store.update(id: "daily", document: next, expected: original.version, summary: "Change") }
        require(try store.read(id: "daily").version == original.version, "malformed history rejects a new edit without stranding its journal")
    }

    static func migrationFreezesValuesAndSelection() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
        let original = bytes("seta sensitivity \"2\"\n")
        let sourceURL = home.appendingPathComponent("main/config_mp.cfg")
        try PrivateFile.write(original, to: sourceURL)
        var settings = GameSettings(); settings.advanced = "SENSITIVITY 7\ncom_maxfps 125"
        let initialValues = try ConfigPersonalValues(effectiveLauncherDvars: settings.dvars(), dpi: settings.dpi)
        let initial = ConfigMigrationInput(source: .flattenedLegacy, expectedOriginal: original, personalValues: initialValues)
        gate.failOnce("active.txt")
        do { _ = try ConfigMigration.run(store: store, input: initial); fatalError("FAIL: selection failure ignored") }
        catch TestFailure.injected {}
        settings.sensitivity = 9; settings.dpi = 1600; settings.advanced = ""
        let later = try ConfigMigrationInput(source: .flattenedLegacy, expectedOriginal: original,
                                             personalValues: ConfigPersonalValues(effectiveLauncherDvars: settings.dvars(), dpi: settings.dpi))
        let resumed = try ConfigMigration.run(store: store, input: later)
        let main = try store.read(id: resumed.profileID)
        for (name, expected) in [("sensitivity", "7"), ("com_maxfps", "125"), ("in_rawmouse", "1"), ("snd_volume", "0.8"), ("m_filter", "0"), ("cl_mouseAccel", "0")] {
            require(main.document.effectiveSetting(named: name) == expected, "pending migration preserves its original effective launcher value for \(name)")
        }
        require(main.metadata.dpi == 800, "pending migration freezes original DPI")
        var edited = main.document; try edited.setSetting(named: "sensitivity", value: "3")
        _ = try store.update(id: main.profile.id, document: edited, expected: main.version, summary: "Changed aim speed")
        let changed = try store.read(id: main.profile.id)
        _ = try store.create(id: "other", displayName: "Other", document: document(Data()))
        try store.select(id: "other", expectedActive: store.activeSelection().originalBytes)
        let completed = try ConfigMigration.run(store: store, input: later)
        require(completed.alreadyCompleted, "completed migration does not rerun after user edits")
        require(try store.read(id: main.profile.id).version == changed.version, "repeated launch never reapplies initial personal settings")
        require(try store.activeSelection().profileID == "other", "repeated migration never reselects Main")
        require(try store.history(id: main.profile.id).count == 2, "completed retry adds no snapshot")
        require(try Data(contentsOf: sourceURL) == original, "source remains exact across pending and completed retries")
    }

    static func invalidTransactionSnapshot() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let gate = WriteGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, writeFile: { try gate.write($0, $1) })
        let original = try store.create(id: "daily", displayName: "Daily", document: document(bytes("seta x \"1\"\n")))
        var next = original.document; try next.setSetting(named: "x", value: "2")
        gate.failOnce("history")
        do { _ = try store.update(id: "daily", document: next, expected: original.version, summary: "Change"); fatalError("FAIL: snapshot failure ignored") }
        catch TestFailure.injected {}
        let directory = home.appendingPathComponent("main/players/daily")
        let journal = directory.appendingPathComponent(".studio-transaction.json")
        var invalid = try JSONSerialization.jsonObject(with: Data(contentsOf: journal)) as! [String: Any]
        var entry = invalid["entry"] as! [String: Any]
        entry["data"] = bytes("seta x \"wrong snapshot\"\n").base64EncodedString()
        invalid["entry"] = entry
        let invalidBytes = try JSONSerialization.data(withJSONObject: invalid)
        try PrivateFile.write(invalidBytes, to: journal)
        rejects(.invalidHistory) { _ = try ConfigProfileStore(homeURL: home, dialect: .cod2x).read(id: "daily") }
        require(try Data(contentsOf: directory.appendingPathComponent("config_mp.cfg")) == original.data, "mismatched recovery snapshot cannot publish config bytes")
        require(try Data(contentsOf: journal) == invalidBytes, "invalid transaction evidence is retained")
    }

    static func chainCompanions() throws {
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let read = try store.create(id: "daily", displayName: "Daily", document: ConfigDocument(data: Data("bind j \"say old\"\n".utf8), dialect: .cod2x))
        let script = Data("set next \"say new\"\n".utf8)
        var document = read.document
        try document.setBind(key: "j", binding: "vstr next")
        let entry = try store.updateChain(id: "daily", document: document, expected: read.version, metadata: read.metadata, chain: script, summary: "Chain")
        let current = try store.read(id: "daily")
        require(current.version.chain == script && entry.chain == script && entry.beforeChain == nil, "companion in current version and history")
        let copy = try store.duplicate(id: "daily", as: "copy", displayName: "Copy", expected: current.version)
        require(copy.version.chain == script && copy.document.effectiveBind(key: "j") == "vstr next", "companion copied with trigger")
        let path = home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: "daily")))
        try PrivateFile.write(Data("// external\n".utf8), to: path)
        rejects(.conflict) { _ = try store.update(id: "daily", document: document, expected: current.version, summary: "Stale") }
        let external = try store.read(id: "daily")
        _ = try store.restore(id: "daily", snapshotID: entry.id, expected: external.version, revision: .before)
        require(try store.read(id: "daily").version == read.version, "undo restores config, metadata and absent companion")
        require(!FileManager.default.fileExists(atPath: path.path), "undo removes absent-before companion")
    }

    static func corpusRoundTrip(_ root: URL) throws {
        guard let files = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else { fatalError("FAIL: corpus unavailable") }
        let home = try temporaryHome(); defer { try? FileManager.default.removeItem(at: home) }
        var count = 0, byteCount = 0
        for case let file as URL in files where ["cfg", "txt"].contains(file.pathExtension.lowercased()) {
            let input = try Data(contentsOf: file)
            for (number, dialect) in [ConfigDialect.stock, .cod2x].enumerated() {
                let store = try ConfigProfileStore(homeURL: home.appendingPathComponent("dialect-\(number)"), dialect: dialect)
                let id = "profile_\(count)"
                let saved = try store.create(id: id, displayName: "Corpus", document: ConfigDocument(data: input, dialect: dialect))
                require(try saved.data == input && store.read(id: id).data == input, "private corpus store round trip")
                let copy = try store.duplicate(id: id, as: id + "_copy", displayName: "Copy", expected: saved.version)
                require(try copy.data == input && store.history(id: id).first?.data == input, "private corpus copy and snapshot round trip")
            }
            count += 1; byteCount += input.count
        }
        require(count > 0, "empty corpus")
        print("PASS: private corpus store/create/copy/history round trip, \(count) files, \(byteCount) bytes, both dialects; content withheld")
    }

    static func main() throws {
        let cases: [(String, () throws -> Void)] = [
            ("profiles and transactions", profilesAndTransactions),
            ("conflicts and read-only", conflictsAndReadOnly),
            ("interruption and retention", interruptedTransactionAndRetention),
            ("migration", migration),
            ("transaction failures", transactionFailureBoundaries),
            ("migration failures", migrationFailureBoundaries),
            ("empty home", emptyHomeMigration),
            ("creation publication race", creationPublicationRace),
            ("metadata publication race", metadataEditBetweenPublicationSteps),
            ("running write transition", runningTransitionDuringWrites),
            ("linked home ancestor", linkedHomeAncestor),
            ("invalid existing history", invalidExistingHistory),
            ("frozen migration", migrationFreezesValuesAndSelection),
            ("invalid transaction snapshot", invalidTransactionSnapshot),
            ("chain companions", chainCompanions)
        ]
        var arguments = Array(CommandLine.arguments.dropFirst())
        var corpus: URL?
        if let index = arguments.firstIndex(of: "--corpus") {
            require(index + 1 < arguments.count, "missing corpus path")
            corpus = URL(fileURLWithPath: arguments[index + 1], isDirectory: true)
            arguments.removeSubrange(index...index + 1)
        }
        let requested = Set(arguments)
        require(requested.isSubset(of: Set(cases.map { $0.0 })), "unknown test case")
        for (name, operation) in cases where requested.isEmpty || requested.contains(name) {
            do { try operation() }
            catch { FileHandle.standardError.write(Data("FAIL: \(name): \(error)\n".utf8)); throw error }
        }
        if requested.isEmpty {
            print("PASS: \(cases.count) cases: profiles, exact-byte transactions, conflict/running guards, recovery, fifty-entry history and idempotent migration")
        } else { print("PASS: \(requested.count) selected cases") }
        if let corpus { try corpusRoundTrip(corpus) }
    }
}

private extension Data {
    func replacing(_ original: Data, with replacement: Data) -> Data {
        guard let range = range(of: original) else { return self }
        var result = self; result.replaceSubrange(range, with: replacement); return result
    }
}
