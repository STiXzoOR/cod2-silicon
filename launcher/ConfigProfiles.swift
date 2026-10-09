import Foundation
import Darwin

enum ConfigProfileError: Error, LocalizedError, Equatable, Sendable {
    case invalidHome, invalidIdentifier, unsafePath, alreadyExists, missingProfile
    case conflict, gameRunning, invalidMetadata, invalidHistory, invalidMigration, cannotLock, cannotPublish
    var errorDescription: String? {
        switch self {
        case .invalidHome: return "Choose a writable game home folder."
        case .invalidIdentifier: return "Loadout identifiers need 1–41 ASCII letters, numbers, dots, underscores or hyphens, starting with a letter or number."
        case .unsafePath: return "A loadout path points outside the game home or through a symbolic link."
        case .alreadyExists: return "A loadout with this identifier already exists."
        case .missingProfile: return "This loadout no longer exists."
        case .conflict: return "The config, metadata or selection changed on disk. Refresh before saving."
        case .gameRunning: return "Quit the game before changing loadouts."
        case .invalidMetadata: return "This loadout's metadata is invalid or uses an unsupported version."
        case .invalidHistory: return "A history snapshot is invalid. Its original file has been kept."
        case .invalidMigration: return "The saved migration cannot be resumed safely. The original backup has been kept."
        case .cannotLock: return "Cannot lock the loadout folder for this change."
        case .cannotPublish: return "Cannot publish this loadout."
        }
    }
}

struct ConfigOrigin: Codable, Equatable, Sendable {
    enum Kind: String, Codable, Sendable { case created, copied, imported, migration }
    var kind: Kind = .created
    var source: String? = nil
    var credit: String? = nil
}

struct ConfigStudioMetadata: Codable, Equatable, Sendable {
    var schemaVersion = 1
    var displayName: String
    var locks: Set<String> = []
    var notes: [String: String] = [:]
    var origin = ConfigOrigin()
    var dpi: Double? = nil
    var lastImportedValues: [String: String] = [:]

    init(displayName: String) { self.displayName = displayName }
    func validate() throws {
        guard schemaVersion == 1, !displayName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              dpi == nil || (dpi!.isFinite && dpi! > 0) else { throw ConfigProfileError.invalidMetadata }
    }
}

struct ConfigProfile: Identifiable, Equatable, Sendable {
    let id: String
    let displayName: String
    let isActive: Bool
}

struct ConfigProfileVersion: Codable, Equatable, Sendable {
    let config: Data
    let metadata: Data?
}

struct ConfigProfileRead: Sendable {
    let profile: ConfigProfile
    let document: ConfigDocument
    let metadata: ConfigStudioMetadata
    let version: ConfigProfileVersion
    var data: Data { version.config }
}

struct ConfigActiveSelection: Equatable, Sendable {
    let profileID: String?
    let originalBytes: Data?
}

struct ConfigTransactionRecovery: Sendable {
    let profileID: String
    let current: ConfigProfileVersion
    let journal: Data
}

/// Synchronous disk operations serialize through flock across store instances.
/// F3 must also serialize launch/attached-process transitions with these calls;
/// gameIsRunning is rechecked inside the lock and before every config write.
final class ConfigProfileStore: Sendable {
    let homeURL: URL
    let dialect: ConfigDialect
    private let gameIsRunning: @Sendable () -> Bool
    private let writeFile: @Sendable (Data, URL) throws -> Void
    private var playersURL: URL { homeURL.appendingPathComponent("main/players", isDirectory: true) }

    init(homeURL: URL, dialect: ConfigDialect, gameIsRunning: @escaping @Sendable () -> Bool = { false },
         writeFile: @escaping @Sendable (Data, URL) throws -> Void = { try PrivateFile.write($0, to: $1) }) throws {
        guard homeURL.isFileURL, homeURL.path.hasPrefix("/"), homeURL.path != "/" else { throw ConfigProfileError.invalidHome }
        let normalizedHome = Self.ownedURL(homeURL)
        guard normalizedHome.path != "/" else { throw ConfigProfileError.invalidHome }
        self.homeURL = normalizedHome
        self.dialect = dialect
        self.gameIsRunning = gameIsRunning
        self.writeFile = writeFile
        try Self.checkPath(self.homeURL, under: self.homeURL)
    }

