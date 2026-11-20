import SwiftUI
import AppKit
import QuickLook

// Owner: ship-home-library. Library: Matches (the game's local match summaries), Demos and Screenshots,
// newest first, read again whenever Library opens, the window comes forward or the game quits.

struct LibraryView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var checkup: ConfigCheckupModel
    @State private var origin = CGPoint.zero
    @State private var quickLookURL: URL?
    init(model: LauncherModel) { self.model = model; checkup = model.studio.checkup }

    var body: some View {
        LibraryTabContent(model: model, origin: origin, quickLookURL: $quickLookURL)
            .id(model.libraryTab)
            .ignoresSafeArea(.container, edges: .top)
            .background(WindowOriginReader(origin: $origin))
            .onAppear { model.reloadLibrary() }
            .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in model.reloadLibrary() }
            .onReceive(NotificationCenter.default.publisher(for: NSWindow.didBecomeKeyNotification)) { note in
                if let window = note.object as? NSWindow, window === model.launcherWindow { model.reloadLibrary() }
            }
            .onChange(of: model.libraryTab) { _, _ in
                model.selectedMedia = nil; model.mediaMessage = nil; model.mediaError = false; model.trashedMedia = nil
                model.reloadLibrary()
            }
            .quickLookPreview($quickLookURL, in: model.media.map(\.url))
            .sheet(item: $model.renamingMedia) { MediaRenameSheet(model: model, entry: $0).sheetShapeAudit() }
            .toolbar {
                // The page's switch sits at the left of the toolbar, as on Servers and Configs.
                ToolbarItem(placement: .navigation) {
                    ScopeSwitch("Library section", selection: $model.libraryTab, options: LibraryTab.allCases.map { ($0, $0.rawValue) })
                }
            }
            .trailingToolbar {
                if model.libraryTab == .matches {
                    Button(action: model.requestDeleteSummaries) { Label("Delete Summaries…", systemImage: "trash").labelStyle(.titleAndIcon) }
                        .disabled(model.storedMatches.isEmpty)
                        .help("Delete the match summaries kept on this Mac")
                } else {
                    Button(action: model.openMediaFolder) { Label("Show in Finder", systemImage: "folder").labelStyle(.titleAndIcon) }
                        .help(model.libraryTab == .demos ? "Show your demos folder in Finder" : "Show your screenshots folder in Finder")
                }
            }
    }
}

/// One tab's page: its own scroll view, which keeps its offset in the model so the tab opens where it was left.
private struct LibraryTabContent: View {
    @ObservedObject var model: LauncherModel
    var origin: CGPoint
    @Binding var quickLookURL: URL?
    @ObservedObject private var checkup: ConfigCheckupModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @State private var position = ScrollPosition()
    @FocusState private var listFocused: Bool
    private let tab: LibraryTab
    init(model: LauncherModel, origin: CGPoint, quickLookURL: Binding<URL?>) {
        self.model = model; self.origin = origin; _quickLookURL = quickLookURL
        checkup = model.studio.checkup; artwork = model.artwork; tab = model.libraryTab
        // Opens where this tab was left; onAppear repeats it once the rows are laid out.
        _position = State(initialValue: ScrollPosition(y: model.libraryOffsets[model.libraryTab] ?? 0))
    }

