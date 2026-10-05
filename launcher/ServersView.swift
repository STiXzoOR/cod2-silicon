import SwiftUI
import AppKit

struct ServersView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @FocusState private var searchFocused: Bool
    @State private var origin = CGPoint.zero
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }

    var body: some View {
        let rows = model.visibleServers
        HStack(alignment: .top, spacing: 22) {
            VStack(alignment: .leading, spacing: 0) {
                PageHeader(title: "Servers", subtitle: "Stock 1.3 and CoD2x 1.4 from the public master servers. Pings refresh when you ask.")
                    .padding(.leading, 16).padding(.top, 92 - 52)
                ColumnHeaders(sort: $model.sort).padding(.top, 18)
                ScrollViewReader { reader in
                    ScrollView {
                        LazyVStack(spacing: 4) {
                            ForEach(rows) { server in
                                ServerRow(server: server, image: artwork.image(for: server.map), selected: server.address == model.selected?.address,
                                          favorite: model.library.favorites.contains(server.address)) {
                                    withAnimation(reduceMotion ? nil : .launcherSpring) { model.selectedServer = server.address }
                                } toggleFavorite: { model.favorite(server.address) } deploy: { model.connect(server) }
                                .id(server.address)
                            }
                            if rows.isEmpty { NoContact(refreshing: model.refreshing, hasServers: !model.servers.isEmpty) }
                        }
                        .padding(.bottom, 12)
                    }
                    .scrollIndicators(.automatic)
                    .bottomEdgeUnderBar()
                    .focusable()
                    .onMoveCommand { direction in
                        guard let index = rows.firstIndex(where: { $0.address == model.selected?.address }) else { return }
                        let next = direction == .down ? min(rows.count - 1, index + 1) : direction == .up ? max(0, index - 1) : index
                        model.selectedServer = rows[next].address
                        reader.scrollTo(rows[next].address)
                    }
                    // Return deploys only while the list has focus; a default button would also
                    // swallow Return in the search and direct-connect fields.
                    .onReturnKey { if let server = model.selected { model.connect(server) } }
                    .accessibilityLabel("Servers")
                }
                .bottomBar { DirectConnectBar(model: model).padding(.bottom, 22).padding(.top, 10) }
            }
            .frame(maxWidth: 790)
            ServerDetails(model: model, server: model.selected, image: model.selected.flatMap { artwork.image(for: $0.map) })
                .frame(width: 352)
                .padding(.top, 88 - 52).padding(.bottom, 22)
        }
        .padding(.leading, max(22, 264 - origin.x)).padding(.trailing, 18).padding(.top, 52)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background { ContourBackdrop().ignoresSafeArea() }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onChange(of: model.visibleServers.count) { _ in model.loadArtwork() }
        .toolbar {
            ToolbarItem(placement: .navigation) {
                HStack(spacing: 8) {
                    Image(systemName: "magnifyingglass").foregroundStyle(palette.secondary)
                    TextField("Search servers, maps or mods", text: $model.search).textFieldStyle(.plain).focused($searchFocused).frame(width: 250)
                    if !model.search.isEmpty {
                        Button { model.search = "" } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(palette.tertiary).accessibilityLabel("Clear search")
                    }
                }
                .padding(.horizontal, 10)
                .background { Button("") { searchFocused = true }.keyboardShortcut("f", modifiers: .command).opacity(0).accessibilityHidden(true) }
            }
            // Three toolbar groups (HIG): search, filters, then count and refresh on the trailing edge.
            ToolbarItem(placement: .navigation) {
                HStack(spacing: 14) {
                    Picker("Server version", selection: $model.filter) { ForEach(ServerFilter.allCases) { Text($0.rawValue).tag($0) } }
                        .pickerStyle(.segmented).labelsHidden().fixedSize()
                    Toggle("Hide empty", isOn: $model.hideEmpty)
                    Toggle("Hide full", isOn: $model.hideFull)
                }
                .toggleStyle(.checkbox).font(.system(size: 13)).padding(.trailing, 10)
            }
        }
        .trailingToolbar {
            Text(model.refreshing ? model.queryProgress : "\(rows.count) of \(model.servers.count) servers")
                .font(.system(size: 12)).foregroundStyle(palette.secondary).monospacedDigit().padding(.leading, 10)
            Button { model.refreshing ? model.cancelRefresh() : model.refresh() } label: {
                Label(model.refreshing ? "Stop refreshing" : "Refresh server list", systemImage: model.refreshing ? "stop.fill" : "arrow.clockwise")
            }
            .keyboardShortcut("r", modifiers: .command)
            .help(model.refreshing ? "Stop querying servers" : "Ask both master servers and ping every server (⌘R)")
        }
    }
}

