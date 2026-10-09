import SwiftUI
import AppKit

/// Configs: the person's loadouts in a middle column, and the selected loadout's
/// Settings or History beside it. Selecting a loadout only shows it; “Use for next
/// game” is the one action that changes which loadout the game starts with.
struct ConfigsView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    init(model: LauncherModel) { self.model = model; studio = model.studio }

    var body: some View {
        Group {
            if let session = studio.session {
                ConfigsShell(model: model, studio: studio, session: session)
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
                                           : "CoD2 Silicon couldn’t open the folder where your loadouts live. Check that your CoD2 Silicon folder is writable, then reopen the app.")
            .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct ConfigsShell: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette
    @Environment(\.undoManager) private var undoManager
    @State private var origin = CGPoint.zero
    @State private var newLoadout: NewLoadoutRequest?
    @State private var renaming = false
    @State private var jumping = false
    @State private var pendingLeave: (@MainActor () -> Void)?

    var body: some View {
        HStack(spacing: 0) {
            LoadoutColumn(studio: studio, session: session, select: { id in leaveText { studio.select(id) } },
                          create: { newLoadout = NewLoadoutRequest(copy: false) })
                .frame(width: StudioMetrics.loadoutColumn)
            Rectangle().fill(palette.separator).frame(width: 1).ignoresSafeArea()
            content
        }
        .padding(.leading, max(0, 252 - origin.x))
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onAppear { studio.refreshComparison() }
        .onChange(of: session.current?.version) { _ in
            studio.refreshComparison()
            if studio.textMode && !studio.textEdited { studio.loadText() }
        }
        .toolbar {
            ToolbarItem(placement: .principal) {
                Picker("Loadout section", selection: Binding(get: { studio.tab }, set: { tab in leaveText { studio.tab = tab } })) {
                    ForEach(ConfigStudioTab.available) { Text($0.title).tag($0) }
                }
                .pickerStyle(.segmented).labelsHidden().fixedSize()
                .disabled(session.current == nil)
            }
        }
        .trailingToolbar {
            if studio.tab == .history {
                Button(action: studio.saveSnapshot) { Label("Save a Snapshot Now", systemImage: "clock.badge.checkmark") }
                    .disabled(studio.isReadOnly || session.current == nil)
                    .help("Save how this loadout is set up now as a snapshot")
            }
            Button { jumping = true } label: { Label("Jump to Any Setting", systemImage: "magnifyingglass").labelStyle(.titleAndIcon) }
                .keyboardShortcut("k", modifiers: .command)
                .disabled(studio.catalog == nil || session.current == nil)
                .help("Jump to any setting (⌘K)")
            moreMenu
        }
        .sheet(item: $newLoadout) { request in
            NewLoadoutSheet(studio: studio, copy: request.copy)
        }
        .sheet(isPresented: $renaming) { RenameSheet(studio: studio) }
        .sheet(isPresented: $jumping) { JumpPalette(studio: studio) }
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

    private var moreMenu: some View {
        Menu {
            Button("New Loadout…") { newLoadout = NewLoadoutRequest(copy: false) }
            if let current = session.current {
                Button("Duplicate “\(current.profile.displayName)”…") { newLoadout = NewLoadoutRequest(copy: true) }
                Button("Rename “\(current.profile.displayName)”…") { renaming = true }
                Divider()
                Button("Use for Next Game", action: studio.useForNextGame).disabled(current.profile.isActive || studio.isReadOnly)
                Button("Show in Finder") {
                    let file = session.home.appendingPathComponent("main/players/\(current.profile.id)/config_mp.cfg")
                    NSWorkspace.shared.activateFileViewerSelecting([file])
                }
            }
        } label: {
            Label("More Actions for This Loadout", systemImage: "ellipsis.circle")
        }
        .help("More actions for this loadout")
    }

    @ViewBuilder private var content: some View {
        VStack(alignment: .leading, spacing: StudioMetrics.sectionGap) {
            if let current = session.current {
                LoadoutHeader(studio: studio, session: session, current: current)
                ConfigBanners(studio: studio, session: session)
                if studio.catalog == nil {
                    if let error = studio.catalogError {
                        StudioBanner(title: "Settings aren’t available", detail: error, tone: .negative)
                        Spacer(minLength: 0)
                    } else {
                        HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Loading settings…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .accessibilityElement(children: .combine)
                    }
                } else {
                    switch studio.tab {
                    case .history: ConfigHistoryView(studio: studio, session: session)
                    default: ConfigSettingsView(studio: studio, session: session, openSettings: { model.page = .settings })
                    }
                }
            } else {
                ConfigBanners(studio: studio, session: session)
                StudioEmptyState(title: "NO LOADOUTS",
                                 message: session.migrationError.map { "Your current settings couldn’t be brought into a loadout yet: \($0)" }
                                    ?? "Your current game settings become your first loadout, Main, the first time CoD2 Silicon prepares a game. You can also start a new one.",
                                 action: ("New Loadout…", { newLoadout = NewLoadoutRequest(copy: false) }))
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .padding(.top, 82).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 22)
        // AppKit measures the window's minimum size by proposing zero width; wrapping banner text
        // would then report an enormous height and push the page off the window.
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
}

private struct NewLoadoutRequest: Identifiable {
    let id = UUID()
    var copy: Bool
}

// MARK: - Loadout column

private struct LoadoutColumn: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    var select: (String) -> Void
    var create: () -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Configs").font(.system(size: 22, weight: .bold)).tracking(-0.2).foregroundStyle(palette.heading)
                .padding(.horizontal, 10).padding(.bottom, 16).accessibilityAddTraits(.isHeader)
            ColumnLabel(text: "MY LOADOUTS").padding(.horizontal, 10).padding(.bottom, 6)
            ScrollView {
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(session.profiles) { profile in
                        LoadoutRow(profile: profile, subtitle: subtitle(profile), selected: profile.id == session.current?.profile.id) { select(profile.id) }
                    }
                    Button(action: create) {
                        HStack(spacing: 10) {
                            Image(systemName: "plus").font(.system(size: 13, weight: .semibold))
                                .frame(width: 30, height: 30)
                                .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.accent.opacity(0.5), style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
                            Text("New Loadout").font(.system(size: 14, weight: .semibold))
                            Spacer(minLength: 0)
                        }
                        .foregroundStyle(palette.accent)
                        .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
                        .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                    }
                    .buttonStyle(RowButtonStyle(radius: 12))
                    .disabled(studio.isReadOnly)
                    .opacity(studio.isReadOnly ? 0.45 : 1)
                    .help("Make a new loadout from the game’s defaults or a copy")
                }
            }
            .scrollIndicators(.never)
            .focusable()
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
        .accessibilityLabel("Loadouts")
    }

    private func subtitle(_ profile: ConfigProfile) -> String {
        if profile.isActive { return "In use" }
        if profile.id == session.current?.profile.id, let origin = session.current?.metadata.origin { return LoadoutHeader.provenance(origin) }
        return "Ready to use"
    }
}

private struct ColumnLabel: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.system(size: 11, weight: .semibold)).tracking(0.88).foregroundStyle(palette.tertiary).accessibilityAddTraits(.isHeader)
    }
}

