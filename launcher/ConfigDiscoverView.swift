import SwiftUI
import AppKit
import UniformTypeIdentifiers

// Discover › Players, Saved and a player's detail. Every card, credit and permission comes
// from a verified signed index; without one the page says so and offers import instead.

/// The Discover rows under My Loadouts in the Configs column.
struct DiscoverColumnSection: View {
    @ObservedObject var discover: ConfigDiscoverModel
    var open: (ConfigDiscoverPage) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("DISCOVER").font(.system(size: 11, weight: .semibold)).tracking(0.88).foregroundStyle(palette.tertiary)
                .padding(.horizontal, 10).padding(.top, 22).padding(.bottom, 6).accessibilityAddTraits(.isHeader)
            row("Players", symbol: "person.2", count: nil, selected: isPlayers) { open(.players) }
            row("Saved", symbol: "bookmark", count: discover.saved.count, selected: discover.page == .saved) { open(.saved) }
        }
        .task { await discover.loadIfNeeded() }
    }

    private var isPlayers: Bool {
        switch discover.page { case .players, .player: true; default: false }
    }

    private func row(_ title: String, symbol: String, count: Int?, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: symbol).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.accent)
                    .frame(width: 30, height: 30)
                    .background(palette.dark ? Palette.hex(0x1b1f17) : palette.crate, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.dark ? Color.white.opacity(0.09) : palette.wellStroke, lineWidth: 1))
                    .accessibilityHidden(true)
                Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                Spacer(minLength: 4)
                if let count { Text("\(count)").font(.system(size: 12)).monospacedDigit().foregroundStyle(palette.secondary) }
            }
            .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 12))
        .accessibilityLabel(count.map { "\(title), \($0) saved" } ?? title)
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

/// The Discover detail column: Players, Saved or one player.
struct ConfigDiscoverView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject private var artwork: MapArtworkStore
    init(model: LauncherModel, discover: ConfigDiscoverModel) { self.model = model; self.discover = discover; artwork = model.artwork }

    var body: some View {
        Group {
            switch discover.page {
            case .player(let id): PlayerDetailPage(model: model, discover: discover, artwork: artwork, entryID: id)
            case .saved: SavedPage(discover: discover, artwork: artwork)
            default: PlayersPage(discover: discover, artwork: artwork)
            }
        }
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .task(id: discover.entries.compactMap(\.favoriteMap)) {
            // Card art comes from the person's own game data, with the existing illustrated fallback.
            let maps = discover.entries.compactMap(\.favoriteMap)
            guard !model.snapshot, !model.dataPath.isEmpty, !maps.isEmpty else { return }
            await artwork.load(dataPath: model.dataPath, maps: maps + model.visibleServers.prefix(48).map(\.map))
        }
    }
}

// MARK: - Shared header

private struct DiscoverHeader<Trailing: View>: View {
    var title: String
    @ViewBuilder var trailing: Trailing
    @Environment(\.palette) private var palette
    var body: some View {
        // Side by side when there's room; otherwise the tools wrap under the title.
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .bottom, spacing: 20) { heading; Spacer(minLength: 12); trailing }
            VStack(alignment: .leading, spacing: 14) { heading; trailing }
        }
    }
    private var heading: some View {
        VStack(alignment: .leading, spacing: 6) {
            Eyebrow(text: "Discover")
            Text(title.uppercased()).font(StudioTypography.discoverTitle).tracking(1.2).foregroundStyle(palette.heading)
                .lineLimit(1).accessibilityLabel(title).accessibilityAddTraits(.isHeader)
        }
        .fixedSize()
    }
}

