import SwiftUI
import AppKit

struct LauncherRoot: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.colorScheme) private var scheme
    @Environment(\.colorSchemeContrast) private var contrast
    var body: some View {
        let palette = Palette.of(scheme, contrast: contrast)
        ZStack {
            if model.onboard {
                SetupView(model: model).transition(.opacity)
            } else {
                // While the game runs Home shows Return to Game and Quit Game; the sidebar and the
                // menus keep other pages out of reach until it ends (`canNavigate`).
                LauncherSplitView(model: model).transition(.opacity)
                    .overlay { if model.jumpPresented { JumpToPalette(model: model) } }
                    .dropDestination(for: URL.self) { urls, _ in
                        // A config or a demo dropped on the window: import review, or the demo plays.
                        let files = urls.filter { $0.isFileURL && ["cfg", "dm_1"].contains($0.pathExtension.lowercased()) }
                        guard !files.isEmpty else { return false }
                        model.open(files)
                        return true
                    }
            }
        }
        // Controls follow the app accent (NSAccentColorName: brass in dark, olive in light);
        // only the one prominent action per view is tinted explicitly.
        .capsuleControls()
        .background(palette.ground)
        .background(LauncherWindowReader(model: model))
        .safeAreaInset(edge: .bottom, spacing: 0) {
            // Over setup the message takes the window's width; beside the sidebar it stays in the page
            // (LauncherSplitView), so the player plate is never covered.
            if model.onboard && !model.notice.isEmpty { NoticeBanner(model: model).padding(12).frame(maxWidth: .infinity).background(palette.ground) }
        }
        .onChange(of: model.notice, initial: true) { _, message in
            guard !message.isEmpty else { return }
            NSAccessibility.post(element: model.launcherWindow ?? NSApplication.shared, notification: .announcementRequested,
                                 userInfo: [.announcement: message, .priority: NSAccessibilityPriorityLevel.medium.rawValue])
        }
        .appSheets(model, sheet: $model.sheet, confirmation: $model.confirmation)
        // The minimum window is 1100×760 points in all. This view sits below the unified toolbar's
        // 52 points of safe area, so asking it for 760 would hang 26 points off the window's bottom.
        .frame(minWidth: 1100, minHeight: 760 - 52)
        .environment(\.palette, palette)
    }
}

/// Keep file panels attached to this window, including in the review harness.
private struct LauncherWindowReader: NSViewRepresentable {
    let model: LauncherModel
    func makeNSView(context: Context) -> WindowView { WindowView(model: model) }
    func updateNSView(_ view: WindowView, context: Context) { view.model = model; if let window = view.window { model.launcherWindow = window } }
    final class WindowView: NSView {
        var model: LauncherModel
        init(model: LauncherModel) { self.model = model; super.init(frame: .zero) }
        required init?(coder: NSCoder) { nil }
        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            guard let window else { return }
            model.launcherWindow = window
            if model.hideLauncherWindowOnAppear {
                model.hideLauncherWindowOnAppear = false
                if model.gameRunning { Task { @MainActor in window.orderOut(nil) } }
            }
        }
    }
}

