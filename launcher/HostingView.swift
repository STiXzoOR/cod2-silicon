import SwiftUI
import AppKit

// Servers › Hosting (Host board): the servers this Mac runs, the form that sets one up, the page of
// a running one, and the export for another machine.

struct HostingView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var hosting: HostingModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero

    var body: some View {
        // The reader gives the page the window's size and no minimum of its own, so a wide control
        // can never push the split view around.
        GeometryReader { proxy in
            let leading = max(22, 264 - origin.x)
            Group {
                if hosting.servers.isEmpty && hosting.draft == nil { none } else { some(width: proxy.size.width - leading - 22) }
            }
            .padding(.leading, leading).padding(.trailing, 22).padding(.top, 84).padding(.bottom, 22)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        }
        .background { ContourBackdrop().ignoresSafeArea() }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .modifier(HostingToolbar(model: model, hosting: hosting))
        .sheet(item: $hosting.exporting) { server in HostingExportSheet(hosting: hosting, server: server).sheetShapeAudit() }
        .sheet(item: $hosting.showingPlayers) { server in HostingPlayersSheet(hosting: hosting, server: server).sheetShapeAudit() }
    }

    private var none: some View {
        VStack(spacing: 14) {
            Image(systemName: "server.rack").font(.system(size: 34, weight: .light)).foregroundStyle(palette.accent).accessibilityHidden(true)
            Text("HOST YOUR OWN SERVER").font(.stencil(34)).tracking(1).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            Text("Run a server on this Mac for friends on your network or over the internet, with the original 1.3 or CoD2x, public rules or a competitive zPAM match. Or write the files for a server that runs somewhere else.")
                .font(.system(size: 14)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).frame(maxWidth: 560).fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 10) {
                Button { hosting.newServer(engine: model.chosenEngine) } label: { Label("New Server", systemImage: "plus") }.prominentAction()
                Button("Write a server.cfg…") { hosting.newServer(engine: model.chosenEngine); hosting.exporting = hosting.draft }
            }
            .controlSize(.large).padding(.top, 4)
            Text("The server uses your own game files. Nothing is contacted until you start it.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    /// The board's layout at 1440 wide: a 280-point list and two columns beside it. A narrower window
    /// (down to the 1100 minimum) keeps everything inside it: the list narrows, and the form's two
    /// columns become one.
    private func some(width: CGFloat) -> some View {
        let listWidth: CGFloat = width < 900 ? 200 : 280
        let columns = width - listWidth - 18 >= 700 ? 2 : 1
        return HStack(alignment: .top, spacing: 18) {
            list.frame(width: listWidth)
            Group {
                if hosting.showingDraft, let draft = hosting.draft {
                    HostingForm(model: model, hosting: hosting, initial: draft, isNew: true, columns: columns)
                } else if let server = hosting.selected {
                    if hosting.isRunning(server.id) && hosting.editing != server.id { HostingRunning(model: model, hosting: hosting, server: server) }
                    else { HostingForm(model: model, hosting: hosting, initial: server, isNew: false, columns: columns) }
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        }
    }

    private var list: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Your servers").font(StudioTypography.groupTitle).foregroundStyle(palette.heading).padding(.leading, 6).padding(.bottom, 4).accessibilityAddTraits(.isHeader)
            if hosting.draft != nil {
                row("New server", "Not saved yet", symbol: "plus", tint: palette.accent, selected: hosting.showingDraft) { hosting.selection = nil }
            }
            ForEach(hosting.servers) { server in
                let running = hosting.isRunning(server.id), failed = meta(server) == "Couldn’t start"
                row(server.name, meta(server), symbol: running ? "dot.radiowaves.left.and.right" : failed ? "exclamationmark.triangle" : "server.rack",
                    tint: running ? palette.positive : failed ? palette.negative : palette.secondary, selected: hosting.selection == server.id) {
                    hosting.selection = server.id
                }
                .contextMenu {
                    Button("Export for Another Machine…") { hosting.exporting = server }
                    Button("Show Its Folder in Finder") { NSWorkspace.shared.activateFileViewerSelecting([hosting.home(server.id)]) }
                        .disabled(!FileManager.default.fileExists(atPath: hosting.home(server.id).path))
                    Divider()
                    Button("Delete Server…") { model.requestDeleteHostedServer(server) }.disabled(running)
                }
            }
            Spacer(minLength: 8)
            Text("A running server keeps going when you close this window. Quitting CoD2 Silicon asks before stopping it.")
                .font(.system(size: 11)).foregroundStyle(palette.tertiary).lineSpacing(2).fixedSize(horizontal: false, vertical: true).padding(.horizontal, 6)
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }
    private func row(_ title: String, _ detail: String, symbol: String, tint: Color, selected: Bool, action: @escaping () -> Void) -> some View {
        let shape = RoundedRectangle(cornerRadius: 12, style: .continuous)
        return Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: symbol).font(.system(size: 13, weight: .semibold)).foregroundStyle(tint).frame(width: 30, height: 30)
                    .background(palette.dark ? Palette.hex(0x1b1f17) : palette.crate, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: shape)
            .overlay(shape.strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(RowButtonStyle(radius: 12))
        .accessibilityLabel("\(title), \(detail)").accessibilityAddTraits(selected ? .isSelected : [])
    }
    private func meta(_ server: HostedServer) -> String {
        switch hosting.phase(server.id) {
        case .running:
            if let status = hosting.statuses[server.id] { return "Running · \(status.players.count) of \(status.maxPlayers) · \(MapCatalog.displayName(HostingCatalog.stockMap(status.map)))" }
            return "Running"
        case .starting: return "Starting…"
        case .failed, .portBusy: return "Couldn’t start"
        case .stopped: return "Stopped · \(server.engine == .cod2x ? "CoD2x" : "Original 1.3")"
        }
    }
}

/// Hosting's toolbar: the same scope switch as the rest of Servers, then Export and New Server.
private struct HostingToolbar: ViewModifier {
    @ObservedObject var model: LauncherModel
    @ObservedObject var hosting: HostingModel
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar { leading; ToolbarSpacer(.flexible); ToolbarItem { export }; ToolbarSpacer(.fixed); ToolbarItem { new } }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar { leading; ToolbarItem { Spacer() }; ToolbarItem { export }; ToolbarItem { new } }
    }
    @ToolbarContentBuilder private var leading: some ToolbarContent {
        ToolbarItem(placement: .navigation) {
            ScopeSwitch("Which servers", selection: $model.serverScope, options: ServerScope.allCases.map { ($0, $0.title) })
        }
    }
    private var export: some View {
        Button { hosting.exporting = hosting.showingDraft ? hosting.draft : hosting.selected } label: {
            Label("Export for Another Machine…", systemImage: "doc.text").labelStyle(.titleAndIcon).font(.system(size: 13, weight: .semibold))
        }
        .disabled(hosting.draft == nil && hosting.selected == nil)
        .help("Write server.cfg and start commands for a Linux or Windows server. Nothing is uploaded.")
    }
    private var new: some View {
        Button { hosting.newServer(engine: model.chosenEngine) } label: {
            Label("New Server", systemImage: "plus").labelStyle(.titleAndIcon).font(.system(size: 13, weight: .semibold))
        }
        .help("Set up another server on this Mac")
    }
}

// MARK: - Form

private struct HostingForm: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var hosting: HostingModel
    /// The server as it was when the form appeared. A new one lives in `hosting.draft` until it is
    /// started or saved; a saved one saves with every edit.
    var initial: HostedServer
    var isNew: Bool
    /// Two columns as on the board, or one when the window is too narrow for two.
    var columns = 2
    @Environment(\.palette) private var palette
    @State private var showAdmin = false
    @State private var mapMenu = MenuAnchorBox()

    private var server: Binding<HostedServer> {
        let initial = initial, isNew = isNew, hosting = hosting
        return Binding(get: { (isNew ? hosting.draft : hosting.servers.first { $0.id == initial.id }) ?? initial },
                       set: { if isNew { hosting.draft = $0 } else { hosting.update($0) } })
    }
    private func hint(_ current: HostedServer) -> String {
        if hosting.isRunning(current.id) { return "Changes apply the next time it starts." }
        guard current.rules == .zpam else { return "Port \(String(current.port))" }
        switch hosting.zpam[current.zpamLine] ?? .unknown {
        case .ready: return "Port \(String(current.port))"
        case .getting: return "Start is ready when zPAM is."
        default: return "Competitive rules need zPAM first."
        }
    }

    var body: some View {
        let current = server.wrappedValue
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .center, spacing: 12) {
                VStack(alignment: .leading, spacing: 3) {
                    Text(isNew ? "New server" : current.name).font(.system(size: 26, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1)
                        .accessibilityAddTraits(.isHeader)
                    // One column has no room for the hint beside the buttons: it goes under the name.
                    if columns == 1 { Text(hint(current)).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1) }
                }
                Spacer(minLength: 12)
                if columns == 2 { Text(hint(current)).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1) }
                if hosting.isRunning(current.id) {
                    Button("Done") { hosting.editing = nil }.prominentAction()
                } else {
                    if isNew { Button("Cancel") { hosting.cancelDraft() } } else { Button("Delete Server…") { model.requestDeleteHostedServer(current) } }
                    Button("Start Server") { if let saved = isNew ? hosting.saveDraft() : current { hosting.start(saved) } }
                        .prominentAction().disabled(current.rules == .zpam && !zpamReady(current))
                        .help(current.rules == .zpam && !zpamReady(current) ? "Get zPAM first" : "Starts the server on this Mac")
                }
            }
            problem(current)
            ScrollView {
                let layout = columns == 2 ? AnyLayout(HStackLayout(alignment: .top, spacing: 14)) : AnyLayout(VStackLayout(alignment: .leading, spacing: 18))
                layout {
                    VStack(alignment: .leading, spacing: 8) {
                        label("Server")
                        FormGroup {
                            FormRow("Name", detail: "What players see in the server list.") {
                                TextField("Server name", text: server.name).textFieldStyle(.roundedBorder).frame(width: 200).accessibilityLabel("Server name")
                            }
                            // Who can join depends on the version, and that takes a sentence: it goes under
                            // the switch, across the row, as Rules' explanation does.
                            VStack(alignment: .leading, spacing: 6) {
                                HStack(spacing: 14) {
                                    Text("Game version").font(.system(size: 14)).foregroundStyle(palette.text)
                                    Spacer(minLength: 8)
                                    Picker("Game version", selection: server.engine) {
                                        Text("CoD2x").tag(GameEngine.cod2x); Text("Original 1.3").tag(GameEngine.original)
                                    }
                                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                                }
                                Text(current.engine == .cod2x
                                     ? (current.releasedCoD2x
                                        ? "CoD2 Silicon and CoD2x \(CoD2xFiles.version) players can join. A running CoD2x server asks CoD2x’s update service about new versions."
                                        : "CoD2 Silicon players only: this server follows the next CoD2x, which the released \(CoD2xFiles.version) can’t join. A running CoD2x server asks CoD2x’s update service about new versions.")
                                     : "Anyone with the original 1.3 can join, on Mac or Windows.")
                                    .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                            }
                            .padding(.horizontal, FormMetrics.rowInset(.panel)).padding(.vertical, 10)
                            .accessibilityElement(children: .contain)
                            if current.engine == .cod2x {
                                FormRow("Let CoD2x \(CoD2xFiles.version) players join",
                                        detail: "For friends on Windows with the released CoD2x. Coloured text codes, spectator x-ray and killfeed icons stay off on this server.") {
                                    SwitchToggle("Let CoD2x \(CoD2xFiles.version) players join", isOn: server.releasedCoD2x)
                                }
                            }
                            FormRow("Who can join", detail: current.reach.detail) {
                                Picker("Who can join", selection: server.reach) { ForEach(HostedServer.Reach.allCases, id: \.self) { Text($0.title).tag($0) } }
                                    .labelsHidden().fixedSize()
                            }
                            FormRow("Players") {
                                Picker("Players", selection: server.players) { ForEach([2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 32], id: \.self) { Text("\($0)").tag($0) } }
                                    .labelsHidden().fixedSize()
                            }
                            FormRow("Password to join", detail: "Leave empty for an open server.") {
                                SecureField("None", text: server.joinPassword).textFieldStyle(.roundedBorder).frame(width: 200).accessibilityLabel("Password to join")
                            }
                            FormRow("Admin password", detail: "For controlling the server from inside the game. Kept on this Mac.") {
                                HStack(spacing: 6) {
                                    Group {
                                        if showAdmin { TextField("Admin password", text: server.adminPassword) } else { SecureField("Admin password", text: server.adminPassword) }
                                    }
                                    .textFieldStyle(.roundedBorder).frame(width: 130).accessibilityLabel("Admin password")
                                    Button(showAdmin ? "Hide" : "Show") { showAdmin.toggle() }.controlSize(.small).fixedSize()
                                }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    VStack(alignment: .leading, spacing: 8) {
                        label("Match")
                        FormGroup {
                            // The switch is wide: its explanation goes under it, across the row.
                            VStack(alignment: .leading, spacing: 6) {
                                HStack(spacing: 14) {
                                    Text("Rules").font(.system(size: 14)).foregroundStyle(palette.text)
                                    Spacer(minLength: 8)
                                    Picker("Rules", selection: server.rules) { ForEach(HostedServer.Rules.allCases, id: \.self) { Text($0.title).tag($0) } }
                                        .pickerStyle(.segmented).labelsHidden().fixedSize()
                                }
                                Text(current.rules.detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                            }
                            .padding(.horizontal, FormMetrics.rowInset(.panel)).padding(.vertical, 10)
                            .accessibilityElement(children: .contain)
                            if current.rules == .zpam { zpamRows(current) } else {
                                FormRow("Game type") {
                                    Picker("Game type", selection: server.gameType) { ForEach(HostingCatalog.gameTypes, id: \.id) { Text($0.title).tag($0.id) } }
                                        .labelsHidden().fixedSize()
                                }
                                FormRow("Friendly fire") { Toggle("Friendly fire", isOn: server.friendlyFire).labelsHidden().toggleStyle(.switch) }
                            }
                        }
                        label("Maps, in order").padding(.top, 8)
                        maps(current)
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                }
            }
        }
        .task(id: "\(current.rules.rawValue)-\(current.engine.rawValue)") {
            // The zPAM project's releases are asked only once the form shows Competitive · zPAM.
            if current.rules == .zpam, !model.snapshot { await hosting.lookUpZPAM(current.zpamLine) }
        }
    }
    private func label(_ text: String) -> some View {
        Text(text.uppercased()).font(.system(size: 11, weight: .semibold)).tracking(0.88).foregroundStyle(palette.tertiary).padding(.leading, 6).accessibilityAddTraits(.isHeader)
    }
    private func zpamReady(_ server: HostedServer) -> Bool { if case .ready = hosting.zpam[server.zpamLine] { return true }; return false }

    @ViewBuilder private func problem(_ current: HostedServer) -> some View {
        switch hosting.phase(current.id) {
        case .failed(let message): Banner(.error, title: "The server couldn’t start", message: message)
        case .portBusy(let next):
            Banner(.error, title: "The server couldn’t start: port \(String(current.port)) is already in use.",
                   message: "Another server or a running game is using it. The next free port is \(String(next)).") {
                Button("Use \(String(next)) and Start") { if let saved = hosting.saveDraft() ?? hosting.selected { hosting.start(saved, port: next) } }
            }
        default: EmptyView()
        }
    }

    @ViewBuilder private func zpamRows(_ current: HostedServer) -> some View {
        let megabytes: (Int) -> String = { "\(($0 + 524_288) / 1_048_576) MB" }
        switch hosting.zpam[current.zpamLine] ?? .unknown {
        case .unknown:
            FormRow("Looking up zPAM…", detail: "Asking the zPAM project’s releases on GitHub which version is current.") { ProgressView().controlSize(.small) }
        case .failed(let message):
            FormRow("zPAM couldn’t be looked up", detail: message) { Button("Try Again") { Task { await hosting.lookUpZPAM(current.zpamLine) } } }
        case .missing(let release):
            FormRow("zPAM isn’t on this Mac yet",
                    detail: "The match mod competitive servers run, made by the author of CoD2x. \(megabytes(release.bytes)) with its map pack, from the zPAM project’s releases on GitHub. Checked after download and kept beside your servers; your game folder is not touched.") {
                Button("Get zPAM \(release.version)") { hosting.getZPAM(current.zpamLine) }
            }
        case .getting(let release, let received):
            FormRow("Getting zPAM \(release.version)…", detail: "\(megabytes(received).dropLast(3)) of \(megabytes(release.bytes))") {
                HStack(spacing: 10) {
                    ProgressView(value: Double(received), total: Double(max(1, release.bytes))).frame(width: 90).accessibilityLabel("Downloading zPAM")
                    Button("Cancel") { hosting.cancelZPAM() }.fixedSize()
                }
            }
        case .ready(let version):
            FormRow("zPAM \(version)", detail: current.engine == .cod2x ? "Installed. A running match server checks CoD2x’s update service and keeps zPAM current by itself."
                                                                        : "Installed, for the original 1.3.") {
                Label("Ready", systemImage: "checkmark").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.positive)
            }
            FormRow("Match mode", detail: "zPAM’s own name for the format.") {
                Picker("Match mode", selection: server.matchMode) { ForEach(HostingCatalog.matchModes, id: \.id) { Text($0.title).tag($0.id) } }
                    .labelsHidden().frame(width: 230)
            }
            FormRow("Players get maps from", detail: "zPAM’s map pack is fetched by each player’s game from this address.") {
                Text("cod2x.me · zPAM’s default").font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize()
            }
        }
    }

    private func maps(_ current: HostedServer) -> some View {
        let box = RoundedRectangle(cornerRadius: FormMetrics.radius(.panel), style: .continuous)
        return VStack(alignment: .leading, spacing: 8) {
            FlowChipsLayout(spacing: 8) {
                ForEach(Array(current.maps.enumerated()), id: \.offset) { index, map in
                    HStack(spacing: 6) {
                        Text(MapCatalog.displayName(map)).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                        Button { server.wrappedValue.maps.remove(at: index) } label: { Image(systemName: "xmark").font(.system(size: 9, weight: .bold)) }
                            .buttonStyle(.plain).foregroundStyle(palette.secondary).disabled(current.maps.count <= 1)
                            .accessibilityLabel("Remove \(MapCatalog.displayName(map))")
                    }
                    .padding(.horizontal, 12).frame(minHeight: StudioMetrics.minimumHitTarget)
                    .background(palette.well, in: Capsule()).overlay(Capsule().strokeBorder(palette.wellStroke, lineWidth: 1))
                    .draggable(map)
                    .dropDestination(for: String.self) { items, _ in
                        guard let moved = items.first, let from = current.maps.firstIndex(of: moved), from != index else { return false }
                        var maps = current.maps
                        maps.remove(at: from); maps.insert(moved, at: min(index, maps.count))
                        server.wrappedValue.maps = maps
                        return true
                    }
                }
                // A real AppKit menu under the button, as the launcher's other Add menus are.
                Button {
                    guard let view = mapMenu.view else { return }
                    let binding = server
                    NativeMenu.popUp(HostingCatalog.maps.filter { !current.maps.contains($0) }.map { map in
                        .init(title: MapCatalog.displayName(map), symbol: "map") { binding.wrappedValue.maps.append(map) }
                    }, below: view)
                } label: { Label("Add Map", systemImage: "plus") }
                .background(MenuAnchor(box: mapMenu))
                .fixedSize().disabled(HostingCatalog.maps.allSatisfy(current.maps.contains))
                .accessibilityLabel("Add Map").accessibilityHint("Shows the maps that aren’t in the rotation yet")
            }
            .padding(.vertical, 12).padding(.horizontal, 14).frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.panel, in: box).overlay(box.strokeBorder(palette.panelStroke, lineWidth: 1))
            Text("Drag to reorder. The server starts on the first map.").font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.leading, 6)
        }
    }
}

