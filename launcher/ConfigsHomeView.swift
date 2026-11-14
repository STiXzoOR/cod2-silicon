import SwiftUI
import AppKit

// Configs · Home: the person's loadouts, then player setups from the signed catalogue with filters
// and a search. Every card, credit and source comes from the verified index; nothing is invented here.

/// Home, Loadouts and Check a Config: the three places in Configs, shown in the toolbar on the two
/// pages that take the whole width.
enum ConfigsSection: String, CaseIterable, Identifiable {
    case home = "Home", loadouts = "Loadouts", checker = "Check a Config"
    var id: String { rawValue }
}

extension ConfigsSection {
    /// The switch shown at the leading edge of the toolbar on Home and Check a Config.
    @MainActor static func switcher(_ discover: ConfigDiscoverModel) -> some View {
        ScopeSwitch("Configs section", selection: Binding<ConfigsSection>(get: { discover.page == .checker ? .checker : .home }, set: { section in
            switch section {
            case .home: discover.open(.home)
            case .checker: discover.page = .checker
            case .loadouts: discover.page = nil
            }
        }), options: ConfigsSection.allCases.map { ($0, $0.rawValue) })
    }
}

/// Home's toolbar, as on the board: the sections, the search, the catalogue's status with Refresh,
/// and Add a Config. Every item sits in the window's toolbar, so the system draws its glass.
private struct ConfigsHomeToolbar: ViewModifier {
    @ObservedObject var discover: ConfigDiscoverModel
    var searchFocused: FocusState<Bool>.Binding
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar {
                leading
                ToolbarSpacer(.flexible)
                ToolbarItem { CatalogueStatusPill(discover: discover) }
                ToolbarSpacer(.fixed)
                ToolbarItem { AddConfigToolbarButton(discover: discover) }
            }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar {
            leading
            ToolbarItem { Spacer() }
            ToolbarItem { CatalogueStatusPill(discover: discover) }
            ToolbarItem { AddConfigToolbarButton(discover: discover) }
        }
    }
    @ToolbarContentBuilder private var leading: some ToolbarContent {
        ToolbarItem(placement: .navigation) { ConfigsSection.switcher(discover) }
        ToolbarItem(placement: .navigation) {
            ToolbarSearchField(prompt: "Search players, teams or styles", width: 280, text: $discover.query, focus: searchFocused, request: discover.findRequest)
                .help("Search setups by player, team, era or where they were published (⌘F)")
        }
    }
}

/// “68 setups · updated today” with Refresh, like the Servers status pill.
private struct CatalogueStatusPill: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    var body: some View {
        let loading: Bool = { if case .loading = discover.state { return true }; return false }()
        HStack(spacing: 6) {
            Text(discover.status()).font(.system(size: 12)).monospacedDigit().foregroundStyle(palette.secondary).lineLimit(1).fixedSize()
                .padding(.leading, 10).accessibilityAddTraits(.isStaticText)
            Button { Task { await discover.reload() } } label: {
                Image(systemName: "arrow.clockwise").font(.system(size: 13, weight: .semibold)).frame(width: 18, height: 18)
            }
            .disabled(loading)
            .accessibilityLabel("Refresh the catalogue").help("Check the catalogue for new setups")
        }
    }
}

/// Add a Config…: one menu for a link, a file, a Windows install or the clipboard.
private struct AddConfigToolbarButton: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @State private var anchor = MenuAnchorBox()
    var body: some View {
        Button {
            guard let view = anchor.view else { return }
            var items = AddConfigButton.items(discover, window: view.window)
            items.append(.init(title: "Check a Config Without Adding It…", symbol: "checkmark.shield", separatorBefore: true) { discover.page = .checker })
            NativeMenu.popUp(items, below: view)
        } label: {
            Label("Add a Config…", systemImage: "plus").labelStyle(.titleAndIcon).font(.system(size: 13, weight: .semibold))
        }
        .background(MenuAnchor(box: anchor))
        .disabled(discover.studio.session == nil)
        .accessibilityLabel("Add a Config").accessibilityHint("Shows the places you can add a config from")
        .help("Add a config from a link, a file, your Windows install or the clipboard. Everything you add is reviewed first.")
    }
}

