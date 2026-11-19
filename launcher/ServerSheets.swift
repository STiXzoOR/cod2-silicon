import SwiftUI

// Owner: ship-servers. Sheets for joining servers (AllSheets board): Connect to Address, a server's
// password, and switching the game version for one match.

/// File › Connect to Address… (⌘L) and the Servers toolbar: accepts `host:port`, a host name with or
/// without a port, or a `cod2x://`/`cod2://` link; asks that one server and shows what answered.
struct ConnectSheet: View {
    @ObservedObject var model: LauncherModel
    @State private var address: String
    @State private var password = ""
    @State private var addFavorite = false
    @State private var lookup = Lookup.idle
    @Environment(\.palette) private var palette
    enum Lookup { case idle, asking(String), answered(GameServer), silent(String) }
    init(model: LauncherModel, address: String?) {
        self.model = model
        let input = ServerAddressInput.parse(address ?? "")
        _address = State(initialValue: input?.address ?? address ?? "")
        _password = State(initialValue: input?.password ?? "")
    }
    private var parsed: (address: String, password: String?)? { ServerAddressInput.parse(address) }
    private var invalid: Bool { !address.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && parsed == nil }
    private var passwordProblem: String? { ServerAddressInput.passwordProblem(password) }
    var body: some View {
        SheetScaffold(icon: .symbol("link"), title: "Connect to an address", message: "For a server that isn’t in the list, or one a friend sent you.",
                      actions: [.cancel { model.dismissSheet() },
                                .primary("Launch", symbol: "play.fill", enabled: parsed != nil && passwordProblem == nil && model.canNavigate, launch)]) {
            VStack(alignment: .leading, spacing: 8) {
                LauncherTextField("Server address", text: $address, prompt: "203.0.113.24:28960", shape: .capsule, invalid: invalid, autofocus: true)
                    .onSubmit(launch)
                status.padding(.leading, 8)
            }
            LauncherTextField("Server password", text: $password, prompt: "Password, only if the server needs one", shape: .capsule, secure: true,
                              invalid: passwordProblem != nil)
            FormGroup {
                FormRow("Add to Favorites", isOn: $addFavorite)
            }
            if let passwordProblem {
                Label(passwordProblem, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                    .fixedSize(horizontal: false, vertical: true).padding(.leading, 8)
            }
            recent
        } note: {
            Text("Accepts an address, a name with a port, or a cod2x:// link.")
        }
        .task(id: parsed?.address) { await ask(parsed?.address) }
        .onChange(of: address) { _, _ in if let link = parsed?.password, password.isEmpty { password = link } }
    }

    @ViewBuilder private var status: some View {
        if invalid {
            Label("Use a host name or IP address, with an optional port from 1 to 65535. For example, 203.0.113.24:28960.", systemImage: "exclamationmark.triangle")
                .font(.system(size: 12)).foregroundStyle(palette.negative).fixedSize(horizontal: false, vertical: true)
        } else {
            switch lookup {
            case .idle: EmptyView()
            case .asking(let target):
                HStack(spacing: 6) { ProgressView().controlSize(.mini); Text("Asking \(target)…") }
                    .font(.system(size: 12)).foregroundStyle(palette.secondary)
                    .accessibilityElement(children: .combine)
            case .answered(let server):
                StatusLabel(.ready, "\(QuakeColors.plain(server.name)) answered · \(MapCatalog.displayName(server.map)) · \(server.playerCount) of \(server.maxPlayers) players · \(server.ping) ms\(server.password ? " · needs a password" : "")")
            case .silent(let target):
                Label("Nothing answered at \(target). Check the address and port. You can still launch; the game will try to connect.", systemImage: "exclamationmark.triangle")
                    .font(.system(size: 12)).foregroundStyle(palette.caution).fixedSize(horizontal: false, vertical: true)
                    .accessibilityElement(children: .combine)
            }
        }
    }

    @ViewBuilder private var recent: some View {
        let entries = model.library.recent.prefix(4).map { address in (address, model.knownServer((try? LaunchLink.canonicalAddress(address)) ?? address)) }
        if !entries.isEmpty {
            VStack(alignment: .leading, spacing: 4) {
                Text("Recent addresses").font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.secondary)
                    .padding(.leading, 4).padding(.bottom, 2).accessibilityAddTraits(.isHeader)
                ForEach(entries, id: \.0) { entry in
                    Button { address = entry.0 } label: {
                        HStack(spacing: 12) {
                            Text(entry.0).font(.typewriter(13)).foregroundStyle(palette.address).frame(width: 170, alignment: .leading)
                            Text(entry.1.map { QuakeColors.plain($0.name) } ?? "Not seen in the list").foregroundStyle(palette.secondary).lineLimit(1)
                                .frame(maxWidth: .infinity, alignment: .leading)
                            if let date = model.lastLaunched(entry.0) {
                                Text(date.formatted(.dateTime.day(.twoDigits).month(.abbreviated))).font(.system(size: 12)).foregroundStyle(palette.tertiary)
                            }
                        }
                        .font(.system(size: 13))
                        .padding(.horizontal, 8).frame(height: 30).contentShape(Rectangle())
                    }
                    .buttonStyle(RowButtonStyle(radius: 8))
                    .accessibilityLabel("Use \(entry.0)\(entry.1.map { ", " + QuakeColors.plain($0.name) } ?? "")")
                }
            }
        }
    }

