import SwiftUI
import AppKit
import UniformTypeIdentifiers

/// Configs: one column with Discover first and the person's loadouts under it, and the chosen
/// page beside it: Player Setups, Saved, Check a Config, or a loadout's Settings, Binds, Check-up
/// or History. Selecting a loadout only shows it; “Use for next game” is the one action that
/// changes which loadout the game starts with.
struct ConfigsView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    init(model: LauncherModel) { self.model = model; studio = model.studio }

    var body: some View {
        Group {
            if let session = studio.session {
                ConfigsShell(model: model, studio: studio, session: session, discover: model.discover)
            } else {
                ConfigsUnavailable(snapshot: model.snapshot)
            }
        }
        .onAppear { studio.loadCatalog() }
    }
}

private struct ConfigsUnavailable: View {
    var snapshot: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        StudioEmptyState(title: "UNAVAILABLE",
                         message: snapshot ? "Configs need a game folder, which this preview doesn’t have."
                                           : "CoD2 Unbound couldn’t open the folder where your loadouts live. Check that your CoD2 Unbound folder is writable, then reopen the app.")
            .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct ConfigsShell: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.palette) private var palette
    @Environment(\.undoManager) private var undoManager
    @State private var origin = CGPoint.zero
    @State private var pendingLeave: (@MainActor () -> Void)?
    @FocusState private var searchFocused: Bool
    /// The traffic lights and the sidebar button span the titlebar's first 140 points.
    private static let windowControls: CGFloat = 140

    var body: some View {
        HStack(spacing: 0) {
            // The same column on every Configs page, so moving between Discover and a loadout changes
            // only what is beside it.
            LoadoutColumn(model: model, studio: studio, session: session, discover: discover,
                          select: { id in leaveText { discover.page = nil; studio.select(id) } },
                          act: { action in leaveText(action) },
                          open: { page in leaveText { if page == .home { discover.openFront() } else { discover.open(page) } } })
                .frame(width: StudioMetrics.loadoutColumn)
                // The column's title shares the titlebar row with the app sidebar beside it; with that
                // sidebar hidden, the row holds the window controls, so the column starts below the toolbar.
                .ignoresSafeArea(.container, edges: origin.x < Self.windowControls ? [] : .top)
            Rectangle().fill(palette.separator).frame(width: 1).ignoresSafeArea()
            Group { if discover.page != nil { ConfigDiscoverView(model: model, discover: discover) } else { content } }
                .ignoresSafeArea(.container, edges: .top)
        }
        .padding(.leading, 20)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(WindowOriginReader(origin: $origin))
        .environment(\.configControlContext, session.current.map { ConfigControlContext(studio: studio, session: session, profileID: $0.profile.id) })
        .onAppear {
            studio.refreshComparison(); studio.checkup.load()
            // Choosing Configs in the sidebar for the first time opens Discover.
            if discover.landsOnHome { discover.landsOnHome = false; if discover.page == nil { discover.open(.home) } }
        }
        .onChange(of: studio.gameRunning) { _, running in if running { studio.binds.gameStarted() } }
        .onChange(of: session.current?.version) { _, _ in
            studio.refreshComparison()
            if studio.textMode && !studio.textEdited { studio.loadText() }
        }
        // A loadout made, imported or chosen elsewhere is shown in place of Discover.
        .onChange(of: session.current?.profile.id) { _, _ in discover.page = nil }
        .configImportFlows(discover)
        // The toolbar keeps its two places on every page: what the page is about in the middle (a
        // loadout's sections, or what a config is checked for) and its search, status and actions
        // trailing. The leading edge sits over the Configs column's title, so nothing goes there.
        .toolbar {
            if discover.page == nil { ToolbarItem(placement: .principal) {
                Picker("Loadout section", selection: Binding(get: { studio.tab }, set: { tab in leaveText { studio.tab = tab } })) {
                    ForEach(ConfigStudioTab.available) { Text($0.title).tag($0) }
                }
                .segmentedChoice()
                .disabled(session.current == nil)
            } }
            if discover.page == .checker { ToolbarItem(placement: .principal) { ConfigCheckerScope(checker: discover.checker) } }
        }
        .trailingToolbar { if discover.page == .checker {
            ConfigCheckerAnother(checker: discover.checker)
        } else if let page = discover.page {
            ToolbarSearchField(prompt: "Search players, teams or styles", width: 280, text: $discover.query, focus: $searchFocused, request: discover.findRequest)
                .help("Search setups by player, team, era or where they were published (⌘F)")
            // The catalogue's count and Refresh, as Servers shows its list's; a player's page has neither.
            if case .player = page {} else { CatalogueStatus(discover: discover) }
        } else {
            if studio.tab == .history {
                Button(action: studio.saveSnapshot) { Label("Save a Snapshot Now", systemImage: "clock.badge.checkmark") }
                    .disabled(studio.isReadOnly || session.current == nil)
                    .help(studio.isReadOnly ? "Configs are read-only while Call of Duty 2 is running" : "Save how this loadout is set up now as a snapshot")
            }
            // The shortcut is a plain hint beside the label, never a boxed key. A toolbar button is
            // read by its title, so VoiceOver says the shortcut too.
            Button { studio.jumpPresented = true } label: {
                Label { Text("Jump to Any Setting  \(Text("⌘K").foregroundStyle(palette.hint))") } icon: { Image(systemName: "magnifyingglass") }
                    .labelStyle(.titleAndIcon)
            }
                .keyboardShortcut("k", modifiers: .command)
                .disabled(studio.catalog == nil || session.current == nil)
                .help("Jump to any setting (⌘K)")
            moreMenu
        } }
        .sheet(isPresented: $studio.jumpPresented) { JumpPalette(studio: studio).sheetShapeAudit() }
        .confirmationDialog("Apply your text edits?", isPresented: Binding(get: { pendingLeave != nil }, set: { if !$0 { pendingLeave = nil } })) {
            Button("Apply Edits") { studio.applyText(undo: undoManager); if !studio.textEdited { let action = pendingLeave; pendingLeave = nil; action?() } }
                .disabled(studio.textBlocked || studio.isReadOnly)
            Button("Discard Edits", role: .destructive) { studio.revertText(); let action = pendingLeave; pendingLeave = nil; action?() }
            Button("Keep Editing", role: .cancel) { pendingLeave = nil }
        } message: {
            Text("Your text edits aren’t saved to this loadout yet.")
        }
    }

    /// Text edits are applied explicitly; leaving them asks first.
    private func leaveText(_ action: @escaping @MainActor () -> Void) {
        if studio.textMode && studio.textEdited { pendingLeave = action } else { action() }
    }

    private static let moreIcon = NSImage(systemSymbolName: "ellipsis.circle", accessibilityDescription: "More Actions for This Loadout") ?? NSImage()
    /// The loadout's actions, the same as its row's context menu and the Loadout menu. A command that
    /// can't run now is dimmed, never hidden.
    private var moreMenu: some View {
        Menu {
            Button("New Loadout…") { leaveText(model.newLoadout) }.disabled(!model.canNewLoadout)
            if let current = session.current {
                let name = current.profile.displayName
                Button("Use for Next Game") { studio.useForNextGame() }.disabled(current.profile.isActive || studio.isReadOnly)
                Divider()
                Button("Duplicate “\(name)”") { leaveText { model.duplicateLoadout() } }.disabled(!model.canDuplicateLoadout)
                Button("Rename “\(name)”…") { model.renameLoadout() }.disabled(!model.canRenameLoadout)
                Button("Export for Windows…") { model.exportLoadout() }.disabled(!model.canExportLoadout)
                Button("Show in Finder") { model.showLoadoutInFinder(current.profile.id) }
                Divider()
                Button("Delete “\(name)”…") { leaveText { model.requestDeleteLoadout() } }.disabled(!model.canDeleteLoadout)
            }
        } label: {
            Label { Text("More Actions for This Loadout") } icon: { Image(nsImage: Self.moreIcon) }.labelStyle(.iconOnly)
        }
        // A borderless menu is an AppKit pop-up button, which VoiceOver names from its image's
        // description (the symbol alone says “More”); the default toolbar menu's node exposes no name.
        .menuStyle(.borderlessButton).menuIndicator(.hidden)
        .accessibilityLabel("More Actions for This Loadout")
        .help("More actions for this loadout")
    }

    @ViewBuilder private var content: some View {
        VStack(alignment: .leading, spacing: StudioMetrics.sectionGap) {
            if let current = session.current {
                LoadoutHeader(studio: studio, session: session, current: current)
                ConfigBanners(studio: studio, session: session)
                if studio.catalog == nil {
                    if let error = studio.catalogError {
                        Banner(.error, title: "Settings aren’t available", message: error)
                        Spacer(minLength: 0)
                    } else {
                        HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Loading settings…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .accessibilityElement(children: .combine)
                    }
                } else {
                    switch studio.tab {
                    case .settings: ConfigSettingsView(studio: studio, session: session, openSettings: { model.openSettings(.general) },
                                                       toggleText: { leaveText { studio.toggleText() } })
                    case .binds: ConfigBindsView(studio: studio, session: session)
                    case .checkup: ConfigCheckupView(studio: studio, session: session)
                    case .history: ConfigHistoryView(studio: studio, session: session)
                    }
                }
            } else {
                ConfigBanners(studio: studio, session: session)
                StudioEmptyState(title: "NO LOADOUTS",
                                 message: session.migrationError.map { "Your current settings couldn’t be brought into a loadout yet: \($0)" }
                                    ?? "Your current game settings become your first loadout, Main, the first time CoD2 Unbound prepares a game. You can also start a new one.",
                                 action: model.canNewLoadout ? ("New Loadout…", { model.newLoadout() }) : nil)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .padding(.top, 82).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 22)
        // AppKit measures the window's minimum size by proposing zero width; wrapping banner text
        // would then report an enormous height and push the page off the window.
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
}

// MARK: - Loadout column

private struct LoadoutColumn: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @ObservedObject var discover: ConfigDiscoverModel
    var select: (String) -> Void
    /// Runs a loadout command once any unapplied text edits are dealt with.
    var act: (@escaping @MainActor () -> Void) -> Void
    var open: (ConfigDiscoverPage) -> Void
    @Environment(\.palette) private var palette
    /// Focus is shown the way a native list shows it: the selected row's edge, not a ring around the column.
    @FocusState private var focused: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Configs").font(.system(size: 22, weight: .bold)).tracking(-0.2).foregroundStyle(palette.heading)
                .padding(.horizontal, 10).padding(.bottom, 16).accessibilityAddTraits(.isHeader)
            ScrollView {
                VStack(alignment: .leading, spacing: 4) {
                    DiscoverColumnSection(discover: discover, open: open)
                    GroupLabel("My loadouts").padding(.horizontal, 10).padding(.top, 22).padding(.bottom, 6)
                    ForEach(session.profiles) { profile in
                        LoadoutRow(profile: profile, subtitle: subtitle(profile),
                                   selected: discover.page == nil && profile.id == session.current?.profile.id, focused: focused) { select(profile.id) }
                            .contextMenu { contextMenu(profile) }
                    }
                    ColumnAction(title: "New Loadout", symbol: "plus", help: studio.isReadOnly ? "Configs are read-only while Call of Duty 2 is running"
                                                                                           : "Make a new loadout from the game’s defaults or a copy") {
                        act(model.newLoadout)
                    }
                    .disabled(!model.canNewLoadout)
                    AddConfigRow(discover: discover)
                }
            }
            .scrollIndicators(.never)
            .focusable(interactions: .edit).focused($focused).focusEffectDisabled()
            .onMoveCommand { direction in
                let profiles = session.profiles
                guard let index = profiles.firstIndex(where: { $0.id == session.current?.profile.id }) else { return }
                let next = direction == .down ? min(profiles.count - 1, index + 1) : direction == .up ? max(0, index - 1) : index
                if next != index { select(profiles[next].id) }
            }
            Text("Each loadout is its own Call of Duty 2 player profile, so the game sees exactly what you set here.")
                .font(.system(size: 11)).foregroundStyle(palette.tertiary).lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true).padding(.horizontal, 10).padding(.top, 8)
        }
        .padding(.top, 26).padding(.horizontal, 12).padding(.bottom, 20)
        .frame(maxHeight: .infinity, alignment: .top)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Discover and loadouts")
        .task { await discover.loadIfNeeded() }
    }

    /// The loadout's actions: the Loadout menu's, for this row (Menus board › Loadout).
    @ViewBuilder private func contextMenu(_ profile: ConfigProfile) -> some View {
        let readOnly = studio.isReadOnly || !model.canNavigate
        Button("Use for Next Game") { act { model.useLoadoutForNextGame(profile.id) } }.disabled(profile.isActive || readOnly)
        Divider()
        Button("Duplicate") { act { model.duplicateLoadout(profile.id) } }.disabled(readOnly)
        Button("Rename…") { act { model.renameLoadout(profile.id) } }.disabled(readOnly)
        Button("Export…") { act { model.exportLoadout(profile.id) } }.disabled(studio.catalog == nil || !model.canNavigate)
        Button("Show in Finder") { model.showLoadoutInFinder(profile.id) }
        Divider()
        Button("Delete Loadout…") { act { model.requestDeleteLoadout(profile.id) } }.disabled(studio.deleteBlock(profile.id) != nil || !model.canNavigate)
    }

    private func subtitle(_ profile: ConfigProfile) -> String {
        if profile.isActive { return "In use" }
        if profile.id == session.current?.profile.id, let origin = session.current?.metadata.origin { return LoadoutHeader.provenance(origin) }
        return "Ready to use"
    }
}