// MARK: - Running

private struct HostingRunning: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var hosting: HostingModel
    var server: HostedServer
    @Environment(\.palette) private var palette

    var body: some View {
        let status = hosting.statuses[server.id]
        VStack(alignment: .leading, spacing: 14) {
            // One row as on the board; a window too narrow for the name beside the buttons puts them under it.
            ViewThatFits(in: .horizontal) {
                HStack(alignment: .center, spacing: 12) { title; Spacer(minLength: 12); actions(status) }
                VStack(alignment: .leading, spacing: 10) { title; HStack(spacing: 8) { actions(status) } }.frame(maxWidth: .infinity, alignment: .leading)
            }
            HStack(spacing: 12) {
                tile(status.map { "\($0.players.count)" } ?? "—", suffix: status.map { " / \($0.maxPlayers)" } ?? "", "players")
                tile(status.map { MapCatalog.displayName(HostingCatalog.stockMap($0.map)) } ?? "Starting…", suffix: "", status.map { [HostingCatalog.gameTypeTitle($0.gameType), $0.matchInfo].compactMap { $0 }.joined(separator: " · ") } ?? "The first map is loading")
                tile(server.engine == .cod2x ? "CoD2x" : "Original 1.3", suffix: "", server.rules.title)
                tile(String(server.port), suffix: "", "UDP port")
            }
            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 8) {
                    label("How to join")
                    FormGroup {
                        // Off the ports the game's own list asks, friends need the address: say so here.
                        FormRow("On your network", detail: server.inLocalList ? hosting.localAddress(server)
                                    : "\(hosting.localAddress(server)) · Not in the game’s own local list, which looks at ports \(String(server.listedPorts.lowerBound)) to \(String(server.listedPorts.upperBound)). Friends connect to this address.",
                                monospacedTitle: false) {
                            Button("Copy") { copy(hosting.localAddress(server)) }
                        }
                        FormRow("Over the internet", detail: "Your router has to pass UDP port \(String(server.port)) to this Mac. CoD2 Silicon can’t check that from here.") {
                            Button("How To") { NSWorkspace.shared.open(HostingCatalog.portGuide) }.help("Opens the server guide on GitHub")
                        }
                        FormRow("Invite link", detail: "Opens CoD2 Silicon on a friend’s Mac on your network. The password is not in the link.") {
                            Button("Copy Link") { if let link = (try? LaunchLink.direct(hosting.localAddress(server)))?.url { copy(link) } }
                        }
                        if hosting.onWiFi {
                            FormRow("This Mac is on Wi-Fi", detail: "AirDrop and Handoff can make players freeze for a moment. A network cable is steadier.") {
                                Button("Why") { NSWorkspace.shared.open(HostingCatalog.wifiGuide) }.help("Opens the server guide on GitHub")
                            }
                        }
                    }
                    // Its full height always: the players below take what is left, never this group's lines.
                    .fixedSize(horizontal: false, vertical: true)
                    label("Players").padding(.top, 8)
                    // The page never runs past the window: as many players as fit, and the rest behind
                    // All Players, which opens every one of them as a dialog.
                    let players = status?.players ?? []
                    ViewThatFits(in: .vertical) {
                        playerRows(players, shown: players.count)
                        playerRows(players, shown: 10)
                        playerRows(players, shown: 8)
                        playerRows(players, shown: 6)
                        playerRows(players, shown: 4)
                        playerRows(players, shown: 3)
                        playerRows(players, shown: 2)
                        playerRows(players, shown: 1)
                        playerRows(players, shown: 0)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .topLeading)
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        label("Server log")
                        Spacer()
                        Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([hosting.logURL(server.id)]) }.buttonStyle(.plain)
                            .font(StudioTypography.help).foregroundStyle(palette.link)
                    }
                    ScrollViewReader { proxy in
                        ScrollView {
                            LazyVStack(alignment: .leading, spacing: 2) {
                                ForEach(Array((hosting.logs[server.id] ?? []).enumerated()), id: \.offset) { index, line in
                                    Text(line).font(.typewriter(12)).foregroundStyle(palette.secondary).textSelection(.enabled).id(index)
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                }
                            }
                            .padding(12)
                        }
                        .onChange(of: hosting.logs[server.id]?.count) { _, count in if let count, count > 0 { proxy.scrollTo(count - 1, anchor: .bottom) } }
                    }
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    .accessibilityLabel("Server log")
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            }
        }
    }
    private var title: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(server.name).font(.system(size: 26, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1).accessibilityAddTraits(.isHeader)
            TimelineView(.periodic(from: .now, by: 30)) { context in
                Label(running(now: context.date), systemImage: "checkmark.circle").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.positive)
            }
        }
    }
    @ViewBuilder private func actions(_ status: HostedStatus?) -> some View {
        Button("Edit…") { hosting.editing = server.id }.help("Change its settings. They apply the next time it starts.").fixedSize()
        Button("Next Map") { hosting.nextMap(server) }.disabled(status == nil).fixedSize()
        Button { model.join("127.0.0.1:\(server.port)", password: server.joinPassword.isEmpty ? nil : server.joinPassword) } label: { Label("Join", systemImage: "play.fill") }
            .prominentAction().disabled(status == nil || !model.canNavigate).fixedSize()
        Button("Stop Server") { hosting.stop(server) }.fixedSize()
    }
    private func playerRows(_ players: [HostedStatus.Player], shown: Int) -> some View {
        let shown = min(shown, players.count)
        return FormGroup {
            if players.isEmpty {
                FormRow("No one is on the server yet", detail: "Share the address above.") { EmptyView() }
            } else {
                ForEach(players.prefix(shown)) { player in HostingPlayerRow(hosting: hosting, server: server, player: player) }
                if shown < players.count {
                    FormRow(shown == 0 ? "\(players.count) players" : "\(players.count - shown) more") {
                        Button("All \(players.count) Players…") { hosting.showingPlayers = server }.fixedSize()
                    }
                }
            }
        }
    }
    private func running(now: Date) -> String {
        guard case .running(let since) = hosting.phase(server.id) else { return "Starting…" }
        let minutes = max(0, Int(now.timeIntervalSince(since) / 60))
        return minutes < 1 ? "Running" : minutes < 60 ? "Running for \(minutes) min" : "Running for \(minutes / 60) h \(minutes % 60) min"
    }
    private func label(_ text: String) -> some View {
        Text(text.uppercased()).font(.system(size: 11, weight: .semibold)).tracking(0.88).foregroundStyle(palette.tertiary).padding(.leading, 6).accessibilityAddTraits(.isHeader)
    }
    private func tile(_ value: String, suffix: String, _ caption: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("\(Text(value).foregroundStyle(palette.numeral))\(Text(suffix).foregroundStyle(palette.tertiary))")
                .font(.stencil(26)).monospacedDigit().lineLimit(1).minimumScaleFactor(0.6)
            Text(caption).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
        }
        .padding(14).frame(maxWidth: .infinity, alignment: .leading)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
        .accessibilityElement(children: .ignore).accessibilityLabel("\(value)\(suffix) \(caption)").accessibilityAddTraits(.isStaticText)
    }
    private func copy(_ text: String) {
        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(text, forType: .string)
        model.notice = "Copied."
    }
}