struct LauncherSplitView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.palette) private var palette
    var body: some View {
        NavigationSplitView(columnVisibility: $model.sidebarVisibility) {
            Sidebar(model: model)
                .navigationSplitViewColumnWidth(min: 220, ideal: 240, max: 300)
        } detail: {
            Group {
                switch model.page {
                case .home: HomeView(model: model)
                case .servers:
                    if model.serverScope == .hosting { HostingView(model: model, hosting: model.hosting) } else { ServersView(model: model) }
                case .library: LibraryView(model: model)
                case .configs: ConfigsView(model: model)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .safeAreaInset(edge: .bottom, spacing: 0) {
                if !model.notice.isEmpty { NoticeBanner(model: model).padding(12).frame(maxWidth: .infinity).background(palette.ground) }
            }
            .background(palette.ground)
        }
        .hiddenToolbarTitle()
        .animation(reduceMotion ? .easeInOut(duration: 0.15) : .launcherSpring, value: model.page)
    }
}

private extension View {
    @ViewBuilder func hiddenToolbarTitle() -> some View {
        if #available(macOS 15.0, *) { self.toolbar(removing: .title) } else { self.navigationTitle("") }
    }
}

// MARK: - Sidebar

/// The sidebar (Side board): brand, the four pages with their shortcuts, starred servers, a labelled
/// Settings row, and the player's plate, which opens the player card.
struct Sidebar: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        List(selection: Binding(get: { model.page }, set: { page in
            guard let page else { return }
            model.navigate(to: page)
        })) {
            Section {
                ForEach(LauncherPage.navigation) { page in
                    SidebarRow(title: page.title, symbol: page.symbol, keys: "⌘\(page.shortcut)", spoken: "Command \(page.shortcut)")
                        .tag(page)
                }
            }
            .disabled(!model.canNavigate)
            let favorites = model.favoriteServers
            if !favorites.isEmpty {
                Section {
                    ForEach(favorites) { server in FavoriteRow(model: model, server: server) }
                } header: { Text("Favorites") }
            }
        }
        .listStyle(.sidebar)
        .sidebarRowSize()
        .safeAreaInset(edge: .top, spacing: 0) { BrandHeader().padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 18) }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            VStack(spacing: 0) {
                // Settings is its own window (⌘,): a visible row, so nobody has to know the shortcut.
                Button { model.openSettings() } label: {
                    SidebarRow(title: "Settings", symbol: "gearshape", keys: "⌘,", spoken: "Command comma")
                        .padding(.horizontal, 10).frame(height: 36).contentShape(Rectangle())
                }
                .buttonStyle(RowButtonStyle(radius: 10))
                .disabled(model.onboard || model.installingUpdate)
                .help("Open Settings (⌘,)")
                .padding(.horizontal, 10)
                Rectangle().fill(palette.separator).frame(height: 1).padding(.horizontal, 18).padding(.vertical, 8)
                PlayerPlate(model: model).padding(.horizontal, 12).padding(.bottom, 12)
            }
        }
    }
    static func shortName(_ name: String) -> String {
        let plain = QuakeColors.plain(name)
        return plain.split(separator: "|").first.map { $0.trimmingCharacters(in: .whitespaces) } ?? plain
    }
}

/// One sidebar row: symbol, title and its shortcut, read as “Home, Command 1”.
private struct SidebarRow: View {
    var title: String, symbol: String, keys: String, spoken: String
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 8) {
            Label(title, systemImage: symbol)
            Spacer(minLength: 6)
            Text(keys).font(.system(size: 12)).foregroundStyle(palette.secondary).opacity(0.85).monospacedDigit()
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(title), \(spoken)")
        .accessibilityAddTraits(.isStaticText)
    }
}

/// A starred server: click shows it in Servers; Control-click has the Menus board's context menu.
private struct FavoriteRow: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer
    @Environment(\.palette) private var palette
    var body: some View {
        let name = Sidebar.shortName(server.name)
        let count = server.maxPlayers > 0 ? "\(server.playerCount)/\(server.maxPlayers)" : nil
        Button { model.showServer(server.address) } label: {
            HStack(spacing: 8) {
                Label { Text(name).lineLimit(1).truncationMode(.tail) } icon: { Image(systemName: "star.fill").foregroundStyle(palette.accent) }
                Spacer(minLength: 6)
                if let count { Text(count).font(.system(size: 12)).foregroundStyle(palette.secondary).monospacedDigit() }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!model.canNavigate)
        .accessibilityLabel("Favorite server \(name)" + (count.map { _ in ", \(server.playerCount) of \(server.maxPlayers) players" } ?? ""))
        .accessibilityHint("Shows it in Servers")
        .contextMenu {
            Button("Launch") { model.connect(server) }.disabled(!model.canNavigate)
            Button("Show in Servers") { model.showServer(server.address) }.disabled(!model.canNavigate)
            Divider()
            Button("Copy Address") { model.copyServer(server) }
            Divider()
            Button("Remove from Favorites…") { model.requestRemoveFavorite(server) }
        }
    }
}

private extension View {
    @ViewBuilder func sidebarRowSize() -> some View {
        if #available(macOS 14.0, *) { self.environment(\.sidebarRowSize, .large) } else { self }
    }
}