/// A command row under the loadouts (New Loadout, Add a Config): a dashed tile and accent words.
private struct ColumnAction: View {
    var title: String
    var symbol: String
    var help: String
    var menu = false
    var action: () -> Void
    @Environment(\.palette) private var palette
    @Environment(\.isEnabled) private var enabled
    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: symbol).font(.system(size: 13, weight: .semibold))
                    .frame(width: 30, height: 30)
                    .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.accent.opacity(0.5), style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
                    .accessibilityHidden(true)
                Text(title).font(.system(size: 14, weight: .semibold))
                if menu { Image(systemName: "chevron.down").font(.system(size: 10, weight: .bold)).accessibilityHidden(true) }
                Spacer(minLength: 0)
            }
            .foregroundStyle(palette.accent)
            .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 12))
        .opacity(enabled ? 1 : 0.45)
        .help(help)
        .accessibilityLabel(title)
    }
}

/// Add a Config: every place a config can come from (File › Import Config… ⌘O, Paste Config ⇧⌘V, a
/// link, a Windows install), whether or not Discover is showing.
private struct AddConfigRow: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @State private var anchor = MenuAnchorBox()
    var body: some View {
        ColumnAction(title: "Add a Config", symbol: "square.and.arrow.down", help: "Add a config from a file, a link, your Windows install or the clipboard", menu: true) {
            if let view = anchor.view { NativeMenu.popUp(AddConfigButton.items(discover, window: view.window), below: view,
                                                         header: AddConfigButton.header, footnote: AddConfigButton.footnote) }
        }
        .background(MenuAnchor(box: anchor))
        .disabled(discover.studio.session == nil || discover.step != nil)
        .accessibilityHint("Shows the places you can add a config from")
    }
}

