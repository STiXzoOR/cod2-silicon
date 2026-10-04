import SwiftUI
import AppKit

struct HomeView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }

    var body: some View {
        let last = model.lastServer
        let map = last?.map ?? "mp_toujane"
        GeometryReader { proxy in
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    heroText(last).padding(.top, 150)
                    Spacer(minLength: 28)
                    StatusChips(model: model)
                    HStack(alignment: .top, spacing: 20) {
                        RecentServersPanel(model: model)
                        DispatchesPanel(model: model)
                    }
                    .frame(height: max(236, proxy.size.height - 638))
                    .padding(.top, 38)
                }
                .padding(.leading, max(32, 300 - origin.x)).padding(.trailing, 32).padding(.bottom, 28)
                .frame(minHeight: max(proxy.size.height, 760), alignment: .top)
            }
            .scrollIndicators(.never)
            .background(alignment: .topLeading) {
                // The window-space slice of the hero; the system mirrors it under the floating sidebar.
                HeroBackdrop(map: map, image: artwork.image(for: map), leading: origin.x)
                    .frame(width: proxy.size.width, height: 620)
                    .heroExtension()
            }
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .trailingToolbar {
            Button { model.checkUpdates() } label: { Label("Check for updates", systemImage: "arrow.down.to.line") }
                .help("Check GitHub Releases for a newer version. Nothing is downloaded.")
            Button { model.openHomeFolder() } label: { Label("Show CoD2 Silicon folder", systemImage: "folder") }
                .help("Show configs, demos and screenshots in Finder.")
        }
    }

    @ViewBuilder private func heroText(_ last: GameServer?) -> some View {
        VStack(alignment: .leading, spacing: 18) {
            if let last {
                Eyebrow(text: ["Last deployment", MapCatalog.region(last.map)].compactMap { $0 }.joined(separator: " · "), size: 15, tracking: 0.34)
                StencilTitle(text: MapCatalog.displayName(last.map).uppercased())
                MetaLine(items: [AnyView(QuakeName(name: last.name).font(.system(size: 16, weight: .semibold))),
                                 AnyView(Text(GameModes.long(last.gametype))),
                                 AnyView(Text("\(last.playerCount) / \(last.maxPlayers) players")),
                                 AnyView(Text("\(last.ping) ms"))])
            } else {
                Eyebrow(text: "Ready for deployment", size: 15, tracking: 0.34)
                StencilTitle(text: "COD2 SILICON")
                MetaLine(items: [AnyView(Text("No recent servers yet")), AnyView(Text("Deploy opens the main menu"))])
            }
            HStack(spacing: 14) {
                Button(action: model.deploy) {
                    Label("Deploy", systemImage: "play.fill").labelStyle(HeroLabelStyle())
                }
                .prominentAction().heroControlSize()
                .accessibilityLabel(last.map { "Deploy to \(QuakeColors.plain($0.name))" } ?? "Deploy to the main menu")
                Button { withAnimation(.launcherSpring) { model.page = .servers } } label: {
                    Label("Find a server", systemImage: "dot.radiowaves.left.and.right").labelStyle(HeroLabelStyle())
                }
                .glassAction().heroControlSize()
                Text("⌘ Return to deploy").font(.system(size: 12)).foregroundStyle(palette.dark ? HeroInk.hint.dark : HeroInk.hint.light).padding(.leading, 6)
            }
            .padding(.top, 14)
        }
        .frame(maxWidth: 760, alignment: .leading)
    }
}

/// 132-point stencil display title with the design's 0.86 line height.
struct StencilTitle: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        let size: CGFloat = 132
        let font = NSFont(name: LauncherFonts.stencilHeavy, size: size)
        let natural = font.map { $0.ascender - $0.descender } ?? size * 1.2
        Text(text)
            .font(.stencil(size)).tracking(size * 0.02)
            .foregroundStyle(palette.dark ? palette.heading : Palette.hex(0x1f2214))
            .lineLimit(1).minimumScaleFactor(0.5)
            .padding(.vertical, -(natural - size * 0.86) / 2)
            .shadow(color: .black.opacity(palette.dark ? 0.45 : 0), radius: 20, y: 6)
            .accessibilityAddTraits(.isHeader)
    }
}