private extension View {
    /// A bottom bar that floats over scrolling content, with the system scroll-edge effect on macOS 26.
    @ViewBuilder func bottomBar<Bar: View>(@ViewBuilder _ bar: () -> Bar) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) { self.safeAreaBar(edge: .bottom, spacing: 0, content: bar) } else { self.safeAreaInset(edge: .bottom, spacing: 0, content: bar) }
        #else
        self.safeAreaInset(edge: .bottom, spacing: 0, content: bar)
        #endif
    }
}

struct ColumnHeaders: View {
    @Binding var sort: ServerSort
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 16) {
            Text("MAP").frame(width: 96, alignment: .leading)
            header("SERVER", .name).frame(maxWidth: .infinity, alignment: .leading)
            header("PLAYERS", .players).frame(width: 80, alignment: .trailing)
            header("PING", .ping).frame(width: 84, alignment: .trailing)
            Color.clear.frame(width: 22, height: 1)
        }
        .font(.stencil(12, heavy: false)).tracking(2.64).foregroundStyle(palette.tertiary)
        .padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.trailing).padding(.bottom, 8)
    }
    private func header(_ title: String, _ key: ServerSort) -> some View {
        Button { sort = key } label: {
            HStack(spacing: 4) {
                Text(title)
                if sort == key { Image(systemName: key == .ping || key == .name ? "chevron.up" : "chevron.down").font(.system(size: 8, weight: .bold)) }
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Sort by \(title.lowercased())")
        .accessibilityAddTraits(sort == key ? .isSelected : [])
    }
}

struct ServerRow: View {
    var server: GameServer
    var image: NSImage?
    var selected: Bool
    var favorite: Bool
    var select: () -> Void
    var toggleFavorite: () -> Void
    var deploy: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        let facts = ServerFacts(server)
        HStack(spacing: 16) {
            MapArt(map: server.map, image: image).frame(width: 96, height: 60)
                .clipShape(RoundedRectangle(cornerRadius: RowMetrics.artRadius, style: .continuous)).shapeAudit(.inset, .rounded(RowMetrics.artRadius), "row art")
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    QuakeName(name: server.name).font(.system(size: 15, weight: .semibold)).lineLimit(1)
                    if server.password { Image(systemName: "lock.fill").font(.system(size: 11)).foregroundStyle(palette.secondary).accessibilityLabel("Password protected") }
                }
                HStack(spacing: 8) {
                    Text("\(MapCatalog.displayName(server.map)) · \(GameModes.short(server.gametype))").foregroundStyle(palette.secondary)
                    VersionBadge(facts: facts)
                }
                .font(.system(size: 12))
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            HStack(alignment: .firstTextBaseline, spacing: 0) {
                Text("\(server.playerCount)").font(.stencil(24))
                    .foregroundStyle(facts.occupancy == .full ? palette.negative : facts.occupancy == .empty ? palette.tertiary : (palette.dark ? Palette.hex(0xefe7d3) : palette.numeral))
                Text("/\(server.maxPlayers)").font(.stencil(15)).foregroundStyle(palette.tertiary)
            }
            .frame(width: 80, alignment: .trailing)
            HStack(spacing: 8) {
                PingBars(level: facts.pingLevel)
                Text("\(server.ping) ms").font(.system(size: 12)).foregroundStyle(palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary).monospacedDigit()
            }
            .frame(width: 84, alignment: .trailing)
            Button(action: toggleFavorite) {
                Image(systemName: favorite ? "star.fill" : "star").font(.system(size: 13))
                    .foregroundStyle(favorite ? (palette.dark ? Palette.hex(0xc99a4c) : palette.link) : palette.secondary)
                    .frame(width: 22, height: 22).contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(favorite ? "Remove \(QuakeColors.plain(server.name)) from favorites" : "Add \(QuakeColors.plain(server.name)) to favorites")
        }
        .padding(.vertical, RowMetrics.inset).padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.trailing)
        .background(selected ? palette.selection : (palette.dark ? Color.white : Color.black).opacity(hovering ? 0.045 : 0), in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous).stroke(selected ? palette.selectionStroke : .clear, lineWidth: 1))
        .contentShape(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(RowMetrics.radius), "server row")
        .onTapGesture(count: 2, perform: deploy)
        .onTapGesture(perform: select)
        .onHover { hovering = $0 }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(QuakeColors.plain(server.name)), \(MapCatalog.displayName(server.map)), \(GameModes.long(server.gametype)), \(facts.versionLabel), \(server.playerCount) of \(server.maxPlayers) players, \(server.ping) milliseconds\(server.password ? ", password protected" : "")")
        .accessibilityAddTraits(selected ? [.isButton, .isSelected] : .isButton)
        .accessibilityAction { select() }
        .accessibilityAction(named: "Deploy", deploy)
    }
}

