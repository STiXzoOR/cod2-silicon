import Foundation
import CryptoKit

private struct FixtureFetcher: ConfigImportTextFetcher {
    let responses: [URL: ConfigImportTextResponse]
    func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse {
        guard let result = responses[request.url] else { throw URLError(.notConnectedToInternet) }
        return result
    }
}
private actor IndexGate: ConfigImportTextFetcher {
    let index: ConfigImportTextResponse
    let signature: ConfigImportTextResponse
    private var pending: CheckedContinuation<Void, Never>?
    private var started: CheckedContinuation<Void, Never>?
    init(index: ConfigImportTextResponse, signature: ConfigImportTextResponse) { self.index = index; self.signature = signature }
    func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse {
        if request.url == index.finalURL {
            await withCheckedContinuation { continuation in
                pending = continuation; started?.resume(); started = nil
            }
            return index
        }
        return signature
    }
    func waitStarted() async {
        if pending != nil { return }
        await withCheckedContinuation { started = $0 }
    }
    func release() { pending?.resume(); pending = nil }
}
@main struct ConfigDirectoryTests {
    static func require(_ value: Bool, _ message: String) { if !value { fatalError("FAIL: " + message) } }
    @MainActor static func rejects(_ error: ConfigDirectoryError, _ body: () async throws -> Void) async {
        do { try await body(); fatalError("FAIL: expected \(error)") }
        catch let actual as ConfigDirectoryError { require(actual == error, "unexpected error \(actual)") }
        catch { fatalError("FAIL: unexpected \(error)") }
    }
    static func main() async throws {
        let scratch = URL(fileURLWithPath: ProcessInfo.processInfo.environment["COD2_DIRECTORY_TEST_SCRATCH"]!)
        let indexURL = URL(string: "https://fixtures.invalid/index.json")!
        let signatureURL = URL(string: "https://fixtures.invalid/index.sig")!
        let profileURL = URL(string: "https://fixtures.invalid/entries/synthetic/config.cfg")!
        let profile = Data("seta sensitivity 4\nseta password synthetic-secret\nexec untrusted.cfg\n".utf8)
        let key = Curve25519.Signing.PrivateKey() // Ephemeral fixture only; never a production key.
        let entry = ConfigDirectoryEntry(id: "synthetic", player: "Authored test fixture", team: nil, era: "test", role: "test",
            feel: "Synthetic test bytes", story: "Not a real player or permission grant.", styleTags: ["fixture"], favoriteMap: nil,
            credit: "Test author", sourceURL: profileURL, permission: .init(grantedBy: "Test author", grantedAt: "2026-10-09",
            evidenceURL: URL(string: "https://fixtures.invalid/permission")!, scope: "public-redistribution"),
            configURL: profileURL, sha256: ConfigDirectory.digest(profile), byteCount: profile.count, platform: .windows)
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        let bytes = try encoder.encode(ConfigDirectoryIndex(schemaVersion: 1, revision: 1, entries: [entry]))
        let signature = try key.signature(for: bytes)
        let trust = try ConfigDirectoryConfiguration(indexURL: indexURL, signatureURL: signatureURL, publicKey: key.publicKey.rawRepresentation)
        let cache = scratch.appendingPathComponent("cache/index-cache.json")
        let fetcher = FixtureFetcher(responses: [
            indexURL: .init(data: bytes, finalURL: indexURL, contentType: "application/json"),
            signatureURL: .init(data: signature, finalURL: signatureURL, contentType: "application/octet-stream"),
            profileURL: .init(data: profile, finalURL: profileURL, contentType: "text/plain")
        ])
        let directory = ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: fetcher)
        let unavailable = try await ConfigDirectory(cacheURL: cache).load()
        require(unavailable.snapshot == nil && unavailable.unavailableReason != nil, "production default honestly unavailable")
        let online = try await directory.load()
        require(online.snapshot?.index.entries.count == 1 && online.snapshot?.origin == .network, "verified network index")
        try directory.setSaved(entryID: entry.id, saved: true, snapshot: online.snapshot!)
        require(try directory.savedEntries(snapshot: online.snapshot!).first?.entry == entry, "Saved exposes verified metadata")
        let offline = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: FixtureFetcher(responses: [:])).load()
        require(offline.snapshot?.origin == .cache && offline.refreshError != nil, "reverified offline cache")
        require(try directory.savedEntries(snapshot: offline.snapshot!).count == 1, "Saved survives offline load")
        let bundle = try await directory.profile(entryID: entry.id, snapshot: online.snapshot!, dialect: .cod2x,
            catalog: ConfigImportCatalogSnapshot(settings: ["sensitivity": .init(archived: true)], commands: [:]))
        let review = ConfigImportSafety(catalog: ConfigImportCatalogSnapshot(settings: ["sensitivity": .init(archived: true)], commands: [:])).review(bundle)
        require(review.candidates.contains { $0.reasons.contains(.credential) && !$0.importable }, "signed profile still safety reviewed")
        require(review.candidates.contains { $0.command.kind == .exec && !$0.importable }, "signed raw exec remains forbidden")
        var tampered = bytes; tampered.append(32)
        let tamperFetcher = FixtureFetcher(responses: [indexURL: .init(data: tampered, finalURL: indexURL, contentType: nil),
            signatureURL: .init(data: signature, finalURL: signatureURL, contentType: nil)])
        await rejects(.invalidSignature) { _ = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: tamperFetcher).load() }
        let wrongKey = Curve25519.Signing.PrivateKey()
        let wrongTrust = try ConfigDirectoryConfiguration(indexURL: indexURL, signatureURL: signatureURL, publicKey: wrongKey.publicKey.rawRepresentation)
        await rejects(.invalidSignature) { _ = try await ConfigDirectory(configuration: wrongTrust, cacheURL: cache, fetcher: fetcher).load() }
        let wrongCache = try await ConfigDirectory(configuration: wrongTrust, cacheURL: cache, fetcher: FixtureFetcher(responses: [:])).load()
        require(wrongCache.snapshot == nil, "cache embedded key cannot bootstrap trust")
        let alteredProfile = FixtureFetcher(responses: [profileURL: .init(data: Data("seta sensitivity 9\n".utf8), finalURL: profileURL, contentType: nil)])
        await rejects(.profileMismatch) { _ = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: alteredProfile)
            .profile(entryID: entry.id, snapshot: online.snapshot!, dialect: .cod2x, catalog: ConfigImportCatalogSnapshot(settings: [:], commands: [:])) }
        let swapped = FixtureFetcher(responses: [profileURL: .init(data: profile, finalURL: URL(string: "https://fixtures.invalid/entries/other/config.cfg")!, contentType: nil)])
        await rejects(.identityMismatch) { _ = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: swapped)
            .profile(entryID: entry.id, snapshot: online.snapshot!, dialect: .cod2x, catalog: ConfigImportCatalogSnapshot(settings: [:], commands: [:])) }
        let malformed = try encoder.encode(ConfigDirectoryIndex(schemaVersion: 2, revision: 1, entries: []))
        let malformedFetcher = FixtureFetcher(responses: [indexURL: .init(data: malformed, finalURL: indexURL, contentType: nil),
            signatureURL: .init(data: try key.signature(for: malformed), finalURL: signatureURL, contentType: nil)])
        await rejects(.invalidIndex) { _ = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: malformedFetcher).load() }
        let emptyBytes = try encoder.encode(ConfigDirectoryIndex(schemaVersion: 1, revision: 2, entries: []))
        let emptyFetcher = FixtureFetcher(responses: [indexURL: .init(data: emptyBytes, finalURL: indexURL, contentType: nil),
            signatureURL: .init(data: try key.signature(for: emptyBytes), finalURL: signatureURL, contentType: nil)])
        let empty = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: emptyFetcher).load()
        require(empty.snapshot?.index.entries.isEmpty == true, "honest signed empty fixture index")
        require(try directory.savedEntries(snapshot: empty.snapshot!).first?.entry == nil, "removed Saved entry stays explicitly unavailable")
        try directory.setSaved(entryID: entry.id, saved: false, snapshot: empty.snapshot!)
        require(try directory.savedEntries(snapshot: empty.snapshot!).isEmpty, "removed entry can be unsaved")
        let raceCache = scratch.appendingPathComponent("race/index-cache.json")
        let gate = IndexGate(index: .init(data: bytes, finalURL: indexURL, contentType: nil),
                             signature: .init(data: signature, finalURL: signatureURL, contentType: nil))
        let oldTask = Task { try await ConfigDirectory(configuration: trust, cacheURL: raceCache, fetcher: gate).load() }
        await gate.waitStarted()
        _ = try await ConfigDirectory(configuration: trust, cacheURL: raceCache, fetcher: emptyFetcher).load()
        await gate.release()
        await rejects(.rollback) { _ = try await oldTask.value }
        await rejects(.rollback) { _ = try await directory.load() }
        try Data("tampered cache".utf8).write(to: cache)
        await rejects(.invalidCache) { _ = try await ConfigDirectory(configuration: trust, cacheURL: cache, fetcher: FixtureFetcher(responses: [:])).load() }
        print("PASS: Ed25519 exact-byte trust, tamper/wrong-key rejection, offline cache, identity/digest, safety and unavailable default")
    }
}