    private static func ownedURL(_ url: URL) -> URL {
        var explicitHome = url.standardizedFileURL
        // Foundation temporary URLs use macOS's system aliases. Resolve only
        // those aliases; an arbitrary home ancestor must never redirect writes.
        for (alias, physical) in [("/var", "/private/var"), ("/tmp", "/private/tmp")] {
            let destination = try? FileManager.default.destinationOfSymbolicLink(atPath: alias)
            if (explicitHome.path == alias || explicitHome.path.hasPrefix(alias + "/")),
               destination == physical || destination == String(physical.dropFirst()) {
                explicitHome = URL(fileURLWithPath: physical + explicitHome.path.dropFirst(alias.count), isDirectory: true)
            }
        }
        return explicitHome
    }

    static func validateIdentifier(_ id: String) throws {
        guard !id.isEmpty, id.utf8.count <= 41, let first = id.utf8.first,
              isAlphanumeric(first), id.utf8.allSatisfy({ isAlphanumeric($0) || [45, 46, 95].contains($0) }) else {
            throw ConfigProfileError.invalidIdentifier
        }
    }
    private static func isAlphanumeric(_ byte: UInt8) -> Bool {
        (65...90).contains(byte) || (97...122).contains(byte) || (48...57).contains(byte)
    }

    /// Reject links in owned descendants rather than following one into licensed
    /// data. The explicitly supplied home and its ancestors may not be links.
    static func checkPath(_ url: URL, under root: URL) throws {
        let rootPath = ownedURL(root).path, path = ownedURL(url).path
        guard path == rootPath || path.hasPrefix(rootPath + "/") else { throw ConfigProfileError.unsafePath }
        var current = URL(fileURLWithPath: "/", isDirectory: true)
        for component in path.split(separator: "/") {
            current.appendPathComponent(String(component))
            var info = stat()
            if lstat(current.path, &info) == 0 {
                guard info.st_mode & S_IFMT != S_IFLNK else { throw ConfigProfileError.unsafePath }
            } else if errno != ENOENT { throw ConfigProfileError.unsafePath }
        }
    }

    private func profileURL(_ id: String) throws -> URL {
        try Self.validateIdentifier(id)
        let url = playersURL.appendingPathComponent(id, isDirectory: true)
        try Self.checkPath(url, under: homeURL)
        return url
    }
    private func guardWritable(_ runningGame: Bool) throws {
        if runningGame || gameIsRunning() { throw ConfigProfileError.gameRunning }
    }
    private func withLock<T>(_ operation: () throws -> T) throws -> T {
        try Self.checkPath(playersURL, under: homeURL)
        try FileManager.default.createDirectory(at: playersURL, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let path = playersURL.appendingPathComponent(".studio.lock")
        let fd = open(path.path, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0o600)
        guard fd >= 0 else { throw ConfigProfileError.cannotLock }
        defer { close(fd) }
        while flock(fd, LOCK_EX) != 0 { if errno != EINTR { throw ConfigProfileError.cannotLock } }
        defer { flock(fd, LOCK_UN) }
        return try operation()
    }
    private func write(_ data: Data, to url: URL) throws {
        try Self.checkPath(url, under: homeURL)
        try guardWritable(false)
        try writeFile(data, url)
        // PrivateFile fsyncs the replacement itself; also persist the rename.
        try syncDirectory(url.deletingLastPathComponent())
    }
    private func syncDirectory(_ directory: URL) throws {
        let fd = open(directory.path, O_RDONLY | O_CLOEXEC)
        guard fd >= 0 else { throw ConfigProfileError.cannotPublish }
        defer { close(fd) }
        guard fsync(fd) == 0 else { throw ConfigProfileError.cannotPublish }
    }
    private func optionalData(_ url: URL) throws -> Data? {
        try Self.checkPath(url, under: homeURL)
        if !FileManager.default.fileExists(atPath: url.path) { return nil }
        return try Data(contentsOf: url)
    }
    private func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        return try encoder.encode(value)
    }
    private func version(at directory: URL) throws -> ConfigProfileVersion {
        guard let data = try optionalData(directory.appendingPathComponent("config_mp.cfg")) else { throw ConfigProfileError.missingProfile }
        return try ConfigProfileVersion(config: data, metadata: optionalData(directory.appendingPathComponent("studio.json")))
    }
    private func metadata(_ data: Data?, id: String) throws -> ConfigStudioMetadata {
        guard let data else { return ConfigStudioMetadata(displayName: id) }
        do {
            let result = try JSONDecoder().decode(ConfigStudioMetadata.self, from: data)
            try result.validate()
            return result
        } catch { throw ConfigProfileError.invalidMetadata }
    }
    private func selection() throws -> ConfigActiveSelection {
        let bytes = try optionalData(playersURL.appendingPathComponent("active.txt"))
        let token = bytes.flatMap { ConfigDocument.tokenize(data: $0, dialect: .stock).tokens.first?.text }
        let id = token.flatMap { (try? Self.validateIdentifier($0)) == nil ? nil : $0 }
        return ConfigActiveSelection(profileID: id, originalBytes: bytes)
    }
    func activeSelection() throws -> ConfigActiveSelection { try withLock { try selection() } }

