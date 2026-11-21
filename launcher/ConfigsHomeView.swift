import SwiftUI
import AppKit

// Discover › Player Setups: one featured setup at a time in a banner, a shelf for each style, the
// teams, and two banners that lead to the person's own configs. See All, a filter or a search shows
// a grid instead. Every card, credit and source comes from the verified index; nothing is invented here.

struct ConfigsHomePage: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    init(model: LauncherModel, discover: ConfigDiscoverModel) { self.model = model; self.discover = discover; artwork = model.artwork }

    /// A search or See All shows a grid; otherwise the front page.
    private var grid: Bool { discover.searching || discover.browsing }
    private var chosen: ConfigSetupFilter { discover.filters.contains(discover.filter) ? discover.filter : .all }

    var body: some View {
        GeometryReader { proxy in
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    if grid, discover.snapshot != nil { back }
                    DiscoverHeader(title: title, subtitle: subtitle) { if !grid { ShareYourSetupButton(model: model) } }
                    content(wide: proxy.size.width >= 700)
                }
                .padding(.top, grid && discover.snapshot != nil ? 76 : 82).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 24)
                // A short page still fills the window, so the note about the catalogue sits at the bottom.
                .frame(minHeight: proxy.size.height, alignment: .top)
            }
            .scrollIndicators(.automatic)
        }
        .background { Button("") { discover.requestFind() }.keyboardShortcut("f", modifiers: .command).opacity(0).accessibilityHidden(true) }
        .task { await discover.loadIfNeeded() }
    }

    private var title: String {
        guard discover.snapshot != nil else { return "Player Setups" }
        return discover.searching ? "Results" : discover.browsing ? chosen.title : "Player Setups"
    }
    private var subtitle: String? {
        guard grid, let total = discover.snapshot?.index.entries.count, total > 0 else { return nil }
        let shown = discover.setups.count, setups = total == 1 ? "1 setup" : "\(total) setups"
        if discover.searching {
            let query = discover.query.trimmingCharacters(in: .whitespaces)
            return shown == 0 ? "None of \(setups)" : "\(shown) of \(setups) \(shown == 1 ? "matches" : "match") “\(query)”"
        }
        return chosen == .all ? "All \(setups)" : "\(shown) of \(setups)"
    }
    private var back: some View {
        Button { discover.openFront() } label: { Label("Player Setups", systemImage: "chevron.left").font(.system(size: 13, weight: .semibold)) }
            .buttonStyle(.plain).foregroundStyle(palette.link).frame(height: 28)
            .padding(.top, -6).padding(.bottom, -12)
            .help("Back to Player Setups")
    }

    @ViewBuilder private func content(wide: Bool) -> some View {
        switch discover.state {
        case .idle, .loading:
            loading
        case .unavailable(let message):
            unreachable(title: "No player setups in this build", message: "\(message) Your own loadouts, Add a Config and Check a Config work without them.", retry: false)
            promos(wide: wide)
            Spacer(minLength: 0)
            footer
        case .failed(let message):
            unreachable(title: "The catalogue couldn’t be reached",
                        message: "Player setups come from the catalogue on GitHub. \(message) Your own loadouts, Add a Config and Check a Config work without it.", retry: true)
            promos(wide: wide)
            Spacer(minLength: 0)
            footer
        case .loaded(let snapshot, let refreshError):
            if refreshError != nil {
                Banner(.neutral, message: "Couldn’t reach the catalogue just now. Showing the copy saved\(discover.fetched.map { " on \($0.formatted(.dateTime.day().month(.wide)))" } ?? " on this Mac").") {
                    Button("Try Again") { Task { await discover.reload() } }
                }
            }
            if snapshot.index.entries.isEmpty {
                StudioEmptyState(title: "No setups yet", message: "The catalogue doesn’t list any player setups yet. You can still bring in a config of your own.")
                    .frame(maxWidth: .infinity)
                promos(wide: wide)
            } else if grid {
                if !discover.searching, discover.filters.count > 1 { filters }
                if discover.setups.isEmpty {
                    EmptyState("No setup matches “\(discover.query.trimmingCharacters(in: .whitespaces))”",
                               message: "Try a team, an era or where it was published. Search looks through \(snapshot.index.entries.count) setups.",
                               actionTitle: "Clear Search", prominent: false) { discover.query = "" }
                        .padding(.vertical, 40).frame(maxWidth: .infinity)
                } else {
                    PlayerGrid(entries: discover.setups, discover: discover, artwork: artwork)
                }
            } else {
                FeaturedBanner(entries: discover.heroes, discover: discover, artwork: artwork)
                ForEach(discover.styleShelves) { shelf in SetupShelf(shelf: shelf, discover: discover, artwork: artwork) }
                if !discover.teams.isEmpty { teams }
                promos(wide: wide)
            }
            Spacer(minLength: 0)
            footer
        }
    }

    // MARK: Grid

    /// The styles as chips. They scroll sideways when the page is too narrow for all of them.
    private var filters: some View {
        ScrollView(.horizontal) {
            HStack(spacing: 8) {
                ForEach(discover.filters) { filter in
                    Button(filter.rawValue) { discover.showAll(filter) }
                        .filterChip(on: chosen == filter)
                        .accessibilityAddTraits(chosen == filter ? .isSelected : [])
                }
            }
            .padding(.vertical, 2)
        }
        .scrollIndicators(.never)
        .padding(.top, -8)
        .accessibilityElement(children: .contain).accessibilityLabel("Narrow the setups")
    }

    // MARK: Front page

    private var teams: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Teams").font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                Text("Every setup published under a team’s name.").font(.system(size: 13)).foregroundStyle(palette.secondary)
            }
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 160), spacing: 12, alignment: .top)], alignment: .leading, spacing: 12) {
                ForEach(discover.teams) { team in TeamTile(team: team) { discover.showTeam(team.name) } }
            }
        }
    }

    /// The two ways in that need no catalogue: bringing a config, and checking one.
    private func promos(wide: Bool) -> some View {
        let add = PromoBanner(eyebrow: "Your own config", title: "Bring the config you already play with", symbol: "square.and.arrow.down",
                              message: "From a file, a link, your Windows install or the clipboard. You review every line before it becomes a loadout.") {
            AddConfigButton(discover: discover, prominent: false)
        }
        let check = PromoBanner(eyebrow: "Before a match", title: "Check a config against a server’s rules", symbol: "checkmark.shield",
                                message: "See which lines the rules would refuse and fix them, without adding anything to your loadouts.") {
            Button("Check a Config") { discover.open(.checker) }.glassAction().actionControlSize()
                .help("Check a config without adding it to your loadouts")
        }
        return Group {
            if wide { HStack(alignment: .top, spacing: 14) { add; check }.fixedSize(horizontal: false, vertical: true) } else { VStack(spacing: 14) { add; check } }
        }
    }

    // MARK: States

    private var loading: some View {
        VStack(alignment: .leading, spacing: 24) {
            HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Getting the catalogue…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                .padding(.top, -8).accessibilityElement(children: .combine)
            RoundedRectangle(cornerRadius: 24, style: .continuous).fill(palette.panel).frame(height: FeaturedBanner.height).opacity(0.9).accessibilityHidden(true)
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 236), spacing: 14, alignment: .top)], alignment: .leading, spacing: 14) {
                ForEach(0..<3, id: \.self) { index in
                    RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous).fill(palette.panel)
                        .frame(height: 250).opacity(0.8 - Double(index) * 0.25)
                }
            }
            .accessibilityHidden(true)
        }
    }
    private func unreachable(title: String, message: String, retry: Bool) -> some View {
        EmptyState(title, message: message, actionTitle: retry ? "Try Again" : nil) { Task { await discover.reload() } }
            .padding(.top, 28).padding(.bottom, 8).frame(maxWidth: .infinity)
    }
    private var footer: some View {
        // The note wraps beside the links, which keep their one line at the trailing edge.
        HStack(alignment: .firstTextBaseline, spacing: 16) { provenance.frame(maxWidth: .infinity, alignment: .leading); links }
            .font(StudioTypography.help).padding(.top, 6)
    }
    private var provenance: some View {
        Text("Setups are collected from public pages and credited to where they were published. Passwords and personal details are removed.")
            .foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
    }
    private var links: some View {
        HStack(spacing: 14) {
            Link("Catalogue on GitHub", destination: ConfigDirectory.repositoryURL)
            Link("Ask to remove or correct a setup", destination: ConfigDirectory.removalURL)
        }
        .foregroundStyle(palette.link).fixedSize()
    }
}