struct MetaLine: View {
    var items: [AnyView]
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 14) {
            ForEach(items.indices, id: \.self) { index in
                if index > 0 { Circle().fill(palette.dark ? Palette.hex(0x8c8573) : Palette.hex(0x8a8572)).frame(width: 4, height: 4).accessibilityHidden(true) }
                items[index]
            }
        }
        .font(.system(size: 16))
        .foregroundStyle(palette.dark ? Palette.hex(0xddd5c3) : Palette.hex(0x3a382e))
        .accessibilityElement(children: .combine)
    }
}

struct HeroLabelStyle: LabelStyle {
    func makeBody(configuration: Configuration) -> some View {
        HStack(spacing: 10) { configuration.icon.font(.system(size: 15, weight: .semibold)); configuration.title }
            .font(.system(size: 17, weight: .semibold))
            .padding(.horizontal, 8).frame(minHeight: 36)
    }
}

extension View {
    @ViewBuilder func heroControlSize() -> some View {
        if #available(macOS 14.0, *) { self.controlSize(.extraLarge) } else { self.controlSize(.large) }
    }
    /// Lets the hero art continue under the floating sidebar on macOS 26.
    @ViewBuilder func heroExtension() -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) { self.backgroundExtensionEffect() } else { self }
        #else
        self
        #endif
    }
}

/// Fallback scene or the player's loading screen, with grain, vignette and the scrims that keep text legible.
struct HeroBackdrop: View {
    var map: String
    var image: NSImage?
    /// Window x of this view: fallback art is composed in window coordinates, like the design.
    var leading: CGFloat = 0
    @Environment(\.palette) private var palette
    var body: some View {
        ZStack {
            if let image {
                MapArt(map: map, image: image, hero: true)
                let stats = HeroLuminance.stats(image)
                if palette.dark {
                    // Sized for the dimmest text over the art (the brass eyebrow and the hint), and held
                    // at full strength across the text column (to 74% of the width) before easing off.
                    let scrim = ScrimMath.darkScrim(high: stats.high, text: HeroInk.limitingLuminance(dark: true))
                    LinearGradient(stops: [.init(color: .black.opacity(scrim), location: 0), .init(color: .black.opacity(scrim), location: 0.74),
                                           .init(color: .black.opacity(0.15), location: 0.95)], startPoint: .leading, endPoint: .trailing)
                } else {
                    let wash = ScrimMath.lightWash(low: stats.low, text: HeroInk.limitingLuminance(dark: false), paper: ScrimMath.luminance(0xef, 0xe8, 0xd8))
                    LinearGradient(stops: [.init(color: palette.ground.opacity(wash), location: 0), .init(color: palette.ground.opacity(wash), location: 0.74),
                                           .init(color: palette.ground.opacity(0.1), location: 0.95)], startPoint: .leading, endPoint: .trailing)
                }
            } else {
                HeroScene(scenery: MapCatalog.scenery(map), dark: palette.dark, leading: leading)
                GrainOverlay(opacity: palette.dark ? 0.13 : 0.10, blend: palette.dark ? .overlay : .multiply)
                if !palette.dark {
                    LinearGradient(stops: [.init(color: palette.ground.opacity(0.85), location: 0.14), .init(color: palette.ground.opacity(0), location: 0.6)], startPoint: .leading, endPoint: .trailing)
                }
            }
            if palette.dark {
                EllipticalGradient(stops: [.init(color: .clear, location: 0.55), .init(color: .black.opacity(0.62), location: 1)],
                                   center: UnitPoint(x: 0.62, y: 0.42), startRadiusFraction: 0, endRadiusFraction: 0.78)
            }
            LinearGradient(stops: [.init(color: palette.ground.opacity(0), location: palette.dark ? 0.62 : 0.6), .init(color: palette.ground, location: 1)],
                           startPoint: .top, endPoint: .bottom)
        }
        .accessibilityHidden(true)
    }
}