struct VersionBadge: View {
    var facts: ServerFacts
    @Environment(\.palette) private var palette
    var body: some View {
        Text(facts.versionLabel)
            .font(.system(size: 11, weight: .semibold))
            .foregroundStyle(facts.isCoD2x ? palette.accent : (palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary))
            .padding(.vertical, 2).padding(.horizontal, 7)
            .overlay(RoundedRectangle(cornerRadius: 6, style: .continuous).stroke(facts.isCoD2x ? palette.selectionStroke : palette.track, lineWidth: 1))
    }
}

struct NoContact: View {
    var refreshing: Bool
    var hasServers: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 8) {
            Text("NO CONTACT").font(.stencil(26)).tracking(3.1).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            Text(refreshing ? "Listening for servers…" : hasServers ? "No servers match these filters." : "Refresh to ask the master servers, or connect directly below.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary)
        }
        .frame(maxWidth: .infinity).padding(.vertical, 80)
        .accessibilityElement(children: .combine)
    }
}

struct ServerDetails: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer?
    var image: NSImage?
    @Environment(\.palette) private var palette
    @Namespace private var glass
    private let radius: CGFloat = 26, inset: CGFloat = 14
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            if let server {
                let facts = ServerFacts(server)
                ZStack {
                    MapArt(map: server.map, image: image)
                    LinearGradient(stops: [.init(color: .clear, location: 0.55), .init(color: .black.opacity(0.55), location: 1)], startPoint: .top, endPoint: .bottom)
                }
                .frame(height: 168)
                .clipShape(RoundedRectangle(cornerRadius: Concentric.radius(in: radius, inset: inset), style: .continuous))
                .shapeAudit(.inset, .rounded(Concentric.radius(in: radius, inset: inset)), "inspector art")
                VStack(alignment: .leading, spacing: 6) {
                    Eyebrow(text: "\(MapCatalog.displayName(server.map)) · \(GameModes.long(server.gametype))", size: 12, tracking: 0.28)
                    QuakeName(name: server.name).font(.system(size: 19, weight: .bold)).lineLimit(2)
                    Text(server.address).font(.typewriter(13)).foregroundStyle(palette.address).textSelection(.enabled)
                        .accessibilityLabel("Address \(server.address)")
                }
                .padding(.horizontal, 6)
                Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 8) {
                    GridRow { fact("Version", facts.versionLabel.replacingOccurrences(of: "Stock ", with: "Stock ")); fact("Mod", server.mod.isEmpty ? "Classic" : server.mod) }
                    GridRow { fact("Frame cap", facts.frameCap); fact("Access", facts.access) }
                }
                .padding(.horizontal, 6)
                if server.password {
                    SecureField("Server password", text: $model.serverPassword).textFieldStyle(.plain)
                        .padding(.horizontal, 14).frame(height: ControlMetrics.action).wellCapsule().padding(.horizontal, 6)
                        .onSubmit { model.connect(server) }
                }
                GlassContainer(spacing: 8) {
                    HStack(spacing: 8) {
                        Button { model.connect(server) } label: {
                            Label("Deploy", systemImage: "play.fill").frame(maxWidth: .infinity)
                        }
                        .prominentAction().actionControlSize()
                        .accessibilityLabel("Deploy to \(QuakeColors.plain(server.name))")
                        .help("Deploy to this server (Return in the server list, or double-click a row)")
                        let favorite = model.library.favorites.contains(server.address)
                        Button { model.favorite(server.address) } label: {
                            Image(systemName: favorite ? "star.fill" : "star").font(.system(size: 15)).foregroundStyle(palette.dark ? palette.accent : palette.link).frame(width: 16, height: 16)
                        }
                        .glassAction(circle: true).actionControlSize()
                        .accessibilityLabel(favorite ? "Remove from favorites" : "Add to favorites")
                    }
                }
                .padding(.horizontal, 6)
                Rectangle().fill(palette.dark ? Color.white.opacity(0.08) : Color.black.opacity(0.08)).frame(height: 1).padding(.horizontal, 6)
                HStack(alignment: .firstTextBaseline) {
                    Eyebrow(text: "Roster", size: 12, tracking: 0.24, color: palette.tertiary)
                    Spacer()
                    Text("Score · Ping").font(.system(size: 11)).foregroundStyle(palette.tertiary)
                }
                .padding(.horizontal, 6)
                ScrollView {
                    VStack(spacing: 2) {
                        ForEach(server.players) { player in
                            HStack(spacing: 10) {
                                QuakeName(name: player.name).font(.system(size: 13)).lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
                                Text("\(player.score)").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).frame(width: 34, alignment: .trailing)
                                Text("\(player.ping)").font(.system(size: 13)).foregroundStyle(palette.tertiary).frame(width: 34, alignment: .trailing)
                            }
                            .frame(height: 26)
                            .accessibilityElement(children: .combine)
                        }
                        if server.players.isEmpty { Text("Nobody here yet.").font(.system(size: 13)).foregroundStyle(palette.tertiary).frame(maxWidth: .infinity, alignment: .leading).padding(.vertical, 8) }
                    }
                    .padding(.horizontal, 6)
                }
                .scrollIndicators(.never)
            } else {
                Spacer()
                Text("Select a server to see who's playing.").font(.system(size: 13)).foregroundStyle(palette.secondary).frame(maxWidth: .infinity)
                Spacer()
            }
        }
        .padding(inset)
        .frame(maxHeight: .infinity, alignment: .top)
        .glassSurface(RoundedRectangle(cornerRadius: radius, style: .continuous))
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Server details")
    }
    private func fact(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label).font(.system(size: 11)).foregroundStyle(palette.tertiary)
            Text(value).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
    }
}