private struct LoadoutRow: View {
    var profile: ConfigProfile
    var subtitle: String
    var selected: Bool
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
                    Image(systemName: "checkmark.circle.fill").font(.system(size: 14)).foregroundStyle(palette.positive).accessibilityHidden(true)
                }
            }
            .padding(.vertical, 6).padding(.horizontal, 10).frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 12))
        .accessibilityLabel("\(profile.displayName), \(profile.isActive ? "in use for your next game" : subtitle)")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

struct MonogramTile: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.stencil(16)).foregroundStyle(palette.accent)
            .frame(width: 30, height: 30)
            .background(palette.dark ? Palette.hex(0x1b1f17) : palette.crate, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous).strokeBorder(palette.dark ? Color.white.opacity(0.09) : palette.wellStroke, lineWidth: 1))
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
        case .created: return "Made in CoD2 Silicon"
        case .copied: return origin.source.map { "Copied from \($0)" } ?? "A copy"
        case .imported: return origin.credit.map { "Imported from \($0)" } ?? "Imported"
        case .migration: return "Your earlier launcher settings"
        }
    }
    private var stamp: String? {
        guard let last = session.history.first else { return nil }
        return (session.history.count == 1 ? "ADDED " : "EDITED ") + StencilDate.dayAndTime(last.date)
    }

    var body: some View {
        // Side by side when there's room; otherwise the controls wrap under the name.
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .bottom, spacing: 20) { identity.layoutPriority(1); Spacer(minLength: 12); controls }
            VStack(alignment: .leading, spacing: 12) { identity; controls }
        }
    }

    private var identity: some View {
        VStack(alignment: .leading, spacing: 6) {
            Eyebrow(text: current.profile.isActive ? "Loadout · In use" : "Loadout")
            Text(current.profile.displayName.uppercased()).font(StudioTypography.loadoutTitle).tracking(0.92).foregroundStyle(palette.heading)
                .lineLimit(1).truncationMode(.tail)
                .accessibilityLabel(current.profile.displayName).accessibilityAddTraits(.isHeader)
            HStack(spacing: 10) {
                Text(Self.provenance(current.metadata.origin)).font(.system(size: 13)).foregroundStyle(palette.secondary).lineLimit(1)
                if let stamp {
                    Circle().fill(palette.tertiary).frame(width: 3, height: 3).accessibilityHidden(true)
                    Text(stamp).font(.typewriter(12)).foregroundStyle(palette.address).lineLimit(1)
                }
            }
            .accessibilityElement(children: .combine)
        }
    }

    /// As drawn: Guided shows the in-use badge; Expert shows its changed count; History its source filter.
    private var controls: some View {
        HStack(spacing: 12) {
            if studio.tab == .settings && studio.level == .expert {
                Text("\(studio.changedCount) changed from the game’s defaults").font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize()
            }
            if !current.profile.isActive {
                Button("Use for Next Game", action: studio.useForNextGame).glassAction().disabled(studio.isReadOnly)
                    .help("The game starts with this loadout next time")
            } else if studio.tab == .settings && studio.level == .guided {
                Label("In use for your next game", systemImage: "checkmark.circle")
                    .font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.positive)
                    .padding(.horizontal, 12).frame(height: 28)
                    .overlay(Capsule().strokeBorder(palette.positive.opacity(0.45), lineWidth: 1))
                    .fixedSize()
            }
            if studio.tab == .settings {
                Picker("Detail level", selection: $studio.level) { ForEach(ConfigDetailLevel.allCases) { Text($0.title).tag($0) } }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
            } else if studio.tab == .history {
                Picker("Show changes from", selection: $studio.historyFilter) { ForEach(ConfigHistoryFilter.allCases) { Text($0.title).tag($0) } }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
            }
        }
    }
}