/// Colours of the text set over the hero, and the one that limits the scrim in each appearance.
enum HeroInk {
    static let dark: [UInt32] = [0xf3ecdc, 0xddd5c3, 0xe2bd72, 0xc9c1ad]   // title, meta, eyebrow, hint
    static let light: [UInt32] = [0x1f2214, 0x3a382e, 0x6b4a12, 0x4d4a3f]
    static let hint = (dark: Palette.hex(0xc9c1ad), light: Palette.hex(0x4d4a3f))
    static func luminance(_ value: UInt32) -> Double {
        ScrimMath.luminance(UInt8(value >> 16 & 255), UInt8(value >> 8 & 255), UInt8(value & 255))
    }
    /// Light text needs the darkest background for its dimmest colour; dark text the lightest for its palest.
    static func limitingLuminance(dark isDark: Bool) -> Double {
        isDark ? dark.map(luminance).min() ?? 0.5 : light.map(luminance).max() ?? 0.1
    }
}

/// Text-region luminance of a loading screen, sampled once per image.
@MainActor enum HeroLuminance {
    private static var cache: [ObjectIdentifier: (low: Double, high: Double)] = [:]
    static func stats(_ image: NSImage) -> (low: Double, high: Double) {
        if let value = cache[ObjectIdentifier(image)] { return value }
        var result = (low: 0.0, high: 1.0)
        let width = 384, height = 288
        var pixels = Data(count: width * height * 4)
        let drawn = pixels.withUnsafeMutableBytes { raw -> Bool in
            guard let cg = image.cgImage(forProposedRect: nil, context: nil, hints: nil), let space = CGColorSpace(name: CGColorSpace.sRGB),
                  let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space,
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return false }
            context.draw(cg, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        // Map each text box (eyebrow, title, meta line, shortcut hint) of the 1208 × 620 hero beside
        // a 232-point sidebar back into the aspect-filled image, and keep the worst of them.
        let frame = CGSize(width: 1208, height: 620), size = image.size
        let scale = max(frame.width / max(size.width, 1), frame.height / max(size.height, 1))
        let shown = CGSize(width: size.width * scale, height: size.height * scale)
        let crop = CGPoint(x: (shown.width - frame.width) / 2, y: (shown.height - frame.height) / 2)
        let boxes = [CGRect(x: 68, y: 150, width: 420, height: 18), CGRect(x: 68, y: 182, width: 760, height: 114),
                     CGRect(x: 68, y: 316, width: 560, height: 22), CGRect(x: 418, y: 388, width: 120, height: 16)]
        // The brightest sample per box: a downsample softens highlights the full image still shows.
        let stats = boxes.compactMap { box in
            ScrimMath.percentiles(rgba: pixels, width: width, height: height,
                                  region: (x: Double((crop.x + box.minX) / shown.width), y: Double((crop.y + box.minY) / shown.height),
                                           width: Double(box.width / shown.width), height: Double(box.height / shown.height)),
                                  lower: 0.01, upper: 1)
        }
        if drawn, !stats.isEmpty { result = (stats.map(\.low).min() ?? 0, stats.map(\.high).max() ?? 1) }
        cache[ObjectIdentifier(image)] = result
        return result
    }
}

struct StatusChips: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        GlassContainer(spacing: 10) {
            HStack(spacing: 10) {
                chip(label: "Frame cap \(model.settings.fps) frames per second") {
                    Text("\(model.settings.fps)").font(.stencil(20)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f))
                    Text("fps cap")
                } action: { model.page = .settings }
                chip(label: "Display \(resolutionText)") {
                    Image(systemName: "display").font(.system(size: 14)).foregroundStyle(palette.chipIcon)
                    Text(resolutionText)
                } action: { model.page = .settings }
                chip(label: model.approximate ? "Approximate shaders" : "Original shaders ready") {
                    Image(systemName: model.approximate ? "exclamationmark.shield" : "checkmark.shield").font(.system(size: 14, weight: .medium))
                        .foregroundStyle(model.approximate ? palette.accent : palette.positive)
                    Text(model.approximate ? "Approximate shaders" : "Original shaders ready")
                } action: { if model.approximate { model.restartSetup() } else { model.page = .settings } }
            }
        }
    }
    private var resolutionText: String {
        let size = model.settings.resolution.replacingOccurrences(of: "x", with: " × ")
        let mode = ["exclusive": "Exclusive fullscreen", "borderless": "Borderless", "spaces": "Native Space", "windowed": "Window"][model.settings.fullscreen] ?? "Fullscreen"
        return "\(size) · \(mode)"
    }
    private func chip<Content: View>(label: String, @ViewBuilder content: () -> Content, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 10) { content() }
                .font(.system(size: 13)).foregroundStyle(palette.chipText)
                .padding(.horizontal, 16).frame(height: 40)
                .glassSurface(RoundedRectangle(cornerRadius: 14, style: .continuous), interactive: true)
                .contentShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
    }
}

