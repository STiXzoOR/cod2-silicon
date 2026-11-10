import Foundation
import Combine
import Darwin

/// The store callback can be invoked on any thread. Actual lifecycle transitions
/// and UI mutations remain synchronous on the main actor, with no suspension
/// between reserving a launch and starting its Process.
private final class ConfigGameGuard: @unchecked Sendable {
    private let lock = NSLock()
    private var reserved = false
    private var pids: Set<Int32> = []
    let discover: @Sendable () -> Set<Int32>
    init(discover: @escaping @Sendable () -> Set<Int32>) { self.discover = discover }
    func set(reserved: Bool, pids: Set<Int32>) {
        lock.lock(); self.reserved = reserved; self.pids = pids; lock.unlock()
    }
    func running() -> Bool {
        lock.lock(); let tracked = reserved || !pids.isEmpty; lock.unlock()
        return tracked || !discover().isEmpty
    }
}

@MainActor final class ConfigStudioSession: ObservableObject {
    @Published private(set) var profiles: [ConfigProfile] = []
    @Published private(set) var current: ConfigProfileRead?
    @Published private(set) var history: [ConfigHistoryEntry] = []
    @Published private(set) var draft: ConfigDocument?
    @Published private(set) var error: String?
    @Published private(set) var migrationError: String?
    @Published private(set) var runningPIDs: Set<Int32> = []
    @Published private(set) var launchReserved = false
    @Published private(set) var migrationCompleted = false
    @Published private(set) var recovery: ConfigTransactionRecovery?
    private var store: ConfigProfileStore
    private let guardState: ConfigGameGuard
    private var observedDisk: [String: DiskStamp]?
    private struct DiskStamp: Equatable {
        let device: dev_t
        let inode: ino_t
        let mode: mode_t
        let size: off_t
        let modifiedSeconds: Int
        let modifiedNanoseconds: Int
        let changedSeconds: Int
        let changedNanoseconds: Int
        init(_ info: stat) {
            device = info.st_dev; inode = info.st_ino; mode = info.st_mode; size = info.st_size
            modifiedSeconds = info.st_mtimespec.tv_sec; modifiedNanoseconds = info.st_mtimespec.tv_nsec
            changedSeconds = info.st_ctimespec.tv_sec; changedNanoseconds = info.st_ctimespec.tv_nsec
        }
    }
    private var draftVersion: ConfigProfileVersion?
    @Published private(set) var draftProfileID: String?
    /// The loadout on screen when it is not the active one. Viewing never
    /// writes active.txt; nil follows the game's active selection.
    @Published private(set) var focusedProfileID: String?
    private var archive: SessionArchive?
    private struct SessionArchive: Codable {
        let id: String
        let before: [String: ConfigProfileVersion]
        var connectionSelection: ConnectionSelection? = nil
    }
    private struct ConnectionSelection: Codable {
        let before: Data
        let applied: Data
        var sourceProfileID: String? = nil
        var disposableProfileID: String? = nil
    }
    private var archiveURL: URL { store.homeURL.appendingPathComponent("main/players/.studio-game-session.json") }
    /// Values one engine couldn't keep, saved for the other engine's next game (see `handOff`).
    private var handOffURL: URL { store.homeURL.appendingPathComponent("main/players/.studio-engine-handoff.json") }
    /// Engine dialect → loadout → setting → value.
    private typealias HandOff = [String: [String: [String: String]]]
    var isReadOnly: Bool { guardState.running() }
    var hasDraft: Bool { draft != nil }
    var home: URL { store.homeURL }
    var configDialect: ConfigDialect { store.dialect }