    var body: some View {
        GeometryReader { geometry in
            let lead = max(24, 264 - origin.x)
            let width = geometry.size.width - lead - 24
            ScrollViewReader { reader in
                ScrollView {
                    VStack(alignment: .leading, spacing: 0) {
                        header
                        banner.padding(.bottom, 14)
                        content(width: width, reader: reader)
                    }
                    .padding(.leading, lead).padding(.trailing, 24).padding(.top, 88).padding(.bottom, 32)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .scrollPosition($position)
                // A tab being left is emptied and scrolls back to the top on its way out; that isn't where it was left.
                .onScrollGeometryChange(for: CGFloat.self, of: { $0.contentOffset.y + $0.contentInsets.top }) { _, offset in
                    if model.libraryTab == tab { model.libraryOffsets[tab] = offset }
                }
                .onAppear {
                    guard let offset = model.libraryOffsets[tab], offset > 1 else { return }
                    Task { @MainActor in try? await Task.sleep(for: .milliseconds(60)); position.scrollTo(y: offset) }
                }
                .focusable(tab != .matches, interactions: .edit).focused($listFocused).focusEffectDisabled()
                .onKeyPress(keys: [.upArrow, .downArrow, .leftArrow, .rightArrow]) { press in
                    guard tab != .matches, !model.media.isEmpty else { return .ignored }
                    let columns = tab == .demos ? 1 : Self.columns(width: width)
                    let current = model.media.firstIndex { $0.url == model.selectedMedia }
                    let step = tab == .demos ? 1 : columns
                    let delta = press.key == .upArrow ? -step : press.key == .downArrow ? step : press.key == .leftArrow ? -1 : 1
                    let index = current.map { min(model.media.count - 1, max(0, $0 + delta)) } ?? 0
                    model.selectedMedia = model.media[index].url
                    reader.scrollTo(model.media[index].id)
                    return .handled
                }
                .onKeyPress(.return) {
                    guard let entry = selected else { return .ignored }
                    open(entry); return .handled
                }
                .onKeyPress(.space) {
                    guard let entry = selected else { return .ignored }
                    preview(entry); return .handled
                }
                .onExitCommand { model.selectedMedia = nil }
                .accessibilityLabel(tab.rawValue)
                // ⌘⌫ (Move to Trash, as the context menus show it) is a key equivalent: it reaches the
                // window as one, not as a key press, so a focus-independent shortcut carries it. The
                // Delete key sends U+007F; SwiftUI's `.delete` is U+0008 and never matches it.
                .background {
                    Button("Move to Trash") { if let entry = selected { model.requestTrash(entry) } }
                        .keyboardShortcut(KeyEquivalent("\u{7f}"), modifiers: .command)
                        .disabled(tab == .matches || selected == nil)
                        .opacity(0).frame(width: 0, height: 0).allowsHitTesting(false).accessibilityHidden(true)
                }
            }
        }
    }

    private var selected: MediaEntry? { model.media.first { $0.url == model.selectedMedia } }
    private var showingMatches: Bool { !model.storedMatches.isEmpty }

    private var header: some View {
        HStack(alignment: .bottom) {
            PageHeader(title: tab.rawValue, subtitle: Self.subtitle(tab))
            Spacer(minLength: 16)
            if tab == .matches && showingMatches {
                Label("Kept on this Mac · the last \(MatchSummaries.recordLimit) matches", systemImage: "lock")
                    .font(.system(size: 12)).foregroundStyle(palette.secondary)
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("Kept on this Mac. The last \(MatchSummaries.recordLimit) matches")
                    .accessibilityAddTraits(.isStaticText)
            }
        }
        .padding(.leading, 16).padding(.trailing, 4).padding(.bottom, 20)
    }
    static func subtitle(_ tab: LibraryTab) -> String {
        switch tab {
        case .matches: "How your recent matches ran: ping and connection."
        case .demos: "Recorded matches play back in the game, from any point."
        case .screenshots: "Screenshots you take in a match, kept in your CoD2 Unbound folder."
        }
    }

    /// What the last action did, or what went wrong, with the one next step.
    @ViewBuilder private var banner: some View {
        if let message = model.mediaMessage, tab != .matches, !(model.mediaError && model.media.isEmpty) {
            if model.mediaError {
                Banner(.error, message: message) { Button("Refresh", action: model.loadMedia).glassAction().controlSize(.large) }
            } else if model.trashedMedia != nil {
                Banner(.neutral, symbol: "trash", message: message) {
                    Button("Undo", action: model.undoTrash).glassAction().controlSize(.large)
                    SymbolButton("xmark", label: "Dismiss") { model.mediaMessage = nil; model.trashedMedia = nil }.controlSize(.large)
                }
            } else {
                Banner(.neutral, symbol: "checkmark.circle", message: message) {
                    SymbolButton("xmark", label: "Dismiss") { model.mediaMessage = nil }.controlSize(.large)
                }
            }
        }
    }

    @ViewBuilder private func content(width: CGFloat, reader: ScrollViewProxy) -> some View {
        switch tab {
        case .matches: matches(width: width)
        case .demos: demos
        case .screenshots: screenshots
        }
    }

    // MARK: Matches

    @ViewBuilder private func matches(width: CGFloat) -> some View {
        // Turning summaries off stops new ones; those already kept stay listed (and deletable) until deleted.
        if !model.settings.matchSummariesEnabled && model.storedMatches.isEmpty {
            EmptyState("Match summaries are off", message: "Turn them on to see how each match ran. They stay on this Mac.",
                       actionTitle: "Turn On in Settings…", prominent: false) { model.openSettings(.privacy) }
                .padding(.top, 130)
        } else if case .failed = checkup.matchState {
            EmptyState("Matches unavailable", message: "Your match summaries couldn’t be read. Check that your CoD2 Unbound folder is readable, then try again.",
                       actionTitle: "Try Again", prominent: false, action: model.refreshLibraryMatches)
                .padding(.top, 130)
        } else if model.storedMatches.isEmpty {
            EmptyState("No matches yet", message: "After each match the game notes how it ran, so Check-up can advise you. Play one and it shows up here.",
                       actionTitle: "Find a Server", prominent: false) { model.navigate(to: .servers) }
                .padding(.top, 130)
        } else {
            let columns = MatchColumns(width: width)
            let stored = model.storedMatches
            VStack(alignment: .leading, spacing: 4) {
                if !model.settings.matchSummariesEnabled {
                    Banner(.neutral, symbol: "pause.circle", message: "Match summaries are off, so new matches aren’t added. These stay on this Mac until you delete them.") {
                        Button("Turn On in Settings…") { model.openSettings(.privacy) }.glassAction().controlSize(.large)
                    }
                    .padding(.bottom, 14)
                }
                MatchColumnHeader(columns: columns)
                LazyVStack(spacing: 4) {
                    ForEach(Array(stored.enumerated().reversed()), id: \.offset) { index, summary in
                        let facts = MatchFacts(summary, index: index)
                        MatchRow(facts: facts, image: artwork.image(for: facts.map), columns: columns) { model.openCheckup(match: facts.id) }
                    }
                }
            }
        }
    }

    // MARK: Demos

    @ViewBuilder private var demos: some View {
        if model.media.isEmpty {
            if model.mediaError, let message = model.mediaMessage {
                EmptyState("Library unavailable", message: message, actionTitle: "Try Again", prominent: false, action: model.loadMedia).padding(.top, 130)
            } else {
                VStack(spacing: 10) {
                    EmptyState("No recordings", message: "Record a match and watch it back here, from any point.")
                    RecordingSteps().padding(.top, 8)
                    Button("Show Demos Folder", action: model.openMediaFolder).glassAction().actionControlSize().padding(.top, 8)
                }
                .frame(maxWidth: .infinity)
                .padding(.top, 130)
            }
        } else {
            DemoColumnHeader()
            LazyVStack(spacing: 4) {
                ForEach(model.media) { entry in
                    let isSelected = entry.url == model.selectedMedia
                    DemoRow(entry: entry, image: entry.facts.map.flatMap { artwork.image(for: $0) }, selected: isSelected, focused: listFocused,
                            play: { model.play(demo: entry.url) }, select: { select(entry) },
                            more: { select(entry); return moreEntries(entry) })
                        .onTapGesture(count: 2) { model.play(demo: entry.url) }
                        .onTapGesture { select(entry) }
                        .contextMenu { demoMenu(entry) }
                        .id(entry.id)
                }
            }
        }
    }
    /// The demo's actions (Menus board), as the context menu. The shortcuts shown are the list's own
    /// (Space, ⌘⌫), which act on the selected demo.
    @ViewBuilder private func demoMenu(_ entry: MediaEntry) -> some View {
        Button("Play") { model.play(demo: entry.url) }
        Button("Quick Look") { preview(entry) }.keyboardShortcut(.space, modifiers: [])
        Button("Show in Finder") { model.revealMedia(entry) }
        Divider()
        Button("Rename…") { model.renamingMedia = entry }
        Button("Copy") { model.copyDemo(entry) }
        Divider()
        Button("Move to Trash") { model.requestTrash(entry) }.keyboardShortcut(.delete, modifiers: .command)
    }
    /// The same actions for the row's More button, which selects the row first so the shortcuts apply to it.
    private func moreEntries(_ entry: MediaEntry) -> [PullDownMenu.Entry] {
        [.item("Play") { model.play(demo: entry.url) },
         .item("Quick Look", key: " ") { preview(entry) },
         .item("Show in Finder") { model.revealMedia(entry) },
         .separator,
         .item("Rename…") { model.renamingMedia = entry },
         .item("Copy") { model.copyDemo(entry) },
         .separator,
         .item("Move to Trash", key: "\u{8}", modifiers: .command) { model.requestTrash(entry) }]
    }

    // MARK: Screenshots

    static func columns(width: CGFloat) -> Int { max(1, Int((width + 18) / (240 + 18))) }

    @ViewBuilder private var screenshots: some View {
        if model.media.isEmpty {
            if model.mediaError, let message = model.mediaMessage {
                EmptyState("Library unavailable", message: message, actionTitle: "Try Again", prominent: false, action: model.loadMedia).padding(.top, 130)
            } else {
                EmptyState("No screenshots", message: "Take one in a match with F12. It appears here when you come back.",
                           actionTitle: "Show Screenshots Folder", prominent: false, action: model.openMediaFolder)
                    .padding(.top, 130)
            }
        } else {
            VStack(alignment: .leading, spacing: 16) {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 240), spacing: 18)], spacing: 18) {
                    ForEach(model.media) { entry in
                        let isSelected = entry.url == model.selectedMedia
                        ScreenshotTile(entry: entry, snapshot: model.snapshot, selected: isSelected, focused: listFocused)
                            .onTapGesture(count: 2) { open(entry) }
                            .onTapGesture { select(entry) }
                            .contextMenu {
                                Button("Open") { open(entry) }
                                Button("Quick Look") { preview(entry) }.keyboardShortcut(.space, modifiers: [])
                                Button("Show in Finder") { model.revealMedia(entry) }
                                Divider()
                                Button("Copy Image") { model.copyMedia(entry) }
                                ShareLink(item: entry.url) { Text("Share") }
                                Divider()
                                Button("Move to Trash") { model.requestTrash(entry) }.keyboardShortcut(.delete, modifiers: .command)
                            }
                            .id(entry.id)
                            .accessibilityAddTraits(isSelected ? .isSelected : [])
                            .accessibilityAction { select(entry) }
                            .accessibilityAction(named: "Open") { open(entry) }
                            .accessibilityAction(named: "Quick Look") { preview(entry) }
                    }
                }
                Text("Double-click to open. Press Space for Quick Look. Control-click for Copy, Show in Finder and Move to Trash.")
                    .font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.horizontal, 6)
            }
        }
    }

    private func select(_ entry: MediaEntry) { model.selectedMedia = entry.url; listFocused = true }
    private func open(_ entry: MediaEntry) { if tab == .demos { model.play(demo: entry.url) } else { model.openMedia(entry) } }
    private func preview(_ entry: MediaEntry) { if model.mediaFileExists(entry.url) { quickLookURL = entry.url } }
}