// MARK: - Export

private struct HostingExportSheet: View {
    @ObservedObject var hosting: HostingModel
    var server: HostedServer
    @Environment(\.palette) private var palette
    @State private var passwords = false
    @State private var matchSettings = true

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(alignment: .top, spacing: 12) {
                Image(systemName: "doc.text").font(.system(size: 22)).foregroundStyle(palette.accent).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 4) {
                    Text("Export “\(server.name)” for another machine").font(.system(size: 17, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                    Text("Writes a folder you can copy to a Linux or Windows server. Nothing is uploaded.").font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
            }
            FormGroup {
                FormRow("server.cfg", detail: "Name, rules, maps and limits, with a comment on every line.") { included }
                FormRow("Start commands", detail: "One for Linux, one for Windows, for \(server.engine == .cod2x ? "CoD2x" : "the original 1.3").") { included }
                if server.rules == .zpam {
                    FormRow("zPAM match settings", detail: "The match mode and its rules. zPAM’s files are not copied; the read-me links the release to download.") {
                        Toggle("Include zPAM match settings", isOn: $matchSettings).labelsHidden().toggleStyle(.switch)
                    }
                }
                FormRow("Passwords", detail: passwords ? "On, the file holds your passwords. Share it with care." : "Off, the file says CHANGE_ME where each password goes.") {
                    Toggle("Include passwords", isOn: $passwords).labelsHidden().toggleStyle(.switch)
                }
            }
            HStack(spacing: 10) {
                Text("Game files are never included.").font(StudioTypography.help).foregroundStyle(palette.tertiary)
                Spacer()
                Button("Cancel", role: .cancel) { hosting.exporting = nil }.keyboardShortcut(.cancelAction)
                Button("Export…") { export() }.prominentAction().keyboardShortcut(.defaultAction)
            }
        }
        .padding(22).frame(width: 560)
    }
    private var included: some View {
        Label("Included", systemImage: "checkmark").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.positive)
    }
    private func export() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false; panel.canChooseDirectories = true; panel.canCreateDirectories = true; panel.allowsMultipleSelection = false
        panel.message = "Choose where to put the server’s folder."; panel.prompt = "Export"
        guard panel.runModal() == .OK, let parent = panel.url else { return }
        do {
            let folder = try hosting.export(server, to: parent, passwords: passwords, matchSettings: matchSettings)
            hosting.exporting = nil
            NSWorkspace.shared.activateFileViewerSelecting([folder])
        } catch { hosting.notify("Couldn’t write the files. \(error.localizedDescription)") }
    }
}

