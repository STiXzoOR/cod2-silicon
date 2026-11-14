import SwiftUI
import AppKit

// Owner: ship-servers. The server list (Servers board): sortable column heads and the rows. Arrow keys
// move the selection, Return launches, a row's context menu follows the Menus board.

struct ServerList: View {
    @ObservedObject var model: LauncherModel
    var rows: [GameServer]
    var compact: Bool
    var focus: FocusState<Bool>.Binding
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    init(model: LauncherModel, rows: [GameServer], compact: Bool, focus: FocusState<Bool>.Binding) {
        self.model = model; self.rows = rows; self.compact = compact; self.focus = focus; artwork = model.artwork
    }
    var body: some View {
        let selected = model.selected?.address
        VStack(alignment: .leading, spacing: 0) {
            ColumnHeaders(sort: model.sort, ascending: model.sortAscending, compact: compact, select: model.sortServers)
            ScrollViewReader { reader in
                ScrollView {
                    LazyVStack(spacing: 4) {
                        ForEach(rows) { server in
                            ServerRow(server: server, image: artwork.image(for: server.map), selected: server.address == selected,
                                      favorite: model.isFavorite(server.address), compact: compact, focused: focus.wrappedValue) {
                                withAnimation(reduceMotion ? nil : .launcherSpring) { model.selectedServer = server.address }
                                focus.wrappedValue = true
                            } toggleFavorite: { model.toggleFavorite(server) } launch: { model.connect(server) }
                            .contextMenu { ServerRowMenu(model: model, server: server) }
                            .id(server.address)
                        }
                    }
                    .padding(.bottom, 24)
                }
                .scrollIndicators(.automatic)
                .mask(VStack(spacing: 0) { Color.black; LinearGradient(colors: [.black, .clear], startPoint: .top, endPoint: .bottom).frame(height: 40) })
                .focusable(interactions: .edit).focused(focus).focusEffectDisabled()
                .onKeyPress(keys: [.upArrow, .downArrow]) { press in
                    guard !rows.isEmpty else { return .ignored }
                    guard let index = rows.firstIndex(where: { $0.address == selected }) else {
                        model.selectedServer = rows[0].address; reader.scrollTo(rows[0].address); return .handled
                    }
                    let next = press.key == .downArrow ? min(rows.count - 1, index + 1) : max(0, index - 1)
                    model.selectedServer = rows[next].address
                    reader.scrollTo(rows[next].address)
                    return .handled
                }
                // Return launches only while the list has focus; a default button would also take Return from the search field.
                .onKeyPress(.return) { guard let server = model.selected else { return .ignored }; model.connect(server); return .handled }
                .onChange(of: model.selectedServer) { _, address in
                    if let address, rows.contains(where: { $0.address == address }) { withAnimation(reduceMotion ? nil : .default) { reader.scrollTo(address) } }
                }
                .accessibilityLabel("Servers")
            }
        }
    }
}

/// Control-click on a server row (Menus board › Server row). Commands that can't run are dimmed, never hidden.
struct ServerRowMenu: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer
    var body: some View {
        Button("Launch") { model.connect(server) }.disabled(!model.canNavigate)
        Menu("Launch with Loadout") {
            ForEach(model.studio.profiles) { profile in
                Button(profile.isActive ? "\(profile.displayName) (in use)" : profile.displayName) { model.join(server.address, loadout: profile.id) }
            }
        }
        .disabled(!model.canNavigate || model.studio.profiles.isEmpty)
        Divider()
        Button(model.isFavorite(server.address) ? "Remove from Favorites" : "Add to Favorites") { model.toggleFavorite(server) }
        Button("Copy Address") { model.copyServer(server) }
        Button("Copy Join Link") { model.copyJoinLink(server) }
        Divider()
        Button("Refresh This Server") { model.refreshServer(server) }.disabled(model.snapshot)
    }
}

struct ColumnHeaders: View {
    var sort: ServerSort
    var ascending: Bool
    var compact = false
    var select: (ServerSort) -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 16) {
            if compact {
                header("Server", .name)
                Spacer(minLength: 12)
                header("Players", .players)
                header("Ping", .ping)
            } else {
                Text("MAP").frame(width: 80, alignment: .leading).accessibilityHidden(true)
                header("Server", .name).frame(maxWidth: .infinity, alignment: .leading)
                header("Players", .players).frame(width: 80, alignment: .trailing)
                header("Ping", .ping).frame(width: 84, alignment: .trailing)
                Color.clear.frame(width: 22, height: 1)
            }
        }
        .font(.stencil(12, heavy: false)).tracking(2.64).foregroundStyle(palette.tertiary)
        .padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.trailing).padding(.bottom, 8)
    }
    private func header(_ title: String, _ key: ServerSort) -> some View {
        let on = sort == key
        let order = key == .name ? (ascending ? "A to Z" : "Z to A") : key == .ping ? (ascending ? "lowest first" : "highest first") : (ascending ? "fewest first" : "most first")
        return Button { select(key) } label: {
            HStack(spacing: 4) {
                Text(title.uppercased())
                if on { Image(systemName: ascending ? "chevron.up" : "chevron.down").font(.system(size: 8, weight: .bold)) }
            }
            .foregroundStyle(on ? palette.secondary : palette.tertiary)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(on ? "Sorted by \(title.lowercased()), \(order)" : "Sort by \(title.lowercased())")
        .accessibilityHint(on ? "Reverses the order" : "")
        .accessibilityAddTraits(on ? .isSelected : [])
        .help(on ? "Reverse the order" : "Sort by \(title.lowercased())")
    }
}