/// “Add a Config”: the page's one prominent action, a pull-down of every import source.
struct AddConfigButton: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @State private var anchor = MenuAnchorBox()
    var body: some View {
        Button { show() } label: {
            HStack(spacing: 8) {
                Image(systemName: "plus").accessibilityHidden(true)
                Text("Add a Config")
                Image(systemName: "chevron.down").font(.system(size: 10, weight: .bold)).accessibilityHidden(true)
            }
        }
        .prominentAction().actionControlSize().fixedSize()
        .background(MenuAnchor(box: anchor))
        .disabled(discover.studio.session == nil)
        .accessibilityLabel("Add a Config")
        .accessibilityHint("Shows the places you can add a config from")
        .help("Add a config from a file, a link, your Windows install or the clipboard")
    }
    private func show() {
        guard let view = anchor.view else { return }
        NativeMenu.popUp([
            .init(title: "From a File…", symbol: "doc", key: "o", modifiers: .command) { discover.chooseFile() },
            .init(title: "From a Link or GitHub…", symbol: "link") { discover.step = .link("") },
            .init(title: "From This Mac’s Server (localhost)…", symbol: "desktopcomputer") { discover.step = .link("http://localhost:") },
            .init(title: "From Your Windows Install…", symbol: "pc") { discover.step = .windows },
            .init(title: "Paste", symbol: "doc.on.clipboard", key: "v", modifiers: [.command, .shift], separatorBefore: true) { discover.importPasteboard() },
        ], below: view)
    }
}

final class MenuAnchorBox { weak var view: NSView? }
struct MenuAnchor: NSViewRepresentable {
    let box: MenuAnchorBox
    func makeNSView(context: Context) -> NSView { let view = NSView(); box.view = view; return view }
    func updateNSView(_ view: NSView, context: Context) { box.view = view }
}

/// A real AppKit menu under a control: native keyboard, VoiceOver and appearance.
enum NativeMenu {
    struct Item {
        var title: String
        var symbol: String
        var key = ""
        var modifiers: NSEvent.ModifierFlags = []
        var separatorBefore = false
        var action: @MainActor () -> Void
    }
    @MainActor private final class Target: NSObject {
        var actions: [@MainActor () -> Void] = []
        @objc func run(_ sender: NSMenuItem) { actions[sender.tag]() }
    }
    @MainActor static func popUp(_ items: [Item], below view: NSView) {
        let menu = NSMenu(), target = Target()
        target.actions = items.map(\.action)
        for (index, item) in items.enumerated() {
            if item.separatorBefore { menu.addItem(.separator()) }
            let row = NSMenuItem(title: item.title, action: #selector(Target.run(_:)), keyEquivalent: item.key)
            row.keyEquivalentModifierMask = item.modifiers
            row.target = target; row.tag = index
            row.image = NSImage(systemSymbolName: item.symbol, accessibilityDescription: nil)
            menu.addItem(row)
        }
        // Tracking is synchronous, so the target outlives the menu.
        menu.popUp(positioning: nil, at: NSPoint(x: 0, y: view.isFlipped ? view.bounds.height + 6 : -6), in: view)
        withExtendedLifetime(target) {}
    }
}

private struct DiscoverSearchField: View {
    @Binding var text: String
    @FocusState private var focused: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: "magnifyingglass").foregroundStyle(palette.secondary).accessibilityHidden(true)
            TextField("Search a player, team or style", text: $text).textFieldStyle(.plain).focused($focused)
                .onExitCommand { text = ""; focused = false }
                .accessibilityLabel("Search players")
            if !text.isEmpty {
                Button { text = "" } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(palette.tertiary).accessibilityLabel("Clear search")
            }
        }
        .padding(.horizontal, 14).frame(width: 260, height: ControlMetrics.action)
        .background(palette.dark ? Color.black.opacity(0.22) : Color.white.opacity(0.6), in: Capsule())
        .overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1))
        .help("Search players by name, team, role or style (⌘F)")
        .background { Button("") { focused = true }.keyboardShortcut("f", modifiers: .command).opacity(0).accessibilityHidden(true) }
    }
}

// MARK: - Players