/// Read-only, failed-save and read-error banners. Actions sit beside the banner text.
struct ConfigBanners: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette
    @State private var confirmRecovery = false

    var body: some View {
        if studio.gameRunning {
            StudioBanner(title: "Configs are read-only while Call of Duty 2 is running. Changes you make in game appear here as the game saves them.",
                         tone: .information)
        }
        if let failure = studio.failure {
            HStack(alignment: .center, spacing: 12) {
                StudioBanner(title: failure.title, detail: failure.message, tone: .negative)
                VStack(spacing: 8) {
                    if failure.retry != nil { Button("Try Again", action: studio.retryFailure).glassAction().disabled(studio.isReadOnly) }
                    Button(session.draft != nil ? "Discard Change" : "Dismiss", action: studio.discardFailedChange).glassAction()
                }
                .fixedSize()
            }
        } else if let error = session.error {
            HStack(alignment: .center, spacing: 12) {
                StudioBanner(title: "Couldn’t read this loadout", detail: error, tone: .negative)
                VStack(spacing: 8) {
                    Button("Try Again", action: studio.refresh).glassAction()
                    if let id = session.focusedProfileID ?? session.profiles.first(where: \.isActive)?.id {
                        Button("Review…") {
                            do { try session.inspectRecovery(profileID: id); confirmRecovery = true }
                            catch { studio.failure = ConfigStudioFailure(title: "Nothing to recover", message: error.localizedDescription, retry: nil) }
                        }
                        .glassAction().disabled(studio.isReadOnly)
                    }
                }
                .fixedSize()
            }
            .alert("Keep the version on disk?", isPresented: $confirmRecovery) {
                Button("Keep Version on Disk") {
                    do { try session.keepDiskVersion() } catch { studio.failure = ConfigStudioFailure(title: "Couldn’t keep the version on disk", message: error.localizedDescription, retry: nil) }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("A change to this loadout was interrupted, and the file changed on disk since. Keeping the disk version saves the interrupted change in the loadout’s recovery folder instead of applying it.")
            }
        }
    }
}