struct ServerRow: View {
    var server: GameServer
    var image: NSImage?
    var selected: Bool
    var favorite: Bool
    var compact = false
    var focused = false
    var select: () -> Void
    var toggleFavorite: () -> Void
    var launch: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        let facts = ServerFacts(server)
        Group {
            if compact {
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 8) {
                        serverName
                        Spacer(minLength: 8)
                        favoriteButton
                    }
                    HStack(alignment: .bottom, spacing: 12) {
                        MapArtFrame(map: server.map, image: image, size: .card, radius: RowMetrics.artRadius)
                        VStack(alignment: .leading, spacing: 8) {
                            mapAndMode(lines: 2)
                            HStack(spacing: 12) {
                                badges
                                players(facts, size: 17, small: 12).fixedSize()
                                ping.fixedSize()
                            }
                        }
                        .font(.system(size: 12))
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
            } else {
                HStack(spacing: 16) {
                    MapArtFrame(map: server.map, image: image, size: .row, radius: RowMetrics.artRadius)
                    VStack(alignment: .leading, spacing: 6) {
                        serverName
                        HStack(spacing: 8) {
                            mapAndMode(lines: 1)
                            badges
                        }
                        .font(.system(size: 12))
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    players(facts, size: 24, small: 15).frame(width: 80, alignment: .trailing)
                    ping.frame(width: 84, alignment: .trailing)
                    favoriteButton
                }
            }
        }
        .padding(.vertical, RowMetrics.inset).padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.trailing)
        .background(selected ? palette.selection : (palette.dark ? Color.white : Color.black).opacity(hovering ? 0.045 : 0), in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous).strokeBorder(selected ? (focused ? palette.accent : palette.selectionStroke) : .clear, lineWidth: selected && focused ? 1.5 : 1))
        .contentShape(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(RowMetrics.radius), "server row")
        .onTapGesture(count: 2, perform: launch)
        .onTapGesture(perform: select)
        .onHover { hovering = $0 }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(QuakeColors.plain(server.name)), \(MapCatalog.displayName(server.map)), \(GameModes.long(server.gametype)), \(ServerVersion.label(server))\(server.mod.isEmpty ? "" : ", \(server.mod)"), \(server.playerCount) of \(server.maxPlayers) players, \(server.ping) milliseconds\(server.password ? ", needs a password" : "")\(favorite ? ", favorite" : "")")
        .accessibilityAddTraits(selected ? [.isButton, .isSelected] : .isButton)
        .accessibilityAction { select() }
        .accessibilityAction(named: "Launch", launch)
        .accessibilityAction(named: favorite ? "Remove from Favorites" : "Add to Favorites", toggleFavorite)
    }

    private func mapAndMode(lines: Int) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 4) {
            Text(MapCatalog.displayName(server.map)).lineLimit(lines)
            Text("· \(server.gametype.lowercased() == "sd" ? "S&D" : GameModes.short(server.gametype))").fixedSize()
        }
        .foregroundStyle(palette.secondary)
    }
    @ViewBuilder private var badges: some View {
        Badge(ServerVersion.label(server), tone: ServerFacts(server).isCoD2x ? .accent : .neutral)
        if !server.mod.isEmpty { Badge(server.mod) }
    }
    private func players(_ facts: ServerFacts, size: CGFloat, small: CGFloat) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 0) {
            Text("\(server.playerCount)").font(.stencil(size))
                .foregroundStyle(facts.occupancy == .full ? palette.negative : facts.occupancy == .empty ? palette.tertiary : (palette.dark ? Palette.hex(0xefe7d3) : palette.numeral))
            Text("/\(server.maxPlayers)").font(.stencil(small)).foregroundStyle(palette.tertiary)
        }
    }
    private var serverName: some View {
        HStack(spacing: 8) {
            QuakeName(name: server.name).font(.system(size: 15, weight: .semibold)).lineLimit(1)
            if server.password {
                Image(systemName: "lock").font(.system(size: 12, weight: .medium)).foregroundStyle(palette.secondary)
                    .accessibilityLabel("Needs a password").help("Needs a password")
            }
        }
    }
    private var ping: some View {
        HStack(spacing: 6) {
            PingBars(level: ServerFacts(server).pingLevel)
            Text("\(server.ping) ms").font(.system(size: 12))
                .foregroundStyle(palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary).monospacedDigit()
        }
    }
    private var favoriteButton: some View {
        Button(action: toggleFavorite) {
            Image(systemName: favorite ? "star.fill" : "star").font(.system(size: 14))
                .foregroundStyle(favorite ? palette.control : palette.secondary)
                .frame(width: 22, height: 22).contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(favorite ? "Remove \(QuakeColors.plain(server.name)) from favorites" : "Add \(QuakeColors.plain(server.name)) to favorites")
        .help(favorite ? "Remove from Favorites (⌘D)" : "Add to Favorites (⌘D)")
    }
}