extension View {
    /// Inset text-field well inside glass bars.
    func wellCapsule() -> some View { modifier(WellCapsule()) }
}
private struct WellCapsule: ViewModifier {
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        content.background(palette.well, in: Capsule()).overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1)).shapeAudit(.field, .capsule, "field")
    }
}

/// The bar is a capsule; its fields and Connect are capsules of one height, inset equally from the
/// bar's top, bottom and trailing end, so they're concentric with it.
struct DirectConnectBar: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    private let inset: CGFloat = 11
    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "mappin.and.ellipse").font(.system(size: 15, weight: .medium)).foregroundStyle(palette.dark ? palette.accent : palette.link).accessibilityHidden(true)
            Text("Direct connect").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.chipText).fixedSize()
            TextField("hostname or IP:port", text: $model.directAddress).textFieldStyle(.plain)
                .padding(.horizontal, 14).frame(height: ControlMetrics.action).wellCapsule()
                .onSubmit { model.connect() }
                .accessibilityLabel("Server address")
            SecureField("Password (optional)", text: $model.serverPassword).textFieldStyle(.plain)
                .padding(.horizontal, 14).frame(width: 190, height: ControlMetrics.action).wellCapsule()
                .accessibilityLabel("Server password, optional")
            Button("Connect") { model.connect() }.prominentAction().actionControlSize()
        }
        .padding(.leading, 20).padding(.trailing, inset)
        .frame(height: ControlMetrics.action + 2 * inset)
        .glassSurface(Capsule())
    }
}

