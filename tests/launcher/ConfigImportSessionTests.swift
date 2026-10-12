import Foundation

@main struct ConfigImportSessionTests {
    static func require(_ value: Bool, _ message: String) {
        if !value { fatalError("FAIL: " + message) }
    }
    static func rejects<E: Error & Equatable>(_ expected: E, _ body: () throws -> Void) {
        do { try body(); fatalError("FAIL: expected \(expected)") }
        catch { require((error as? E) == expected, "unexpected error \(error)") }
    }
    static let catalog = ConfigImportCatalogSnapshot(settings: [
        "sensitivity": .init(archived: true), "name": .init(archived: true),
        "r_mode": .init(archived: true), "r_fullscreen": .init(archived: true)
    ], commands: ["+attack": .init(bindable: true), "+forward": .init(bindable: true)])
    static func session(_ text: String, platform: ConfigImportPlatform = .native) throws -> ConfigImportSession {
        try ConfigImportSession(bundle: .init(source: .init(kind: .paste), entryPath: "config.cfg",
            files: [.init(path: "config.cfg", data: Data(text.utf8), dialect: .cod2x)]),
            dialect: .cod2x, catalog: catalog, displayName: "Imported", platform: platform)
    }
    final class CountingCatalog: ConfigImportCatalog, @unchecked Sendable {
        private let lock = NSLock()
        private var calls = 0
        var count: Int { lock.lock(); defer { lock.unlock() }; return calls }
        func setting(named: String, dialect: ConfigDialect) -> ConfigImportSetting? {
            lock.lock(); calls += 1; lock.unlock()
            return ConfigImportSessionTests.catalog.setting(named: named, dialect: dialect)
        }
        func command(named: String, dialect: ConfigDialect) -> ConfigImportCommandInfo? {
            lock.lock(); calls += 1; lock.unlock()
            return ConfigImportSessionTests.catalog.command(named: named, dialect: dialect)
        }
    }
    @MainActor static func bulkSelection(_ store: ConfigProfileStore, baseline: ConfigProfileRead) throws {
        let text = (0..<80).map { "seta fixture_\($0) \($0)\n" }.joined()
            + "seta sensitivity 9\nseta r_mode 4\nseta password synthetic-secret\nbind MOUSE1 +forward\n"
        let catalog = CountingCatalog()
        var imported = try ConfigImportSession(bundle: .init(source: .init(kind: .paste), entryPath: "bulk.cfg",
            files: [.init(path: "bulk.cfg", data: Data(text.utf8), dialect: .cod2x)]), dialect: .cod2x, catalog: catalog, displayName: "Bulk")
        imported.setTarget(.existing(baseline)); imported.setMode(.everything)
        let rows = imported.rows, selected = imported.selectedIDs, removed = selected.sorted().first!
        let keep = selected.subtracting([removed])
        var repeated = imported
        let repeatedStart = catalog.count
        repeated.setMode(.review)
        for id in repeated.selectedIDs where !keep.contains(id) { try repeated.select(id, selected: false) }
        for id in keep { try repeated.select(id, selected: true) }
        let repeatedWork = catalog.count - repeatedStart
        let bulkStart = catalog.count
        try imported.replaceSelection(keep)
        let bulkWork = catalog.count - bulkStart
        require(imported.mode == .review && imported.selectedIDs == keep && repeated.selectedIDs == keep,
                "one bulk untick retains every other eligible selection including explicit risky rows")
        require(bulkWork > 0 && repeatedWork > bulkWork * 20,
                "bulk selection reviews eligibility once; repeated=\(repeatedWork), bulk=\(bulkWork) catalog lookups")
        for row in rows where !row.canSelect {
            rejects(ConfigImportSessionError.unavailableSelection) { try imported.replaceSelection(keep.union([row.id])) }
            require(imported.selectedIDs == keep && imported.mode == .review, "blocked, locked and machine rows cannot enter the bulk selection")
        }
        rejects(ConfigImportSessionError.unavailableSelection) { try imported.replaceSelection(keep.union(["missing-row"])) }
        var applied = try session("seta name BulkApplied\n")
        applied.setMode(.everything)
        _ = try applied.apply(to: store)
        rejects(ConfigImportSessionError.alreadyApplied) { try applied.replaceSelection([]) }
        print("PASS: one bulk selection retains permissions with \(bulkWork) catalog lookups versus \(repeatedWork)")
    }
    @MainActor static func main() throws {
        let root = URL(fileURLWithPath: ProcessInfo.processInfo.environment["COD2_IMPORT_SESSION_SCRATCH"]!)
        let home = root.appendingPathComponent("home")
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let original = Data("// retained bytes\r\nseta sensitivity \"2\"\r\nbind MOUSE1 \"+attack\"\r\n".utf8)
        var metadata = ConfigStudioMetadata(displayName: "Main")
        metadata.locks = ["SENSITIVITY", "bind:mouse1"]
        let main = try store.create(id: "Main", displayName: "Main", document: .init(data: original, dialect: .cod2x), metadata: metadata)
        try store.select(id: "Main", expectedActive: nil)
        try bulkSelection(store, baseline: main)
        var imported = try session("seta sensitivity 4\nseta name New\nbind MOUSE1 +forward\nseta password test-only\nexec absent.cfg\nunknown_forwarded\n")
        require(imported.mode == .review && imported.target.isNew, "default new target and Review")
        require(imported.rows.filter { $0.candidate.classification == .needsOK }.allSatisfy { !imported.selectedIDs.contains($0.id) }, "risky rows unchecked")
        imported.setMode(.everything)
        require(imported.rows.filter { !$0.candidate.importable || $0.candidate.classification == .blocked }.allSatisfy { !imported.selectedIDs.contains($0.id) }, "everything never selects forbidden rows")
        let result = try imported.apply(to: store)
        require(result.historyEntry.source == .import, "new import is one Import snapshot")
        require(try store.history(id: result.profileID).count == 1, "one creation transaction")
        require(try store.read(id: "Main").version == main.version, "default import preserves Main")
        require(try store.activeSelection().profileID == "Main", "default import preserves selection")
        require(!result.historyEntry.data.contains(Data("password".utf8)) && !result.historyEntry.data.contains(Data("exec".utf8)), "forbidden bytes absent")
        rejects(ConfigImportSessionError.alreadyApplied) { _ = try imported.apply(to: store) }

        var locked = try session("seta sensitivity 8\nbind MOUSE1 +forward\nbind w \"toggle sensitivity 1 9\"\nunbindall\nseta name Other\n")
        locked.setTarget(.existing(main))
        locked.setMode(.everything)
        require(locked.rows.filter { !$0.lockedNames.isEmpty }.count == 4, "direct, nested, bind and unbindall locks")
        let changed = try locked.apply(to: store)
        let after = try store.read(id: "Main")
        require(after.document.effectiveSetting(named: "sensitivity") == "2" && after.document.effectiveBind(key: "MOUSE1") == "+attack", "locks preserve values")
        require(after.metadata.locks == metadata.locks, "locks preserved in metadata")
        require(try store.history(id: "Main").count == 2 && changed.historyEntry.source == .import, "one existing import transaction")
        var stale = try session("seta name Stale\n")
        stale.setTarget(.existing(after)); stale.setMode(.everything)
        var external = after.document; try external.setSetting(named: "name", value: "External")
        _ = try store.update(id: "Main", document: external, expected: after.version, summary: "External edit")
        let externalVersion = try store.read(id: "Main").version
        rejects(ConfigProfileError.conflict) { _ = try stale.apply(to: store) }
        require(try store.read(id: "Main").version == externalVersion && stale.receipt == nil, "failed apply keeps bytes and review")

        var windows = try session("seta sensitivity 6\nseta r_mode 4\nseta r_fullscreen 1\nseta in_mouse 1\nbind 0x00 +attack\n", platform: .windows)
        windows.setMode(.everything)
        require(windows.rows.filter { $0.platformNote != nil }.count == 4, "Windows display, driver and unsupported key warnings")
        let win = try windows.apply(to: store)
        let winDocument = ConfigDocument(data: win.historyEntry.data, dialect: .cod2x)
        require(winDocument.effectiveSetting(named: "sensitivity") == "6" && winDocument.effectiveSetting(named: "r_mode") == nil, "portable values retained, display stays machine-owned")
        require(!win.historyEntry.data.contains(Data("in_mouse".utf8)), "Windows driver omitted")

        var composed = try session("seta sensitivity 4\nbind w \"toggle r_mode 1 2\"\n", platform: .windows)
        composed.setMode(.everything)
        require(composed.rows.last?.platformNote != nil, "nested platform settings cannot bypass translation")
        let machine = try session("seta r_rendererPreference dx9\nseta r_renderer gl\nbind COMMAND +attack\n", platform: .windows)
        require(machine.rows.allSatisfy { $0.platformNote != nil }, "machine renderer and Windows Command choices require explicit review")
        var unterminated = try session("seta sensitivity 8\n")
        let broken = try store.create(id: "Broken", displayName: "Broken", document: .init(data: Data("seta sensitivity 2\u{0}ignored".utf8), dialect: .cod2x))
        unterminated.setTarget(.existing(broken))
        rejects(ConfigImportSessionError.unsafeComposition) { _ = try unterminated.apply(to: store) }
        require(try store.read(id: "Broken").version == broken.version, "parser failure precedes disk writes")

        var source = try session("seta helper \"sensitivity 7\"\nbind w \"vstr helper\"\n")
        source.setMode(.everything)
        let helper = source.rows.first { $0.candidate.command.tokens.dropFirst().first?.text == "helper" }!
        try source.select(helper.id, selected: false)
        rejects(ConfigImportSessionError.unsafeComposition) { _ = try source.apply(to: store) }

        // Identity locks apply to helper definitions as well as binds. Imported
        // helpers must not rewrite a variable used by an existing locked action.
        var actionMeta = ConfigStudioMetadata(displayName: "Action")
        actionMeta.locks = ["action:+attack"]
        let actionRead = try store.create(id: "Action", displayName: "Action", document: .init(data: Data("seta fire \"+attack\"\nbind MOUSE1 \"vstr fire\"\n".utf8), dialect: .cod2x), metadata: actionMeta)
        var action = try session("seta fire \"+forward\"\nseta sensitivity 5\n")
        action.setTarget(.existing(actionRead)); action.setMode(.everything)
        require(action.rows.first?.lockedNames == ["action:+attack"], "locked action protects its referenced helper")

        let secretRead = try store.create(id: "Secret", displayName: "Secret", document: .init(data: Data("bind w \"rconpassword synthetic-baseline-secret\"\n".utf8), dialect: .cod2x))
        var secret = try session("bind w +forward\n"); secret.setTarget(.existing(secretRead))
        require(secret.rows.first?.oldValue == nil, "old binding credentials stay redacted")

        let guarded = try ConfigStudioSession(home: root.appendingPathComponent("guarded"), dialect: .cod2x)
        var studioImport = try session("seta sensitivity 3\n")
        guarded.gameLaunched(pid: 99999)
        rejects(ConfigProfileError.gameRunning) { _ = try studioImport.apply(to: guarded) }
        try guarded.reconcileGames([])
        _ = try studioImport.apply(to: guarded)
        require(guarded.profiles.count == 1 && studioImport.receipt != nil, "same session store commits after exit")
        let stockStudio = try ConfigStudioSession(home: root.appendingPathComponent("stock"), dialect: .stock)
        var mismatched = try session("seta sensitivity 4\n")
        rejects(ConfigImportSessionError.invalidBundle) { _ = try mismatched.apply(to: stockStudio) }
        require(stockStudio.profiles.isEmpty, "cross-dialect import cannot create a profile")
        var pending = try session("seta sensitivity 4\n")
        guarded.setDraft(ConfigDocument(data: Data("seta sensitivity 6\n".utf8), dialect: .cod2x))
        rejects(ConfigProfileError.conflict) { _ = try pending.apply(to: guarded) }
        require(guarded.hasDraft && guarded.profiles.count == 1, "import cannot dispose or overwrite a draft")

        let failureHome = root.appendingPathComponent("failure")
        let normal = try ConfigProfileStore(homeURL: failureHome, dialect: .cod2x)
        let beforeFailure = try normal.create(id: "Main", displayName: "Main", document: .init(data: original, dialect: .cod2x))
        let failing = try ConfigProfileStore(homeURL: failureHome, dialect: .cod2x, writeFile: { _, _ in throw ConfigProfileError.cannotPublish })
        var failed = try session("seta sensitivity 9\n"); failed.setTarget(.existing(beforeFailure))
        rejects(ConfigProfileError.cannotPublish) { _ = try failed.apply(to: failing) }
        require(try normal.read(id: "Main").version == beforeFailure.version, "failed first write retains prior bytes")

        let resource = URL(fileURLWithPath: CommandLine.arguments[1]).appendingPathComponent("ConfigCatalog.json")
        let realCatalog = try ConfigCatalog(data: Data(contentsOf: resource))
        let adapter = ConfigImportCatalogAdapter(catalog: realCatalog)
        require(adapter.setting(named: "sensitivity", dialect: .cod2x)?.archived == true, "F4 archive adapter")
        require(adapter.command(named: "+attack", dialect: .stock)?.bindable == true, "F4 command adapter")
        print("PASS: import defaults, safety, locks, one transaction, conflicts, Windows review and shared session ownership")
    }
}