private struct LoadoutRow: View {
    var profile: ConfigProfile
    var subtitle: String
    var selected: Bool
    var focused: Bool
    var action: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                MonogramTile(text: String(profile.displayName.prefix(1)).uppercased())
                VStack(alignment: .leading, spacing: 3) {
                    Text(profile.displayName).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1).truncationMode(.tail)
                    Text(subtitle).font(.system(size: 11)).foregroundStyle(palette.secondary).lineLimit(1)
                }
                Spacer(minLength: 4)
                if profile.isActive {
                    Image(systemName: "checkmark").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.positive).accessibilityHidden(true)
                }
            }
            .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            // While the column has the keyboard focus, the selected row's edge is the accent itself.
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous)
                .strokeBorder(selected ? (focused ? palette.accent : palette.selectionStroke) : .clear, lineWidth: selected && focused ? 2 : 1))
            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 12))
        .accessibilityLabel("\(profile.displayName), \(profile.isActive ? "in use for your next game" : subtitle)")
        .accessibilityHint("Shows this loadout. Control-click for its actions.")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

/// The column's 30-point tile (.ctile): a loadout's stencil initial, or a page's symbol.
struct MonogramTile: View {
    var text: String = ""
    var symbol: String? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        Group {
            if let symbol { Image(systemName: symbol).font(.system(size: 13, weight: .semibold)) } else { Text(text).font(.stencil(16)) }
        }
        .foregroundStyle(palette.accent)
        .frame(width: 30, height: 30)
        .background(palette.tileFill, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.tileStroke, lineWidth: 1))
        .accessibilityHidden(true)
    }
}