struct PanelHeading: View {
    var title: String
    var action: String?
    var perform: (() -> Void)?
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.dark ? Palette.hex(0xefe9db) : palette.heading).accessibilityAddTraits(.isHeader)
            Spacer()
            if let action, let perform { Button(action, action: perform).buttonStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.link) }
        }
    }
}

struct RecentServersPanel: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }
    var body: some View {
        ContentPanel {
            PanelHeading(title: "Recent servers", action: "Show all") { model.page = .servers }
            VStack(spacing: 6) {
                if model.recentServers.isEmpty {
                    Text("Servers you join appear here.").font(.system(size: 13)).foregroundStyle(palette.secondary).frame(maxWidth: .infinity, alignment: .leading).padding(.top, 8)
                }
                ForEach(model.recentServers.prefix(3)) { server in
                    let facts = ServerFacts(server)
                    Button { model.showServer(server.address) } label: {
                        HStack(spacing: 14) {
                            MapArt(map: server.map, image: artwork.image(for: server.map))
                                .frame(width: 72, height: 44).clipShape(RoundedRectangle(cornerRadius: 9, style: .continuous))
                            VStack(alignment: .leading, spacing: 4) {
                                Text(QuakeColors.plain(server.name)).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                                Text("\(MapCatalog.displayName(server.map)) · \(GameModes.long(server.gametype)) · \(facts.isCoD2x ? "CoD2x 1.4" : "Stock 1.3")")
                                    .font(.system(size: 12)).foregroundStyle(palette.secondary).lineLimit(1)
                            }
                            Spacer(minLength: 8)
                            Text("\(server.playerCount)/\(server.maxPlayers)").font(.stencil(22)).foregroundStyle(palette.numeral)
                            Text("\(server.ping) ms").font(.system(size: 12)).foregroundStyle(palette.secondary).frame(width: 46, alignment: .trailing)
                        }
                        .padding(8).contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                    }
                    .buttonStyle(RowButtonStyle(radius: 12))
                    .padding(.horizontal, -8)
                    .accessibilityLabel("\(QuakeColors.plain(server.name)), \(MapCatalog.displayName(server.map)), \(server.playerCount) of \(server.maxPlayers) players, \(server.ping) milliseconds")
                }
            }
            .padding(.top, 10)
            Spacer(minLength: 0)
        }
    }
}

/// Hover and press feedback for content rows.
struct RowButtonStyle: ButtonStyle {
    var radius: CGFloat
    @State private var hovering = false
    @Environment(\.palette) private var palette
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .background((palette.dark ? Color.white : Color.black).opacity(configuration.isPressed ? 0.07 : hovering ? 0.04 : 0), in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .onHover { hovering = $0 }
    }
}

struct DispatchesPanel: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel {
            PanelHeading(title: "Dispatches", action: "Release notes") {
                NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon/releases")!)
            }
            VStack(alignment: .leading, spacing: 14) {
                ForEach(Array(model.dispatches.enumerated()), id: \.offset) { _, dispatch in
                    HStack(alignment: .firstTextBaseline, spacing: 14) {
                        Text(dispatch.stamp).font(.typewriter(12)).foregroundStyle(palette.dark ? Palette.hex(0xb9a982) : Palette.hex(0x7a5a1c)).frame(width: 74, alignment: .leading)
                        VStack(alignment: .leading, spacing: 4) {
                            Text(dispatch.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                            Text(dispatch.summary).font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(2.6).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    .accessibilityElement(children: .combine)
                }
            }
            .padding(.top, 14)
            Spacer(minLength: 0)
        }
    }
}