    private func identifiers() throws -> [String] {
        try FileManager.default.contentsOfDirectory(at: playersURL, includingPropertiesForKeys: [.isDirectoryKey, .isSymbolicLinkKey])
            .filter { (try? Self.validateIdentifier($0.lastPathComponent)) != nil }
            .filter { url in
                let values = try url.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
                return values.isDirectory == true && values.isSymbolicLink != true
            }.map(\.lastPathComponent)
    }
    func list() throws -> [ConfigProfile] {
        try withLock {
            let active = try selection().profileID
            return try identifiers().sorted { $0.localizedStandardCompare($1) == .orderedAscending }.compactMap { id in
                let directory = try profileURL(id)
                try recover(directory: directory)
                guard try optionalData(directory.appendingPathComponent("config_mp.cfg")) != nil else { return nil }
                let value = try version(at: directory)
                return try ConfigProfile(id: id, displayName: metadata(value.metadata, id: id).displayName, isActive: id.caseInsensitiveCompare(active ?? "") == .orderedSame)
            }
        }
    }
    private func readLocked(id: String) throws -> ConfigProfileRead {
        let directory = try profileURL(id)
        try recover(directory: directory)
        let value = try version(at: directory), meta = try metadata(value.metadata, id: id)
        let active = try selection().profileID
        return ConfigProfileRead(profile: ConfigProfile(id: id, displayName: meta.displayName, isActive: id.caseInsensitiveCompare(active ?? "") == .orderedSame),
                                 document: ConfigDocument(data: value.config, dialect: dialect), metadata: meta, version: value)
    }
    func read(id: String) throws -> ConfigProfileRead { try withLock { try readLocked(id: id) } }