// MARK: - Matches

/// The Matches columns: the board's widths, narrower when the window is.
private struct MatchColumns {
    let ping: CGFloat, connection: CGFloat, action: CGFloat
    init(width: CGFloat) {
        let compact = width < 900
        ping = compact ? 76 : 100; connection = compact ? 116 : 130; action = compact ? 96 : 110
    }
}

private struct MatchColumnHeader: View {
    var columns: MatchColumns
    var body: some View {
        HStack(spacing: 16) {
            title("Map").frame(width: 80, alignment: .leading)
            title("Match").frame(maxWidth: .infinity, alignment: .leading)
            title("Ping").frame(width: columns.ping, alignment: .trailing)
            title("Connection").frame(width: columns.connection, alignment: .trailing)
            Color.clear.frame(width: columns.action, height: 1)
        }
        .padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.buttonTrailing(.large)).padding(.bottom, 8)
        .accessibilityHidden(true)
    }
    private func title(_ text: String) -> some View { GroupLabel(text, header: false) }
}

/// The Demos columns (Library board): Map, Recording, Recorded and Size over the rows.
private struct DemoColumnHeader: View {
    var body: some View {
        HStack(spacing: 16) {
            GroupLabel("Map", header: false).frame(width: 80, alignment: .leading)
            GroupLabel("Recording", header: false).frame(maxWidth: .infinity, alignment: .leading)
            GroupLabel("Recorded", header: false).frame(width: 130, alignment: .trailing)
            GroupLabel("Size", header: false).frame(width: 70, alignment: .trailing)
            // Room for Play and the More button, measured from a hidden Play.
            Label { Text("Play") } icon: { Image(systemName: "play.fill").font(.system(size: 11)) }
                .font(.system(size: 13, weight: .semibold)).padding(.horizontal, 14).hidden().frame(height: 1)
            Color.clear.frame(width: 28, height: 1)
        }
        .padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.buttonTrailing(.large)).padding(.bottom, 8)
        .accessibilityHidden(true)
    }
}