/// One player on a running server, with the button that removes them.
private struct HostingPlayerRow: View {
    var hosting: HostingModel
    var server: HostedServer
    var player: HostedStatus.Player
    var body: some View {
        FormRow(QuakeColors.plain(player.name), detail: "\(player.ping) ms") {
            Button("Kick") { hosting.kick(player, from: server) }.controlSize(.small)
                .accessibilityLabel("Remove \(QuakeColors.plain(player.name)) from the server")
        }
    }
}

/// Everyone on a running server, when its page has room for only some of them.
private struct HostingPlayersSheet: View {
    @ObservedObject var hosting: HostingModel
    var server: HostedServer
    var body: some View {
        let status = hosting.statuses[server.id]
        let players = status?.players ?? []
        SheetScaffold(icon: .symbol("person.2"), title: "Players on \(server.name)",
                      message: status.map { "\(players.count) of \($0.maxPlayers). Kick removes a player; they can join again." } ?? "The server isn’t answering.",
                      actions: [.primary("Done") { hosting.showingPlayers = nil }]) {
            ScrollView {
                FormGroup {
                    if players.isEmpty { FormRow("No one is on the server", detail: nil) { EmptyView() } }
                    ForEach(players) { player in HostingPlayerRow(hosting: hosting, server: server, player: player) }
                }
            }
            .frame(maxHeight: 380)
        }
    }
}

