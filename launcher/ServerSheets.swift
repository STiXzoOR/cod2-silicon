import SwiftUI

// Owner: ship-servers. Sheets for joining servers: Connect to Address, a server's password, and the
// game-version switch a CoD2x server needs. Minimal bodies on the shared SheetScaffold; ship-servers
// finishes them against the AllSheets board.

struct ConnectSheet: View {
    @ObservedObject var model: LauncherModel
    @State private var address: String
    @State private var password = ""
    @Environment(\.palette) private var palette
    init(model: LauncherModel, address: String?) {
        self.model = model
        _address = State(initialValue: address ?? model.directAddress)
    }
    private var trimmed: String { address.trimmingCharacters(in: .whitespacesAndNewlines) }
    private var error: String? { trimmed.isEmpty ? nil : (model.directConnectError ?? model.directAddressError) }
    var body: some View {
        SheetScaffold(icon: .symbol("mappin.and.ellipse"), title: "Connect to a server",
                      message: "Enter the server’s address. Add its password if it has one.",
                      actions: [.cancel { model.dismissSheet() }, .primary("Connect", enabled: !trimmed.isEmpty, connect)]) {
            VStack(alignment: .leading, spacing: 10) {
                LauncherTextField("Server address", text: $address, prompt: "hostname or IP:port", shape: .capsule, monospaced: true, invalid: error != nil)
                    .onChange(of: address) { _, value in model.directAddress = value }
                LauncherTextField("Server password, optional", text: $password, prompt: "Password (optional)", shape: .capsule, secure: true)
                if let error {
                    Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
        .onAppear { model.directAddress = address }
    }
    private func connect() {
        model.directAddress = address
        model.directPassword = password
        model.connect()
        if model.directConnectError == nil && model.directAddressError == nil { model.dismissSheet() }
    }
}

struct PasswordSheet: View {
    @ObservedObject var model: LauncherModel
    var address: String
    @State private var password = ""
    var body: some View {
        let server = model.servers.first { $0.address == address }
        SheetScaffold(icon: .symbol("lock"), title: "Join “\(server.map { QuakeColors.plain($0.name) } ?? address)”",
                      message: "This server asks for a password.",
                      actions: [.cancel { model.dismissSheet() }, .primary("Join", enabled: !password.isEmpty) { join(server) }]) {
            LauncherTextField("Server password", text: $password, shape: .capsule, secure: true)
        }
    }
    private func join(_ server: GameServer?) {
        guard let server else { model.dismissSheet(); return }
        model.selectedServer = server.address
        model.serverPassword = password
        model.dismissSheet()
        model.connect(server)
    }
}

/// Replaces the alert the main window used to show; the words are unchanged.
struct SwitchVersionSheet: View {
    @ObservedObject var model: LauncherModel
    var request: EngineRequest
    var body: some View {
        SheetScaffold(icon: .symbol("arrow.triangle.2.circlepath"), title: "Turn on CoD2x for this server?", message: request.message,
                      actions: [.cancel { model.dismissSheet() }, .primary("Turn On CoD2x and Join") { model.turnOnCoD2x(andJoin: request) }]) {
            EmptyView()
        }
    }
}