private struct MatchRow: View {
    var facts: MatchFacts
    var image: NSImage?
    var columns: MatchColumns
    var checkup: () -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        HStack(spacing: 16) {
            HStack(spacing: 16) {
                MapArtFrame(map: facts.map, image: image, size: .row, radius: RowMetrics.artRadius)
                VStack(alignment: .leading, spacing: 5) {
                    Text(facts.title).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                    Text(facts.stamp(time: true)).font(.typewriter(12)).foregroundStyle(palette.secondary).lineLimit(1)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                figure(facts.ping.map(String.init), unit: "ms").frame(width: columns.ping, alignment: .trailing)
                StatusLabel(facts.connection.kind, facts.connection.words).frame(width: columns.connection, alignment: .trailing)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(facts.spoken)
            .accessibilityAddTraits(.isStaticText)
            Button("Check-up", action: checkup).glassAction().controlSize(.large)
                .frame(width: columns.action, alignment: .trailing)
                .accessibilityLabel("Check-up for \(facts.title)")
                .help("Open Check-up for this match")
        }
        .padding(.vertical, RowMetrics.inset).padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.buttonTrailing(.large))
        .background(hovering ? palette.hover : .clear, in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(RowMetrics.radius), "match row")
        .onHover { hovering = $0 }
        .accessibilityElement(children: .contain)
    }
    private func figure(_ value: String?, unit: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 4) {
            Text(value ?? "—").font(.value).foregroundStyle(palette.heading)
            Text(unit).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineLimit(1)
        }
    }
}