struct BrandHeader: View {
    var body: some View {
        HStack(spacing: 10) {
            BrandMark().frame(width: 36, height: 36)
            BrandWordmark(height: 15)
            Spacer(minLength: 0)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("CoD2 Unbound")
        .accessibilityAddTraits(.isHeader)
    }
}

/// The player's plate at the bottom of the sidebar: their name in its colours and the next game's
/// loadout. It opens the player card.
struct PlayerPlate: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let plain = QuakeColors.plain(model.playerName)
        Button { model.playerCardPresented.toggle() } label: {
            HStack(spacing: 12) {
                MiniDogTag().scaleEffect(0.87).frame(width: 26, height: 33)
                VStack(alignment: .leading, spacing: 3) {
                    QuakeName(name: model.playerName).font(.system(size: 13, weight: .semibold)).lineLimit(1)
                    Text(model.playerPlateDetail).font(.system(size: 11)).foregroundStyle(palette.secondary).lineLimit(1)
                }
                Spacer(minLength: 0)
                Image(systemName: "chevron.right").font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.tertiary)
            }
            .padding(.horizontal, 12).padding(.vertical, 10)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .modifier(PlayerTagPlate(fill: palette.dark ? Color.white.opacity(0.05) : Color.white.opacity(0.5),
                                 stroke: palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.8)))
        .disabled(model.onboard)
        .help("Your name, CD key and the loadout for your next game")
        .accessibilityLabel("\(plain), \(model.keyVerified ? "CD key verified" : "no CD key yet"). Open your player card")
        .popover(isPresented: $model.playerCardPresented, arrowEdge: .trailing) { PlayerCardPopover(model: model) }
    }
}

/// The plate sits in the window's bottom corner: on macOS 26 its corners follow the window's
/// (concentric, never tighter than 10 points); earlier versions keep a fixed 16.
private struct PlayerTagPlate: ViewModifier {
    var fill: Color, stroke: Color
    @Environment(\.usesGlass) private var usesGlass
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, #available(macOS 26.0, *) {
            let shape = ConcentricRectangle(corners: .concentric(minimum: .fixed(10)), isUniform: true)
            content.background(fill, in: shape).overlay(shape.stroke(stroke, lineWidth: 1))
        } else { fixed(content) }
        #else
        fixed(content)
        #endif
    }
    private func fixed(_ content: Content) -> some View {
        content.background(fill, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(stroke, lineWidth: 1))
    }
}

struct NoticeBanner: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        if !model.notice.isEmpty {
            // The toast draws its glass and its buttons' in one container, not glass sampling glass.
            banner
                .transition(reduceMotion ? .opacity : .move(edge: .bottom).combined(with: .opacity))
        }
    }
    /// On one line it is 48 points tall, a capsule with 28-point buttons 10 points in (concentric with
    /// its 24-point ends). A message that wraps keeps every word; the buttons stay 10 points from the
    /// bottom corner, and from the second line on the top corner is clear of them.
    private var banner: some View {
        GlassContainer {
            HStack(alignment: .bottom, spacing: 12) {
                message.padding(.vertical, 16).frame(maxWidth: .infinity, alignment: .leading)
                actions.padding(.bottom, 10)
            }
            .padding(.leading, 20).padding(.trailing, 10)
            .frame(minHeight: 48)
            .frame(maxWidth: 720)
            .glassSurface(RoundedRectangle(cornerRadius: 24, style: .continuous))
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Status message")
    }
    private var message: some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Image(systemName: model.crashReport == nil ? "info.circle" : "exclamationmark.triangle")
                .foregroundStyle(model.crashReport == nil ? palette.accent : palette.caution).accessibilityHidden(true)
            Text(model.notice).font(.system(size: 13)).foregroundStyle(palette.text).textSelection(.enabled)
                .fixedSize(horizontal: false, vertical: true).id(model.notice)
        }
    }
    @ViewBuilder private var actions: some View {
        HStack(spacing: 8) {
            if model.gameRunning { Button("Return to Game", action: model.returnToGame).glassAction().controlSize(.large) }
            if let report = model.crashReport {
                Button(report.lastPathComponent.hasPrefix("game-") ? "Show Game Log" : "Show Crash Report") { NSWorkspace.shared.activateFileViewerSelecting([report]) }.glassAction().controlSize(.large)
            }
            if !model.gameRunning {
                SymbolButton("xmark", label: "Dismiss message", help: "Dismiss") {
                    withAnimation(.easeInOut(duration: 0.15)) { model.notice = "" }
                }
                .controlSize(.large)
            }
        }
    }
}