/// “68 setups · updated today” with Refresh: a toolbar pill, as Servers shows its list's count.
struct CatalogueStatus: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    var body: some View {
        let loading: Bool = { if case .loading = discover.state { return true }; return false }()
        HStack(spacing: 6) {
            Text(discover.status()).font(.system(size: 12)).monospacedDigit().foregroundStyle(palette.secondary).lineLimit(1).fixedSize()
                .accessibilityAddTraits(.isStaticText)
                .padding(.leading, 10)
            Button { Task { await discover.reload() } } label: {
                Image(systemName: "arrow.clockwise").font(.system(size: 13, weight: .semibold)).frame(width: 18, height: 18)
            }
            .disabled(loading)
            .accessibilityLabel("Refresh the catalogue").help("Check the catalogue for new setups")
        }
        .fixedSize()
    }
}

// MARK: - Featured banner

/// One featured setup at a time over its picture, with arrows and dots to page through the rest.
private struct FeaturedBanner: View {
    static let height: CGFloat = 300
    var entries: [ConfigDirectoryEntry]
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    @State private var index = 0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    // Text sits on the scrim over the art, so it keeps the dark-appearance inks in both appearances.
    private let ink = Palette.dark

    var body: some View {
        let position = min(index, max(0, entries.count - 1))
        if entries.indices.contains(position) {
            let entry = entries[position]
            ZStack(alignment: .bottomLeading) {
                MapArtFrame(map: entry.artMap, image: artwork.image(for: entry.artMap), size: .fill, hero: true)
                    .id(entry.id).transition(.opacity)
                LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: .black.opacity(0.5), location: 0.42), .init(color: .black.opacity(0.88), location: 1)],
                               startPoint: .top, endPoint: .bottom)
                    .accessibilityHidden(true)
                ViewThatFits(in: .horizontal) {
                    HStack(alignment: .bottom, spacing: 18) { identity(entry, position); Spacer(minLength: 12); actions(entry) }
                    VStack(alignment: .leading, spacing: 14) { identity(entry, position); actions(entry) }
                }
                .padding(.horizontal, 28).padding(.bottom, entries.count > 1 ? 32 : 24)
            }
            .frame(height: Self.height)
            .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 24, style: .continuous).stroke(Color.white.opacity(0.08), lineWidth: 1))
            // The paging arrows sit together in the top corner, as a shelf's do beside its title, clear of the banner's own corner.
            .overlay(alignment: .topTrailing) {
                if entries.count > 1 {
                    // Glass over the art keeps the dark look in both appearances, so the arrows stay readable.
                    HStack(spacing: 8) { arrow("chevron.left", "Previous featured setup", -1); arrow("chevron.right", "Next featured setup", 1) }.padding(24)
                        .environment(\.palette, .dark).environment(\.colorScheme, .dark)
                }
            }
            .overlay(alignment: .bottom) { if entries.count > 1 { dots(position).padding(.bottom, 12) } }
            .shapeAudit(.container, .rounded(24), "featured banner")
            .accessibilityElement(children: .contain)
            .accessibilityLabel("Featured setups")
        }
    }

    private func identity(_ entry: ConfigDirectoryEntry, _ position: Int) -> some View {
        HStack(alignment: .bottom, spacing: 18) {
            Text(PlayerInitials.of(entry.player)).font(.stencil(34)).foregroundStyle(ink.accent)
                .frame(width: 78, height: 78)
                .background(ink.tileFill, in: Circle())
                .overlay(Circle().strokeBorder(ink.ground, lineWidth: 3))
                .overlay(Circle().stroke(ink.accent.opacity(0.6), lineWidth: 1))
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 6) {
                Eyebrow(text: (["Featured setup"] + entry.styleTags.prefix(1)).joined(separator: " · "), color: Palette.hex(0xf2d394))
                Text(entry.player.uppercased()).font(.stencil(56)).tracking(1.1).foregroundStyle(ink.heading).lineLimit(1).minimumScaleFactor(0.5)
                Text(entry.feel).font(.system(size: 14)).italic().foregroundStyle(ink.chipText).lineLimit(2)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("Featured setup \(position + 1) of \(entries.count): \(entry.player)\(entry.team.map { ", \($0)" } ?? ""). \(entry.feel)")
            .accessibilityAddTraits(.isHeader)
        }
    }

    private func actions(_ entry: ConfigDirectoryEntry) -> some View {
        GlassContainer(spacing: 8) {
            HStack(spacing: 8) {
                Button("See This Setup") { discover.open(.player(entry.id)) }.prominentAction().actionControlSize()
                    .help("Shows what this setup changes for you")
                let saved = discover.isSaved(entry.id)
                Button { discover.setSaved(entry.id, !saved) } label: {
                    Image(systemName: saved ? "bookmark.fill" : "bookmark").font(.system(size: 15)).frame(width: 16, height: 16)
                }
                .glassAction(circle: true).actionControlSize()
                .accessibilityLabel(saved ? "Remove from Saved" : "Save for later")
                .help(saved ? "Remove from Saved" : "Save for later")
            }
        }
        .fixedSize()
    }

    private func arrow(_ symbol: String, _ label: String, _ step: Int) -> some View {
        Button {
            withAnimation(reduceMotion ? .easeInOut(duration: 0.15) : .easeInOut(duration: 0.3)) { index = (min(index, entries.count - 1) + step + entries.count) % entries.count }
        } label: {
            Image(systemName: symbol).font(.system(size: 13, weight: .semibold)).frame(width: 16, height: 16)
        }
        .glassAction(circle: true).actionControlSize()
        .accessibilityLabel(label).help(label)
    }

    private func dots(_ position: Int) -> some View {
        HStack(spacing: 6) {
            ForEach(entries.indices, id: \.self) { dot in
                Capsule().fill(dot == position ? ink.accent : Color.white.opacity(0.38)).frame(width: dot == position ? 18 : 6, height: 6)
            }
        }
        .accessibilityHidden(true)
    }
}