struct ConfigsHomePage: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    @FocusState private var searchFocused: Bool

    var body: some View {
        GeometryReader { proxy in
            ScrollView {
                VStack(alignment: .leading, spacing: 22) {
                    if !discover.searching { loadouts }
                    setups
                }
                .padding(.top, 84).padding(.leading, 22).padding(.trailing, 32).padding(.bottom, 24)
                // A short page still fills the window, so the note about the catalogue sits at the bottom.
                .frame(minHeight: proxy.size.height, alignment: .top)
            }
            .scrollIndicators(.automatic)
        }
        .modifier(ConfigsHomeToolbar(discover: discover, searchFocused: $searchFocused))
        .background { Button("") { discover.requestFind() }.keyboardShortcut("f", modifiers: .command).opacity(0).accessibilityHidden(true) }
        .task { await discover.loadIfNeeded() }
    }
    /// Four across in the standard window, fewer when it is narrow.
    static let columns = [GridItem(.adaptive(minimum: 236), spacing: 14, alignment: .top)]

    // MARK: Your loadouts

    private var loadouts: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline) {
                Text("Your loadouts").font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                Spacer(minLength: 12)
                Button("All Loadouts") { discover.page = nil }.buttonStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.link)
                    .help("Open your loadouts to change settings and keys")
            }
            LazyVGrid(columns: Self.columns, alignment: .leading, spacing: 14) {
                ForEach(model.loadoutCards) { choice in
                    LoadoutCard(choice: choice) { discover.page = nil; model.studio.select(choice.id) }
                }
                Button { model.newLoadout() } label: {
                    VStack(alignment: .leading, spacing: 8) {
                        Label("New Loadout", systemImage: "plus").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                        Text("Start from the game’s defaults, or from a player’s setup below.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                            .multilineTextAlignment(.leading).fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(StudioMetrics.cardInset).frame(maxWidth: .infinity, minHeight: 104, alignment: .leading)
                    .overlay(RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous)
                        .strokeBorder(palette.panelStroke, style: StrokeStyle(lineWidth: 1, dash: [6, 5])))
                    .contentShape(RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous))
                }
                .buttonStyle(.plain).disabled(!model.canNewLoadout)
                .accessibilityLabel("New Loadout").accessibilityHint("Starts a loadout from the game’s defaults")
            }
        }
    }

    // MARK: Player setups

    private var setups: some View {
        VStack(alignment: .leading, spacing: 12) {
            switch discover.state {
            case .idle, .loading:
                heading("Player setups")
                loading
            case .unavailable(let message):
                heading("Player setups")
                unreachable(title: "NO PLAYER SETUPS IN THIS BUILD", message: "\(message) Your own loadouts and Add a Config work without them.", retry: false)
            case .failed(let message):
                heading("Player setups")
                unreachable(title: "THE CATALOGUE COULDN’T BE REACHED",
                            message: "Player setups come from the catalogue on GitHub. \(message) Your own loadouts and Add a Config work without it.", retry: true)
            case .loaded(let snapshot, let refreshError):
                header(total: snapshot.index.entries.count)
                if refreshError != nil {
                    Banner(.neutral, message: "Couldn’t reach the catalogue just now. Showing the copy saved\(discover.fetched.map { " on \($0.formatted(.dateTime.day().month(.wide)))" } ?? " on this Mac").") {
                        Button("Try Again") { Task { await discover.reload() } }
                    }
                }
                if snapshot.index.entries.isEmpty {
                    StudioEmptyState(title: "NO SETUPS YET", message: "The catalogue doesn’t list any player setups yet. You can still bring in a config of your own.")
                        .frame(maxWidth: .infinity)
                } else if discover.setups.isEmpty {
                    EmptyState("NO SETUP MATCHES “\(discover.query.trimmingCharacters(in: .whitespaces).uppercased())”",
                               message: "Try a team, an era or where it was published. Search looks through \(snapshot.index.entries.count) setups.",
                               actionTitle: "Clear Search", prominent: false) { discover.query = "" }
                        .padding(.vertical, 40).frame(maxWidth: .infinity)
                } else {
                    LazyVGrid(columns: Self.columns, alignment: .leading, spacing: 14) {
                        ForEach(discover.setups) { entry in
                            SetupCard(entry: entry, saved: discover.isSaved(entry.id)) { discover.open(.player(entry.id)) }
                        }
                    }
                }
                Spacer(minLength: 0)
                footer
            }
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }
    private func heading(_ text: String) -> some View {
        Text(text).font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
    }

    /// “Player setups · Featured · 8 of 59” with the filters at the trailing edge; a search shows “Results”.
    private func header(total: Int) -> some View {
        let chosen = discover.filters.contains(discover.filter) ? discover.filter : ConfigSetupFilter.all
        let shown = discover.setups.count
        let count = discover.searching ? (shown == 0 ? "None of \(total)" : "\(shown) of \(total)")
                                       : chosen == .all ? "All \(total)" : "\(chosen.rawValue) · \(shown) of \(total)"
        return ViewThatFits(in: .horizontal) {
            HStack(alignment: .center, spacing: 14) { title(count, total); Spacer(minLength: 12); filters(chosen) }
            VStack(alignment: .leading, spacing: 10) { title(count, total); filters(chosen) }
        }
    }
    private func title(_ count: String, _ total: Int) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 14) {
            heading(discover.searching ? "Results" : "Player setups")
            if total > 0 { Text(count).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.secondary) }
        }
        .fixedSize()
    }
    @ViewBuilder private func filters(_ chosen: ConfigSetupFilter) -> some View {
        if !discover.searching, discover.filters.count > 1 {
            HStack(spacing: 8) {
                ForEach(discover.filters) { filter in
                    Button(filter.rawValue) { discover.filter = filter }
                        .filterChip(on: chosen == filter)
                        .accessibilityAddTraits(chosen == filter ? .isSelected : [])
                }
                if !discover.saved.isEmpty {
                    Button { discover.open(.saved) } label: { Label("Saved · \(discover.saved.count)", systemImage: "bookmark") }
                        .filterChip(on: false).help("The setups you saved for later")
                }
            }
            .fixedSize()
            .accessibilityElement(children: .contain).accessibilityLabel("Narrow the setups")
        }
    }

    private var loading: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Getting the catalogue…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                .accessibilityElement(children: .combine)
            LazyVGrid(columns: Self.columns, alignment: .leading, spacing: 14) {
                ForEach(0..<4, id: \.self) { index in
                    RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous).fill(palette.panel)
                        .frame(height: 172).opacity(0.9 - Double(index) * 0.2)
                }
            }
            .accessibilityHidden(true)
        }
    }
    private func unreachable(title: String, message: String, retry: Bool) -> some View {
        VStack(spacing: 14) {
            EmptyState(title, message: message, actionTitle: retry ? "Try Again" : nil) { Task { await discover.reload() } }
            Button("Check a Config") { discover.page = .checker }
        }
        .padding(.vertical, 48).frame(maxWidth: .infinity)
    }
    private var footer: some View {
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .firstTextBaseline, spacing: 16) { provenance; Spacer(minLength: 12); links }
            VStack(alignment: .leading, spacing: 6) { provenance; links }
        }
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