private struct PlayersPage: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    @Environment(\.palette) private var palette

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                DiscoverHeader(title: "Players") {
                    HStack(spacing: 10) {
                        if discover.snapshot.map({ !$0.index.entries.isEmpty }) == true { DiscoverSearchField(text: $discover.query) }
                        AddConfigButton(discover: discover)
                    }
                }
                content
            }
            .padding(.top, 82).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 28)
        }
        .scrollIndicators(.automatic)
    }

    @ViewBuilder private var content: some View {
        switch discover.state {
        case .idle, .loading:
            HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Looking for shared setups…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                .frame(maxWidth: .infinity).padding(.vertical, 60).accessibilityElement(children: .combine)
        case .unavailable:
            StudioEmptyState(title: "NO PLAYERS YET",
                             message: "Shared setups from players aren’t available yet. Each one needs the player’s permission and a signed catalog entry before it appears here. You can still bring in a config of your own.",
                             action: ("Add a Config from a File…", { discover.chooseFile() }))
                .frame(maxWidth: .infinity)
        case .failed(let message):
            StudioEmptyState(title: "OUT OF REACH", message: "The player directory couldn’t be checked. \(message)",
                             action: ("Try Again", { Task { await discover.reload() } }))
                .frame(maxWidth: .infinity)
        case .loaded(let snapshot, let refreshError):
            if let refreshError {
                StudioBanner(title: "Showing the copy saved on this Mac.", detail: "The directory couldn’t be refreshed: \(refreshError)", tone: .information)
            }
            if snapshot.index.entries.isEmpty {
                StudioEmptyState(title: "NO PLAYERS YET", message: "The signed catalog doesn’t list any shared setups yet. You can still bring in a config of your own.",
                                 action: ("Add a Config from a File…", { discover.chooseFile() }))
                    .frame(maxWidth: .infinity)
            } else if discover.shelves.isEmpty {
                StudioEmptyState(title: "NO ONE BY THAT NAME", message: "Try a team, a role or a style.")
                    .frame(maxWidth: .infinity)
            } else {
                ForEach(discover.shelves) { shelf in
                    VStack(alignment: .leading, spacing: 12) {
                        VStack(alignment: .leading, spacing: 3) {
                            Text(shelf.id).font(.system(size: 18, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                            Text(shelf.entries.count == 1 ? "1 setup" : "\(shelf.entries.count) setups").font(StudioTypography.help).foregroundStyle(palette.secondary)
                        }
                        PlayerGrid(entries: shelf.entries, discover: discover, artwork: artwork)
                    }
                }
                Label("Every setup here is shared with its player’s permission. You still review every line before anything changes.", systemImage: "checkmark.seal")
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

private struct PlayerGrid: View {
    var entries: [ConfigDirectoryEntry]
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 230), spacing: 14, alignment: .top)], alignment: .leading, spacing: 14) {
            ForEach(entries) { entry in
                PlayerCard(entry: entry, saved: discover.isSaved(entry.id), image: entry.favoriteMap.flatMap { artwork.image(for: $0) }) {
                    discover.open(.player(entry.id))
                }
            }
        }
    }
}

enum PlayerInitials {
    /// “[Player 1]” → “P1”: first letters of the first two words, digits kept.
    static func of(_ name: String) -> String {
        let words = name.split(whereSeparator: { !$0.isLetter && !$0.isNumber }).prefix(2)
        let text = words.map { word in word.allSatisfy(\.isNumber) ? String(word) : String(word.prefix(1)) }.joined().uppercased()
        return text.isEmpty ? "?" : text
    }
}

private struct PlayerCard: View {
    var entry: ConfigDirectoryEntry
    var saved: Bool
    var image: NSImage?
    var open: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false