    init(home: URL, dialect: ConfigDialect, discoverGames: @escaping @Sendable () -> Set<Int32> = { [] }) throws {
        let guardState = ConfigGameGuard(discover: discoverGames)
        self.guardState = guardState
        store = try ConfigProfileStore(homeURL: home, dialect: dialect, gameIsRunning: { guardState.running() })
        try ConfigProfileStore.checkPath(archiveURL, under: store.homeURL)
        if FileManager.default.fileExists(atPath: archiveURL.path) {
            let saved = try JSONDecoder().decode(SessionArchive.self, from: Data(contentsOf: archiveURL))
            guard UUID(uuidString: saved.id) != nil else { throw ConfigProfileError.invalidHistory }
            for id in saved.before.keys { try ConfigProfileStore.validateIdentifier(id) }
            if let selection = saved.connectionSelection {
                guard let beforeID = ConfigDocument.tokenize(data: selection.before, dialect: .stock).tokens.first?.text,
                      let appliedID = String(data: selection.applied, encoding: .utf8) else { throw ConfigProfileError.invalidHistory }
                try ConfigProfileStore.validateIdentifier(beforeID)
                try ConfigProfileStore.validateIdentifier(appliedID)
                if let id = selection.disposableProfileID {
                    guard id == appliedID, id.range(of: "^csconn[0-9a-f]{32}$", options: .regularExpression) != nil,
                          saved.before[id] == nil, let source = selection.sourceProfileID else { throw ConfigProfileError.invalidHistory }
                    try ConfigProfileStore.validateIdentifier(source)
                }
            }
            archive = saved
        }
    }

    /// The game's config language follows the engine the launcher starts (Settings › Game version).
    /// Loadouts are read again in the new one; an unsaved draft keeps its bytes.
    func setDialect(_ dialect: ConfigDialect) throws {
        guard dialect != store.dialect else { return }
        let guardState = guardState
        store = try ConfigProfileStore(homeURL: store.homeURL, dialect: dialect, gameIsRunning: { guardState.running() })
        if let draft { self.draft = ConfigDocument(data: draft.serialized(), dialect: dialect) }
        current = nil; observedDisk = nil
        try refresh()
    }

    /// Reads the game's current bytes without rebasing or clearing an unsaved draft.
    /// The root polls this several times a second, so unchanged results are
    /// not republished to observing views.
    func refresh() throws {
        do {
            let before = try diskRevision(historyProfileID: focusedProfileID ?? activeReviewedID())
            let listed = try managedProfiles()
            if listed != profiles { profiles = listed }
            // A loadout removed elsewhere (the game's profile menu) returns the view to the active one.
            if let focused = focusedProfileID, !profiles.contains(where: { $0.id.caseInsensitiveCompare(focused) == .orderedSame }) {
                focusedProfileID = nil
            }
            if let id = try focusedProfileID ?? activeReviewedID() {
                let read = try store.read(id: id), entries = try store.history(id: id)
                if read.profile != current?.profile || read.version != current?.version { current = read }
                if entries != history { history = entries }
            } else {
                if current != nil { current = nil }
                if !history.isEmpty { history = [] }
            }
            if error != nil { error = nil }
            let after = try diskRevision()
            // An external write during the read must be retried by the next poll.
            observedDisk = before == after ? after : nil
        } catch { observedDisk = nil; self.error = error.localizedDescription; throw error }
    }