    /// Inspect an interrupted transaction without replaying it or losing the
    /// external writer's version. The exact receipt is required for disposition.
    func inspectRecovery(id: String) throws -> ConfigTransactionRecovery {
        try withLock {
            let directory = try profileURL(id)
            guard let journal = try optionalData(directory.appendingPathComponent(".studio-transaction.json")) else { throw ConfigProfileError.conflict }
            return try ConfigTransactionRecovery(profileID: id, current: version(at: directory), journal: journal)
        }
    }
    func keepDiskVersion(_ recovery: ConfigTransactionRecovery, runningGame: Bool = false) throws {
        try guardWritable(runningGame)
        try withLock {
            try guardWritable(runningGame)
            let directory = try profileURL(recovery.profileID)
            let journalURL = directory.appendingPathComponent(".studio-transaction.json")
            guard try optionalData(journalURL) == recovery.journal, try version(at: directory) == recovery.current else { throw ConfigProfileError.conflict }
            let transaction = try JSONDecoder().decode(Transaction.self, from: recovery.journal)
            guard transaction.schemaVersion == 1, UUID(uuidString: transaction.entry.id) != nil else { throw ConfigProfileError.invalidHistory }
            let saved = directory.appendingPathComponent("recovery/" + transaction.entry.id + ".json")
            if let existing = try optionalData(saved) {
                guard existing == recovery.journal else { throw ConfigProfileError.conflict }
            } else { try write(recovery.journal, to: saved) }
            guard try optionalData(journalURL) == recovery.journal, try version(at: directory) == recovery.current else { throw ConfigProfileError.conflict }
            let staged = directory.appendingPathComponent("history/" + transaction.entry.id + ".json")
            if let existing = try optionalData(staged) {
                guard existing == (try encode(transaction.entry)) else { throw ConfigProfileError.conflict }
                try guardWritable(runningGame)
                try FileManager.default.removeItem(at: staged)
                try syncDirectory(staged.deletingLastPathComponent())
            }
            try guardWritable(runningGame)
            try FileManager.default.removeItem(at: journalURL)
            try syncDirectory(directory)
        }
    }

    @discardableResult
    func create(id: String, displayName: String, document: ConfigDocument, metadata: ConfigStudioMetadata? = nil, runningGame: Bool = false) throws -> ConfigProfileRead {
        try guardWritable(runningGame)
        return try withLock {
            try guardWritable(runningGame)
            var meta = metadata ?? ConfigStudioMetadata(displayName: displayName); meta.displayName = displayName
            try createLocked(id: id, document: document, metadata: meta, source: .you, summary: "Created loadout", before: nil)
            return try readLocked(id: id)
        }
    }

