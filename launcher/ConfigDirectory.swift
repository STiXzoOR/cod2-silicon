import Foundation
import CryptoKit
import Darwin

enum ConfigDirectoryError: Error, Equatable, LocalizedError, Sendable {
    case invalidConfiguration, invalidSignature, invalidIndex, invalidCache, unsafeCache, identityMismatch, profileMismatch, missingEntry, rollback
    var errorDescription: String? {
        switch self {
        case .invalidConfiguration: return "The directory has no valid publication or trust configuration."
        case .invalidSignature: return "The directory signature could not be verified."
        case .invalidIndex: return "The directory index has invalid metadata or an unsupported version."
        case .invalidCache: return "The cached directory could not be verified."
        case .unsafeCache: return "The directory cache path is not a regular private cache file."
        case .identityMismatch: return "The downloaded file is not the selected directory resource."
        case .profileMismatch: return "The config bytes differ from the signed directory entry."
        case .missingEntry: return "This entry is not in the verified directory."
        case .rollback: return "The directory response is older than the verified cache, or changes an existing revision."
        }
    }
}
struct ConfigDirectoryPermission: Codable, Equatable, Sendable {
    let grantedBy: String
    let grantedAt: String
    let evidenceURL: URL
    let scope: String
}
enum ConfigDirectoryPlatform: String, Codable, Sendable { case windows, native }
struct ConfigDirectoryEntry: Codable, Equatable, Sendable, Identifiable {
    let id: String
    let player: String
    let team: String?
    let era: String
    let role: String
    let feel: String
    let story: String
    let styleTags: [String]
    let favoriteMap: String?
    let credit: String
    let sourceURL: URL
    let permission: ConfigDirectoryPermission
    let configURL: URL
    let sha256: String
    let byteCount: Int
    let platform: ConfigDirectoryPlatform
}
struct ConfigDirectoryIndex: Codable, Equatable, Sendable {
    let schemaVersion: Int
    /// Monotonic publication revision. A cache never silently accepts rollback.
    let revision: Int
    let entries: [ConfigDirectoryEntry]
}
struct ConfigDirectoryConfiguration: Sendable {
    let indexURL: URL
    let signatureURL: URL
    let publicKey: Data
    init(indexURL: URL, signatureURL: URL, publicKey: Data) throws {
        guard ConfigDirectory.publicURL(indexURL), ConfigDirectory.publicURL(signatureURL),
              indexURL != signatureURL, indexURL.deletingLastPathComponent() == signatureURL.deletingLastPathComponent(),
              publicKey.count == 32, (try? Curve25519.Signing.PublicKey(rawRepresentation: publicKey)) != nil else {
            throw ConfigDirectoryError.invalidConfiguration
        }
        self.indexURL = indexURL; self.signatureURL = signatureURL; self.publicKey = publicKey
    }
}
struct ConfigDirectorySnapshot: Sendable {
    enum Origin: Sendable { case network, cache }
    let index: ConfigDirectoryIndex
    let origin: Origin
    fileprivate let bytes: Data
    fileprivate let signature: Data
    fileprivate let configuration: ConfigDirectoryConfiguration
    fileprivate init(index: ConfigDirectoryIndex, origin: Origin, bytes: Data, signature: Data, configuration: ConfigDirectoryConfiguration) {
        self.index = index; self.origin = origin; self.bytes = bytes; self.signature = signature; self.configuration = configuration
    }
}
struct ConfigDirectoryLoad: Sendable {
    let snapshot: ConfigDirectorySnapshot?
    let unavailableReason: String?
    let refreshError: String?
}
struct ConfigDirectorySavedEntry: Sendable, Identifiable {
    let id: String
    /// Nil means the current signed index no longer offers this bookmark.
    let entry: ConfigDirectoryEntry?
}

/// There is deliberately no production URL or embedded key yet. Explicit
/// configuration is required, and tests generate ephemeral fixture keys.
struct ConfigDirectory: Sendable {
    var configuration: ConfigDirectoryConfiguration? = nil
    let cacheURL: URL
    var fetcher: any ConfigImportTextFetcher = ConfigImportURLSessionFetcher()
    static let maximumIndexBytes = 1_048_576
    private struct Cache: Codable {
        let schemaVersion: Int
        let indexURL: URL
        let signatureURL: URL
        let publicKey: Data
        let index: Data
        let signature: Data
    }
    private struct Saved: Codable {
        let schemaVersion: Int
        let ids: Set<String>
    }
    private var savedURL: URL { cacheURL.appendingPathExtension("saved.json") }