    /// Waits for typing to pause, then asks that one server for its status (never in the review harness).
    private func ask(_ target: String?) async {
        guard let target else { lookup = .idle; return }
        if model.snapshot {
            lookup = model.knownServer(target).map(Lookup.answered) ?? .silent(target); return
        }
        lookup = .asking(target)
        try? await Task.sleep(for: .milliseconds(600))
        guard !Task.isCancelled else { return }
        if let server = await ServerQueries.server(target) {
            guard !Task.isCancelled else { return }
            lookup = .answered(server)
        } else if !Task.isCancelled {
            lookup = .silent(target)
        }
    }

    private func launch() {
        guard let parsed, passwordProblem == nil, model.canNavigate else { return }
        let target: String
        if case .answered(let server) = lookup, (try? LaunchLink.canonicalAddress(server.address)) == parsed.address {
            model.remember(server)
            if !model.library.cache.contains(where: { $0.address == server.address }) { model.library.cache.append(server) }
            target = server.address
        } else {
            target = parsed.address
        }
        if addFavorite, !model.isFavorite(target) { model.favorite(target) } else if addFavorite { model.saveLibrary() }
        model.dismissSheet()
        model.join(target, password: password.isEmpty ? nil : password)
    }
}

/// Launch on a locked server: the password (filled in when it was remembered), and whether to keep it
/// in this Mac's keychain.
struct PasswordSheet: View {
    @ObservedObject var model: LauncherModel
    var address: String
    @State private var password: String
    @State private var remember: Bool
    private let saved: Bool
    @Environment(\.palette) private var palette
    init(model: LauncherModel, address: String) {
        self.model = model; self.address = address
        let existing = model.savedPassword(for: address)
        saved = existing != nil
        _password = State(initialValue: existing ?? "")
        _remember = State(initialValue: true)
    }
    private var name: String { model.knownServer((try? LaunchLink.canonicalAddress(address)) ?? address).map { QuakeColors.plain($0.name) } ?? address }
    private var problem: String? { ServerAddressInput.passwordProblem(password) }
    var body: some View {
        SheetScaffold(icon: .symbol("lock"), title: "“\(name)” needs a password", message: "The people who run the server hand it out.", width: 460,
                      actions: [.cancel { model.dismissSheet() }, .primary("Launch", symbol: "play.fill", enabled: !password.isEmpty && problem == nil && model.canNavigate, launch)]) {
            VStack(alignment: .leading, spacing: 10) {
                LauncherTextField("Password for \(name)", text: $password, prompt: "Password", shape: .capsule, secure: true, invalid: problem != nil, autofocus: true)
                    .onSubmit(launch)
                if let problem {
                    Label(problem, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Checkbox("Remember for this server, on this Mac only", isOn: $remember).padding(.leading, 6)
                    .help(saved ? "Untick to forget the saved password when you launch" : "Keeps the password in your keychain on this Mac; it never syncs")
            }
        }
    }
    private func launch() {
        guard !password.isEmpty, problem == nil, model.canNavigate else { return }
        model.dismissSheet()
        if remember {
            if !model.rememberPassword(password, for: address) {
                model.flash("The password couldn’t be saved in your keychain, so it was used for this game only.")
            }
        } else if saved {
            model.rememberPassword(nil, for: address)
        }
        model.join(address, password: password)
    }
}

/// Launching on a server that runs the other game version: start that version for this match only.
struct SwitchVersionSheet: View {
    @ObservedObject var model: LauncherModel
    var request: EngineRequest
    @State private var dontAsk = false
    var body: some View {
        let server = (try? LaunchLink.canonicalAddress(request.link.address)).flatMap(model.knownServer)
        let target = server.map(ServerVersion.engine) ?? (model.normalEngine == .cod2x ? .original : .cod2x)
        let normal = model.normalEngine
        let name = server.map { QuakeColors.plain($0.name) } ?? request.link.address
        let loadout = model.usesLoadouts ? model.studio.activeProfile?.displayName : nil
        SheetScaffold(icon: .symbol("cpu"), title: "“\(name)” runs \(server.map(ServerVersion.label) ?? target.versionName)",
                      message: "You normally play \(normal.versionName). CoD2 Unbound will start \(target.versionName) for this match and go back to \(normal.versionName) afterwards."
                        + (loadout.map { " Your \($0) loadout works on both." } ?? ""),
                      width: 480,
                      actions: [.cancel { model.dismissSheet() },
                                .primary("Launch with \(target.versionName)", symbol: "play.fill", enabled: model.canNavigate) {
                                    model.launchWithServerVersion(request, dontAskAgain: dontAsk)
                                }]) {
            Checkbox("Don’t ask again; always use the version the server runs", isOn: $dontAsk).padding(.leading, 6)
                .help("Settings › Advanced › Reset Warnings brings this question back")
        }
    }
}