    var body: some View {
        Button(action: open) {
            VStack(alignment: .leading, spacing: 0) {
                ZStack(alignment: .bottomLeading) {
                    MapArt(map: entry.favoriteMap ?? "", image: image).frame(height: 104).clipped()
                    Text(PlayerInitials.of(entry.player)).font(.stencil(18)).foregroundStyle(palette.accent)
                        .frame(width: 40, height: 40)
                        .background(palette.dark ? Palette.hex(0x1b1f17) : palette.crate, in: Circle())
                        .overlay(Circle().strokeBorder(palette.panel, lineWidth: 2))
                        .overlay(Circle().stroke(palette.selectionStroke, lineWidth: 1).padding(-1))
                        .offset(x: 14, y: 18)
                        .accessibilityHidden(true)
                }
                .zIndex(1)
                VStack(alignment: .leading, spacing: 6) {
                    HStack(spacing: 6) {
                        Text(entry.player).font(.system(size: 15, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1)
                        Spacer(minLength: 4)
                        if saved { Image(systemName: "bookmark.fill").font(.system(size: 11)).foregroundStyle(palette.accent).accessibilityHidden(true) }
                    }
                    Text([entry.team, entry.role].compactMap { $0 }.joined(separator: " · ")).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
                    Text(entry.feel).font(.system(size: 13)).italic().foregroundStyle(palette.dark ? Palette.hex(0xe2dbc9) : palette.text)
                        .fixedSize(horizontal: false, vertical: true)
                    if !entry.styleTags.isEmpty { FlowChips(tags: entry.styleTags) }
                }
                .padding(.top, 26).padding(.horizontal, 16).padding(.bottom, 16)
            }
            // Cards in a row share the tallest card's height.
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(palette.panel)
            .clipShape(RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous)
                .stroke(hovering ? palette.selectionStroke : palette.panelStroke, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous))
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel([entry.player, entry.team, entry.role].compactMap { $0 }.joined(separator: ", ") + ". " + entry.feel + (saved ? " Saved." : ""))
        .accessibilityHint("Shows what this setup changes for you")
        .accessibilityAddTraits(.isButton)
    }
}

/// Style chips that wrap onto more lines rather than truncating.
struct FlowChips: View {
    var tags: [String]
    @Environment(\.palette) private var palette
    var body: some View {
        ChipFlow(spacing: 5) {
            ForEach(tags, id: \.self) { tag in
                Text(tag).font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.chipText).lineLimit(1)
                    .padding(.horizontal, 9).frame(height: 22)
                    .background(palette.dark ? Color.white.opacity(0.06) : Color.white.opacity(0.55), in: Capsule())
                    .overlay(Capsule().stroke(palette.dark ? Color.white.opacity(0.12) : palette.wellStroke, lineWidth: 1))
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Style: " + tags.joined(separator: ", "))
    }
}

private struct ChipFlow: Layout {
    var spacing: CGFloat
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let rows = arrange(width: proposal.width ?? .infinity, subviews: subviews)
        return CGSize(width: rows.map(\.width).max() ?? 0, height: rows.reduce(0) { $0 + $1.height } + spacing * CGFloat(max(0, rows.count - 1)))
    }
    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for row in arrange(width: bounds.width, subviews: subviews) {
            var x = bounds.minX
            for index in row.items {
                let size = subviews[index].sizeThatFits(.unspecified)
                subviews[index].place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(size))
                x += size.width + spacing
            }
            y += row.height + spacing
        }
    }
    private func arrange(width: CGFloat, subviews: Subviews) -> [(items: [Int], width: CGFloat, height: CGFloat)] {
        var rows: [(items: [Int], width: CGFloat, height: CGFloat)] = []
        for (index, view) in subviews.enumerated() {
            let size = view.sizeThatFits(.unspecified)
            if let last = rows.last, last.width + spacing + size.width <= width {
                rows[rows.count - 1] = (last.items + [index], last.width + spacing + size.width, max(last.height, size.height))
            } else {
                rows.append(([index], size.width, size.height))
            }
        }
        return rows
    }
}

// MARK: - Saved

private struct SavedPage: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    @Environment(\.palette) private var palette

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                DiscoverHeader(title: "Saved") { AddConfigButton(discover: discover) }
                let verified = discover.saved.compactMap(\.entry)
                let gone = discover.saved.filter { $0.entry == nil }
                if discover.saved.isEmpty {
                    StudioEmptyState(title: "NOTHING SAVED",
                                     message: discover.snapshot == nil
                                        ? "Players you save appear here. The player directory isn’t available yet, so there’s nothing to save."
                                        : "Players you save appear here, so you can come back to their setups.",
                                     action: ("Browse Players", { discover.open(.players) }))
                        .frame(maxWidth: .infinity)
                } else {
                    if !verified.isEmpty { PlayerGrid(entries: verified, discover: discover, artwork: artwork) }
                    if !gone.isEmpty {
                        VStack(alignment: .leading, spacing: 0) {
                            ForEach(Array(gone.enumerated()), id: \.element.id) { index, item in
                                HStack(spacing: 12) {
                                    Image(systemName: "bookmark.slash").foregroundStyle(palette.secondary).accessibilityHidden(true)
                                    VStack(alignment: .leading, spacing: 3) {
                                        Text("No longer in the directory").font(.system(size: 14)).foregroundStyle(palette.text)
                                        Text("This saved setup isn’t in the signed catalog any more, so its details can’t be shown.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                                    }
                                    Spacer(minLength: 8)
                                    Button("Remove") { discover.setSaved(item.id, false) }.glassAction()
                                        .accessibilityLabel("Remove this saved setup")
                                }
                                .padding(.vertical, 12)
                                .overlay(alignment: .top) { if index > 0 { Rectangle().fill(palette.separator).frame(height: 1) } }
                            }
                        }
                        .padding(.horizontal, StudioMetrics.panelInset)
                        .background(palette.panel, in: RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous))
                        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
                    }
                }
            }
            .padding(.top, 82).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 28)
        }
    }
}