    private func createLocked(id: String, document: ConfigDocument, metadata: ConfigStudioMetadata, source: ConfigHistorySource, summary: String, before: ConfigProfileVersion?) throws {
        let target = try profileURL(id)
        guard !FileManager.default.fileExists(atPath: target.path), try !identifiers().contains(where: { $0.caseInsensitiveCompare(id) == .orderedSame }) else { throw ConfigProfileError.alreadyExists }
        try metadata.validate()
        let value = try ConfigProfileVersion(config: document.serialized(), metadata: encode(metadata))
        let staging = playersURL.appendingPathComponent(".studio-create-" + UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: staging, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
        defer { try? FileManager.default.removeItem(at: staging) }
        let entry = ConfigHistoryEntry(source: source, summary: summary, before: before ?? value, after: value)
        try write(encode(entry), to: staging.appendingPathComponent("history/" + entry.id + ".json"))
        try guardWritable(false)
        try write(value.config, to: staging.appendingPathComponent("config_mp.cfg"))
        try write(value.metadata!, to: staging.appendingPathComponent("studio.json"))
        try guardWritable(false)
        guard renamex_np(staging.path, target.path, UInt32(RENAME_EXCL)) == 0 else {
            if errno == EEXIST || errno == ENOTEMPTY { throw ConfigProfileError.alreadyExists }
            throw ConfigProfileError.cannotPublish
        }
        try syncDirectory(playersURL)
    }

    @discardableResult
    func duplicate(id: String, as newID: String, displayName: String, expected: ConfigProfileVersion, runningGame: Bool = false) throws -> ConfigProfileRead {
        try guardWritable(runningGame)
        return try withLock {
            try guardWritable(runningGame)
            let original = try readLocked(id: id)
            guard original.version == expected else { throw ConfigProfileError.conflict }
            var meta = original.metadata; meta.displayName = displayName; meta.origin = ConfigOrigin(kind: .copied, source: id)
            try createLocked(id: newID, document: original.document, metadata: meta, source: .you, summary: "Copied loadout", before: nil)
            return try readLocked(id: newID)
        }
    }

    func select(id: String, expectedActive: Data?, runningGame: Bool = false) throws {
        try guardWritable(runningGame)
        try withLock {
            try guardWritable(runningGame)
            _ = try readLocked(id: id)
            guard try selection().originalBytes == expectedActive else { throw ConfigProfileError.conflict }
            try guardWritable(runningGame)
            try write(Data(id.utf8), to: playersURL.appendingPathComponent("active.txt"))
        }
    }

    private struct Transaction: Codable {
        var schemaVersion = 1
        let before: ConfigProfileVersion
        let after: ConfigProfileVersion
        let entry: ConfigHistoryEntry
    }
    private func recover(directory: URL) throws {
        let journal = directory.appendingPathComponent(".studio-transaction.json")
        guard let data = try optionalData(journal) else { return }
        try guardWritable(false)
        let transaction = try JSONDecoder().decode(Transaction.self, from: data)
        guard transaction.schemaVersion == 1 else { throw ConfigProfileError.invalidHistory }
        let current = try version(at: directory)
        guard (current.config == transaction.before.config || current.config == transaction.after.config),
              (current.metadata == transaction.before.metadata || current.metadata == transaction.after.metadata) else { throw ConfigProfileError.conflict }
        try finish(transaction, directory: directory)
    }
    private func finish(_ transaction: Transaction, directory: URL) throws {
        let entry = transaction.entry, history = directory.appendingPathComponent("history", isDirectory: true)
        guard transaction.schemaVersion == 1, UUID(uuidString: entry.id) != nil,
              entry.beforeData == transaction.before.config, entry.data == transaction.after.config,
              entry.beforeMetadata == transaction.before.metadata, entry.metadata == transaction.after.metadata else { throw ConfigProfileError.invalidHistory }
        _ = try ConfigHistory.load(directory: history)
        try guardWritable(false)
        let snapshot = history.appendingPathComponent(entry.id + ".json"), encoded = try encode(entry)
        if let existing = try optionalData(snapshot) {
            guard existing == encoded else { throw ConfigProfileError.conflict }
        } else { try write(encoded, to: snapshot) }
        // Recheck disk bytes after staging the snapshot, before overwriting.
        let current = try version(at: directory)
        guard (current.config == transaction.before.config || current.config == transaction.after.config),
              (current.metadata == transaction.before.metadata || current.metadata == transaction.after.metadata) else { throw ConfigProfileError.conflict }
        try guardWritable(false)
        if current.config != transaction.after.config { try write(transaction.after.config, to: directory.appendingPathComponent("config_mp.cfg")) }
        // Config publication can take time. Do not overwrite a sidecar edited
        // externally during that write, or complete an already-diverged pair.
        let afterConfig = try version(at: directory)
        guard afterConfig.config == transaction.after.config,
              (afterConfig.metadata == transaction.before.metadata || afterConfig.metadata == transaction.after.metadata) else { throw ConfigProfileError.conflict }
        try guardWritable(false)
        if afterConfig.metadata != transaction.after.metadata {
            let target = directory.appendingPathComponent("studio.json")
            if let metadata = transaction.after.metadata { try write(metadata, to: target) }
            else if FileManager.default.fileExists(atPath: target.path) { try FileManager.default.removeItem(at: target) }
        }
        guard try version(at: directory) == transaction.after else { throw ConfigProfileError.conflict }
        try ConfigHistory.prune(directory: history, beforeRemoval: { try self.guardWritable(false) })
        try syncDirectory(history)
        try guardWritable(false)
        try FileManager.default.removeItem(at: directory.appendingPathComponent(".studio-transaction.json"))
        try syncDirectory(directory)
    }
    private func updateLocked(id: String, document: ConfigDocument, expected: ConfigProfileVersion, metadata: ConfigStudioMetadata?, source: ConfigHistorySource, summary: String) throws -> ConfigHistoryEntry {
        let original = try readLocked(id: id)
        guard original.version == expected else { throw ConfigProfileError.conflict }
        try metadata?.validate()
        let value = try ConfigProfileVersion(config: document.serialized(), metadata: metadata.map(encode) ?? original.version.metadata)
        let entry = ConfigHistoryEntry(source: source, summary: summary, before: expected, after: value)
        let transaction = Transaction(before: expected, after: value, entry: entry)
        let directory = try profileURL(id)
        _ = try ConfigHistory.load(directory: directory.appendingPathComponent("history"))
        try guardWritable(false)
        try write(encode(transaction), to: directory.appendingPathComponent(".studio-transaction.json"))
        try finish(transaction, directory: directory)
        return entry
    }
    @discardableResult
    func update(id: String, document: ConfigDocument, expected: ConfigProfileVersion, metadata: ConfigStudioMetadata? = nil, source: ConfigHistorySource = .you, summary: String, runningGame: Bool = false) throws -> ConfigHistoryEntry {
        try guardWritable(runningGame)
        return try withLock {
            try guardWritable(runningGame)
            return try updateLocked(id: id, document: document, expected: expected, metadata: metadata, source: source, summary: summary)
        }
    }
    func history(id: String) throws -> [ConfigHistoryEntry] {
        try withLock {
            let directory = try profileURL(id); try recover(directory: directory)
            _ = try version(at: directory)
            return try ConfigHistory.load(directory: directory.appendingPathComponent("history", isDirectory: true))
        }
    }
    @discardableResult
    func restore(id: String, snapshotID: String, expected: ConfigProfileVersion, revision: ConfigSnapshotRevision = .after, runningGame: Bool = false) throws -> ConfigHistoryEntry {
        try guardWritable(runningGame)
        return try withLock {
            try guardWritable(runningGame)
            let directory = try profileURL(id); try recover(directory: directory)
            guard let entry = try ConfigHistory.load(directory: directory.appendingPathComponent("history")).first(where: { $0.id == snapshotID }) else { throw ConfigProfileError.invalidHistory }
            let bytes = revision == .before ? entry.beforeData : entry.data
            let meta = revision == .before ? entry.beforeMetadata : entry.metadata
            // A legacy absent sidecar is restored byte-for-byte, too.
            let original = try readLocked(id: id)
            guard original.version == expected else { throw ConfigProfileError.conflict }
            if let meta { _ = try metadata(meta, id: id) }
            let after = ConfigProfileVersion(config: bytes, metadata: meta)
            let restored = ConfigHistoryEntry(source: .restore, summary: "Restored snapshot", before: expected, after: after)
            let transaction = Transaction(before: expected, after: after, entry: restored)
            try write(encode(transaction), to: directory.appendingPathComponent(".studio-transaction.json"))
            try finish(transaction, directory: directory)
            return restored
        }
    }
    /// Call after the last attached or launched engine has exited. It records the
    /// observed result without rewriting a file that the game has just saved.
    func recordGameSession(id: String, before: ConfigProfileVersion, summary: String = "Game session", sessionID: String? = nil, runningGame: Bool = false) throws {
        try guardWritable(runningGame)
        try withLock {
            try guardWritable(runningGame)
            let current = try readLocked(id: id)
            if current.version == before { return }
            let directory = try profileURL(id)
            let history = try ConfigHistory.load(directory: directory.appendingPathComponent("history"))
            if let sessionID {
                guard UUID(uuidString: sessionID) != nil else { throw ConfigProfileError.invalidHistory }
                if let recorded = history.first(where: { $0.id == sessionID }) {
                    guard recorded.source == .gameSession, recorded.beforeData == before.config,
                          recorded.beforeMetadata == before.metadata else { throw ConfigProfileError.conflict }
                    return
                }
            }
            let entry = ConfigHistoryEntry(source: .gameSession, summary: summary, before: before, after: current.version, id: sessionID)
            try write(encode(entry), to: directory.appendingPathComponent("history/" + entry.id + ".json"))
            try ConfigHistory.prune(directory: directory.appendingPathComponent("history"), beforeRemoval: { try self.guardWritable(false) })
            try syncDirectory(directory.appendingPathComponent("history"))
        }
    }

    // Migration is kept here so the entire selection/profile operation shares
    // the same lock as ordinary transactions.
    private struct MigrationRecord: Codable {
        let schemaVersion: Int
        let source: ConfigMigrationSource
        let original: Data
        let targetID: String
        let expectedActive: Data?
        let target: ConfigProfileVersion
        var completed: Bool
    }
    private var migrationURL: URL { playersURL.appendingPathComponent(".studio-migration", isDirectory: true) }
    private func migrationSourceBytes(_ source: ConfigMigrationSource, excludingProfileID: String? = nil) throws -> Data {
        switch source {
        case .emptyHome:
            guard try optionalData(homeURL.appendingPathComponent("main/config_mp.cfg")) == nil else { throw ConfigProfileError.conflict }
            for listed in try FileManager.default.contentsOfDirectory(at: playersURL, includingPropertiesForKeys: [.isDirectoryKey]) {
                let name = listed.lastPathComponent
                if name.hasPrefix(".studio-") || name == excludingProfileID { continue }
                // Resource-value URLs can canonicalize /var to /private/var
                // when appending a child. Rebuild from the explicit home root.
                let directory = playersURL.appendingPathComponent(name, isDirectory: true)
                try Self.checkPath(directory, under: homeURL)
                if try listed.resourceValues(forKeys: [.isDirectoryKey]).isDirectory == true {
                    guard try optionalData(playersURL.appendingPathComponent(name + "/config_mp.cfg")) == nil else { throw ConfigProfileError.conflict }
                }
            }
            return Data()
        case .flattenedLegacy:
            guard let data = try optionalData(homeURL.appendingPathComponent("main/config_mp.cfg")) else { throw ConfigProfileError.conflict }
            return data
        case .profile(let id):
            guard let data = try optionalData(profileURL(id).appendingPathComponent("config_mp.cfg")) else { throw ConfigProfileError.conflict }
            return data
        }
    }
    fileprivate func originalMigrationBackup() throws -> Data? {
        try withLock { try optionalData(migrationURL.appendingPathComponent("original.cfg")) }
    }
    fileprivate func migrate(input: ConfigMigrationInput, runningGame: Bool) throws -> ConfigMigrationResult {
        try guardWritable(runningGame)
        return try withLock {
            try guardWritable(runningGame)
            let recordURL = migrationURL.appendingPathComponent("migration.json")
            var record: MigrationRecord
            if let saved = try optionalData(recordURL) {
                record = try JSONDecoder().decode(MigrationRecord.self, from: saved)
                guard record.schemaVersion == 1, try optionalData(migrationURL.appendingPathComponent("original.cfg")) == record.original else { throw ConfigProfileError.invalidMigration }
                try Self.validateIdentifier(record.targetID)
                if record.completed {
                    _ = try readLocked(id: record.targetID)
                    return ConfigMigrationResult(profileID: record.targetID, alreadyCompleted: true)
                }
            } else {
                let original = try migrationSourceBytes(input.source)
                guard original == input.expectedOriginal else { throw ConfigProfileError.conflict }
                let backup = migrationURL.appendingPathComponent("original.cfg")
                if let existing = try optionalData(backup) { guard existing == original else { throw ConfigProfileError.conflict } }
                else { try write(original, to: backup) }
                guard try migrationSourceBytes(input.source) == original else { throw ConfigProfileError.conflict }
                // Snapshot first: even a parser rejection leaves the original.
                var document = ConfigDocument(data: original, dialect: dialect)
                for value in input.personalValues.settings { try document.setSetting(named: value.name, value: value.value) }
                let names = try identifiers()
                var targetID = "Main", suffix = 2
                while names.contains(where: { $0.caseInsensitiveCompare(targetID) == .orderedSame }) {
                    targetID = "Main_\(suffix)"; suffix += 1
                }
                var meta = ConfigStudioMetadata(displayName: "Main")
                meta.origin = ConfigOrigin(kind: .migration, source: input.source.description)
                meta.dpi = input.personalValues.dpi
                record = try MigrationRecord(schemaVersion: 1, source: input.source, original: original, targetID: targetID,
                                             expectedActive: selection().originalBytes,
                                             target: ConfigProfileVersion(config: document.serialized(), metadata: encode(meta)), completed: false)
                try write(encode(record), to: recordURL)
            }
            guard try migrationSourceBytes(record.source, excludingProfileID: record.targetID) == record.original else { throw ConfigProfileError.conflict }
            let directory = try profileURL(record.targetID)
            if FileManager.default.fileExists(atPath: directory.path) {
                guard try readLocked(id: record.targetID).version == record.target else { throw ConfigProfileError.conflict }
            } else {
                let meta = try metadata(record.target.metadata, id: record.targetID)
                try createLocked(id: record.targetID, document: ConfigDocument(data: record.target.config, dialect: dialect), metadata: meta,
                                 source: .migration, summary: "Imported existing launcher settings", before: ConfigProfileVersion(config: record.original, metadata: nil))
            }
            guard try migrationSourceBytes(record.source, excludingProfileID: record.targetID) == record.original else { throw ConfigProfileError.conflict }
            let active = try selection().originalBytes, desiredActive = Data(record.targetID.utf8)
            guard active == record.expectedActive || active == desiredActive else { throw ConfigProfileError.conflict }
            try guardWritable(runningGame)
            if active != desiredActive { try write(desiredActive, to: playersURL.appendingPathComponent("active.txt")) }
            record.completed = true
            try write(encode(record), to: recordURL)
            return ConfigMigrationResult(profileID: record.targetID, alreadyCompleted: false)
        }
    }
}

enum ConfigMigrationSource: Codable, Equatable, Sendable {
    /// Caller has positively established that no writable profile config exists.
    case emptyHome
    /// Caller explicitly chooses the obsolete native flattened source.
    case flattenedLegacy
    /// Normal engine profile under the writable home; never fs_basepath.
    case profile(String)
    var description: String {
        switch self {
        case .emptyHome: return "New installation"
        case .flattenedLegacy: return "main/config_mp.cfg"
        case .profile(let id): return "players/\(id)/config_mp.cfg"
        }
    }
}

struct ConfigPersonalValues: Sendable {
    struct Setting: Sendable { let name: String; let value: String }
    let settings: [Setting]
    let dpi: Double
    /// Pass GameSettings.dvars() before changing old launch semantics: advanced
    /// personal overrides are included and last-wins matching is ASCII-insensitive.
    init(effectiveLauncherDvars: [(String, String)], dpi: Double) throws {
        guard dpi.isFinite && dpi > 0 else { throw ConfigProfileError.invalidMetadata }
        self.dpi = dpi
        let machine = GameSettings.machineDvarNames
        var values: [Setting] = []
        for (name, value) in effectiveLauncherDvars {
            let lower = name.lowercased()
            guard !machine.contains(lower), !lower.contains("password"), !lower.contains("codkey"), !["cdkey", "cl_cdkey"].contains(lower) else { continue }
            values.removeAll { $0.name.lowercased() == lower }
            values.append(Setting(name: name, value: value))
        }
        settings = values
    }
}

struct ConfigMigrationInput: Sendable {
    let source: ConfigMigrationSource
    let expectedOriginal: Data
    let personalValues: ConfigPersonalValues
}
struct ConfigMigrationResult: Sendable {
    let profileID: String
    let alreadyCompleted: Bool
}
enum ConfigMigration {
    static func run(store: ConfigProfileStore, input: ConfigMigrationInput, runningGame: Bool = false) throws -> ConfigMigrationResult {
        try store.migrate(input: input, runningGame: runningGame)
    }
    static func originalBackup(store: ConfigProfileStore) throws -> Data? { try store.originalMigrationBackup() }
}