private struct LoadoutCard: View {
    var choice: LoadoutChoice
    var open: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous)
        Button(action: open) {
            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 8) {
                    Text(choice.name).font(.system(size: 15, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1)
                    Spacer(minLength: 4)
                    if choice.inUse {
                        Text("Next game").font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.accent)
                            .padding(.horizontal, 8).padding(.vertical, 3).background(palette.selection, in: Capsule())
                    }
                }
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    Text(choice.cap.map { $0 == 0 ? "No" : "\($0)" } ?? "—").font(.stencil(26)).monospacedDigit().foregroundStyle(palette.numeral)
                    Text("fps cap").font(StudioTypography.help).foregroundStyle(palette.secondary)
                    if let size = resolution {
                        Text("·").foregroundStyle(palette.tertiary).accessibilityHidden(true)
                        Text(size).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.secondary)
                    }
                }
                Text(choice.inUse ? "In use for your next game" : "Ready to use").font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            .padding(StudioMetrics.cardInset).frame(maxWidth: .infinity, minHeight: 104, alignment: .topLeading)
            .background(palette.panel, in: shape)
            .overlay(shape.strokeBorder(choice.inUse || hovering ? palette.selectionStroke : palette.panelStroke, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(.plain).onHover { hovering = $0 }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(spoken)
        .accessibilityHint("Opens this loadout").accessibilityAddTraits(.isButton)
    }
    private var resolution: String? { choice.resolution.flatMap { DisplayResolution($0) }.map { "\($0.width) × \($0.height)" } }
    private var spoken: String {
        let size: DisplayResolution? = choice.resolution.flatMap { DisplayResolution($0) }
        var text = "\(choice.name) loadout"
        if choice.inUse { text += ", used for your next game" }
        if let cap = choice.cap { text += cap == 0 ? ". No frame rate cap" : ". \(cap) frames per second cap" } else { text += ". No frame rate cap set" }
        if let size { text += ", \(size.width) by \(size.height)" }
        return text
    }
}