// MARK: - Demos

/// The three steps for recording a demo, as on the No Recordings board.
private struct RecordingSteps: View {
    @Environment(\.palette) private var palette
    private let steps = [("1", "In a match, open the console", "~"), ("2", "Start recording", "/record"), ("3", "Stop when you’re done", "/stoprecord")]
    var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(steps.enumerated()), id: \.offset) { index, step in
                if index > 0 { Rectangle().fill(palette.separator).frame(height: 1) }
                HStack(spacing: 14) {
                    Text(step.0).font(.system(size: 15, weight: .semibold)).monospacedDigit().foregroundStyle(palette.accent).frame(width: 16)
                    Text(step.1).font(.system(size: 14)).foregroundStyle(palette.text)
                    Text(step.2).font(.typewriter(12)).foregroundStyle(palette.chipText)
                        .padding(.horizontal, 12).frame(height: 26)
                        .background(palette.button, in: Capsule()).overlay(Capsule().strokeBorder(palette.buttonStroke, lineWidth: 1))
                    Spacer(minLength: 0)
                }
                .padding(.horizontal, 20).frame(minHeight: 44)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Step \(step.0): \(step.1), \(step.2 == "~" ? "the tilde key" : "type \(step.2)")")
                .accessibilityAddTraits(.isStaticText)
            }
        }
        .frame(width: 440)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "recording steps")
    }
}

