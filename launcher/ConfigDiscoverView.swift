import SwiftUI
import AppKit
import UniformTypeIdentifiers

// Discover › Saved and a player's detail, and the switch between Discover's pages. Every card, credit
// and source comes from a verified signed index. Player Setups itself is in ConfigsHomeView.swift.

/// The Discover rows at the top of the Configs column, above My Loadouts.
struct DiscoverColumnSection: View {
    @ObservedObject var discover: ConfigDiscoverModel
    var open: (ConfigDiscoverPage) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            GroupLabel("Discover").padding(.horizontal, 10).padding(.bottom, 6)
            row("Player Setups", symbol: "person.2", count: nil, selected: isPlayers) { open(.home) }
            // Saved has nothing to show until the catalogue lists someone.
            if !discover.saved.isEmpty || !discover.entries.isEmpty {
                row("Saved", symbol: "bookmark", count: discover.saved.count, selected: discover.page == .saved) { open(.saved) }
            }
            row("Check a Config", symbol: "checkmark.shield", count: nil, selected: discover.page == .checker) { open(.checker) }
        }
        .task { await discover.loadIfNeeded() }
    }

    private var isPlayers: Bool {
        switch discover.page { case .home, .player: true; default: false }
    }

    private func row(_ title: String, symbol: String, count: Int?, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 10) {
                MonogramTile(symbol: symbol)
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
            case .checker: ConfigCheckerPage(model: model, checker: discover.checker)
            default: ConfigsHomePage(model: model, discover: discover)
            }
        }
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .task(id: Set(discover.entries.map(\.artMap))) {
            // Card art comes from the person's own game data, with the existing illustrated fallback.
            model.discoverMaps = Array(Set(discover.entries.map(\.artMap))).sorted()
            model.loadArtwork()
        }
    }
}

// MARK: - Shared header

/// A Discover page's title (SF bold 30, as every page's), at the place a loadout's name has.
struct DiscoverHeader<Trailing: View>: View {
    var title: String
    var subtitle: String? = nil
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
            Text(title).font(.system(size: 30, weight: .bold)).tracking(-0.3).foregroundStyle(palette.heading)
                .lineLimit(1).truncationMode(.tail).accessibilityAddTraits(.isHeader)
            if let subtitle { Text(subtitle).font(.system(size: 13)).monospacedDigit().foregroundStyle(palette.secondary).lineLimit(1) }
        }
    }
}