/// One player setup: who, where it was published, and the three numbers its config writes.
private struct SetupCard: View {
    var entry: ConfigDirectoryEntry
    var saved: Bool
    var open: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: StudioMetrics.cardRadius, style: .continuous)
        Button(action: open) {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 12) {
                    Text(PlayerInitials.of(entry.player)).font(.stencil(18)).foregroundStyle(palette.accent)
                        .frame(width: 40, height: 40)
                        .background(palette.dark ? Palette.hex(0x1b1f17) : palette.crate, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                        .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                        .accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 3) {
                        HStack(spacing: 6) {
                            Text(entry.player).font(.system(size: 15, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1)
                            if saved { Image(systemName: "bookmark.fill").font(.system(size: 10)).foregroundStyle(palette.accent).accessibilityHidden(true) }
                        }
                        Text([entry.team, entry.era].compactMap { $0 }.joined(separator: " · ")).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
                    }
                    Spacer(minLength: 0)
                }
                Text(entry.feel).font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xe2dbc9) : palette.text).lineLimit(1)
                HStack(alignment: .top, spacing: 16) {
                    fact(entry.facts?.sensitivity, "sensitivity")
                    fact(entry.facts?.frameCap.map { $0 == "0" ? "None" : $0 }, "fps cap")
                    fact(size, "resolution")
                }
                Text("As published by \(entry.provenance.publishedBy)").font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
            }
            .padding(StudioMetrics.cardInset).frame(maxWidth: .infinity, alignment: .topLeading)
            .background(palette.panel, in: shape)
            .overlay(shape.strokeBorder(hovering ? palette.selectionStroke : palette.panelStroke, lineWidth: 1))
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
            Text(value ?? "—").font(.stencil(20)).monospacedDigit().foregroundStyle(palette.numeral).lineLimit(1)
            Text(label).font(StudioTypography.help).foregroundStyle(palette.secondary)
        }
    }
    private var spoken: String {
        var parts = [[entry.player, entry.team, entry.era].compactMap { $0 }.joined(separator: ", ") + ".", entry.feel + "."]
        if let sensitivity = entry.facts?.sensitivity { parts.append("Sensitivity \(sensitivity).") }
        if let cap = entry.facts?.frameCap { parts.append(cap == "0" ? "No frame rate cap." : "\(cap) frames per second cap.") }
        parts.append("As published by \(entry.provenance.publishedBy).")
        if saved { parts.append("Saved.") }
        return parts.joined(separator: " ")
    }
}
