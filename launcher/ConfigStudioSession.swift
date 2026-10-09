import Foundation
import Combine

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
    private let store: ConfigProfileStore
    private let guardState: ConfigGameGuard
    private var draftVersion: ConfigProfileVersion?
    @Published private(set) var draftProfileID: String?
    private var archive: SessionArchive?
    private struct SessionArchive: Codable {
        let id: String
        let before: [String: ConfigProfileVersion]
    }
    private var archiveURL: URL { store.homeURL.appendingPathComponent("main/players/.studio-game-session.json") }
    var isReadOnly: Bool { guardState.running() }
    var hasDraft: Bool { draft != nil }
    var home: URL { store.homeURL }

    init(home: URL, dialect: ConfigDialect, discoverGames: @escaping @Sendable () -> Set<Int32> = { [] }) throws {
        let guardState = ConfigGameGuard(discover: discoverGames)
        self.guardState = guardState
        store = try ConfigProfileStore(homeURL: home, dialect: dialect, gameIsRunning: { guardState.running() })
        try ConfigProfileStore.checkPath(archiveURL, under: store.homeURL)
        if FileManager.default.fileExists(atPath: archiveURL.path) {
            let saved = try JSONDecoder().decode(SessionArchive.self, from: Data(contentsOf: archiveURL))
            guard UUID(uuidString: saved.id) != nil else { throw ConfigProfileError.invalidHistory }
            for id in saved.before.keys { try ConfigProfileStore.validateIdentifier(id) }
            archive = saved
        }
    }

    /// Reads the game's current bytes without rebasing or clearing an unsaved draft.
    func refresh() throws {
        do {
            profiles = try store.list()
            if let id = try store.activeSelection().profileID {
                current = try store.read(id: id)
                history = try store.history(id: id)
            } else { current = nil; history = [] }
            error = nil
        } catch { self.error = error.localizedDescription; throw error }
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
    func selectProfile(_ id: String) throws {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil else { throw ConfigProfileError.conflict }
        try store.select(id: id, expectedActive: store.activeSelection().originalBytes)
        try refresh()
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
    func prepareLaunch(settings: GameSettings, forwarded: [String] = []) throws -> ConfigStudioLaunch? {
        guard !isReadOnly else { throw ConfigProfileError.gameRunning }
        guard draft == nil else { throw ConfigProfileError.conflict }
        if archive != nil { try finishSession() }
        var launch: ConfigStudioLaunch?
        do {
            try initialize(settings: settings, forwarded: forwarded)
        } catch { /* The complete legacy plan remains available. */ }
        if migrationCompleted {
            try refresh()
            guard let id = current?.profile.id else { throw ConfigProfileError.missingProfile }
            let chain = try ConfigStudioLaunch.chainBasename(profileID: id)
            let chainURL = store.homeURL.appendingPathComponent("main/" + chain)
            try ConfigProfileStore.checkPath(chainURL, under: store.homeURL)
            let values = try? chainURL.resourceValues(forKeys: [.isRegularFileKey])
            launch = ConfigStudioLaunch(profileID: id, chain: values?.isRegularFile == true ? chain : nil)
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
        runningPIDs = pids
        guardState.set(reserved: launchReserved, pids: pids)
        if starting && archive == nil { try archiveSession() }
        if pids.isEmpty && !launchReserved && !guardState.running() { try finishSession() }
        try refresh()
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
        for (id, before) in archive.before {
            try store.recordGameSession(id: id, before: before, sessionID: archive.id)
        }
        try ConfigProfileStore.checkPath(archiveURL, under: store.homeURL)
        try FileManager.default.removeItem(at: archiveURL)
        self.archive = nil
    }
}