// MARK: - Shelf

/// One style: its first setups in a row that scrolls sideways, and See All for the rest.
private struct SetupShelf: View {
    var shelf: ConfigStyleShelf
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    /// The card the arrows last brought to the leading edge. A mouse wheel can't scroll sideways.
    @State private var first = 0
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    private static let page = 3

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .bottom, spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(shelf.filter.rawValue).font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                    Text(shelf.filter.note).font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
                Spacer(minLength: 12)
                Button("See All \(shelf.total)") { discover.showAll(shelf.filter) }
                    .buttonStyle(.plain).font(.system(size: 13, weight: .semibold)).monospacedDigit().foregroundStyle(palette.link)
                    .accessibilityLabel("See all \(shelf.total) \(shelf.filter.rawValue) setups")
                    .help("Show every \(shelf.filter.rawValue) setup")
                if shelf.entries.count > Self.page {
                    HStack(spacing: 6) {
                        arrow("chevron.left", "Earlier \(shelf.filter.rawValue) setups", -Self.page).disabled(first == 0)
                        arrow("chevron.right", "More \(shelf.filter.rawValue) setups", Self.page).disabled(first >= shelf.entries.count - Self.page)
                    }
                }
            }
            ScrollViewReader { proxy in
                ScrollView(.horizontal) {
                    HStack(alignment: .top, spacing: 14) {
                        ForEach(shelf.entries) { entry in
                            SetupCard(entry: entry, saved: discover.isSaved(entry.id), image: artwork.image(for: entry.artMap)) { discover.open(.player(entry.id)) }
                                .frame(width: 250).id(entry.id)
                        }
                    }
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.trailing, StudioMetrics.pageInset)
                }
                .scrollIndicators(.never)
                .onChange(of: first) { _, value in
                    guard shelf.entries.indices.contains(value) else { return }
                    withAnimation(reduceMotion ? nil : .easeInOut(duration: 0.3)) { proxy.scrollTo(shelf.entries[value].id, anchor: .leading) }
                }
            }
            // The row runs to the window's edge, so a card cut off there shows that there are more.
            .padding(.trailing, -StudioMetrics.pageInset)
        }
    }

    private func arrow(_ symbol: String, _ label: String, _ step: Int) -> some View {
        Button { first = max(0, min(shelf.entries.count - Self.page, first + step)) } label: {
            Image(systemName: symbol).font(.system(size: 11, weight: .bold)).frame(width: 12, height: 12)
        }
        .glassAction(circle: true)
        .accessibilityLabel(label).help(label)
    }
}