// MARK: - Header and banners

struct LoadoutHeader: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    var current: ConfigProfileRead
    @Environment(\.palette) private var palette

    static func provenance(_ origin: ConfigOrigin) -> String {
        switch origin.kind {
        case .created: return "Made in CoD2 Unbound"
        case .copied: return origin.source.map { "Copied from \($0)" } ?? "A copy"
        case .imported: return origin.credit.map { "Imported from \($0)" } ?? "Imported"
        case .migration: return "Your earlier launcher settings"
        }
    }
    private var stamp: String? {
        guard let last = session.history.first else { return nil }
        let day = DateFormatter(), time = DateFormatter()
        day.setLocalizedDateFormatFromTemplate("dMMM"); time.setLocalizedDateFormatFromTemplate("HHmm")
        return (session.history.count == 1 ? "Added " : "Edited ") + day.string(from: last.date) + " at " + time.string(from: last.date)
    }

    var body: some View {
        // Side by side when there's room; otherwise the controls wrap under the name.
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .bottom, spacing: 20) { identity.layoutPriority(1); Spacer(minLength: 12); controls }
            VStack(alignment: .leading, spacing: 12) { identity; controls }
        }
    }

    private var identity: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(current.profile.displayName).font(.system(size: 30, weight: .bold)).tracking(-0.3).foregroundStyle(palette.heading)
                .lineLimit(1).truncationMode(.tail)
                .accessibilityAddTraits(.isHeader)
            Text(([Self.provenance(current.metadata.origin)] + [stamp].compactMap { $0 }).joined(separator: " · "))
                .font(.system(size: 13)).monospacedDigit().foregroundStyle(palette.secondary).lineLimit(1)
        }
        .padding(.leading, 4)
    }

    /// As drawn: Guided shows the in-use badge; Expert shows its changed count; History its source filter.
    private var controls: some View {
        HStack(spacing: 12) {
            if studio.tab == .settings && studio.level == .expert {
                Text("\(studio.changedCount) changed from the game’s defaults").font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize()
            }
            if !current.profile.isActive {
                Button("Use for Next Game", action: studio.useForNextGame).glassAction().actionControlSize().disabled(studio.isReadOnly)
                    .help(studio.isReadOnly ? "Configs are read-only while Call of Duty 2 is running" : "The game starts with this loadout next time")
            } else {
                StatusLabel(.ready, "In use for your next game").padding(.trailing, 8)
            }
            if studio.tab == .settings {
                Picker("Detail level", selection: $studio.level) { ForEach(ConfigDetailLevel.allCases) { Text($0.title).tag($0) } }
                    .segmentedChoice()
            } else if studio.tab == .binds {
                BindsLayoutPicker(binds: studio.binds)
            } else if studio.tab == .checkup {
                TuneForPicker(checkup: studio.checkup)
            } else if studio.tab == .history {
                // Five sources: a pop-up, not a segmented control.
                HStack(spacing: 10) {
                    Text("Show changes from").font(.system(size: 13)).foregroundStyle(palette.secondary).accessibilityHidden(true)
                    PopUpButton("Show changes from", selection: $studio.historyFilter, options: ConfigHistoryFilter.allCases.map { (value: $0, title: $0.title) })
                }
            }
        }
    }
}

