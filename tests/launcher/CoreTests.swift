import Foundation

@main struct CoreTests {
    static func main() throws {
        let fixtures = URL(fileURLWithPath: CommandLine.arguments[1])
        let master = try Data(contentsOf: fixtures.appendingPathComponent("master.cod2x.me-120.bin"))
        let servers = WireParser.master(master)
        precondition(servers.count > 50 && servers.contains("151.80.40.237:28962"))
        precondition(WireParser.master(Data(master.dropLast(4))).count <= servers.count)
        precondition(WireParser.master(Data("bad".utf8)).isEmpty)
        let status = try Data(contentsOf: fixtures.appendingPathComponent("status.bin"))
        let server = try WireParser.status(status, address: "127.0.0.1:28960", ping: 31)
        precondition(server.map == "mp_trainstation" && server.maxPlayers == 33)
        precondition(server.version == "1.3" && server.mod == "kingbot" && !server.players.isEmpty)
        precondition(server.players[0].name == "Pypson")
        precondition((try? WireParser.status(Data("bad".utf8), address: "x", ping: 0)) == nil)
        let runs = QuakeColors.runs("^1Red ^7white ^^00Sinister ^xliteral")
        precondition(runs.map(\.text).joined() == "Red white ^0Sinister ^xliteral")
        precondition(runs.first?.code == 1)
        precondition(KeyStore.valid("0000-0000-0000-0000-86D3"))
        precondition(!KeyStore.valid("000000000000000086D4"))
        precondition(!KeyStore.valid(""))
        let scratch = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: scratch) }
        try FileManager.default.createDirectory(at: scratch, withIntermediateDirectories: true)
        let preferences = scratch.appendingPathComponent("preferences")
        try "name=Player\ncodkey=invalid\n".write(to: preferences, atomically: true, encoding: .utf8)
        try KeyStore.save("000000000000000086D3", at: preferences)
        precondition(KeyStore.hasValidKey(at: preferences))
        let saved = try String(contentsOf: preferences, encoding: .utf8)
        precondition(saved.contains("name=Player"))
        let attrs = try FileManager.default.attributesOfItem(atPath: preferences.path)
        precondition((attrs[.posixPermissions] as? NSNumber)?.intValue == 0o600)
        var settings = GameSettings()
        settings.advanced = "cg_fov=90\nname=Player One"
        let config = try settings.config()
        precondition(config.contains("seta com_maxfps \"333\""))
        precondition(config.contains("seta name \"Player One\""))
        settings.advanced = "name=bad;quit"
        precondition((try? settings.config()) == nil)
        settings.advanced = "fs_basepath=/tmp"
        precondition((try? settings.config()) == nil)
        precondition((try? LaunchLink("cod2x://connect/127.0.0.1:28960"))?.address == "127.0.0.1:28960")
        precondition((try? LaunchLink("cod2x://connect/localhost%3Bquit")) == nil)
        print("PASS: real master/status fixtures, malformed packets, colours, native key CRC/private storage, config and URL injection")
    }
}