// MARK: - Review harness

extension LauncherModel {
    /// The Hosting board's states, with invented sample data.
    func previewHosting(_ screen: String) {
        var friends = HostedServer.new(id: "friends", name: "Friday Night Rifles")
        friends.adminPassword = "sample"
        var scrim = HostedServer.new(id: "scrim", name: "Scrim Room")
        scrim.adminPassword = "sample"; scrim.rules = .zpam; scrim.port = 28961; scrim.maps = ["mp_toujane", "mp_dawnville", "mp_matmata"]
        let release = ZPAMRelease(version: "4.12", asset: "zpam412.zip", url: ZPAMRelease.projectURL, bytes: 183_607_173, sha256: String(repeating: "0", count: 64))
        let status = HostedStatus(map: "mp_toujane", gameType: "tdm", maxPlayers: 12,
                                  players: [.init(id: 0, name: "Sample Player", score: 14, ping: 4), .init(id: 1, name: "Second Sample", score: 9, ping: 21),
                                            .init(id: 2, name: "Third Sample", score: 3, ping: 38)])
        let log = ["CoD2 MP 1.3 build mac-arm64", "Opening IP socket: 0.0.0.0:28960", "execing server.cfg", "------ Server Initialization ------",
                   "Server: mp_toujane", "-----------------------------------", "Sample Player connected", "Second Sample connected", "Third Sample connected"]
        switch screen {
        case "servers-hosting-new":
            hosting.preview(servers: [], selection: nil, draft: HostedServer.new(id: "draft", name: "My Server"))
        case "servers-hosting-new-released":
            var draft = HostedServer.new(id: "draft", name: "My Server"); draft.releasedCoD2x = true
            hosting.preview(servers: [], selection: nil, draft: draft)
        case "servers-hosting-running-wifi":
            hosting.preview(servers: [friends, scrim], selection: "friends", phases: ["friends": .running(since: Date().addingTimeInterval(-41 * 60))],
                            statuses: ["friends": status], logs: ["friends": log], onWiFi: true)
        case "servers-hosting-zpam-missing", "servers-hosting-zpam-getting", "servers-hosting-zpam-ready":
            let state: ZPAMState = screen.hasSuffix("missing") ? .missing(release) : screen.hasSuffix("getting") ? .getting(release, received: 71_303_168) : .ready(version: "4.12")
            hosting.preview(servers: [friends, scrim], selection: "scrim", zpam: [.four: state])
        case "servers-hosting-port-busy":
            hosting.preview(servers: [friends, scrim], selection: "friends", phases: ["friends": .portBusy(next: 28962)])
        case "servers-hosting-running":
            hosting.preview(servers: [friends, scrim], selection: "friends", phases: ["friends": .running(since: Date().addingTimeInterval(-41 * 60))],
                            statuses: ["friends": status], logs: ["friends": log])
        case "servers-hosting-full", "servers-hosting-players":
            // A full server: the page lists the players it has room for, the dialog all of them.
            var full = status
            full.players = (0..<12).map { .init(id: $0, name: ["Sample Player", "Second Sample", "Third Sample"][$0 % 3] + ($0 < 3 ? "" : " \($0 + 1)"), score: 20 - $0, ping: 4 + $0 * 7) }
            hosting.preview(servers: [friends, scrim], selection: "friends", phases: ["friends": .running(since: Date().addingTimeInterval(-41 * 60))],
                            statuses: ["friends": full], logs: ["friends": log], showingPlayers: screen.hasSuffix("players") ? friends : nil)
        case "servers-hosting-export":
            hosting.preview(servers: [friends, scrim], selection: "scrim", zpam: [.four: .ready(version: "4.12")], exporting: scrim)
        default:
            hosting.preview(servers: [], selection: nil)
        }
    }
}