// MARK: - Player detail

private struct PlayerDetailPage: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    var entryID: String
    @Environment(\.palette) private var palette
    @State private var width: CGFloat = 0

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Button { discover.open(.players) } label: { Label("Players", systemImage: "chevron.left").font(.system(size: 13, weight: .semibold)) }
                    .buttonStyle(.plain).foregroundStyle(palette.link).frame(height: 28)
                    .keyboardShortcut("[", modifiers: .command)
                    .help("Back to Players (⌘[)")
                if let entry = discover.entry(entryID) {
                    PlayerHero(entry: entry, saved: discover.isSaved(entry.id), image: entry.favoriteMap.flatMap { artwork.image(for: $0) },
                               canTry: discover.studio.session != nil && !discover.studio.isReadOnly,
                               canCompare: discover.studio.session != nil && discover.canCompare(entry.id),
                               tryIt: { discover.tryEntry(entry) }, compare: { discover.compareInExpert(entry) },
                               toggleSaved: { discover.setSaved(entry.id, !discover.isSaved(entry.id)) })
                    // Two columns (1.25 : 1, as drawn) when there's room; one column at the minimum window size.
                    Group {
                        if width >= 760 {
                            HStack(alignment: .top, spacing: 16) {
                                VStack(spacing: 16) { StoryPanel(entry: entry); ChangesPanel(discover: discover) }.frame(maxWidth: .infinity)
                                InSetupPanel(entry: entry, discover: discover).frame(width: (width - 16) / 2.25)
                            }
                        } else {
                            VStack(spacing: 16) { StoryPanel(entry: entry); ChangesPanel(discover: discover); InSetupPanel(entry: entry, discover: discover) }
                        }
                    }
                    .frame(maxWidth: .infinity)
                    .onGeometryChange(for: CGFloat.self, of: { $0.size.width }) { width = $0 }
                } else {
                    StudioEmptyState(title: "NOT IN THE DIRECTORY", message: "This setup isn’t in the signed catalog any more.",
                                     action: ("Browse Players", { discover.open(.players) }))
                        .frame(maxWidth: .infinity)
                }
            }
            .padding(.top, 76).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 28)
        }
        .onAppear { if discover.detail?.entryID != entryID { discover.loadDetail(entryID) } }
    }
}

private struct PlayerHero: View {
    var entry: ConfigDirectoryEntry
    var saved: Bool
    var image: NSImage?
    var canTry: Bool
    var canCompare: Bool
    var tryIt: () -> Void
    var compare: () -> Void
    var toggleSaved: () -> Void
    // Text sits on the scrim over the art, so it keeps the dark-appearance inks in both appearances.
    private let ink = Palette.dark