// MARK: - Cards

/// One player setup: a picture, who and when, how it feels, and the three numbers its config writes.
struct SetupCard: View {
    var entry: ConfigDirectoryEntry
    var saved: Bool
    var image: NSImage?
    /// A grid names where each setup was published; a shelf leaves that to the setup's own page.
    var source = false
    var open: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous)
        Button(action: open) {
            VStack(alignment: .leading, spacing: 0) {
                ZStack(alignment: .bottomLeading) {
                    MapArtFrame(map: entry.artMap, image: image, size: .fill).frame(height: 104).frame(maxWidth: .infinity)
                    Text(PlayerInitials.of(entry.player)).font(.stencil(18)).foregroundStyle(palette.accent)
                        .frame(width: 40, height: 40)
                        .background(palette.tileFill, in: Circle())
                        .overlay(Circle().strokeBorder(palette.panel, lineWidth: 2))
                        .overlay(Circle().stroke(palette.selectionStroke, lineWidth: 1).padding(-1))
                        .offset(x: 14, y: 18)
                        .accessibilityHidden(true)
                }
                .zIndex(1)
                VStack(alignment: .leading, spacing: 8) {
                    VStack(alignment: .leading, spacing: 2) {
                        HStack(spacing: 6) {
                            Text(entry.player).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).lineLimit(1)
                            Spacer(minLength: 4)
                            if saved { Image(systemName: "bookmark.fill").font(.system(size: 11)).foregroundStyle(palette.accent).accessibilityHidden(true) }
                        }
                        Text([entry.team, entry.era].compactMap { $0 }.joined(separator: " · ")).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
                    }
                    Text(entry.feel).font(.system(size: 13)).foregroundStyle(palette.chipText).lineLimit(1)
                    HStack(alignment: .top, spacing: 16) {
                        fact(entry.facts?.sensitivity, "sensitivity")
                        fact(size, "resolution")
                    }
                    if source {
                        Text("As published by \(entry.provenance.publishedBy)").font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
                    }
                }
                .padding(.top, 26).padding(.horizontal, StudioMetrics.cardInset).padding(.bottom, 14)
            }
            // Cards in a row share the tallest card's height.
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(palette.panel)
            .clipShape(shape)
            .overlay(shape.stroke(hovering ? palette.selectionStroke : palette.panelStroke, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(.plain).onHover { hovering = $0 }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(spoken)
        .accessibilityHint("Shows what this setup changes for you").accessibilityAddTraits(.isButton)
    }
    private var size: String? {
        guard let written = entry.facts?.resolution else { return nil }
        guard let size = DisplayResolution(written) else { return written }
        return "\(size.width)×\(size.height)"
    }
    private func fact(_ value: String?, _ label: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(value ?? "—").font(.value).foregroundStyle(palette.heading).lineLimit(1)
            Text(label).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
        }
    }
    private var spoken: String {
        var parts = [[entry.player, entry.team, entry.era].compactMap { $0 }.joined(separator: ", ") + ".", entry.feel + "."]
        if let sensitivity = entry.facts?.sensitivity { parts.append("Sensitivity \(sensitivity).") }
        if source { parts.append("As published by \(entry.provenance.publishedBy).") }
        if saved { parts.append("Saved.") }
        return parts.joined(separator: " ")
    }
}