/// Topographic contour lines behind the server list: original, quiet and content-layer.
struct ContourBackdrop: View {
    @Environment(\.palette) private var palette
    var body: some View {
        Canvas { context, size in
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .radialGradient(
                Gradient(colors: [(palette.dark ? Palette.hex(0x3a3220) : Palette.hex(0xe2d6b8)).opacity(palette.dark ? 0.45 : 0.6), .clear]),
                center: CGPoint(x: size.width * 0.7, y: 0), startRadius: 0, endRadius: size.width * 0.9))
            let lines = ["M260 120 C 420 60 620 90 760 150 S 1100 260 1440 180", "M260 170 C 430 110 610 140 770 200 S 1110 310 1440 230",
                         "M260 225 C 440 165 600 195 780 255 S 1120 365 1440 285", "M260 285 C 450 225 590 255 790 315 S 1130 425 1440 345",
                         "M260 350 C 460 290 580 320 800 380 S 1140 490 1440 410", "M980 520 C 1060 470 1180 470 1240 520 S 1300 640 1210 680 S 1010 640 980 520Z",
                         "M1020 530 C 1080 495 1160 495 1205 530 S 1250 620 1185 650 S 1040 620 1020 530Z"]
            let scale = CGAffineTransform(scaleX: size.width / 1440, y: size.height / 900)
            for line in lines {
                context.stroke(SVGPath.parse(line, transform: scale), with: .color((palette.dark ? Palette.hex(0xc9b27a) : Palette.hex(0x6f5a2a)).opacity(palette.dark ? 0.035 : 0.06)), lineWidth: 1.2)
            }
        }
        .accessibilityHidden(true)
    }
}

/// List rows (servers, demos): 60-point art 10 points in, concentric with a 20-point corner. Trailing
/// content keeps the corner radius from the edge; a button there sits as far in as from the top.
enum RowMetrics {
    static let inset: CGFloat = 10, radius: CGFloat = 20, trailing: CGFloat = 20, height: CGFloat = 60 + 2 * 10
    static var artRadius: CGFloat { Concentric.radius(in: radius, inset: inset) }
    static func buttonTrailing(_ size: ControlSize) -> CGFloat { (height - ControlMetrics.height(size)) / 2 }
}

private extension View {
    /// Rows fade out over the last 36 points above the direct-connect bar, so none is sliced at rest or
    /// reads through the glass; the mask also clips anything drawn into the bar's inset.
    func bottomEdgeUnderBar() -> some View {
        self.mask(VStack(spacing: 0) {
            Color.black
            LinearGradient(colors: [.black, .clear], startPoint: .top, endPoint: .bottom).frame(height: 36)
        })
    }
    /// Return while this view has focus (macOS 14+); on macOS 13 double-click and the Deploy button remain.
    @ViewBuilder func onReturnKey(_ action: @escaping () -> Void) -> some View {
        if #available(macOS 14.0, *) { self.onKeyPress(.return) { action(); return .handled } } else { self }
    }
}