    var body: some View {
        ZStack(alignment: .bottomLeading) {
            MapArt(map: entry.favoriteMap ?? "", image: image, hero: true)
            // A scrim strong enough for the light inks over the pale and the dark scenes alike.
            LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: .black.opacity(0.5), location: 0.35), .init(color: .black.opacity(0.85), location: 1)],
                           startPoint: .top, endPoint: .bottom)
                .accessibilityHidden(true)
            ViewThatFits(in: .horizontal) {
                HStack(alignment: .bottom, spacing: 18) { identity; Spacer(minLength: 12); actions }
                VStack(alignment: .leading, spacing: 14) { identity; actions }
            }
            .padding(.horizontal, 28).padding(.bottom, 24)
        }
        .frame(height: 250)
        .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 24, style: .continuous).stroke(Color.white.opacity(0.08), lineWidth: 1))
        .shapeAudit(.container, .rounded(24), "player hero")
    }

    private var identity: some View {
        HStack(alignment: .bottom, spacing: 18) {
            Text(PlayerInitials.of(entry.player)).font(.stencil(34)).foregroundStyle(ink.accent)
                .frame(width: 78, height: 78)
                .background(Palette.hex(0x1b1f17), in: Circle())
                .overlay(Circle().strokeBorder(Palette.hex(0x0d0f0c), lineWidth: 3))
                .overlay(Circle().stroke(ink.accent.opacity(0.6), lineWidth: 1))
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 6) {
                Eyebrow(text: "\(entry.era) · \(entry.role)", color: Palette.hex(0xf2d394))
                Text(entry.player.uppercased()).font(.stencil(56)).tracking(1.1).foregroundStyle(ink.heading).lineLimit(1).minimumScaleFactor(0.6)
                    .accessibilityLabel(entry.player).accessibilityAddTraits(.isHeader)
                Text(entry.feel).font(.system(size: 14)).italic().foregroundStyle(Palette.hex(0xe2dbc9)).lineLimit(2)
            }
        }
    }

    private var actions: some View {
        GlassContainer(spacing: 8) {
            HStack(spacing: 8) {
                Button("Try This Setup", action: tryIt).prominentAction().actionControlSize().disabled(!canTry)
                    .help("Review this setup line by line. Trying it makes a new loadout; your others stay as they are.")
                // Available once their setup is downloaded and reviewed below; comparing writes nothing.
                Button("Compare in Expert", action: compare).glassAction().actionControlSize().disabled(!canCompare)
                    .help("See their values beside yours in Expert, setting by setting.")
                Button(action: toggleSaved) {
                    Image(systemName: saved ? "bookmark.fill" : "bookmark").font(.system(size: 15)).frame(width: 16, height: 16)
                }
                .glassAction(circle: true).actionControlSize()
                .accessibilityLabel(saved ? "Remove from Saved" : "Save for later")
                .help(saved ? "Remove from Saved" : "Save for later")
            }
        }
        .fixedSize()
    }
}

private struct DetailHeading: View {
    var title: String
    var trailing: String? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            Spacer(minLength: 8)
            if let trailing { Text(trailing).font(StudioTypography.help).foregroundStyle(palette.secondary) }
        }
    }
}

private struct StoryPanel: View {
    var entry: ConfigDirectoryEntry
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 18, leading: 20, bottom: 18, trailing: 20)) {
            VStack(alignment: .leading, spacing: 12) {
                DetailHeading(title: "Story")
                Text(entry.story).font(.system(size: 14)).lineSpacing(4).foregroundStyle(palette.dark ? Palette.hex(0xe2dbc9) : palette.text)
                    .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
                Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 8) {
                    GridRow {
                        fact("Era", entry.era)
                        fact("Team", entry.team ?? "None listed")
                        fact("Role", entry.role)
                    }
                    if let map = entry.favoriteMap { GridRow { fact("Favorite map", MapCatalog.displayName(map)) } }
                }
                .padding(.top, 4)
                VStack(alignment: .leading, spacing: 6) {
                    // One line when it fits; otherwise the credit line breaks after the source.
                    ViewThatFits(in: .horizontal) {
                        HStack(spacing: 8) { permission; Dot(); credit }
                        VStack(alignment: .leading, spacing: 4) { permission; credit }
                    }
                    .font(StudioTypography.help)
                    Text("Permission from \(entry.permission.grantedBy) on \(entry.permission.grantedAt). The signature shows this entry came from the catalog unchanged; it doesn’t make a config safe, so you still review every line.")
                        .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    Link("See the permission record", destination: entry.permission.evidenceURL).font(StudioTypography.help).foregroundStyle(palette.link)
                }
                .padding(.top, 4)
            }
        }
    }
    private var permission: some View {
        HStack(spacing: 8) {
            Image(systemName: "checkmark.seal").foregroundStyle(palette.positive).accessibilityHidden(true)
            Text("Shared with permission").foregroundStyle(palette.secondary)
            Dot()
            Link("Source", destination: entry.sourceURL).foregroundStyle(palette.link)
        }
        .fixedSize()
    }
    private var credit: some View {
        HStack(spacing: 8) {
            Text("Credit: \(entry.credit)").foregroundStyle(palette.secondary)
            Dot()
            Text("Signed catalog entry").foregroundStyle(palette.secondary)
        }
        .fixedSize()
    }
    private func fact(_ key: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(key).font(.system(size: 11)).foregroundStyle(palette.tertiary)
            Text(value).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(2)
        }
        .accessibilityElement(children: .combine)
    }
}