private struct MediaRenameSheet: View {
    @ObservedObject var model: LauncherModel
    var entry: MediaEntry
    @Environment(\.dismiss) private var dismiss
    @Environment(\.palette) private var palette
    @State private var name: String
    @State private var error: String?
    init(model: LauncherModel, entry: MediaEntry) {
        self.model = model; self.entry = entry
        _name = State(initialValue: entry.url.deletingPathExtension().lastPathComponent)
    }
    var body: some View {
        SheetScaffold(icon: .symbol("pencil"), title: "Rename recording", message: "Choose a name you’ll recognize in your library.", width: 460,
                      actions: [.cancel { dismiss() },
                                .primary("Rename", enabled: !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty) {
                                    error = model.renameMedia(entry, to: name)
                                    if error == nil { dismiss() }
                                }]) {
            VStack(alignment: .leading, spacing: 8) {
                LauncherTextField("Recording name", text: $name, shape: .capsule, monospaced: true, invalid: error != nil, autofocus: true)
                if let error {
                    Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true).padding(.leading, 8)
                }
            }
        }
    }
}

struct DemoRow: View {
    var entry: MediaEntry
    var image: NSImage?
    var selected = false
    var focused = false
    var play: () -> Void
    var select: () -> Void
    /// The More menu's entries, built when it opens.
    var more: () -> [PullDownMenu.Entry]
    @Environment(\.palette) private var palette
    @State private var hovering = false
    @State private var anchor = PullDownAnchorBox()
    var body: some View {
        let map = entry.facts.map.map { "mp_" + $0 } ?? entry.url.lastPathComponent
        let name = entry.url.lastPathComponent
        let mapName = entry.facts.map.map(MapCatalog.displayName)
        HStack(spacing: 16) {
            HStack(spacing: 16) {
                MapArtFrame(map: map, image: image, size: .row, radius: RowMetrics.artRadius)
                VStack(alignment: .leading, spacing: 5) {
                    Text(name).font(.typewriter(14, bold: true)).foregroundStyle(palette.heading).lineLimit(1).truncationMode(.middle)
                    Text([mapName, "Recorded match"].compactMap { $0 }.joined(separator: " · "))
                        .font(.system(size: 12)).foregroundStyle(palette.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Text(entry.facts.stamp).font(.typewriter(13)).foregroundStyle(palette.secondary).frame(width: 130, alignment: .trailing)
                Text(entry.facts.size).font(.system(size: 13)).foregroundStyle(palette.tertiary).monospacedDigit().frame(width: 70, alignment: .trailing)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(["Demo \(name)", mapName, entry.facts.stamp, entry.facts.size].compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: ", "))
            // Pressing the row selects it (Return then plays, Space previews, Command-Delete moves it to the Trash).
            .accessibilityAddTraits(selected ? [.isButton, .isSelected] : .isButton)
            .accessibilityAction { select() }
            .accessibilityAction(named: "Play") { play() }
            Button(action: play) { Label { Text("Play") } icon: { Image(systemName: "play.fill").font(.system(size: 11)) } }
                .glassAction().controlSize(.large)
                .accessibilityLabel("Play \(name)")
            Button {
                let entries = more()
                if let view = anchor.view { PullDownMenu.show(entries, below: view) }
            } label: {
                Image(systemName: "ellipsis.circle").font(.system(size: 17)).foregroundStyle(palette.chipIcon)
                    .frame(width: 28, height: 28).contentShape(Circle())
            }
                .buttonStyle(.plain)
                .background(PullDownAnchor(box: anchor))
                .accessibilityLabel("More actions for \(name)")
                .help("More actions")
        }
        .padding(.vertical, RowMetrics.inset).padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.buttonTrailing(.large))
        .background(selected ? palette.selection : hovering ? palette.hover : .clear, in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous)
            .strokeBorder(selected ? (focused ? palette.accent.opacity(0.75) : palette.selectionStroke) : .clear, lineWidth: selected && focused ? 2 : 1))
        .contentShape(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(RowMetrics.radius), "demo row")
        .onHover { hovering = $0 }
        .accessibilityElement(children: .contain)
    }
}