// MARK: - Shared pieces

/// Design Rules empty state: stencil headline, one line, at most one action.
struct StudioEmptyState: View {
    var title: String
    var message: String
    var action: (title: String, run: () -> Void)? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 10) {
            Text(title).font(.stencil(26)).tracking(3.1).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary).accessibilityAddTraits(.isHeader)
            Text(message).font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).frame(maxWidth: 420)
                .fixedSize(horizontal: false, vertical: true)
            if let action { Button(action.title, action: action.run).glassAction().padding(.top, 4) }
        }
        .padding(.vertical, 60).padding(.horizontal, 20)
        .accessibilityElement(children: .contain)
    }
}

/// A rail row: purpose groups and filters. Selected rows take the light glass highlight.
struct StudioRailButton: View {
    var title: String
    var symbol: String
    var count: Int? = nil
    var selected: Bool
    var action: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: symbol).font(.system(size: 13)).frame(width: 18)
                    .foregroundStyle(selected ? (palette.dark ? palette.accent : palette.control) : palette.chipIcon).accessibilityHidden(true)
                Text(title).font(.system(size: 13, weight: .medium)).foregroundStyle(selected ? palette.heading : palette.text).lineLimit(1)
                Spacer(minLength: 4)
                if let count { Text("\(count)").font(.system(size: 11)).monospacedDigit().foregroundStyle(palette.tertiary) }
            }
            .padding(.horizontal, 10).frame(height: 34)
            .background(selected ? (palette.dark ? Color.white.opacity(0.12) : Color.white.opacity(0.7)) : .clear, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            .contentShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 10))
        .accessibilityLabel(count.map { "\(title), \($0) settings" } ?? title)
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

// MARK: - Sheets

private struct NewLoadoutSheet: View {
    @ObservedObject var studio: ConfigStudioModel
    @State var copy: Bool
    @State private var name = ""
    @Environment(\.dismiss) private var dismiss
    @Environment(\.palette) private var palette

    var body: some View {
        let source = studio.current?.profile.displayName
        VStack(alignment: .leading, spacing: 16) {
            Text("New loadout").font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            TextField("Name", text: $name, prompt: Text("For example, Scrim")).textFieldStyle(.roundedBorder).font(.system(size: 14))
            Picker("Start from", selection: $copy) {
                Text("The game’s defaults").tag(false)
                if let source { Text("A copy of \(source)").tag(true) }
            }
            .pickerStyle(.radioGroup)
            Text("It won’t be used for your next game until you choose it.").font(StudioTypography.help).foregroundStyle(palette.secondary)
            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }.glassAction().keyboardShortcut(.cancelAction)
                Button("Create Loadout") { if studio.createLoadout(named: name, copyingCurrent: copy && source != nil) { dismiss() } }
                    .prominentAction().keyboardShortcut(.defaultAction)
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty || studio.isReadOnly)
            }
        }
        .padding(26).frame(width: 420)
    }
}

private struct RenameSheet: View {
    @ObservedObject var studio: ConfigStudioModel
    @State private var name = ""
    @Environment(\.dismiss) private var dismiss
    @Environment(\.undoManager) private var undoManager
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Rename loadout").font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            TextField("Name", text: $name).textFieldStyle(.roundedBorder).font(.system(size: 14))
            Text("The game’s own profile menu keeps showing the folder name, \(studio.current?.profile.id ?? "").")
                .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }.glassAction().keyboardShortcut(.cancelAction)
                Button("Rename") { studio.rename(name, undo: undoManager); dismiss() }
                    .prominentAction().keyboardShortcut(.defaultAction)
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty || studio.isReadOnly)
            }
        }
        .padding(26).frame(width: 420)
        .onAppear { name = studio.current?.profile.displayName ?? "" }
    }
}
