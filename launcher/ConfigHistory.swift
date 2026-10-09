import Foundation

enum ConfigHistorySource: String, Codable, Sendable {
    case you, `import`, checkup, gameSession, restore, migration
}

enum ConfigSnapshotRevision: Sendable { case before, after }

/// Both sides are retained: an ordinary history row restores its result, while
/// undo and the first migration can also recover the original bytes.
struct ConfigHistoryEntry: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let date: Date
    let source: ConfigHistorySource
    let summary: String
    let beforeData: Data
    let data: Data
    let beforeMetadata: Data?
    let metadata: Data?

    init(source: ConfigHistorySource, summary: String, before: ConfigProfileVersion, after: ConfigProfileVersion) {
        id = UUID().uuidString.lowercased()
        date = Date()
        self.source = source
        self.summary = summary
        beforeData = before.config
        data = after.config
        beforeMetadata = before.metadata
        metadata = after.metadata
    }
}

enum ConfigHistory {
    static let retention = 50

    static func load(directory: URL) throws -> [ConfigHistoryEntry] {
        guard FileManager.default.fileExists(atPath: directory.path) else { return [] }
        return try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)
            .filter { $0.pathExtension == "json" }
            .map { url in
                try ConfigProfileStore.checkPath(url, under: directory)
                let data = try Data(contentsOf: url)
                let entry: ConfigHistoryEntry
                do { entry = try JSONDecoder().decode(ConfigHistoryEntry.self, from: data) }
                catch { throw ConfigProfileError.invalidHistory }
                guard UUID(uuidString: entry.id) != nil, url.deletingPathExtension().lastPathComponent == entry.id else {
                    throw ConfigProfileError.invalidHistory
                }
                return entry
            }
            .sorted { $0.date == $1.date ? $0.id > $1.id : $0.date > $1.date }
    }

    static func prune(directory: URL, beforeRemoval: () throws -> Void) throws {
        for entry in try load(directory: directory).dropFirst(retention) {
            try beforeRemoval()
            try FileManager.default.removeItem(at: directory.appendingPathComponent(entry.id + ".json"))
        }
    }
}
