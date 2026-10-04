import Foundation
import Network

@MainActor final class UDPRequest {
    private let connection: NWConnection
    private var continuation: CheckedContinuation<[Data], Never>?
    private var packets: [Data] = []
    private var timeout: Task<Void, Never>?
    private var cancelled = false
    private var collect = false
    init(host: String, port: UInt16) {
        connection = NWConnection(host: NWEndpoint.Host(host), port: NWEndpoint.Port(rawValue: port)!, using: .udp)
    }
    func exchange(_ command: String, seconds: Double = 1.0, collect: Bool = false) async -> [Data] {
        await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                if cancelled || Task.isCancelled { continuation.resume(returning: []); return }
                self.continuation = continuation
                self.collect = collect
                connection.stateUpdateHandler = { [weak self] state in
                    Task { @MainActor in
                        guard let self else { return }
                        switch state {
                        case .ready:
                            self.receive()
                            self.connection.send(content: WireParser.marker + Data(command.utf8), completion: .contentProcessed { [weak self] error in
                                if error != nil { Task { @MainActor in self?.finish() } }
                            })
                        case .failed, .cancelled: self.finish()
                        default: break
                        }
                    }
                }
                connection.start(queue: .global(qos: .utility))
                timeout = Task { [weak self] in
                    try? await Task.sleep(for: .seconds(seconds))
                    guard !Task.isCancelled else { return }; self?.finish()
                }
            }
        } onCancel: {
            Task { @MainActor [weak self] in self?.cancelled = true; self?.finish() }
        }
    }
    private func receive() {
        connection.receiveMessage { [weak self] data, _, _, error in
            Task { @MainActor in
                guard let self, self.continuation != nil else { return }
                if let data, data.count <= 65535, self.packets.count < 64 { self.packets.append(data) }
                if error != nil || (!self.collect && !self.packets.isEmpty) || self.packets.count >= 64 { self.finish() } else { self.receive() }
            }
        }
    }
    private func finish() {
        guard let continuation else { return }
        self.continuation = nil
        timeout?.cancel(); connection.stateUpdateHandler = nil; connection.cancel()
        continuation.resume(returning: packets)
    }
}

@MainActor enum ServerQueries {
    private static func queryMaster(_ host: String, _ protocolNumber: Int) async -> [String] {
        let packets = await UDPRequest(host: host, port: 20710).exchange("getservers \(protocolNumber) full empty", seconds: 1.5, collect: true)
        return packets.flatMap(WireParser.master)
    }
    static func masterAddresses() async -> [String] {
        await withTaskGroup(of: [String].self) { group in
            for host in ["cod2master.activision.com", "master.cod2x.me"] {
                for protocolNumber in [118, 120] {
                    group.addTask { await queryMaster(host, protocolNumber) }
                }
            }
            var addresses: Set<String> = []
            for await result in group { addresses.formUnion(result) }
            return Array(addresses.sorted().prefix(2048))
        }
    }
    static func server(_ address: String) async -> GameServer? {
        guard let link = try? LaunchLink.direct(address) else { return nil }
        let parts = link.address.split(separator: ":")
        let host = String(parts[0]), port = parts.count == 2 ? UInt16(parts[1]) ?? 28960 : 28960
        let challenge = String(UUID().uuidString.prefix(12))
        let start = ContinuousClock.now
        let packets = await UDPRequest(host: host, port: port).exchange("getstatus \(challenge)", seconds: 0.65)
        let elapsed = start.duration(to: .now)
        var status = packets.compactMap { try? WireParser.status($0, address: address, ping: Int(elapsed.components.seconds * 1000 + elapsed.components.attoseconds / 1_000_000_000_000_000)) }.first
        let infoPackets = await UDPRequest(host: host, port: port).exchange("getinfo \(challenge)", seconds: 0.35)
        if let info = infoPackets.compactMap({ try? WireParser.status($0, address: address, ping: 0) }).first {
            if status == nil { status = info }
            else { status?.fields.merge(info.fields) { _, new in new } }
        }
        return status
    }
}