private struct Dot: View {
    @Environment(\.palette) private var palette
    var body: some View { Circle().fill(palette.tertiary).frame(width: 3, height: 3).accessibilityHidden(true) }
}

private struct ChangesPanel: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 18, leading: 20, bottom: 8, trailing: 20)) {
            switch discover.detail?.phase {
            case .ready(let summary):
                DetailHeading(title: "What changes for you", trailing: summary.comparedWith.map { "Compared with \($0)" }).padding(.bottom, 6)
                ForEach(Array(summary.changes.enumerated()), id: \.element.id) { index, change in
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: change.locked ? "lock" : "arrow.left.arrow.right").font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(change.locked ? palette.chipIcon : palette.accent)
                            .frame(width: 30, height: 30)
                            .background(change.locked ? (palette.dark ? Color.white.opacity(0.06) : palette.crate) : palette.selection,
                                        in: RoundedRectangle(cornerRadius: 9, style: .continuous))
                            .accessibilityHidden(true)
                        VStack(alignment: .leading, spacing: 3) {
                            Text(change.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                            Text(change.detail).font(StudioTypography.help).foregroundStyle(palette.secondary)
                        }
                        .fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(.vertical, 12)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .overlay(alignment: .top) { if index > 0 { Rectangle().fill(palette.separator).frame(height: 1) } }
                    .accessibilityElement(children: .combine)
                }
                if summary.more > 0 {
                    Text(summary.more == 1 ? "And 1 more change. Try the setup to see every line." : "And \(summary.more) more changes. Try the setup to see every line.")
                        .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.bottom, 10)
                }
            case .failed(let message):
                DetailHeading(title: "What changes for you").padding(.bottom, 8)
                StudioBanner(title: "This setup couldn’t be checked.", detail: message, tone: .negative)
                Button("Try Again") { if let id = discover.detail?.entryID { discover.loadDetail(id) } }.glassAction().padding(.vertical, 10)
            default:
                DetailHeading(title: "What changes for you").padding(.bottom, 8)
                HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Checking what this setup changes…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                    .padding(.vertical, 12).accessibilityElement(children: .combine)
            }
        }
    }
}

private struct InSetupPanel: View {
    var entry: ConfigDirectoryEntry
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 18, leading: 20, bottom: 18, trailing: 20)) {
            VStack(alignment: .leading, spacing: 14) {
                DetailHeading(title: "In this setup")
                if case .ready(let summary)? = discover.detail?.phase {
                    VStack(alignment: .leading, spacing: 8) {
                        count(summary.settings, one: "setting", many: "settings", symbol: "slider.horizontal.3")
                        count(summary.keys, one: "key binding", many: "key bindings", symbol: "keyboard")
                        count(summary.needsOK, one: "line needs your OK", many: "lines need your OK", symbol: "exclamationmark.triangle")
                        count(summary.never, one: "line is never imported", many: "lines are never imported", symbol: "nosign")
                    }
                } else {
                    Text("Counts appear once the setup is checked.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                }
                if !entry.styleTags.isEmpty { FlowChips(tags: entry.styleTags) }
                if entry.platform == .windows {
                    Text("Made on Windows. Display and driver choices stay with this Mac.")
                        .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
                Text(discover.studio.activeProfile.map { "Trying it makes a new loadout. \($0.displayName) stays exactly as it is." } ?? "Trying it makes a new loadout.")
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
    private func count(_ value: Int, one: String, many: String, symbol: String) -> some View {
        Label { Text("\(value) \(value == 1 ? one : many)").monospacedDigit() } icon: { Image(systemName: symbol).foregroundStyle(palette.chipIcon) }
            .font(.system(size: 13)).foregroundStyle(palette.text)
    }
}