    /// Poll file revisions without loading configs, metadata or snapshot payloads.
    /// ctime and inode catch in-place edits that preserve mtime and atomic replacements.
    private func diskRevision(historyProfileID: String? = nil) throws -> [String: DiskStamp] {
        let players = home.appendingPathComponent("main/players", isDirectory: true)
        try ConfigProfileStore.checkPath(players, under: home)
        var result: [String: DiskStamp] = [:]
        @discardableResult func observe(_ url: URL) throws -> Bool {
            var info = stat()
            guard lstat(url.path, &info) == 0 else {
                if errno == ENOENT { return false }
                throw ConfigProfileError.unsafePath
            }
            guard info.st_mode & S_IFMT != S_IFLNK else { throw ConfigProfileError.unsafePath }
            result[url.path] = DiskStamp(info)
            return true
        }
        guard try observe(players) else { return result }
        try observe(players.appendingPathComponent("active.txt"))
        for directory in try FileManager.default.contentsOfDirectory(at: players, includingPropertiesForKeys: nil) {
            let id = directory.lastPathComponent
            guard (try? ConfigProfileStore.validateIdentifier(id)) != nil else { continue }
            var info = stat()
            guard lstat(directory.path, &info) == 0, info.st_mode & S_IFMT == S_IFDIR else { continue }
            try observe(directory)
            for name in ["config_mp.cfg", "studio.json", ".studio-transaction.json"] {
                try observe(directory.appendingPathComponent(name))
            }
            try observe(home.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: id))))
            if id == (historyProfileID ?? current?.profile.id) {
                let history = directory.appendingPathComponent("history", isDirectory: true)
                if try observe(history) {
                    for file in try FileManager.default.contentsOfDirectory(at: history, includingPropertiesForKeys: nil) where file.pathExtension == "json" {
                        try observe(file)
                    }
                }
            }
        }
        return result
    }
    private func refreshIfChanged() throws {
        do {
            if try observedDisk == nil || diskRevision() != observedDisk { try refresh() }
        } catch { observedDisk = nil; self.error = error.localizedDescription; throw error }
    }
    /// Shows a loadout without selecting it for the next game. Pass nil to
    /// follow the active loadout again.
    func focus(_ id: String?) throws {
        if let id, !(try managedProfiles()).contains(where: { $0.id.caseInsensitiveCompare(id) == .orderedSame }) {
            throw ConfigProfileError.missingProfile
        }
        let previous = focusedProfileID
        focusedProfileID = id
        do { try refresh() } catch { focusedProfileID = previous; throw error }
    }
    /// Loadouts people manage; a temporary per-connection copy is never one of them.
    private func managedProfiles() throws -> [ConfigProfile] {
        let temporary = archive?.connectionSelection?.disposableProfileID
        return try store.list().filter { $0.id != temporary }
    }
    /// The loadout the next game uses, whichever one is on screen. While a temporary
    /// per-connection copy is selected, that is the saved loadout it was copied from.
    private func activeReviewedID() throws -> String? {
        guard let id = try store.activeSelection().profileID else { return nil }
        let temporary = archive?.connectionSelection?.disposableProfileID
        return id == temporary ? archive?.connectionSelection?.sourceProfileID ?? id : id
    }
    /// Reads the loadout the next game uses, for callers that must not follow the view.
    func readActive() throws -> ConfigProfileRead? { try activeReviewedID().map { try store.read(id: $0) } }

    /// Both engines read and write each loadout's config_mp.cfg. An engine that can't hold a value
    /// the other one saved keeps its own default and writes that back, so before a game starts with
    /// `engine`: what the other engine replaced goes back into each loadout (one History entry per
    /// loadout), and the values `engine` itself can't hold in the active loadout are remembered for
    /// the other engine's next game. Settings either engine can hold are never touched.
    func handOff(to engine: ConfigDialect, limits: ConfigEngineLimits) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil else { throw ConfigProfileError.conflict }
        try ConfigProfileStore.checkPath(handOffURL, under: store.homeURL)
        let saved = FileManager.default.fileExists(atPath: handOffURL.path)
        // An unreadable record must never stop a game from starting; it only loses what it held.
        var handOff = saved ? ((try? JSONDecoder().decode(HandOff.self, from: Data(contentsOf: handOffURL))) ?? [:]) : [:]
        for (id, values) in (handOff[engine.rawValue] ?? [:]).sorted(by: { $0.key < $1.key }) {
            // Removed since (or not a loadout name): nothing to put back.
            guard (try? ConfigProfileStore.validateIdentifier(id)) != nil, let read = try? store.read(id: id) else { continue }
            var document = read.document
            for (name, value) in values.sorted(by: { $0.key < $1.key }) where document.effectiveSetting(named: name) != value {
                try? document.setSetting(named: name, value: value)
            }
            if document.serialized() != read.data {
                try store.update(id: id, document: document, expected: read.version, summary: engine == .cod2x
                    ? "Put back CoD2x values that Original 1.3 can’t keep" : "Put back Original 1.3 values that CoD2x can’t keep")
            }
        }
        handOff[engine.rawValue] = nil
        let other: ConfigDialect = engine == .cod2x ? .stock : .cod2x
        // No readable active loadout (removed in game, say): nothing to remember.
        if let active = (try? readActive()) ?? nil {
            for name in limits.names(from: other) {
                guard let value = active.document.effectiveSetting(named: name), limits.refuses(value, setting: name, from: other) else { continue }
                // The first value kept wins: a second game with this engine sees its own write-back.
                if handOff[other.rawValue]?[active.profile.id]?[name] == nil { handOff[other.rawValue, default: [:]][active.profile.id, default: [:]][name] = value }
            }
        }
        if !handOff.isEmpty { try PrivateFile.write(JSONEncoder().encode(handOff), to: handOffURL) }
        else if saved { try FileManager.default.removeItem(at: handOffURL) }
        try refresh()
    }
    func setDraft(_ document: ConfigDocument) {
        if draft == nil { draftVersion = current?.version; draftProfileID = current?.profile.id }
        draft = document
    }
    func discardDraft() throws {
        try refresh()
        draft = nil; draftVersion = nil; draftProfileID = nil; recovery = nil
    }
    /// A deliberate reapply after the caller has shown the external version.
    /// Refresh alone never grants permission to replace that version.
    func rebaseDraft(on expected: ConfigProfileVersion) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft != nil, let id = draftProfileID else { throw ConfigProfileError.missingProfile }
        guard try store.read(id: id).version == expected else { throw ConfigProfileError.conflict }
        draftVersion = expected
    }
    func saveDraft(metadata: ConfigStudioMetadata? = nil, source: ConfigHistorySource = .you, summary: String) throws {
        guard let draft, let expected = draftVersion, let id = draftProfileID else { throw ConfigProfileError.missingProfile }
        do {
            _ = try store.update(id: id, document: draft, expected: expected, metadata: metadata, source: source, summary: summary)
            self.draft = nil; draftVersion = nil; draftProfileID = nil
            try refresh()
        } catch { self.error = error.localizedDescription; throw error }
    }
    func update(document: ConfigDocument, metadata: ConfigStudioMetadata? = nil, summary: String) throws {
        guard draft == nil, current != nil else { throw ConfigProfileError.conflict }
        setDraft(document)
        try saveDraft(metadata: metadata, summary: summary)
    }
    /// Advanced-bind models prepare and validate the receipt. Keep companion
    /// publication inside the same main-actor lifecycle boundary as F2 edits.
    func updateChain(profileID: String, document: ConfigDocument, metadata: ConfigStudioMetadata, chain: Data?,
                     expected: ConfigProfileVersion, summary: String) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil, current?.profile.id == profileID, current?.version == expected else { throw ConfigProfileError.conflict }
        do {
            _ = try store.updateChain(id: profileID, document: document, expected: expected, metadata: metadata, chain: chain, summary: summary)
            try refresh()
        } catch { self.error = error.localizedDescription; throw error }
    }
    /// Model units that publish through the store themselves (B1 bindings, A1
    /// Check-up) run inside the same guard as every Studio mutation: no running
    /// game, no undisposed draft, and only the loadout on screen. The store is
    /// lent for this one call and the shared state refreshes afterwards.
    func mutate<T>(profileID: String, _ body: (ConfigProfileStore) throws -> T) throws -> T {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil, current?.profile.id.caseInsensitiveCompare(profileID) == .orderedSame else { throw ConfigProfileError.conflict }
        defer { try? refresh() }
        return try body(store)
    }
    func selectProfile(_ id: String) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil else { throw ConfigProfileError.conflict }
        try store.select(id: id, expectedActive: store.activeSelection().originalBytes)
        try refresh()
    }
    /// Review an alternative without changing the active loadout or a draft.
    func readProfile(_ id: String) throws -> ConfigProfileRead { try store.read(id: id) }
    func readAdvancedChain(profileID: String) throws -> ConfigDocument? {
        let path = store.homeURL.appendingPathComponent("main/" + (try ConfigStudioLaunch.chainBasename(profileID: profileID)))
        try ConfigProfileStore.checkPath(path, under: store.homeURL)
        guard FileManager.default.fileExists(atPath: path.path) else { return nil }
        let values = try path.resourceValues(forKeys: [.isRegularFileKey])
        guard values.isRegularFile == true else { return nil }
        return ConfigDocument(data: try Data(contentsOf: path), dialect: store.dialect)
    }
    @discardableResult
    func createProfile(id: String, displayName: String, document: ConfigDocument? = nil) throws -> ConfigProfileRead {
        let result = try store.create(id: id, displayName: displayName, document: document ?? ConfigDocument(data: Data(), dialect: store.dialect))
        try refresh(); return result
    }
    @discardableResult
    func duplicateProfile(as id: String, displayName: String) throws -> ConfigProfileRead {
        guard let current else { throw ConfigProfileError.missingProfile }
        let result = try store.duplicate(id: current.profile.id, as: id, displayName: displayName, expected: current.version)
        try refresh(); return result
    }

    /// Review captures an explicit target without changing active.txt or exposing
    /// a second store to the view. Pending drafts must be disposed first.
    func readImportTarget(id: String) throws -> ConfigProfileRead {
        guard draft == nil else { throw ConfigProfileError.conflict }
        return try store.read(id: id)
    }
    /// I2 constructs and rechecks the reviewed bytes before calling this seam.
    /// F2 rechecks game ownership and the exact disk revision inside its lock.
    func commitReviewedImport(id: String, displayName: String, document: ConfigDocument,
                              expected: ConfigProfileVersion?, metadata: ConfigStudioMetadata) throws -> ConfigHistoryEntry {
        guard draft == nil, document.dialect == store.dialect else { throw ConfigProfileError.conflict }
        let entry: ConfigHistoryEntry
        if let expected {
            entry = try store.update(id: id, document: document, expected: expected, metadata: metadata,
                                     source: .import, summary: "Imported reviewed config")
        } else {
            _ = try store.create(id: id, displayName: displayName, document: document, metadata: metadata,
                                 source: .import, summary: "Imported reviewed config")
            guard let created = try store.history(id: id).first else { throw ConfigProfileError.invalidHistory }
            entry = created
        }
        // A refresh error must not make an already committed import retryable.
        // refresh publishes its own error; the successful receipt still returns.
        try? refresh()
        return entry
    }
    func restore(snapshotID: String, revision: ConfigSnapshotRevision = .after) throws {
        guard draft == nil, let current else { throw ConfigProfileError.conflict }
        _ = try store.restore(id: current.profile.id, snapshotID: snapshotID, expected: current.version, revision: revision)
        try refresh()
    }
    func inspectRecovery(profileID: String) throws {
        recovery = try store.inspectRecovery(id: profileID)
    }
    /// Explicitly keep the external disk version. Archive the interrupted intent,
    /// then let the UI compare its retained draft against refreshed disk bytes.
    func keepDiskVersion() throws {
        guard let recovery else { throw ConfigProfileError.conflict }
        try store.keepDiskVersion(recovery)
        self.recovery = nil
        try refresh()
    }

    private func migrationInput(settings: GameSettings, forwarded: [String]) throws -> ConfigMigrationInput {
        let effective = try settings.dvars()
        var mod = effective.last { $0.0.lowercased() == "fs_game" }?.1 ?? ""
        for index in forwarded.indices where index + 2 < forwarded.count {
            if ["+set", "+seta"].contains(forwarded[index].lowercased()), forwarded[index + 1].lowercased() == "fs_game" {
                mod = forwarded[index + 2].trimmingCharacters(in: CharacterSet(charactersIn: "\""))
            }
        }
        // Stock mod configs are outside F2's main-only ownership contract.
        guard store.dialect != .stock || mod.isEmpty else { throw ConfigProfileError.invalidMigration }
        let shared = store.homeURL.appendingPathComponent("main/config_mp.cfg")
        try ConfigProfileStore.checkPath(shared, under: store.homeURL)
        let source: ConfigMigrationSource, original: Data
        if store.dialect == .cod2x && FileManager.default.fileExists(atPath: shared.path) {
            source = .flattenedLegacy; original = try Data(contentsOf: shared)
        } else if let id = try store.activeSelection().profileID {
            source = .profile(id); original = try store.read(id: id).data
        } else {
            guard try store.list().isEmpty else { throw ConfigProfileError.conflict }
            source = .emptyHome; original = Data()
        }
        return try ConfigMigrationInput(source: source, expectedOriginal: original,
            personalValues: ConfigPersonalValues(effectiveLauncherDvars: effective, dpi: settings.dpi))
    }

    /// Migration failure is a visible fallback to the old, complete launch plan.
    /// A failed process start must call cancelLaunch before returning to the UI.
    func initialize(settings: GameSettings, forwarded: [String] = []) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        do {
            _ = try ConfigMigration.run(store: store, input: migrationInput(settings: settings, forwarded: forwarded))
            migrationCompleted = true; migrationError = nil
            try refresh()
        } catch {
            migrationCompleted = false; migrationError = error.localizedDescription
            throw error
        }
    }
    func prepareLaunch(settings: GameSettings, forwarded: [String] = [], connectionProfileID: String? = nil,
                       connection: ConfigConnectionLaunch? = nil, connectionExpectedVersion: ConfigProfileVersion? = nil,
                       connectionChainBytes: Data? = nil) throws -> ConfigStudioLaunch? {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil else { throw ConfigProfileError.conflict }
        if archive != nil { try finishSession() }
        var launch: ConfigStudioLaunch?
        do {
            try initialize(settings: settings, forwarded: forwarded)
        } catch { /* The complete legacy plan remains available. */ }
        let disposable = connection?.values.isEmpty == false
        if disposable {
            // Validate before allocating a write owner. GameLaunchPlan also
            // checks this against the actual password-safe pending link.
            _ = try connection!.arguments(for: LaunchLink.direct(connection!.address))
        }
        if connectionProfileID != nil || disposable {
            guard migrationCompleted else { throw ConfigProfileError.invalidMigration }
            // Without an explicit choice, the active loadout serves the connection, not the one on screen.
            guard let chosenID = try connectionProfileID ?? activeReviewedID() else { throw ConfigProfileError.missingProfile }
            let chosen = try store.read(id: chosenID)
            if disposable {
                guard chosen.version == connectionExpectedVersion,
                      try readAdvancedChain(profileID: chosenID)?.serialized() == connectionChainBytes else { throw ConfigProfileError.conflict }
            }
            let selection = try store.activeSelection()
            guard let before = selection.originalBytes else { throw ConfigProfileError.missingProfile }
            if disposable {
                // Archived +set values are serialized by the engine. Give all
                // writes from this temporary connection a disposable owner,
                // while retaining the saved loadout's optional command chain.
                let id = "csconn" + UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
                try archiveSession()
                guard archive?.before[id] == nil else { throw ConfigProfileError.alreadyExists }
                archive?.connectionSelection = ConnectionSelection(before: before, applied: Data(id.utf8),
                    sourceProfileID: chosenID, disposableProfileID: id)
                try PrivateFile.write(JSONEncoder().encode(archive!), to: archiveURL)
                _ = try store.duplicate(id: chosenID, as: id, displayName: chosen.profile.displayName, expected: chosen.version)
                try store.select(id: id, expectedActive: before)
            } else if selection.profileID != chosenID {
                try archiveSession()
                archive?.connectionSelection = ConnectionSelection(before: before, applied: Data(chosenID.utf8))
                try PrivateFile.write(JSONEncoder().encode(archive!), to: archiveURL)
                try store.select(id: chosenID, expectedActive: before)
            }
        }
        if migrationCompleted {
            try refresh()
            // The active loadout launches, whichever loadout is on screen; a temporary
            // per-connection copy keeps its saved loadout's command chain.
            guard let selected = try activeReviewedID() else { throw ConfigProfileError.missingProfile }
            let id = try store.read(id: selected).profile.id
            let chain = try ConfigStudioLaunch.chainBasename(profileID: id)
            let chainURL = store.homeURL.appendingPathComponent("main/" + chain)
            try ConfigProfileStore.checkPath(chainURL, under: store.homeURL)
            let values = try? chainURL.resourceValues(forKeys: [.isRegularFileKey])
            launch = ConfigStudioLaunch(profileID: id, chain: values?.isRegularFile == true ? chain : nil, connectionIsolated: disposable)
        }
        // Even legacy launches get an observed baseline when the store is usable.
        if migrationCompleted { try archiveSession() }
        else { do { try archiveSession() } catch { self.error = error.localizedDescription } }
        launchReserved = true; guardState.set(reserved: true, pids: runningPIDs)
        return launch
    }
    func cancelLaunch() throws {
        launchReserved = false; guardState.set(reserved: false, pids: runningPIDs)
        if runningPIDs.isEmpty && guardState.discover().isEmpty { try finishSession() }
    }
    func gameLaunched(pid: Int32) {
        launchReserved = false; runningPIDs.insert(pid)
        guardState.set(reserved: false, pids: runningPIDs)
    }
    /// Call with every live owned/attached PID. Duplicate exit notifications are
    /// harmless; no history publication occurs until this set is empty.
    func reconcileGames(_ pids: Set<Int32>) throws {
        let starting = runningPIDs.isEmpty && !pids.isEmpty
        if runningPIDs != pids { runningPIDs = pids }
        guardState.set(reserved: launchReserved, pids: pids)
        if starting && archive == nil { try archiveSession() }
        if pids.isEmpty && !launchReserved && !guardState.running() { try finishSession() }
        try refreshIfChanged()
    }
    private func archiveSession() throws {
        if archive != nil { return }
        var before: [String: ConfigProfileVersion] = [:]
        for profile in try store.list() { before[profile.id] = try store.read(id: profile.id).version }
        let saved = SessionArchive(id: UUID().uuidString.lowercased(), before: before)
        try ConfigProfileStore.checkPath(archiveURL, under: store.homeURL)
        try PrivateFile.write(JSONEncoder().encode(saved), to: archiveURL)
        archive = saved
    }
    /// Retryable after an exit-time conflict. Completed profile rows use the
    /// durable session ID, so an interrupted retry cannot duplicate snapshots.
    func finishSession() throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard let archive else { return }
        // The game's own profile menu can remove old profiles and create new
        // ones. Never recreate a deleted profile or let it block future launch.
        for profile in try store.list() where profile.id != archive.connectionSelection?.disposableProfileID {
            let before = archive.before[profile.id] ?? ConfigProfileVersion(config: Data(), metadata: nil)
            try store.recordGameSession(id: profile.id, before: before, sessionID: archive.id)
        }
        if let selection = archive.connectionSelection {
            // A game/user selection made later wins. Only undo our exact
            // temporary choice after the final game has exited. Preserve the
            // original active.txt bytes, including whitespace and comments.
            try store.restoreConnectionSelection(original: selection.before, expectedActive: selection.applied)
            if let id = selection.disposableProfileID, let source = selection.sourceProfileID {
                try store.discardConnectionProfile(id: id, sourceProfileID: source, expectedActive: selection.applied)
            }
        }
        try ConfigProfileStore.checkPath(archiveURL, under: store.homeURL)
        try FileManager.default.removeItem(at: archiveURL)
        self.archive = nil
    }
}

