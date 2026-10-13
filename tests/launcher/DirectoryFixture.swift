import Foundation
import CryptoKit

// A signed, synthetic player directory for the review harness and focused tests, never the
// app. Bracketed placeholder names only (Design Rules: placeholders, never people); the key is
// generated per run and the documentation-only `.invalid` host never resolves.

struct DirectoryFixtureFetcher: ConfigImportTextFetcher {
    let responses: [URL: ConfigImportTextResponse]
    func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse {
        guard let result = responses[request.url] else { throw URLError(.notConnectedToInternet) }
        return result
    }
}

enum DirectoryFixture {
    static let base = URL(string: "https://fixtures.invalid/cod2-configs/")!

    /// A Windows-made setup with something in every review section: plain settings, a locked
    /// brightness, risky lines, a password, a file it runs, a Windows-only choice and a no-op.
    static let firstConfig = """
    // [Player 1]'s config, sample review data
    unbindall
    bind MOUSE5 "weapnext"
    bind F "+melee"
    seta sensitivity "2.1"
    seta com_maxfps "250"
    seta r_picmip "0"
    seta cl_maxpackets "100"
    seta snaps "30"
    seta cg_chatHeight "2"
    seta r_gamma "1.5"
    seta r_vsync "1"
    seta r_mode "-1"
    seta name "[Player 1]"
    seta rconpassword "sample-secret"
    connect 203.0.113.90:28960
    exec practice.cfg

    """

    struct Player {
        let id: String, name: String, team: String?, era: String, role: String, feel: String, tags: [String], map: String
        let platform: ConfigDirectoryPlatform, config: String
    }
    static let players: [Player] = [
        .init(id: "player-1", name: "[Player 1]", team: "[Team]", era: "2005–2008", role: "Rifle", feel: "Precise and unhurried.",
              tags: ["Slow aim", "Clean HUD", "Sharp image"], map: "mp_toujane", platform: .windows, config: firstConfig),
        .init(id: "player-2", name: "[Player 2]", team: "[Team]", era: "2005–2008", role: "Sniper", feel: "Patient, then sudden.",
              tags: ["Slow aim", "Bright picture"], map: "mp_carentan", platform: .windows,
              config: "seta sensitivity \"1.8\"\nseta r_gamma \"1.6\"\nseta com_maxfps \"125\"\n"),
        .init(id: "player-3", name: "[Player 3]", team: "[Team]", era: "2005–2008", role: "SMG", feel: "Fast hands, crisp picture.",
              tags: ["Crisp textures", "333 movement"], map: "mp_leningrad", platform: .native,
              config: "seta sensitivity \"3.2\"\nseta com_maxfps \"333\"\nseta r_picmip \"0\"\n"),
        .init(id: "player-4", name: "[Player 4]", team: "[Team]", era: "2009–2012", role: "Rifle · SMG", feel: "Jumps first, shoots second.",
              tags: ["333 movement", "Clean HUD"], map: "mp_matmata", platform: .native,
              config: "seta com_maxfps \"333\"\nseta cg_drawFPS \"Simple\"\nseta cg_chatHeight \"0\"\n"),
        .init(id: "player-5", name: "[Player 5]", team: nil, era: "2009–2012", role: "Support", feel: "Calm, clean screen.",
              tags: ["333 movement", "Slow aim", "Clean HUD"], map: "mp_railyard", platform: .windows,
              config: "seta sensitivity \"2.4\"\nseta com_maxfps \"333\"\n"),
    ]

    /// Signs a fresh index with an ephemeral key and serves it, and each config, from memory.
    static func directory(cacheURL: URL, entries: [Player] = players) throws -> ConfigDirectory {
        let key = Curve25519.Signing.PrivateKey()
        var responses: [URL: ConfigImportTextResponse] = [:]
        let index = ConfigDirectoryIndex(schemaVersion: 1, revision: 1, entries: entries.map { player in
            let data = Data(player.config.utf8)
            let config = base.appendingPathComponent("entries/\(player.id)/config_mp.cfg")
            responses[config] = .init(data: data, finalURL: config, contentType: "text/plain")
            return ConfigDirectoryEntry(id: player.id, player: player.name, team: player.team, era: player.era, role: player.role,
                feel: player.feel, story: "[Two or three sentences from the player or the catalog editor about how they played, and why this setup looks the way it does.] Sample review data, not a real player.",
                styleTags: player.tags, favoriteMap: player.map, credit: player.name, sourceURL: base.appendingPathComponent("entries/\(player.id)"),
                permission: .init(grantedBy: player.name, grantedAt: "2026-10-09", evidenceURL: base.appendingPathComponent("permissions/\(player.id)"),
                                  scope: "public-redistribution"),
                configURL: config, sha256: ConfigDirectory.digest(data), byteCount: data.count, platform: player.platform)
        })
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        let bytes = try encoder.encode(index)
        let indexURL = base.appendingPathComponent("index.json"), signatureURL = base.appendingPathComponent("index.sig")
        responses[indexURL] = .init(data: bytes, finalURL: indexURL, contentType: "application/json")
        responses[signatureURL] = .init(data: try key.signature(for: bytes), finalURL: signatureURL, contentType: "application/octet-stream")
        let trust = try ConfigDirectoryConfiguration(indexURL: indexURL, signatureURL: signatureURL, publicKey: key.publicKey.rawRepresentation)
        return ConfigDirectory(configuration: trust, cacheURL: cacheURL, fetcher: DirectoryFixtureFetcher(responses: responses))
    }
}