/// “Add a Config”: a pull-down of every import source.
struct AddConfigButton: View {
    @ObservedObject var discover: ConfigDiscoverModel
    /// The page's one prominent action, or a glass button beside one.
    var prominent = true
    @State private var anchor = MenuAnchorBox()
    var body: some View {
        let button = Button { show() } label: {
            HStack(spacing: 8) {
                Image(systemName: "plus").accessibilityHidden(true)
                Text("Add a Config")
                Image(systemName: "chevron.down").font(.system(size: 10, weight: .bold)).accessibilityHidden(true)
            }
        }
        Group { if prominent { button.prominentAction() } else { button.glassAction() } }
        .actionControlSize().fixedSize()
        .background(MenuAnchor(box: anchor))
        .disabled(discover.studio.session == nil)
        .accessibilityLabel("Add a Config")
        .accessibilityHint("Shows the places you can add a config from")
        .help("Add a config from a file, a link, your Windows install or the clipboard")
    }
    private func show() {
        guard let view = anchor.view else { return }
        NativeMenu.popUp(Self.items(discover, window: view.window), below: view, header: Self.header, footnote: Self.footnote)
    }
    /// The menu's group label and closing note (Discover board › Add a Config menu).
    static let header = "Bring in a config"
    static let footnote = "Everything you add is reviewed first. Nothing changes until you choose Import."
    /// Every import source, with the File menu's shortcuts shown beside the matching items.
    @MainActor static func items(_ discover: ConfigDiscoverModel, window: NSWindow?) -> [NativeMenu.Item] {
        [.init(title: "From a File…", key: "o", modifiers: .command) { discover.chooseFile(in: window) },
         .init(title: "From a Link or GitHub…") { discover.step = .link("") },
         .init(title: "From This Mac’s Server (localhost)…") { discover.step = .link("http://localhost:") },
         .init(title: "From Your Windows Install…") { discover.step = .windows },
         .init(title: "Paste Config", key: "v", modifiers: [.command, .shift], separatorBefore: true) { discover.importPasteboard() }]
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
        /// An SF Symbol beside the title; none when empty.
        var symbol = ""
        var key = ""
        var modifiers: NSEvent.ModifierFlags = []
        var separatorBefore = false
        /// Dimmed, never hidden, when it can't run now; the footnote says why.
        var enabled = true
        var action: @MainActor () -> Void
    }
    @MainActor private final class Target: NSObject {
        var actions: [@MainActor () -> Void] = []
        @objc func run(_ sender: NSMenuItem) { actions[sender.tag]() }
    }
    @MainActor static func popUp(_ items: [Item], below view: NSView, header: String? = nil, footnote: String? = nil) {
        let menu = NSMenu(), target = Target()
        menu.autoenablesItems = false
        target.actions = items.map(\.action)
        if let header {
            if #available(macOS 14.0, *) { menu.addItem(.sectionHeader(title: header)) }
            else { let item = NSMenuItem(title: header, action: nil, keyEquivalent: ""); item.isEnabled = false; menu.addItem(item) }
        }
        for (index, item) in items.enumerated() {
            if item.separatorBefore { menu.addItem(.separator()) }
            let row = NSMenuItem(title: item.title, action: #selector(Target.run(_:)), keyEquivalent: item.key)
            row.keyEquivalentModifierMask = item.modifiers
            row.target = target; row.tag = index; row.isEnabled = item.enabled
            if !item.symbol.isEmpty { row.image = NSImage(systemSymbolName: item.symbol, accessibilityDescription: nil) }
            menu.addItem(row)
        }
        if let footnote {
            // A note, not a command: dimmed, in the small secondary type of a menu's help line.
            let note = NSMenuItem(title: footnote, action: nil, keyEquivalent: "")
            note.isEnabled = false
            note.attributedTitle = NSAttributedString(string: footnote, attributes: [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.secondaryLabelColor])
            menu.addItem(note)
        }
        // Tracking is synchronous, so the target outlives the menu.
        menu.popUp(positioning: nil, at: NSPoint(x: 0, y: view.isFlipped ? view.bounds.height + 6 : -6), in: view)
        withExtendedLifetime(target) {}
    }
}

struct PlayerGrid: View {
    var entries: [ConfigDirectoryEntry]
    @ObservedObject var discover: ConfigDiscoverModel
    @ObservedObject var artwork: MapArtworkStore
    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 236), spacing: 14, alignment: .top)], alignment: .leading, spacing: 14) {
            ForEach(entries) { entry in
                SetupCard(entry: entry, saved: discover.isSaved(entry.id), image: artwork.image(for: entry.artMap), source: true) {
                    discover.open(.player(entry.id))
                }
            }
        }
    }
}

enum PlayerInitials {
    /// “Iron Larks 2” → “IL2”: first letters of the first two words, digits kept.
    static func of(_ name: String) -> String {
        let words = name.split(whereSeparator: { !$0.isLetter && !$0.isNumber }).prefix(2)
        let text = words.map { word in word.allSatisfy(\.isNumber) ? String(word) : String(word.prefix(1)) }.joined().uppercased()
        return text.isEmpty ? "?" : text
    }
}

