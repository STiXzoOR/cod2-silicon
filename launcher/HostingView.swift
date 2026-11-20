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
            let leading = max(22, 284 - origin.x)
            Group {
                if hosting.servers.isEmpty && hosting.draft == nil { none } else { some(width: proxy.size.width - leading - 32) }
            }
            .padding(.leading, leading).padding(.trailing, 32).padding(.top, 84).padding(.bottom, 24)
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
        // An empty state (stencil heading, as Design Rules allow), set larger as the page's only content.
        VStack(spacing: 12) {
            Image(systemName: "dot.radiowaves.left.and.right").font(.system(size: 44, weight: .light)).foregroundStyle(palette.secondary).accessibilityHidden(true)
            Text("HOST YOUR OWN SERVER").font(.stencil(34)).tracking(4).foregroundStyle(palette.chipIcon).multilineTextAlignment(.center)
                .accessibilityAddTraits(.isHeader)
            Text("Run a server on this Mac for friends on your network or over the internet, with the original 1.3 or CoD2x, public rules or a competitive zPAM match. Or write the files for a server that runs somewhere else.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).frame(maxWidth: 560).fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 10) {
                Button { hosting.newServer(engine: model.chosenEngine) } label: { Label("New Server", systemImage: "plus") }.prominentAction()
                Button("Write a server.cfg…") { hosting.newServer(engine: model.chosenEngine); hosting.exporting = hosting.draft }.glassAction()
            }
            .actionControlSize().padding(.top, 6)
            Text("The server uses your own game files. Nothing is contacted until you start it.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary)
        }
        .padding(.top, 176)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
    }

    /// The board's layout at 1440 wide: a 280-point list and two columns beside it. A narrower window
    /// (down to the 1100 minimum) keeps everything inside it: the list narrows, and the form's two
    /// columns become one.
    private func some(width: CGFloat) -> some View {
        let listWidth: CGFloat = width < 900 ? 220 : 300
        let columns = width - listWidth - 20 >= 700 ? 2 : 1
        return HStack(alignment: .top, spacing: 20) {
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
            GroupLabel("Your servers").padding(.horizontal, 8).padding(.top, 4).padding(.bottom, 6)
            if hosting.draft != nil {
                row("New server", "Not saved yet", symbol: "plus", tint: palette.tertiary, selected: hosting.showingDraft) { hosting.selection = nil }
            }
            ForEach(hosting.servers) { server in
                let running = hosting.isRunning(server.id), failed = meta(server) == "Couldn’t start"
                row(server.name, meta(server), symbol: running ? "checkmark.circle" : failed ? "exclamationmark.triangle" : "minus.circle",
                    tint: running ? palette.positive : failed ? palette.negative : palette.tertiary, selected: hosting.selection == server.id) {
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
            Text("A running server keeps going when you close this window. Quitting CoD2 Unbound asks before stopping it.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).lineSpacing(1.5).fixedSize(horizontal: false, vertical: true).padding(.horizontal, 8)
        }
        .padding(.vertical, 14).padding(.horizontal, 12)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "panel")
    }
    private func row(_ title: String, _ detail: String, symbol: String, tint: Color, selected: Bool, action: @escaping () -> Void) -> some View {
        let shape = RoundedRectangle(cornerRadius: 14, style: .continuous)
        return Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: symbol).font(.system(size: 15, weight: .medium)).foregroundStyle(tint).frame(width: 18)
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 3) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 9).padding(.horizontal, 10).frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: shape)
            .overlay(shape.strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(RowButtonStyle(radius: 14))
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
            content.toolbar {
                leading
                // With no servers yet the page itself offers New Server and the export, as the board draws it.
                if hasServers { ToolbarSpacer(.flexible); ToolbarItem { export }; ToolbarSpacer(.fixed); ToolbarItem { new } }
            }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar {
            leading
            if hasServers { ToolbarItem { Spacer() }; ToolbarItem { export }; ToolbarItem { new } }
        }
    }
    private var hasServers: Bool { !(hosting.servers.isEmpty && hosting.draft == nil) }
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
                    Text(isNew ? "New server" : current.name).font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1)
                        .accessibilityAddTraits(.isHeader)
                    // One column has no room for the hint beside the buttons: it goes under the name.
                    if columns == 1 { Text(hint(current)).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1) }
                }
                Spacer(minLength: 12)
                if columns == 2 { Text(hint(current)).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1) }
                if hosting.isRunning(current.id) {
                    Button("Done") { hosting.editing = nil }.prominentAction().actionControlSize()
                } else {
                    Group {
                        if isNew { Button("Cancel") { hosting.cancelDraft() } } else { Button("Delete Server…") { model.requestDeleteHostedServer(current) } }
                    }
                    .glassAction().actionControlSize()
                    Button("Start Server") { if let saved = isNew ? hosting.saveDraft() : current { hosting.start(saved) } }
                        .prominentAction().actionControlSize().disabled(current.rules == .zpam && !zpamReady(current))
                        .help(current.rules == .zpam && !zpamReady(current) ? "Get zPAM first" : "Starts the server on this Mac")
                }
            }
            problem(current)
            ScrollView {
                let layout = columns == 2 ? AnyLayout(HStackLayout(alignment: .top, spacing: 20)) : AnyLayout(VStackLayout(alignment: .leading, spacing: 18))
                layout {
                    VStack(alignment: .leading, spacing: 0) {
                        label("Server")
                        // A narrow column: each row puts its title and control on one line and its help under both.
                        FormGroup {
                            FormWideRow("Name", detail: "What players see in the server list.") {
                                LauncherTextField("Server name", text: server.name, width: 200)
                            }
                            FormWideRow("Game version", detail: current.engine == .cod2x
                                        ? "Anyone with CoD2x \(CoD2xFiles.version) or newer can join, on Mac, Windows or Linux. A running CoD2x server asks CoD2x’s update service about new versions."
                                        : "Anyone with the original 1.3 can join, on Mac or Windows.") {
                                Picker("Game version", selection: server.engine) {
                                    Text("CoD2x").tag(GameEngine.cod2x); Text("Original 1.3").tag(GameEngine.original)
                                }
                                .segmentedChoice()
                                .accessibilityLabel("Game version")
                            }
                            FormWideRow("Who can join", detail: current.reach.detail) {
                                PopUpButton("Who can join", selection: server.reach, options: HostedServer.Reach.allCases.map { ($0, $0.title) })
                            }
                            FormRow("Players") {
                                PopUpButton("Players", selection: server.players, options: [2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 32].map { ($0, "\($0)") })
                            }
                            FormWideRow("Password to join", detail: "Leave empty for an open server.") {
                                LauncherTextField("Password to join", text: server.joinPassword, prompt: "None", secure: true, width: 180)
                            }
                            FormWideRow("Admin password", detail: "For controlling the server from inside the game. Kept on this Mac.") {
                                HStack(spacing: 8) {
                                    LauncherTextField("Admin password", text: server.adminPassword, secure: !showAdmin, monospaced: true, width: 120)
                                    Button(showAdmin ? "Hide" : "Show") { showAdmin.toggle() }.glassAction().controlSize(.large).fixedSize()
                                }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    VStack(alignment: .leading, spacing: 0) {
                        label("Match")
                        FormGroup {
                            FormWideRow("Rules", detail: current.rules.detail) {
                                Picker("Rules", selection: server.rules) { ForEach(HostedServer.Rules.allCases, id: \.self) { Text($0.title).tag($0) } }
                                    .segmentedChoice()
                                    .accessibilityLabel("Rules")
                            }
                            if current.rules == .zpam { zpamRows(current) } else {
                                FormRow("Game type") {
                                    PopUpButton("Game type", selection: server.gameType, options: HostingCatalog.gameTypes.map { ($0.id, $0.title) })
                                }
                                FormRow("Friendly fire", isOn: server.friendlyFire)
                            }
                        }
                        label("Maps, in order").padding(.top, 14)
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
    /// A section title above a form group (.sec-title): SF 13 semibold, secondary, sentence case.
    private func label(_ text: String) -> some View {
        Text(text).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.horizontal, 6).padding(.bottom, 8).accessibilityAddTraits(.isHeader)
    }
    private func zpamReady(_ server: HostedServer) -> Bool { if case .ready = hosting.zpam[server.zpamLine] { return true }; return false }

    @ViewBuilder private func problem(_ current: HostedServer) -> some View {
        switch hosting.phase(current.id) {
        case .failed(let message): Banner(.error, title: "The server couldn’t start", message: message)
        case .portBusy(let next):
            Banner(.error, title: "The server couldn’t start: port \(String(current.port)) is already in use.",
                   message: "Another server or a running game is using it. The next free port is \(String(next)).") {
                Button("Use \(String(next)) and Start") { if let saved = hosting.saveDraft() ?? hosting.selected { hosting.start(saved, port: next) } }
                    .prominentAction().controlSize(.large)
            }
        default: EmptyView()
        }
    }

    @ViewBuilder private func zpamRows(_ current: HostedServer) -> some View {
        let megabytes: (Int) -> String = { "\(($0 + 524_288) / 1_048_576) MB" }
        switch hosting.zpam[current.zpamLine] ?? .unknown {
        case .unknown:
            FormWideRow("Looking up zPAM…", detail: "Asking the zPAM project’s releases on GitHub which version is current.") { ProgressView().controlSize(.small) }
        case .failed(let message):
            FormWideRow("zPAM couldn’t be looked up", detail: message) {
                Button("Try Again") { Task { await hosting.lookUpZPAM(current.zpamLine) } }.glassAction().controlSize(.large)
            }
        case .missing(let release):
            FormWideRow("zPAM isn’t on this Mac yet",
                        detail: "The match mod competitive servers run, made by the author of CoD2x. \(megabytes(release.bytes)) with its map pack, from the zPAM project’s releases on GitHub. Checked after download and kept beside your servers; your game folder is not touched.") {
                Button("Get zPAM \(release.version)") { hosting.getZPAM(current.zpamLine) }.prominentAction().controlSize(.large).fixedSize()
            }
        case .getting(let release, let received):
            FormWideRow("Getting zPAM \(release.version)…", detail: "\(megabytes(received).dropLast(3)) of \(megabytes(release.bytes))") {
                HStack(spacing: 10) {
                    ProgressBar("Downloading zPAM", value: Double(received) / Double(max(1, release.bytes))).frame(minWidth: 60, maxWidth: 180)
                    Button("Cancel") { hosting.cancelZPAM() }.glassAction().controlSize(.regular).fixedSize()
                }
            }
        case .ready(let version):
            FormWideRow("zPAM \(version)", detail: current.engine == .cod2x ? "Installed. A running match server checks CoD2x’s update service and keeps zPAM current by itself."
                                                                            : "Installed, for the original 1.3.") {
                StatusLabel(.ready)
            }
            FormWideRow("Match mode", detail: "zPAM’s own name for the format.") {
                PopUpButton("Match mode", selection: server.matchMode, options: HostingCatalog.matchModes.map { ($0.id, $0.title) })
            }
            FormWideRow("Players get maps from", detail: "zPAM’s map pack is fetched by each player’s game from this address.") {
                Text("cod2x.me · zPAM’s default").font(.system(size: 13)).foregroundStyle(palette.secondary).lineLimit(1)
            }
        }
    }

    /// Each map is a tile with its picture, so two maps are told apart at a glance and not only by name.
    private func maps(_ current: HostedServer) -> some View {
        let box = RoundedRectangle(cornerRadius: FormMetrics.radius(.panel), style: .continuous)
        return VStack(alignment: .leading, spacing: 8) {
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 10, alignment: .top), count: 4), alignment: .leading, spacing: 12) {
                ForEach(Array(current.maps.enumerated()), id: \.offset) { index, map in
                    HostingMapTile(artwork: model.artwork, map: map, order: index + 1, canRemove: current.maps.count > 1) { server.wrappedValue.maps.remove(at: index) }
                        .draggable(map)
                        .dropDestination(for: String.self) { items, _ in
                            guard let moved = items.first, let from = current.maps.firstIndex(of: moved), from != index else { return false }
                            var maps = current.maps
                            maps.remove(at: from); maps.insert(moved, at: min(index, maps.count))
                            server.wrappedValue.maps = maps
                            return true
                        }
                }
                Button { hosting.addingMap = true } label: {
                    VStack(alignment: .leading, spacing: 6) {
                        Color.clear.aspectRatio(4.0 / 3.0, contentMode: .fit)
                            .overlay(Image(systemName: "plus").font(.system(size: 17, weight: .semibold)).accessibilityHidden(true))
                            .overlay(RoundedRectangle(cornerRadius: HostingMapTile.radius, style: .continuous)
                                .strokeBorder(palette.selectionStroke, style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
                        Text("Add Map").font(.system(size: 12, weight: .semibold)).lineLimit(1)
                    }
                    .foregroundStyle(palette.accent)
                    .contentShape(RoundedRectangle(cornerRadius: HostingMapTile.radius, style: .continuous))
                }
                .buttonStyle(.plain)
                .disabled(HostingCatalog.maps.allSatisfy(current.maps.contains))
                .accessibilityLabel("Add Map").accessibilityHint("Shows the maps that aren’t in the rotation yet, with their pictures")
                .popover(isPresented: $hosting.addingMap, arrowEdge: .leading) {
                    HostingMapPicker(artwork: model.artwork, maps: Binding(get: { server.wrappedValue.maps }, set: { server.wrappedValue.maps = $0 })) { hosting.addingMap = false }
                        .studioPopover()
                }
            }
            .padding(HostingMapTile.inset).frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.panel, in: box).overlay(box.strokeBorder(palette.panelStroke, lineWidth: 1))
            Text("Drag to reorder. The server starts on the first map.").font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.horizontal, 6)
        }
    }
}

/// One map in the rotation: its loading screen from the person's own game (or the drawn scene when
/// there is none), its place in the order, and Remove.
private struct HostingMapTile: View {
    static let inset: CGFloat = 10
    static var radius: CGFloat { Concentric.radius(in: FormMetrics.radius(.panel), inset: inset) }
    @ObservedObject var artwork: MapArtworkStore
    var map: String
    var order: Int
    var canRemove: Bool
    var remove: () -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        let name = MapCatalog.displayName(map)
        VStack(alignment: .leading, spacing: 6) {
            Color.clear.aspectRatio(4.0 / 3.0, contentMode: .fit)
                .overlay { MapArtFrame(map: map, image: artwork.image(for: map), size: .fill, radius: Self.radius) }
                .overlay(RoundedRectangle(cornerRadius: Self.radius, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
                .overlay(alignment: .topLeading) {
                    Text("\(order)").font(.stencil(13)).monospacedDigit().foregroundStyle(Palette.dark.numeral)
                        .padding(.horizontal, 5).frame(minWidth: 20, minHeight: 20)
                        .background(Color.black.opacity(0.66), in: RoundedRectangle(cornerRadius: 6, style: .continuous))
                        .padding(5).accessibilityHidden(true)
                }
                .overlay(alignment: .topTrailing) {
                    Button(action: remove) {
                        Image(systemName: "xmark").font(.system(size: 8, weight: .heavy)).foregroundStyle(Palette.dark.heading)
                            .frame(width: 20, height: 20)
                            .background(Color.black.opacity(0.66), in: Circle())
                            .overlay(Circle().strokeBorder(Color.white.opacity(0.16), lineWidth: 1))
                            .frame(width: StudioMetrics.minimumHitTarget, height: StudioMetrics.minimumHitTarget).contentShape(Circle())
                    }
                    .buttonStyle(.plain).disabled(!canRemove).opacity(canRemove ? 1 : 0.4)
                    .accessibilityLabel("Remove \(name)")
                    .help(canRemove ? "Remove \(name) from the rotation" : "A server needs at least one map")
                }
            Text(name).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1).truncationMode(.tail)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(name), map \(order)\(order == 1 ? ", the server starts here" : "")")
    }
}

/// Add Map: the maps not in the rotation yet, each with its picture and where it is set. Clicking
/// one adds it; the picker stays open so several can be added.
private struct HostingMapPicker: View {
    @ObservedObject var artwork: MapArtworkStore
    @Binding var maps: [String]
    var done: () -> Void
    @Environment(\.palette) private var palette
    private static let tile: CGFloat = 108

    var body: some View {
        let available = HostingCatalog.maps.filter { !maps.contains($0) }
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .firstTextBaseline) {
                Text("Add a map").padding(.leading, 6).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                Spacer(minLength: 12)
                Text(available.isEmpty ? "Every map is in the rotation" : "\(available.count) not in the rotation").font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary)
            }
            if !available.isEmpty {
                LazyVGrid(columns: Array(repeating: GridItem(.fixed(Self.tile + 12), spacing: 4, alignment: .top), count: 4), alignment: .leading, spacing: 4) {
                    ForEach(available, id: \.self) { map in
                        let name = MapCatalog.displayName(map)
                        Button { maps.append(map) } label: {
                            VStack(alignment: .leading, spacing: 6) {
                                MapArtFrame(map: map, image: artwork.image(for: map), size: .fill, radius: 8)
                                    .frame(width: Self.tile, height: Self.tile * 3 / 4)
                                    .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
                                VStack(alignment: .leading, spacing: 1) {
                                    Text(name).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                                    Text(MapCatalog.region(map).map { $0.capitalized } ?? " ").font(.system(size: 11)).foregroundStyle(palette.tertiary).lineLimit(1)
                                }
                                .padding(.horizontal, 2)
                            }
                            .padding(6)
                            .contentShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
                        }
                        .buttonStyle(RowButtonStyle(radius: 14))
                        .accessibilityLabel("Add \(name)\(MapCatalog.region(map).map { ", \($0.capitalized)" } ?? "")")
                    }
                }
            }
            HStack(spacing: 12) {
                Text("Click a map to add it to the end of the rotation.").font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.leading, 6)
                Spacer(minLength: 12)
                Button("Done", action: done).glassAction().keyboardShortcut(.defaultAction)
            }
        }
        .frame(width: 4 * (Self.tile + 12) + 3 * 4)
        .padding(.horizontal, 14).padding(.top, 16).padding(.bottom, 14)
    }
}

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
            HStack(spacing: 14) {
                tile(status.map { "\($0.players.count)" } ?? "—", suffix: status.map { " / \($0.maxPlayers)" } ?? "", "players", numeral: true)
                tile(status.map { MapCatalog.displayName(HostingCatalog.stockMap($0.map)) } ?? "Starting…", suffix: "", status.map { [HostingCatalog.gameTypeTitle($0.gameType), $0.matchInfo].compactMap { $0 }.joined(separator: " · ") } ?? "The first map is loading")
                tile(server.engine == .cod2x ? "CoD2x" : "Original 1.3", suffix: "", server.rules.title)
                tile(String(server.port), suffix: "", "UDP port", numeral: true)
            }
            HStack(alignment: .top, spacing: 20) {
                VStack(alignment: .leading, spacing: 0) {
                    label("How to join")
                    FormGroup {
                        // Off the ports the game's own list asks, friends need the address: say so here.
                        FormWideRow("On your network") {
                            Button("Copy") { copy(hosting.localAddress(server)) }.glassAction().controlSize(.large)
                        } detail: {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(hosting.localAddress(server)).font(.typewriter(12))
                                if !server.inLocalList {
                                    Text("Not in the game’s own local list, which looks at ports \(String(server.listedPorts.lowerBound)) to \(String(server.listedPorts.upperBound)). Friends connect to this address.")
                                }
                            }
                        }
                        FormWideRow("Over the internet", detail: "Your router has to pass UDP port \(String(server.port)) to this Mac. CoD2 Unbound can’t check that from here.") {
                            Button("How To") { NSWorkspace.shared.open(HostingCatalog.portGuide) }.glassAction().controlSize(.large).help("Opens the server guide on GitHub")
                        }
                        FormWideRow("Invite link", detail: "Opens CoD2 Unbound on a friend’s Mac on your network. The password is not in the link.") {
                            Button("Copy Link") { if let link = (try? LaunchLink.direct(hosting.localAddress(server)))?.url { copy(link) } }.glassAction().controlSize(.large)
                        }
                        if hosting.onWiFi {
                            FormWideRow("This Mac is on Wi-Fi", detail: "AirDrop and Handoff can make players freeze for a moment. A network cable is steadier.") {
                                Button("Why") { NSWorkspace.shared.open(HostingCatalog.wifiGuide) }.glassAction().controlSize(.large).help("Opens the server guide on GitHub")
                            }
                        }
                    }
                    // Its full height always: the players below take what is left, never this group's lines.
                    .fixedSize(horizontal: false, vertical: true)
                    label("Players").padding(.top, 14)
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
                VStack(alignment: .leading, spacing: 0) {
                    HStack(alignment: .firstTextBaseline) {
                        label("Server log")
                        Spacer()
                        Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([hosting.logURL(server.id)]) }.buttonStyle(.plain)
                            .font(.system(size: 13)).foregroundStyle(palette.link).padding(.trailing, 6)
                    }
                    ScrollViewReader { proxy in
                        ScrollView {
                            LazyVStack(alignment: .leading, spacing: 2) {
                                ForEach(Array((hosting.logs[server.id] ?? []).enumerated()), id: \.offset) { index, line in
                                    Text(line).font(.typewriter(12)).foregroundStyle(palette.secondary).textSelection(.enabled).id(index)
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                }
                            }
                            .padding(.vertical, 12).padding(.horizontal, 14)
                        }
                        .onChange(of: hosting.logs[server.id]?.count) { _, count in if let count, count > 0 { proxy.scrollTo(count - 1, anchor: .bottom) } }
                    }
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    .accessibilityLabel("Server log")
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            }
        }
    }
    private var title: some View {
        HStack(alignment: .firstTextBaseline, spacing: 14) {
            Text(server.name).font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1).accessibilityAddTraits(.isHeader)
            TimelineView(.periodic(from: .now, by: 30)) { context in
                StatusLabel(.ready, running(now: context.date))
            }
        }
    }
    @ViewBuilder private func actions(_ status: HostedStatus?) -> some View {
        Group {
            Button("Edit…") { hosting.editing = server.id }.glassAction().help("Change its settings. They apply the next time it starts.").fixedSize()
            Button("Next Map") { hosting.nextMap(server) }.glassAction().disabled(status == nil).fixedSize()
            Button { model.join("127.0.0.1:\(server.port)", password: server.joinPassword.isEmpty ? nil : server.joinPassword) } label: { Label("Join", systemImage: "play.fill") }
                .prominentAction().disabled(status == nil || !model.canNavigate).fixedSize()
            Button("Stop Server") { hosting.stop(server) }.destructiveAction().fixedSize()
        }
        .controlSize(.large)
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
                        Button("All \(players.count) Players…") { hosting.showingPlayers = server }.glassAction().controlSize(.regular).fixedSize()
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
    /// A section title above a form group (.sec-title): SF 13 semibold, secondary, sentence case.
    private func label(_ text: String) -> some View {
        Text(text).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.horizontal, 6).padding(.bottom, 8).accessibilityAddTraits(.isHeader)
    }
    /// A fact card: a count or the port as a stencil numeral (30, so 28 and up), words in SF 20.
    private func tile(_ value: String, suffix: String, _ caption: String, numeral: Bool = false) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Group {
                if numeral {
                    Text("\(Text(value).foregroundStyle(palette.numeral))\(Text(suffix).font(.stencil(18)).foregroundStyle(palette.tertiary))")
                        .font(.stencil(30)).monospacedDigit()
                } else {
                    Text(value).font(.system(size: 20, weight: .semibold)).foregroundStyle(palette.heading)
                }
            }
            .lineLimit(1).minimumScaleFactor(0.6)
            Text(caption).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
        }
        .padding(16).frame(maxWidth: .infinity, alignment: .leading)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
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
        SheetScaffold(icon: .symbol("doc.text"), title: "Export “\(server.name)” for another machine",
                      message: "Writes a folder you can copy to a Linux or Windows server. Nothing is uploaded.", width: 560,
                      actions: [.cancel { hosting.exporting = nil }, .primary("Export…") { export() }]) {
            FormGroup {
                FormRow("server.cfg", detail: "Name, rules, maps and limits, with a comment on every line.") { included }
                FormRow("Start commands", detail: "One for Linux, one for Windows, for \(server.engine == .cod2x ? "CoD2x" : "the original 1.3").") { included }
                if server.rules == .zpam {
                    FormRow("zPAM match settings", detail: "The match mode and its rules. zPAM’s files are not copied; the read-me links the release to download.") {
                        SwitchToggle("Include zPAM match settings", isOn: $matchSettings)
                    }
                }
                FormRow("Passwords", detail: passwords ? "On, the file holds your passwords. Share it with care." : "Off, the file says CHANGE_ME where each password goes.") {
                    SwitchToggle("Include passwords", isOn: $passwords)
                }
            }
        } note: {
            Text("Game files are never included.")
        }
    }
    private var included: some View { StatusLabel(.ready, "Included") }
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
    @Environment(\.palette) private var palette
    var body: some View {
        FormRow(QuakeColors.plain(player.name), minHeight: 40) {
            Text("\(player.ping) ms").font(.system(size: 12)).monospacedDigit().foregroundStyle(palette.tertiary)
            Button("Kick") { hosting.kick(player, from: server) }.glassAction().controlSize(.regular)
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
        case "servers-hosting-new", "servers-hosting-add-map":
            hosting.preview(servers: [], selection: nil, draft: HostedServer.new(id: "draft", name: "My Server"))
            hosting.addingMap = screen.hasSuffix("add-map")
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
