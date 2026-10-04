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
                LauncherSplitView(model: model).transition(.opacity)
            }
        }
        // Controls follow the app accent (NSAccentColorName: brass in dark, olive in light);
        // only the one prominent action per view is tinted explicitly.
        .environment(\.palette, palette)
        .background(palette.ground)
        .overlay(alignment: .bottom) { NoticeBanner(model: model).padding(.bottom, 24) }
        .frame(minWidth: 1100, minHeight: 760)
    }
}

struct LauncherSplitView: View {
    @ObservedObject var model: LauncherModel
    @State private var columns = NavigationSplitViewVisibility.all
    @Environment(\.palette) private var palette
    var body: some View {
        NavigationSplitView(columnVisibility: $columns) {
            Sidebar(model: model)
                .navigationSplitViewColumnWidth(min: 220, ideal: 232, max: 300)
        } detail: {
            Group {
                switch model.page {
                case .deploy: HomeView(model: model)
                case .servers: ServersView(model: model)
                case .library: LibraryView(model: model)
                case .settings: SettingsView(model: model)
                case .about: AboutView(model: model)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(palette.ground)
        }
        .hiddenToolbarTitle()
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
            withAnimation(.launcherSpring) { model.page = page }
            if page == .library { model.loadMedia() }
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
                    }
                } header: { Text("Favorites") }
            }
        }
        .listStyle(.sidebar)
        .sidebarRowSize()
        .safeAreaInset(edge: .top, spacing: 0) { BrandHeader().padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 18) }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            PlayerTag(name: model.playerName, verified: model.keyVerified).padding(.horizontal, 12).padding(.bottom, 12)
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
        .background(palette.dark ? Color.white.opacity(0.05) : Color.white.opacity(0.5), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.8), lineWidth: 1))
        .accessibilityElement(children: .combine)
    }
}

// MARK: - Shared content components

struct Eyebrow: View {
    var text: String
    var size: CGFloat = 13
    var tracking: CGFloat = 0.32
    var color: Color?
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text.uppercased()).font(.stencil(size, heavy: false)).tracking(size * tracking).foregroundStyle(color ?? palette.accent)
    }
}

struct PageHeader: View {
    var eyebrow: String? = nil
    var title: String
    var subtitle: String? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            if let eyebrow { Eyebrow(text: eyebrow) }
            Text(title).font(.system(size: 30, weight: .bold)).tracking(-0.3).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            if let subtitle { Text(subtitle).font(.system(size: 13)).foregroundStyle(palette.secondary) }
        }
    }
}

/// Content panel: opaque, quiet and concentric with its rows. Never glass.
struct ContentPanel<Content: View>: View {
    var radius: CGFloat = 22
    var padding = EdgeInsets(top: 20, leading: 22, bottom: 20, trailing: 22)
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 0) { content }
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .topLeading)
            .background(palette.panel, in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
    }
}

struct NoticeBanner: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        if !model.notice.isEmpty {
            HStack(spacing: 12) {
                Image(systemName: model.crashReport == nil ? "info.circle" : "exclamationmark.triangle").foregroundStyle(palette.accent)
                Text(model.notice).font(.system(size: 13)).foregroundStyle(palette.text).textSelection(.enabled).lineLimit(3)
                if let report = model.crashReport {
                    Button("Show Crash Report") { NSWorkspace.shared.activateFileViewerSelecting([report]) }.glassAction()
                }
                Button { withAnimation(reduceMotion ? nil : .launcherSpring) { model.notice = "" } } label: { Image(systemName: "xmark") }
                    .buttonStyle(.plain).foregroundStyle(palette.secondary).accessibilityLabel("Dismiss message")
            }
            .padding(.vertical, 12).padding(.horizontal, 18)
            .frame(maxWidth: 720)
            .glassSurface(Capsule())
            .transition(reduceMotion ? .opacity : .move(edge: .bottom).combined(with: .opacity))
            .accessibilityElement(children: .contain)
        }
    }
}

/// Pins a group of toolbar items to the trailing edge: ToolbarSpacer on macOS 26, a flexible space before.
struct TrailingToolbar<Items: View>: ViewModifier {
    @ViewBuilder var items: Items
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar { ToolbarSpacer(.flexible); ToolbarItemGroup(placement: .automatic) { items } }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar { ToolbarItem(placement: .automatic) { Spacer() }; ToolbarItemGroup(placement: .automatic) { items } }
    }
}
extension View {
    func trailingToolbar<Items: View>(@ViewBuilder _ items: () -> Items) -> some View { modifier(TrailingToolbar(items: items)) }
}

/// Reads the detail column's origin in window coordinates so content can keep the design's grid.
struct WindowOriginReader: View {
    @Binding var origin: CGPoint
    var body: some View {
        GeometryReader { proxy in
            Color.clear.onAppear { origin = proxy.frame(in: .global).origin }
                .onChange(of: proxy.frame(in: .global).origin) { origin = $0 }
        }
    }
}