/// The saved settings one engine can't hold of the other's, from the catalogue's two modes: an
/// engine refuses a value outside its own range or choices, and any value of a setting it protects,
/// keeping its default. Today that is CoD2x's cl_maxpackets (to 125), snaps (to 40), r_lodScale and
/// window position (vid_xpos/vid_ypos past ±4096), and the original's com_hunkMegs (CoD2x pins it to 512).
struct ConfigEngineLimits: Sendable, Equatable {
    struct Limit: Sendable, Equatable {
        var minimum: Double?
        var maximum: Double?
        var choices: Set<String>?
        var protected = false
    }
    /// Keyed by the engine whose saved values are at risk, then the lower-cased setting name:
    /// the other engine's limits.
    var limits: [ConfigDialect: [String: Limit]] = [:]

    init(limits: [ConfigDialect: [String: Limit]]) { self.limits = limits }
    init(catalog: ConfigCatalog) {
        func number(_ fact: ConfigCatalogFact?) -> Double? {
            guard fact?.status == .literal, case .number(let value) = fact?.value else { return nil }
            return value
        }
        func choices(_ fact: ConfigCatalogFact?) -> Set<String>? {
            guard fact?.status == .literal, case .array(let values) = fact?.value else { return nil }
            return Set(values.compactMap { if case .string(let text) = $0 { return text.lowercased() }; return nil })
        }
        for setting in catalog.settings where setting.availability == .registered {
            for (from, to) in [(ConfigDialect.cod2x, ConfigCatalogMode.stock), (.stock, .cod2x)] {
                guard let mode = ConfigCatalogMode(rawValue: from.rawValue), let saved = setting.variant(for: mode),
                      let other = setting.variant(for: to), saved.flags & 1 != 0 else { continue }
                let limit = Limit(minimum: number(other.minimum), maximum: number(other.maximum), choices: choices(other.enumValues),
                                  protected: other.flags & 0xd0 != 0 && saved.flags & 0xd0 == 0)
                let lower = limit.minimum.map { minimum in number(saved.minimum).map { $0 < minimum } ?? true } ?? false
                let upper = limit.maximum.map { maximum in number(saved.maximum).map { $0 > maximum } ?? true } ?? false
                let wider = limit.choices.map { other in choices(saved.enumValues).map { !$0.isSubset(of: other) } ?? true } ?? false
                if limit.protected || lower || upper || wider { limits[from, default: [:]][setting.name.lowercased()] = limit }
            }
        }
    }
    /// Lower-cased names of the settings whose `dialect` values the other engine may not hold.
    func names(from dialect: ConfigDialect) -> [String] { (limits[dialect] ?? [:]).keys.sorted() }
    /// Whether the other engine refuses this `dialect` value (and so writes back its own).
    func refuses(_ value: String, setting name: String, from dialect: ConfigDialect) -> Bool {
        guard let limit = limits[dialect]?[name.lowercased()] else { return false }
        if limit.protected { return true }
        let trimmed = value.trimmingCharacters(in: .whitespaces)
        if let choices = limit.choices {
            if let index = Int(trimmed) { return index < 0 || index >= choices.count }
            return !choices.contains(trimmed.lowercased())
        }
        guard let number = Double(trimmed), number.isFinite else { return false }
        return limit.minimum.map { number < $0 } == true || limit.maximum.map { number > $0 } == true
    }
}