/// Style chips that wrap onto more lines rather than truncating.
struct FlowChips: View {
    var tags: [String]
    @Environment(\.palette) private var palette
    var body: some View {
        ChipFlow(spacing: 8) {
            ForEach(tags, id: \.self) { tag in
                chip(tag)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Style: " + tags.joined(separator: ", "))
    }

    private func chip(_ tag: String) -> some View { Chip(tag) { Text(tag) } }
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
                DiscoverHeader(title: "Saved", subtitle: discover.saved.isEmpty ? nil : discover.saved.count == 1 ? "1 setup" : "\(discover.saved.count) setups") { EmptyView() }
                let verified = discover.saved.compactMap(\.entry)
                let gone = discover.saved.filter { $0.entry == nil }
                if discover.saved.isEmpty {
                    StudioEmptyState(title: "Nothing saved",
                                     message: discover.snapshot == nil
                                        ? "Players you save appear here. The catalogue isn’t available yet, so there’s nothing to save."
                                        : "Players you save appear here, so you can come back to their setups.",
                                     action: ("Browse Player Setups", { discover.openFront() }))
                        .frame(maxWidth: .infinity)
                } else {
                    if !verified.isEmpty { PlayerGrid(entries: verified, discover: discover, artwork: artwork) }
                    if !gone.isEmpty {
                        VStack(alignment: .leading, spacing: 0) {
                            ForEach(Array(gone.enumerated()), id: \.element.id) { index, item in
                                HStack(spacing: 12) {
                                    Image(systemName: "bookmark.slash").foregroundStyle(palette.secondary).accessibilityHidden(true)
                                    VStack(alignment: .leading, spacing: 3) {
                                        Text("No longer in the catalogue").font(.system(size: 14)).foregroundStyle(palette.text)
                                        Text("This saved setup isn’t in the signed catalogue any more, so its details can’t be shown.").font(StudioTypography.help).foregroundStyle(palette.secondary)
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
            VStack(alignment: .leading, spacing: 20) {
                Button { discover.open(.home) } label: { Label("Player Setups", systemImage: "chevron.left").font(.system(size: 13, weight: .semibold)) }
                    .buttonStyle(.plain).foregroundStyle(palette.link).frame(height: 28).padding(.top, -6).padding(.bottom, -12)
                    .keyboardShortcut("[", modifiers: .command)
                    .help("Back to Player Setups (⌘[)")
                if let entry = discover.entry(entryID) {
                    PlayerHero(entry: entry, saved: discover.isSaved(entry.id), image: artwork.image(for: entry.artMap),
                               canTry: discover.studio.session != nil && !discover.studio.isReadOnly,
                               tryIt: { discover.tryEntry(entry) },
                               toggleSaved: { discover.setSaved(entry.id, !discover.isSaved(entry.id)) })
                    let changes = ChangesPanel(discover: discover, canCompare: discover.studio.session != nil && discover.canCompare(entry.id),
                                               compare: { discover.compareInExpert(entry) })
                    // Two equal columns, as drawn, when there's room; one column at the minimum window size.
                    Group {
                        if width >= 760 {
                            // The panels in both columns end on one line, as drawn.
                            HStack(alignment: .top, spacing: 16) {
                                VStack(spacing: 16) { StoryPanel(entry: entry); InSetupPanel(entry: entry, discover: discover) }.frame(maxWidth: .infinity)
                                changes.frame(maxWidth: .infinity, maxHeight: .infinity)
                            }
                            .fixedSize(horizontal: false, vertical: true)
                        } else {
                            VStack(spacing: 16) { StoryPanel(entry: entry); changes; InSetupPanel(entry: entry, discover: discover) }
                        }
                    }
                    .frame(maxWidth: .infinity)
                    .onGeometryChange(for: CGFloat.self, of: { $0.size.width }) { width = $0 }
                    PlayerProvenance(entry: entry)
                } else {
                    StudioEmptyState(title: "Not in the catalogue", message: "This setup isn’t in the signed catalogue any more.",
                                     action: ("Browse Player Setups", { discover.open(.home) }))
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
    var tryIt: () -> Void
    var toggleSaved: () -> Void
    // Text sits on the scrim over the art, so it keeps the dark-appearance inks in both appearances.
    private let ink = Palette.dark

    var body: some View {
        ZStack(alignment: .bottomLeading) {
            MapArtFrame(map: entry.artMap, image: image, size: .fill, hero: true)
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
        .frame(height: 240)
        .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 24, style: .continuous).stroke(Color.white.opacity(0.08), lineWidth: 1))
        .shapeAudit(.container, .rounded(24), "player hero")
    }

    private var identity: some View {
        HStack(alignment: .bottom, spacing: 18) {
            Text(PlayerInitials.of(entry.player)).font(.stencil(34)).foregroundStyle(ink.accent)
                .frame(width: 78, height: 78)
                .background(ink.tileFill, in: Circle())
                .overlay(Circle().strokeBorder(ink.ground, lineWidth: 3))
                .overlay(Circle().stroke(ink.accent.opacity(0.6), lineWidth: 1))
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 8) {
                Eyebrow(text: [entry.team, entry.era].compactMap { $0 }.joined(separator: " · "), color: ink.accent)
                Text(entry.player.uppercased()).font(.stencil(56)).tracking(1.1).foregroundStyle(ink.heading).lineLimit(1).minimumScaleFactor(0.6)
                    .accessibilityLabel(entry.player).accessibilityAddTraits(.isHeader)
                Text(entry.feel).font(.system(size: 14)).italic().foregroundStyle(ink.chipText).lineLimit(2)
            }
        }
    }

    private var actions: some View {
        GlassContainer(spacing: 8) {
            HStack(spacing: 8) {
                Button("Try This Setup", action: tryIt).prominentAction().actionControlSize().disabled(!canTry)
                    .help("Review this setup line by line. Trying it makes a new loadout; your others stay as they are.")
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
            Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            Spacer(minLength: 8)
            if let trailing { Text(trailing).font(StudioTypography.help).foregroundStyle(palette.tertiary) }
        }
    }
}

private struct StoryPanel: View {
    var entry: ConfigDirectoryEntry
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 12) {
                DetailHeading(title: "Story")
                Text(entry.story).font(.system(size: 14)).lineSpacing(4).foregroundStyle(palette.text)
                    .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
                if !entry.styleTags.isEmpty { FlowChips(tags: entry.styleTags) }
                Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 8) {
                    GridRow {
                        fact("Era", entry.era)
                        fact("Team", entry.team ?? "None listed")
                        if let role = entry.role { fact("Role", role) }
                    }
                    if let map = entry.favoriteMap { GridRow { fact("Favorite map", MapCatalog.displayName(map)) } }
                }
                .padding(.top, 4)
            }
        }
    }
    private func fact(_ key: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(key).font(.system(size: 11)).foregroundStyle(palette.tertiary)
            Text(value).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(2)
        }
        .accessibilityElement(children: .combine)
    }
}

/// Where the setup came from, under the panels (the board's page footer): the signed entry, the
/// source and credit, how it was copied, and the way to ask for a change.
private struct PlayerProvenance: View {
    var entry: ConfigDirectoryEntry
    @Environment(\.palette) private var palette
    var body: some View {
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .firstTextBaseline, spacing: 24) { details; Spacer(minLength: 0); removal }
            VStack(alignment: .leading, spacing: 8) { details; removal }
        }
        .font(StudioTypography.help)
        .padding(.top, 4)
    }
    private var removal: some View {
        Link("Ask to remove or correct this setup", destination: ConfigDirectory.removalURL).foregroundStyle(palette.link).fixedSize()
    }
    private var details: some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Label { Text("Signed catalogue entry") } icon: { Image(systemName: "checkmark.shield").font(.system(size: 13, weight: .semibold)) }
                .font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.positive).fixedSize()
            VStack(alignment: .leading, spacing: 4) {
                // One line when it fits; otherwise the credit line breaks after the source.
                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 8) { permission; Dot(); credit }
                    VStack(alignment: .leading, spacing: 4) { permission; credit }
                }
                Text(provenanceNote).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
    /// The catalogue says where it found a setup, never that the player approved the listing.
    private var provenanceNote: String {
        // A setup a player sent from the app says so, with the day it was sent (ShareSetup board).
        if let shared = ConfigShareCredit.line(entry) {
            return "\(shared). \(entry.provenance.changes) The signature shows this entry came from the catalogue unchanged; it doesn’t make a config safe, so you still review every line."
        }
        let how: String
        switch entry.provenance.attribution {
        case .selfPublished: how = "The player published it there."
        case .listed: how = "That page lists it under this name; the catalogue can’t confirm who made it."
        case .uploaded: how = "It was uploaded there under this name; the catalogue can’t confirm who made it."
        }
        return "Copied on \(entry.provenance.retrievedAt). \(how) \(entry.provenance.changes) The signature shows this entry came from the catalogue unchanged; it doesn’t make a config safe, so you still review every line."
    }
    private var permission: some View {
        HStack(spacing: 8) {
            Text("As published by \(entry.provenance.publishedBy)").foregroundStyle(palette.secondary)
            Dot()
            Link("Source", destination: entry.sourceURL).foregroundStyle(palette.link)
        }
        .fixedSize()
    }
    private var credit: some View {
        Text("Credit: \(entry.credit)").foregroundStyle(palette.secondary).fixedSize()
    }
}

