import Foundation

@main struct NetworkTests {
    @MainActor static func main() async throws {
        if CommandLine.arguments.contains("--live") {
            let addresses = await ServerQueries.masterAddresses()
            precondition(!addresses.isEmpty)
            var answered = 0
            for address in addresses.prefix(12) {
                if let server = await ServerQueries.server(address) {
                    precondition(server.ping < 650 && server.maxPlayers > 0)
                    answered += 1
                    if answered == 3 { break }
                }
            }
            precondition(answered > 0)
            print("PASS: Network.framework live discovery \(addresses.count) endpoints, \(answered) status/info responses, measured first-reply RTT")
            return
        }
        let port = UInt16(CommandLine.arguments[1])!
        guard let server = await ServerQueries.server("127.0.0.1:\(port)") else { fatalError("Local UDP fixture did not answer") }
        precondition(server.map == "mp_trainstation" && server.players.count > 0 && server.ping < 500)
        let task = Task { await UDPRequest(host: "127.0.0.1", port: port + 1).exchange("getstatus", seconds: 5) }
        try await Task.sleep(for: .milliseconds(20)); task.cancel()
        let start = ContinuousClock.now
        let packets = await task.value
        precondition(packets.isEmpty && start.duration(to: .now) < .seconds(1))
        print("PASS: real Network.framework UDP exchange, parser integration, RTT and cancellation")
    }
}
