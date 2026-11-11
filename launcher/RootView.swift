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
                LauncherSplitView(model: model).disabled(model.gameRunning).transition(.opacity)
            }
        }
        // Controls follow the app accent (NSAccentColorName: brass in dark, olive in light);
        // only the one prominent action per view is tinted explicitly.
        .capsuleControls()
        .background(palette.ground)
        .background(LauncherWindowReader(model: model))
        .safeAreaInset(edge: .bottom, spacing: 0) {
            if !model.notice.isEmpty { NoticeBanner(model: model).padding(12).frame(maxWidth: .infinity).background(palette.ground) }
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
        override func viewDidMoveToWindow() { super.viewDidMoveToWindow(); if let window { model.launcherWindow = window } }
    }
}

struct LauncherSplitView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.palette) private var palette
    var body: some View {
        NavigationSplitView(columnVisibility: $model.sidebarVisibility) {
            Sidebar(model: model)
                .disabled(!model.canNavigate)
                .navigationSplitViewColumnWidth(min: 220, ideal: 232, max: 300)
        } detail: {
            Group {
                switch model.page {
                case .home: HomeView(model: model)
                case .servers: ServersView(model: model)
                case .library: LibraryView(model: model)
                case .configs: ConfigsView(model: model)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
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

struct Sidebar: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        List(selection: Binding(get: { model.page }, set: { page in
            guard let page else { return }
            model.navigate(to: page)
        })) {
            Section {
                // Icons take the app accent (brass/olive), or the accent the person chose in System Settings.
                ForEach(LauncherPage.navigation) { page in
                    Label(page.title, systemImage: page.symbol).tag(page)
                }
            }
            if !model.favoriteServers.isEmpty {
                Section {
                    ForEach(model.favoriteServers.prefix(6)) { server in
                        Button { model.showServer(server.address) } label: {
                            Label { Text(Self.shortName(server.name)).lineLimit(1) } icon: { Image(systemName: "star.fill").foregroundStyle(palette.dark ? Palette.hex(0xc99a4c) : palette.link) }
                        }
                        .buttonStyle(.plain)
                        .accessibilityLabel("Favorite server \(QuakeColors.plain(server.name))")
                        .contextMenu {
                            Button("Launch") { model.connect(server) }.disabled(!model.canNavigate)
                            Button("Show in Server List") { model.showServer(server.address) }
                            Divider()
                            Button("Remove from Favorites") { model.favorite(server.address) }
                            Button("Copy Address") {
                                NSPasteboard.general.clearContents()
                                NSPasteboard.general.setString(server.address, forType: .string)
                                model.notice = "Server address copied."
                            }
                        }
                    }
                } header: { Text("Favorites") }
            }
            Section {
                // Settings is its own window (⌘,), not a page; this row opens it.
                Button { model.openSettings() } label: { Label("Settings", systemImage: "gearshape") }
                    .buttonStyle(.plain)
                    .help("Open Settings (⌘,)")
            }
        }
        .listStyle(.sidebar)
        .sidebarRowSize()
        .safeAreaInset(edge: .top, spacing: 0) { BrandHeader().padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 18) }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            PlayerTag(name: model.playerName, verified: model.keyVerified)
                .help("Your player name comes from the active in-game profile.").padding(.horizontal, 12).padding(.bottom, 12)
        }
    }
    static func shortName(_ name: String) -> String {
        let plain = QuakeColors.plain(name)
        return plain.split(separator: "|").first.map { $0.trimmingCharacters(in: .whitespaces) } ?? plain
    }
}

private extension View {
    @ViewBuilder func sidebarRowSize() -> some View {
        if #available(macOS 14.0, *) { self.environment(\.sidebarRowSize, .large) } else { self }
    }
}

struct BrandHeader: View {
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 10) {
            BrandMark().frame(width: 34, height: 34)
            VStack(alignment: .leading, spacing: 3) {
                Text("COD2").font(.stencil(24)).tracking(0.96).foregroundStyle(palette.dark ? Palette.hex(0xf0e7d0) : Palette.hex(0x2c3018))
                Text("SILICON").font(.system(size: 10, weight: .semibold)).tracking(3.4).foregroundStyle(palette.dark ? Palette.hex(0xb9b19c) : palette.secondary)
            }
            Spacer(minLength: 0)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("CoD2 Silicon")
        .accessibilityAddTraits(.isHeader)
    }
}

struct PlayerTag: View {
    var name: String
    var verified: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 12) {
            MiniDogTag()
            VStack(alignment: .leading, spacing: 4) {
                QuakeName(name: name).font(.system(size: 13, weight: .semibold)).lineLimit(1)
                Text(verified ? "CD key verified" : "CD key needed").font(.system(size: 11)).foregroundStyle(palette.secondary)
            }
            Spacer(minLength: 0)
        }
        .padding(12)
        .modifier(PlayerTagPlate(fill: palette.dark ? Color.white.opacity(0.05) : Color.white.opacity(0.5),
                                 stroke: palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.8)))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Player: \(QuakeColors.plain(name)). \(verified ? "CD key verified" : "CD key needed"). Name from your active in-game profile.")
        .accessibilityAddTraits(.isStaticText)
    }
}

/// The tag sits in the window's bottom corner: on macOS 26 its corners follow the window's
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
    private var banner: some View {
        Toast(symbol: model.crashReport == nil ? "info.circle" : "exclamationmark.triangle", message: model.notice) {
            if model.gameRunning { Button("Return to Game", action: model.returnToGame).glassAction().actionControlSize() }
            if let report = model.crashReport {
                Button("Show Crash Report") { NSWorkspace.shared.activateFileViewerSelecting([report]) }.glassAction().actionControlSize()
            }
            if !model.gameRunning {
                Button("Dismiss") { withAnimation(.easeInOut(duration: 0.15)) { model.notice = "" } }
                    .glassAction().actionControlSize().accessibilityLabel("Dismiss message")
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Status message")
    }
}