private struct Dot: View {
    @Environment(\.palette) private var palette
    var body: some View { Circle().fill(palette.tertiary).frame(width: 3, height: 3).accessibilityHidden(true) }
}

private struct ChangesPanel: View {
    @ObservedObject var discover: ConfigDiscoverModel
    var canCompare: Bool
    var compare: () -> Void
    @Environment(\.palette) private var palette
    /// The panel's title, what it compares with, and Compare beside them (comparing writes nothing).
    private func heading(_ comparedWith: String?) -> some View {
        HStack(alignment: .top, spacing: 8) {
            VStack(alignment: .leading, spacing: 3) {
                Text("What changes for you").font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                if let comparedWith { Text("Compared with \(comparedWith)").font(StudioTypography.help).foregroundStyle(palette.tertiary) }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            // Available once their setup is downloaded and checked; comparing writes nothing.
            Button("Compare", action: compare).glassAction().actionControlSize().disabled(!canCompare)
                .help("See their values beside yours in Expert, setting by setting.")
        }
    }
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 8, trailing: 20)) {
            // Fills the height its row gives it, so it ends level with the panels beside it.
            VStack(alignment: .leading, spacing: 0) { content }.frame(maxHeight: .infinity, alignment: .top)
        }
    }
    @ViewBuilder private var content: some View {
            switch discover.detail?.phase {
            case .ready(let summary):
                heading(summary.comparedWith).padding(.bottom, 6)
                ForEach(Array(summary.changes.enumerated()), id: \.element.id) { index, change in
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: change.locked ? "lock" : "arrow.left.arrow.right").font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(change.locked ? palette.chipIcon : palette.accent)
                            .frame(width: 30, height: 30)
                            .background(change.locked ? palette.button : palette.selection, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                            .accessibilityHidden(true)
                        VStack(alignment: .leading, spacing: 3) {
                            Text(change.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                            Text(change.detail).font(.system(size: 13)).foregroundStyle(palette.secondary)
                        }
                        .fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(.vertical, 6)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityElement(children: .combine)
                }
                if summary.more > 0 {
                    Text(summary.more == 1 ? "And 1 more change. Try the setup to see every line." : "And \(summary.more) more changes. Try the setup to see every line.")
                        .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.bottom, 10)
                }
            case .failed(let message):
                heading(nil).padding(.bottom, 8)
                StudioBanner(title: "This setup couldn’t be checked.", detail: message, tone: .negative)
                Button("Try Again") { if let id = discover.detail?.entryID { discover.loadDetail(id) } }.glassAction().actionControlSize().padding(.vertical, 10)
            default:
                heading(nil).padding(.bottom, 8)
                HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Checking what this setup changes…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                    .padding(.vertical, 12).accessibilityElement(children: .combine)
            }
    }
}

private struct InSetupPanel: View {
    var entry: ConfigDirectoryEntry
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
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