// MARK: - Screenshots

/// A screenshot fitted inside its 4:3 tile (never cropped or stretched), with the map, or the file name, and the date.
struct ScreenshotTile: View {
    var entry: MediaEntry
    var snapshot: Bool
    var selected = false
    var focused = false
    @Environment(\.palette) private var palette
    @State private var thumbnail: NSImage?
    var body: some View {
        let caption = entry.facts.map.map(MapCatalog.displayName) ?? entry.url.deletingPathExtension().lastPathComponent
        let shape = RoundedRectangle(cornerRadius: 16, style: .continuous)
        VStack(spacing: 0) {
            ZStack {
                Palette.hex(0x1b1d16)
                if let thumbnail { Image(nsImage: thumbnail).resizable().aspectRatio(contentMode: .fit) }
                // A screenshot is a picture of the game, so its stand-in doesn't follow the appearance.
                else { SceneThumbnail(scenery: MapCatalog.scenery(entry.facts.map ?? entry.url.lastPathComponent), dark: true) }
            }
            .aspectRatio(4 / 3, contentMode: .fit)
            .clipped()
            HStack {
                Text(caption).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1).truncationMode(.middle)
                Spacer(minLength: 8)
                Text(entry.facts.stamp).font(.typewriter(12)).foregroundStyle(palette.secondary).lineLimit(1)
            }
            .padding(.vertical, 10).padding(.horizontal, 14)
        }
        .background(palette.panel)
        .clipShape(shape)
        .overlay(shape.strokeBorder(selected ? (focused ? palette.accent.opacity(0.75) : palette.selectionStroke) : palette.panelStroke, lineWidth: selected && focused ? 2 : 1))
        .contentShape(shape)
        .shapeAudit(.container, .rounded(16), "screenshot tile")
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Screenshot, \(caption), \(entry.facts.stamp)")
        .accessibilityAddTraits(.isButton)
        .task(id: entry.url) {
            guard !snapshot else { return }
            let url = entry.url
            let image = await Task.detached(priority: .utility) { () -> CGImage? in
                guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
                return CGImageSourceCreateThumbnailAtIndex(source, 0, [kCGImageSourceCreateThumbnailFromImageAlways: true, kCGImageSourceCreateThumbnailWithTransform: true,
                                                                       kCGImageSourceThumbnailMaxPixelSize: 900] as CFDictionary)
            }.value
            if let image { thumbnail = NSImage(cgImage: image, size: NSSize(width: image.width, height: image.height)) }
        }
    }
}