private struct TeamTile: View {
    var team: ConfigDirectoryTeam
    var open: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 16, style: .continuous)
        Button(action: open) {
            VStack(alignment: .leading, spacing: 4) {
                Text(team.name).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).lineLimit(1)
                Text("\(team.count) setups").font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.secondary)
            }
            .padding(.horizontal, 18).frame(maxWidth: .infinity, minHeight: 72, alignment: .leading)
            .background(palette.crate, in: shape)
            .overlay(shape.strokeBorder(hovering ? palette.selectionStroke : palette.wellStroke, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(.plain).onHover { hovering = $0 }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(team.name), \(team.count) setups")
        .accessibilityHint("Shows this team’s setups").accessibilityAddTraits(.isButton)
    }
}

/// A wide banner with one action: a short heading, what it does for the person, and a quiet symbol behind.
private struct PromoBanner<Action: View>: View {
    var eyebrow: String
    var title: String
    var symbol: String
    var message: String
    @ViewBuilder var action: Action
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous)
        VStack(alignment: .leading, spacing: 8) {
            Eyebrow(text: eyebrow, size: 12, tracking: 0.28)
            Text(title).font(StudioTypography.groupTitle).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                .accessibilityAddTraits(.isHeader)
            Text(message).font(.system(size: 13)).foregroundStyle(palette.secondary).lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true).frame(maxWidth: 360, alignment: .leading)
            action.padding(.top, 6)
        }
        .padding(StudioMetrics.panelInset)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(alignment: .bottomTrailing) {
            Image(systemName: symbol).font(.system(size: 130, weight: .light)).foregroundStyle(palette.accent.opacity(0.12))
                .offset(x: 16, y: 26).accessibilityHidden(true)
        }
        .background(palette.panel)
        .clipShape(shape)
        .overlay(shape.stroke(palette.panelStroke, lineWidth: 1))
    }
}