private struct BindsLayoutPicker: View {
    @ObservedObject var binds: ConfigBindsModel
    var body: some View {
        Picker("Binds view", selection: $binds.layout) {
            ForEach(ConfigBindsLayout.allCases) { Text($0.title).tag($0) }
        }
        .segmentedChoice()
    }
}

private struct TuneForPicker: View {
    @ObservedObject var checkup: ConfigCheckupModel
    @Environment(\.palette) private var palette
    var body: some View {
        // In the narrowest window the words would wrap onto two lines; the control then stands alone.
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 10) {
                Text("Tune for").font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize().accessibilityHidden(true)
                picker
            }
            picker
        }
    }
    private var picker: some View {
        Picker("Tune for", selection: $checkup.tuneFor) { ForEach(ConfigTuneFor.allCases, id: \.self) { Text($0.title).tag($0) } }
            .segmentedChoice()
    }
}

/// Read-only, failed-save and read-error banners. Actions sit beside the banner text.
struct ConfigBanners: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette
    @State private var recovery: ConfirmationRequest?
    private var keepDiskVersion: ConfirmationRequest {
        ConfirmationRequest(title: "Keep the version on disk?",
                            message: "A change to this loadout was interrupted, and the file changed on disk since. Keeping the disk version saves the interrupted change in the loadout’s recovery folder instead of applying it.",
                            actionTitle: "Keep Version on Disk", destructive: false) { [studio, session] in
            do { try session.keepDiskVersion() } catch { studio.failure = ConfigStudioFailure(title: "Couldn’t keep the version on disk", message: error.localizedDescription, retry: nil) }
        }
    }

    var body: some View {
        if let notice = studio.notice {
            Banner(.neutral, symbol: "checkmark.circle", message: notice) {
                HStack(spacing: 8) {
                    if let action = studio.noticeAction { Button(action.title) { action.run() }.glassAction() }
                    Button("Dismiss") { studio.notice = nil }.glassAction()
                }
                .fixedSize()
            }
        }
        if studio.gameRunning {
            // Every control that can't change the loadout now is dimmed; this is the one place that says why.
            Banner(.neutral, symbol: "lock", message: "Configs are read-only while Call of Duty 2 is running. Changes you make in game appear here as the game saves them.")
        }
        if let failure = studio.failure {
            Banner(.error, title: failure.title, message: failure.message) {
                HStack(spacing: 8) {
                    if failure.retry != nil { Button("Try Again", action: studio.retryFailure).glassAction().disabled(studio.isReadOnly) }
                    Button(session.draft != nil ? "Discard Change" : "Dismiss", action: studio.discardFailedChange).glassAction()
                }
                .fixedSize()
            }
        } else if let error = session.error {
            Banner(.error, title: "Couldn’t read this loadout", message: error) {
                HStack(spacing: 8) {
                    Button("Try Again", action: studio.refresh).glassAction()
                    if let id = session.focusedProfileID ?? session.profiles.first(where: \.isActive)?.id {
                        Button("Review…") {
                            do { try session.inspectRecovery(profileID: id); recovery = keepDiskVersion }
                            catch { studio.failure = ConfigStudioFailure(title: "Nothing to recover", message: error.localizedDescription, retry: nil) }
                        }
                        .glassAction().disabled(studio.isReadOnly)
                    }
                }
                .fixedSize()
            }
            .confirmationAlert($recovery)
        }
    }
}

// MARK: - Shared pieces

/// Design Rules empty state on the shared EmptyState: stencil headline, one line, at most one action.
struct StudioEmptyState: View {
    var title: String
    var message: String
    var action: (title: String, run: () -> Void)? = nil
    var body: some View {
        EmptyState(title, message: message, actionTitle: action?.title, action: action?.run)
            .padding(.vertical, 60).padding(.horizontal, 20)
    }
}

/// A rail row's label: purpose groups and filters. The rail is a native selection List, so the
/// system draws selection, focus and the selected state; the row only supplies its words.
struct StudioRailRow: View {
    var title: String
    var symbol: String
    var count: Int? = nil
    var body: some View {
        Label {
            HStack(spacing: 4) {
                Text(title).lineLimit(1)
                Spacer(minLength: 4)
                if let count { Text("\(count)").font(.system(size: 11)).monospacedDigit().foregroundStyle(.secondary) }
            }
        } icon: {
            Image(systemName: symbol)
        }
        .font(.system(size: 13, weight: .medium))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(count.map { "\(title), \($0) settings" } ?? title)
        .accessibilityAddTraits(.isStaticText)
    }
}