    func savedEntries(snapshot: ConfigDirectorySnapshot) throws -> [ConfigDirectorySavedEntry] {
        try checkSnapshot(snapshot)
        let entries = Dictionary(uniqueKeysWithValues: snapshot.index.entries.map { ($0.id, $0) })
        return try readSaved().sorted().map { .init(id: $0, entry: entries[$0]) }
    }
    func setSaved(entryID: String, saved: Bool, snapshot: ConfigDirectorySnapshot) throws {
        try checkSnapshot(snapshot)
        guard Self.validID(entryID), !saved || snapshot.index.entries.contains(where: { $0.id == entryID }) else { throw ConfigDirectoryError.missingEntry }
        try withCacheLock {
            var ids = try readSaved()
            if saved { ids.insert(entryID) } else { ids.remove(entryID) }
            guard ids.count <= 2048 else { throw ConfigDirectoryError.invalidCache }
            try checkCachePath(savedURL)
            let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
            try encoder.encode(Saved(schemaVersion: 1, ids: ids)).write(to: savedURL, options: .atomic)
            try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: savedURL.path)
        }
    }
    private func readSaved() throws -> Set<String> {
        try checkCachePath(savedURL)
        guard FileManager.default.fileExists(atPath: savedURL.path) else { return [] }
        let handle = try FileHandle(forReadingFrom: savedURL); defer { try? handle.close() }
        guard let data = try handle.read(upToCount: 262145), data.count <= 262144,
              let saved = try? JSONDecoder().decode(Saved.self, from: data), saved.schemaVersion == 1,
              saved.ids.count <= 2048, saved.ids.allSatisfy(Self.validID) else { throw ConfigDirectoryError.invalidCache }
        return saved.ids
    }
    private func checkSnapshot(_ snapshot: ConfigDirectorySnapshot) throws {
        guard let configuration, configuration.publicKey == snapshot.configuration.publicKey,
              configuration.indexURL == snapshot.configuration.indexURL,
              configuration.signatureURL == snapshot.configuration.signatureURL else { throw ConfigDirectoryError.invalidConfiguration }
        _ = try verify(bytes: snapshot.bytes, signature: snapshot.signature, configuration: configuration)
    }

    func load() async throws -> ConfigDirectoryLoad {
        guard let configuration else {
            return .init(snapshot: nil, unavailableReason: "The player directory is not available yet. Permissioned entries and signed publication are still being prepared.", refreshError: nil)
        }
        let cached = try withCacheLock { try readCache(configuration) }
        let responses: (ConfigImportTextResponse, ConfigImportTextResponse)
        do {
            let root = configuration.indexURL.deletingLastPathComponent()
            let index = try await fetcher.fetch(.init(url: configuration.indexURL, maximumBytes: Self.maximumIndexBytes, root: root))
            let signature = try await fetcher.fetch(.init(url: configuration.signatureURL, maximumBytes: 64, root: root))
            responses = (index, signature)
        } catch {
            // Cancellation and policy failures are not network outages.
            try Task.checkCancellation()
            let offline = (error as? URLError).map { [.notConnectedToInternet, .networkConnectionLost, .timedOut,
                .cannotFindHost, .cannotConnectToHost, .dnsLookupFailed].contains($0.code) } == true
            let httpUnavailable: Bool
            if case ConfigImportError.httpStatus(let status) = error { httpUnavailable = status >= 500 }
            else { httpUnavailable = false }
            guard offline || httpUnavailable else { throw error }
            return .init(snapshot: cached, unavailableReason: cached == nil ? "The directory is unavailable and no verified offline copy exists." : nil,
                         refreshError: error.localizedDescription)
        }
        guard responses.0.finalURL == configuration.indexURL, responses.1.finalURL == configuration.signatureURL else {
            throw ConfigDirectoryError.identityMismatch
        }
        let index = try verify(bytes: responses.0.data, signature: responses.1.data, configuration: configuration)
        let result = ConfigDirectorySnapshot(index: index, origin: .network, bytes: responses.0.data,
                                             signature: responses.1.data, configuration: configuration)
        try withCacheLock {
            // A different loader may have published a newer verified cache
            // while this request was suspended. Check the current disk copy.
            if let latest = try readCache(configuration) {
                guard index.revision >= latest.index.revision,
                      index.revision != latest.index.revision || result.bytes == latest.bytes else { throw ConfigDirectoryError.rollback }
            }
            try writeCache(result)
        }
        return .init(snapshot: result, unavailableReason: nil, refreshError: nil)
    }

    /// A signed identity proves publication integrity, not script safety. Return
    /// an ordinary I1 bundle for the same review/apply flow as every other source.
    func profile(entryID: String, snapshot: ConfigDirectorySnapshot, dialect: ConfigDialect,
                 catalog: any ConfigImportCatalog) async throws -> ConfigImportBundle {
        try checkSnapshot(snapshot)
        guard let entry = snapshot.index.entries.first(where: { $0.id == entryID }) else { throw ConfigDirectoryError.missingEntry }
        let response = try await fetcher.fetch(.init(url: entry.configURL, maximumBytes: entry.byteCount,
                                                    root: entry.configURL.deletingLastPathComponent()))
        guard response.finalURL == entry.configURL else { throw ConfigDirectoryError.identityMismatch }
        guard response.data.count == entry.byteCount, Self.digest(response.data) == entry.sha256 else { throw ConfigDirectoryError.profileMismatch }
        // No dependency is trusted by association or fetched without its own
        // signed digest. Raw exec stays non-importable under normal I1 review.
        let loaded = try await ConfigImportLoader(catalog: catalog).load(.paste(response.data), dialect: dialect)
        return .init(source: .init(kind: .https, originalURL: entry.sourceURL, rootURL: entry.configURL.deletingLastPathComponent(), fetchedURL: entry.configURL),
                     entryPath: loaded.entryPath, files: loaded.files, unresolved: loaded.unresolved)
    }
    static func digest(_ bytes: Data) -> String { SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined() }
    static func publicURL(_ url: URL) -> Bool {
        ConfigImportLoader.permittedURL(url) && url.scheme?.lowercased() == "https" && url.query == nil && url.fragment == nil
    }
    private func verify(bytes: Data, signature: Data, configuration: ConfigDirectoryConfiguration) throws -> ConfigDirectoryIndex {
        guard bytes.count <= Self.maximumIndexBytes, signature.count == 64,
              let key = try? Curve25519.Signing.PublicKey(rawRepresentation: configuration.publicKey),
              key.isValidSignature(signature, for: bytes) else { throw ConfigDirectoryError.invalidSignature }
        guard let index = try? JSONDecoder().decode(ConfigDirectoryIndex.self, from: bytes),
              index.schemaVersion == 1, index.revision >= 1, index.entries.count <= 2048,
              Set(index.entries.map(\.id)).count == index.entries.count,
              Self.validShape(bytes), index.entries.allSatisfy(Self.validEntry) else { throw ConfigDirectoryError.invalidIndex }
        return index
    }
    private static func validEntry(_ entry: ConfigDirectoryEntry) -> Bool {
        func text(_ value: String, maximum: Int = 256) -> Bool {
            !value.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && value.utf8.count <= maximum
                && !value.unicodeScalars.contains { $0.value < 32 && $0.value != 10 || $0.value == 127 }
        }
        guard validID(entry.id),
              text(entry.player), text(entry.era), text(entry.role), text(entry.feel), text(entry.story, maximum: 8192), text(entry.credit),
              entry.team.map({ text($0) }) ?? true, entry.favoriteMap.map({ text($0) }) ?? true,
              entry.styleTags.count <= 16, entry.styleTags.allSatisfy({ text($0, maximum: 64) }),
              Set(entry.styleTags).count == entry.styleTags.count,
              publicURL(entry.sourceURL), publicURL(entry.configURL), publicURL(entry.permission.evidenceURL),
              text(entry.permission.grantedBy), entry.permission.scope == "public-redistribution",
              validDate(entry.permission.grantedAt), (1...65535).contains(entry.byteCount),
              entry.sha256.utf8.count == 64, entry.sha256.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else { return false }
        return true
    }
    private static func validID(_ value: String) -> Bool {
        let bytes = Array(value.utf8)
        let alphanumeric: (UInt8) -> Bool = { (97...122).contains($0) || (48...57).contains($0) }
        return !bytes.isEmpty && bytes.count <= 64 && alphanumeric(bytes[0]) && bytes.allSatisfy { alphanumeric($0) || $0 == 45 }
    }
    private static func validDate(_ value: String) -> Bool {
        let formatter = DateFormatter(); formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.calendar = Calendar(identifier: .gregorian); formatter.timeZone = TimeZone(secondsFromGMT: 0)
        formatter.dateFormat = "yyyy-MM-dd"; formatter.isLenient = false
        guard let date = formatter.date(from: value) else { return false }
        return formatter.string(from: date) == value
    }
    /// Match the published schema's closed objects. Unsupported metadata cannot
    /// silently masquerade as understood permission or download identity.
    private static func validShape(_ bytes: Data) -> Bool {
        guard let root = try? JSONSerialization.jsonObject(with: bytes) as? [String: Any],
              Set(root.keys) == ["schemaVersion", "revision", "entries"], let entries = root["entries"] as? [[String: Any]] else { return false }
        let required: Set<String> = ["id", "player", "era", "role", "feel", "story", "styleTags", "credit", "sourceURL", "permission", "configURL", "sha256", "byteCount", "platform"]
        for entry in entries {
            let keys = Set(entry.keys)
            guard required.isSubset(of: keys), keys.isSubset(of: required.union(["team", "favoriteMap"])),
                  let permission = entry["permission"] as? [String: Any],
                  Set(permission.keys) == ["grantedBy", "grantedAt", "evidenceURL", "scope"] else { return false }
        }
        return true
    }
    private func checkCachePath(_ url: URL? = nil) throws {
        let cacheURL = url ?? self.cacheURL
        guard cacheURL.isFileURL, cacheURL.path.hasPrefix("/"), cacheURL.path != "/" else { throw ConfigDirectoryError.unsafeCache }
        var path = URL(fileURLWithPath: "/")
        for component in cacheURL.standardizedFileURL.path.split(separator: "/") {
            path.appendPathComponent(String(component))
            var info = stat()
            if lstat(path.path, &info) == 0 {
                // The OS owns these canonical aliases; resolve them only.
                if ["/tmp", "/var"].contains(path.path), info.st_mode & S_IFMT == S_IFLNK { continue }
                guard info.st_mode & S_IFMT != S_IFLNK,
                      path != cacheURL || info.st_mode & S_IFMT == S_IFREG else { throw ConfigDirectoryError.unsafeCache }
            } else if errno != ENOENT { throw ConfigDirectoryError.unsafeCache }
        }
    }
    private func withCacheLock<T>(_ body: () throws -> T) throws -> T {
        let lockURL = cacheURL.appendingPathExtension("lock")
        try checkCachePath(); try checkCachePath(lockURL)
        try FileManager.default.createDirectory(at: cacheURL.deletingLastPathComponent(), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        try checkCachePath(lockURL)
        let fd = open(lockURL.path, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0o600)
        guard fd >= 0 else { throw ConfigDirectoryError.unsafeCache }
        defer { close(fd) }
        while flock(fd, LOCK_EX) != 0 { if errno != EINTR { throw ConfigDirectoryError.unsafeCache } }
        defer { flock(fd, LOCK_UN) }
        return try body()
    }
    private func readCache(_ configuration: ConfigDirectoryConfiguration) throws -> ConfigDirectorySnapshot? {
        try checkCachePath()
        guard FileManager.default.fileExists(atPath: cacheURL.path) else { return nil }
        let handle = try FileHandle(forReadingFrom: cacheURL); defer { try? handle.close() }
        let maximum = Self.maximumIndexBytes * 2
        guard let data = try handle.read(upToCount: maximum + 1), data.count <= maximum,
              let cache = try? JSONDecoder().decode(Cache.self, from: data), cache.schemaVersion == 1 else { throw ConfigDirectoryError.invalidCache }
        // A trust-root change starts with no applicable cache; it never grants
        // the old cache's embedded key authority over the new configuration.
        guard cache.publicKey == configuration.publicKey, cache.indexURL == configuration.indexURL,
              cache.signatureURL == configuration.signatureURL else { return nil }
        let index: ConfigDirectoryIndex
        do { index = try verify(bytes: cache.index, signature: cache.signature, configuration: configuration) }
        catch { throw ConfigDirectoryError.invalidCache }
        return .init(index: index, origin: .cache, bytes: cache.index, signature: cache.signature, configuration: configuration)
    }
    private func writeCache(_ snapshot: ConfigDirectorySnapshot) throws {
        try checkCachePath()
        try FileManager.default.createDirectory(at: cacheURL.deletingLastPathComponent(), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        try checkCachePath()
        let cache = Cache(schemaVersion: 1, indexURL: snapshot.configuration.indexURL, signatureURL: snapshot.configuration.signatureURL,
                          publicKey: snapshot.configuration.publicKey, index: snapshot.bytes, signature: snapshot.signature)
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        try encoder.encode(cache).write(to: cacheURL, options: .atomic)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: cacheURL.path)
    }
}
