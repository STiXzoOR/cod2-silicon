import SwiftUI
import AppKit

// Owner: ship-home-library. Home: the last map's art behind the hero, the next game's loadout and display,
// anything that needs fixing, and three cards (Jump back in, Last match, What's New).

struct HomeView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @ObservedObject private var checkup: ConfigCheckupModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    @State private var heroHeight: CGFloat = 290
    init(model: LauncherModel) { self.model = model; artwork = model.artwork; checkup = model.studio.checkup }

    var body: some View {
        let running = model.gameRunning
        let shown = running ? model.serverInPlay : model.homeLastServer
        // A server joined by address with no match summary has no known map: the backdrop keeps the default scene.
        let map = (shown ?? model.homeLastServer).flatMap(\.knownMap) ?? "mp_toujane"
        GeometryReader { proxy in
            let height = proxy.size.height
            let lead = max(32, 300 - origin.x)
            let width = proxy.size.width - lead - 32
            let stacked = width < 980
            // The board's grid at 900 points tall (hero 138 from the top, cards 276 tall and 28 from the bottom),
            // tightened on shorter windows; past the minimum the page scrolls instead of overlapping.
            let heroTop = min(138, max(84, height - 762))
            let cardHeight = min(276, max(262, height - 624))
            // One row of cards at every size, so Home never needs scrolling at the minimum window. A window too
            // narrow for three cards keeps two, and What's New becomes a chip that opens it as a dialog.
            let cards = cardHeight
            let bottom = 40 + 32 + cards + 28
            let content = max(height, heroTop + heroHeight + 24 + bottom)
            ScrollView {
                ZStack(alignment: .topLeading) {
                    HomeHero(model: model, server: shown, running: running)
                        .onGeometryChange(for: CGFloat.self, of: { $0.size.height }) { heroHeight = $0 }
                        .padding(.top, heroTop)
                    VStack(alignment: .leading, spacing: 32) {
                        HomeChips(model: model, news: stacked)
                        HomeCards(model: model, stacked: stacked, cardHeight: cardHeight, returning: !model.library.recent.isEmpty)
                    }
                    .padding(.bottom, 28)
                    .frame(maxHeight: .infinity, alignment: .bottom)
                }
                .padding(.leading, lead).padding(.trailing, 32)
                .frame(width: proxy.size.width, height: content, alignment: .topLeading)
            }
            .scrollIndicators(.automatic)
            .scrollBounceBehavior(.basedOnSize)
            .background(alignment: .topLeading) {
                // The window-space slice of the hero; the system mirrors it under the floating sidebar.
                HeroBackdrop(map: map, image: artwork.image(for: map), leading: origin.x)
                    .frame(width: proxy.size.width, height: 620)
                    .heroExtension()
            }
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .modifier(UpdatePillToolbar(model: model))
        .onAppear { model.refreshAttention() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in model.refreshAttention(); model.refreshDisplay() }
    }
}

// MARK: - Hero

/// The eyebrow, the map in the stencil face, where and when, and the pair of buttons with their hint.
private struct HomeHero: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer?
    var running: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            if running {
                Eyebrow(text: (["In the field now", server?.knownMap.flatMap(MapCatalog.region)].compactMap { $0 }).joined(separator: " · "), size: 15, tracking: 0.34)
                StencilTitle(text: (server?.knownMap.map(MapCatalog.displayName) ?? "CoD2 Silicon").uppercased())
                TimelineView(.periodic(from: .now, by: 30)) { context in
                    MetaLine(items: meta(server, when: Self.playing(since: model.gameStartedAt, now: context.date)))
                }
            } else if let server {
                Eyebrow(text: (["Last played", server.knownMap.flatMap(MapCatalog.region)].compactMap { $0 }).joined(separator: " · "), size: 15, tracking: 0.34)
                StencilTitle(text: (server.knownMap.map(MapCatalog.displayName) ?? "CoD2 Silicon").uppercased())
                MetaLine(items: meta(server, when: model.lastPlayed(server.address).map { PlayedDate.sentence($0) }))
            } else {
                Eyebrow(text: "Ready to play", size: 15, tracking: 0.34)
                StencilTitle(text: "COD2 SILICON")
                Text("You’re set up. Pick a server to join your first match, or open the game’s own menu.")
                    .font(.system(size: 16)).lineSpacing(5)
                    .foregroundStyle(palette.dark ? Palette.hex(0xddd5c3) : Palette.hex(0x3a382e))
                    .frame(maxWidth: 520, alignment: .leading).fixedSize(horizontal: false, vertical: true)
            }
            HStack(spacing: 14) { buttons }.padding(.top, 14)
        }
        .frame(maxWidth: 780, alignment: .leading)
    }
    private func meta(_ server: GameServer?, when: String?) -> [AnyView] {
        var items: [AnyView] = []
        if let server {
            items.append(AnyView(QuakeName(name: server.name).font(.system(size: 16, weight: .semibold))))
            if let mode = server.knownMode { items.append(AnyView(Text(GameModes.long(mode)))) }
        }
        if let when { items.append(AnyView(Text(when))) }
        return items
    }
    static func playing(since start: Date, now: Date) -> String {
        let minutes = max(0, Int(now.timeIntervalSince(start) / 60))
        if minutes < 1 { return "Playing for less than a minute" }
        return minutes < 60 ? "Playing for \(minutes) min" : "Playing for \(minutes / 60) h \(minutes % 60) min"
    }
    @ViewBuilder private var buttons: some View {
        let canNavigate = model.canNavigate
        if running {
            Button(action: model.returnToGame) { Label("Return to Game", systemImage: "play.fill").labelStyle(HeroLabelStyle()) }
                .prominentAction().actionControlSize()
            Button(action: model.quitGame) { Label(model.quittingGame ? "Quitting…" : "Quit Game", systemImage: "power").labelStyle(HeroLabelStyle()) }
                .glassAction().actionControlSize().disabled(model.quittingGame)
                .help("Quits Call of Duty 2. The launcher stays open.")
            hint("The game is running. Settings you change apply next time.")
        } else if server != nil {
            Button(action: model.launch) { Label("Launch", systemImage: "play.fill").labelStyle(HeroLabelStyle()) }
                .prominentAction().actionControlSize()
                .accessibilityLabel("Launch on \(server.map { QuakeColors.plain($0.name) } ?? "the last server")")
            Button { model.navigate(to: .servers) } label: {
                Label("Find a Server", systemImage: "dot.radiowaves.left.and.right").labelStyle(HeroLabelStyle())
            }.glassAction().actionControlSize().disabled(!canNavigate)
            hint("⌘↩ launches and rejoins this server").accessibilityLabel("Command Return launches and rejoins this server")
        } else {
            Button { model.navigate(to: .servers) } label: {
                Label("Find a Server", systemImage: "dot.radiowaves.left.and.right").labelStyle(HeroLabelStyle())
            }.prominentAction().actionControlSize().disabled(!canNavigate)
            Button(action: model.openGameMenu) { Label("Open Game Menu", systemImage: "play.fill").labelStyle(HeroLabelStyle()) }
                .glassAction().actionControlSize()
            hint("⇧⌘↩").accessibilityLabel("Shift Command Return opens the game menu")
        }
    }
    private func hint(_ text: String) -> some View {
        Text(text).font(.system(size: 12)).tracking(0.5).foregroundStyle(palette.dark ? HeroInk.hint.dark : HeroInk.hint.light).padding(.leading, 6)
            .fixedSize(horizontal: false, vertical: true)
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
        .lineLimit(1)
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
                MapArtFrame(map: map, image: image, size: .fill, hero: true)
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
    // Weakly held so a freed image's identifier, if reused, never returns stale numbers.
    private final class Entry { weak var image: NSImage?; let value: (low: Double, high: Double); init(_ image: NSImage, _ value: (low: Double, high: Double)) { self.image = image; self.value = value } }
    private static var cache: [ObjectIdentifier: Entry] = [:]
    static func stats(_ image: NSImage) -> (low: Double, high: Double) {
        if let entry = cache[ObjectIdentifier(image)], entry.image === image { return entry.value }
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
                     CGRect(x: 68, y: 316, width: 560, height: 22), CGRect(x: 418, y: 388, width: 420, height: 16)]
        // The brightest sample per box: a downsample softens highlights the full image still shows.
        let stats = boxes.compactMap { box in
            ScrimMath.percentiles(rgba: pixels, width: width, height: height,
                                  region: (x: Double((crop.x + box.minX) / shown.width), y: Double((crop.y + box.minY) / shown.height),
                                           width: Double(box.width / shown.width), height: Double(box.height / shown.height)),
                                  lower: 0.01, upper: 1)
        }
        if drawn, !stats.isEmpty { result = (stats.map(\.low).min() ?? 0, stats.map(\.high).max() ?? 1) }
        cache = cache.filter { $0.value.image != nil }
        cache[ObjectIdentifier(image)] = Entry(image, result)
        return result
    }
}

// MARK: - Update pill

/// “CoD2 Silicon 0.3.1 is ready to install · What’s New · Install Update” at the trailing end of the
/// toolbar, while a release is ready or being installed. Checking and up-to-date live in Settings › Updates.
private struct UpdatePillToolbar: ViewModifier {
    @ObservedObject var model: LauncherModel
    func body(content: Content) -> some View {
        let state = model.updateState
        let shown: Bool = switch state {
        case .ready, .installing: true
        case .failed: model.updateOffer != nil
        default: false
        }
        if shown { toolbar(content, state: state) } else { content }
    }
    /// The pill draws its own glass around its prominent button (as on the board), so the toolbar's
    /// shared glass is hidden for it; otherwise the system tints the whole item with the button's colour.
    @ViewBuilder private func toolbar(_ content: Content, state: UpdateState) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar {
                ToolbarSpacer(.flexible)
                ToolbarItem(placement: .automatic) { UpdatePill(model: model, state: state) }.sharedBackgroundVisibility(.hidden)
            }
        } else { content.trailingToolbar { UpdatePill(model: model, state: state) } }
        #else
        content.trailingToolbar { UpdatePill(model: model, state: state) }
        #endif
    }
}

private struct UpdatePill: View {
    @ObservedObject var model: LauncherModel
    var state: UpdateState
    @Environment(\.palette) private var palette
    var body: some View {
        // 44 tall; Install Update is 36 tall and 4 in from the end, concentric with the pill.
        GlassContainer(spacing: 4) {
            content
                .padding(.leading, 16).padding(.trailing, endsWithButton ? 4 : 16)
                .frame(height: 44)
                .glassSurface(Capsule())
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Update")
    }
    private var endsWithButton: Bool { if case .ready = state { return true }; return false }
    @ViewBuilder private var content: some View {
        HStack(spacing: 12) {
            switch state {
            case .installing(let message):
                ProgressView().controlSize(.small).accessibilityHidden(true)
                Text(message).font(.system(size: 13)).foregroundStyle(palette.text).lineLimit(1)
            case .failed(let message):
                Image(systemName: "exclamationmark.triangle").foregroundStyle(palette.negative).accessibilityHidden(true)
                Text(message).font(.system(size: 13)).foregroundStyle(palette.text).lineLimit(1).frame(maxWidth: 420, alignment: .leading)
                    .help(message)
                Button("Details") { model.openSettings(.updates) }.quietAction()
            case .ready(let version, let installable):
                Image(systemName: "arrow.down.circle").foregroundStyle(palette.accent).accessibilityHidden(true)
                Text(version.isEmpty ? "A new version of CoD2 Silicon is available" : installable ? "CoD2 Silicon \(version) is ready to install" : "CoD2 Silicon \(version) is available")
                    .font(.system(size: 13)).foregroundStyle(palette.text).lineLimit(1)
                Button("What’s New", action: model.showWhatsNew).quietAction()
                if installable { Button("Install Update", action: model.installUpdate).prominentAction().actionControlSize() }
                else { Button("View Release") { NSWorkspace.shared.open(model.releaseURL ?? AppUpdate.releases) }.prominentAction().actionControlSize() }
            default: EmptyView()
            }
        }
    }
}

// MARK: - Chips

/// The next game's loadout and frame cap, the display, and a chip for each thing that needs fixing.
private struct HomeChips: View {
    @ObservedObject var model: LauncherModel
    /// The window has no room for the What's New card: a chip opens it as a dialog instead.
    var news = false
    @Environment(\.palette) private var palette
    var body: some View {
        let attention = model.homeAttention
        GlassContainer(spacing: 10) {
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 10) { loadoutChip; displayChip; resolutionChip; ForEach(attention) { attentionChip($0) }; if news { newsChip } }
                VStack(alignment: .leading, spacing: 10) {
                    HStack(spacing: 10) { loadoutChip; displayChip; if news { newsChip } }
                    HStack(spacing: 10) { resolutionChip; ForEach(attention) { attentionChip($0) } }
                }
            }
        }
    }
    private var newsChip: some View {
        Chip("What’s New. Shows the news and releases", size: .large, glass: true, action: model.showNews) {
            Image(systemName: "newspaper").font(.system(size: 14)).foregroundStyle(palette.chipIcon)
            Text("What’s New")
        }
    }

    private var capFace: (numeral: String?, text: String, spoken: String) {
        let info = model.nextGameFrameCap
        let loadout = info.name.map { " · \($0) loadout" } ?? ""
        guard let fps = info.value else { return (nil, "Frame cap unavailable" + loadout, "Frame cap unavailable" + (info.name.map { ", \($0) loadout" } ?? "")) }
        let chip = FrameCap.chip(fps)
        let spokenLoadout = info.name.map { "\($0) loadout, " } ?? ""
        // The resolution is the loadout's, so it sits with the loadout. Before loadouts it is a launcher setting, on the display chip.
        let size = model.usesLoadouts ? model.nextGameResolution.value : nil
        return (chip.numeral, chip.text + (size.map { " · " + SettingsText.resolution($0) } ?? "") + loadout,
                spokenLoadout + (fps > 0 ? "\(fps) frames per second cap" : "no frame cap") + (size.map { ", " + $0.replacingOccurrences(of: "x", with: " by ") } ?? ""))
    }

    private var loadoutChip: some View { LoadoutChip(model: model, face: capFace) }

    private var displayChip: some View {
        let size = model.settings.resolution.replacingOccurrences(of: "x", with: " × ")
        let mode = ["exclusive": "Exclusive fullscreen", "borderless": "Borderless", "spaces": "Native Space", "windowed": "Window"][model.settings.fullscreen] ?? "Fullscreen"
        let spokenSize = model.settings.resolution.replacingOccurrences(of: "x", with: " by ")
        let spoken = model.usesLoadouts ? "Display mode, \(mode.lowercased())" : "Display, \(spokenSize), \(mode.lowercased())"
        return Chip("\(spoken). Open Display settings", size: .large, glass: true, action: { model.openSettings(.display) }) {
            Image(systemName: "display").font(.system(size: 14)).foregroundStyle(palette.chipIcon)
            Text(model.usesLoadouts ? mode : "\(size) · \(mode)")
        }
    }

    /// The loadout asks for a size this display can't show (it was made on a larger one).
    @ViewBuilder private var resolutionChip: some View {
        let next = model.nextGameResolution
        if let used = next.used {
            let asked = SettingsText.resolution(next.value)
            Chip("This display can’t show \(next.value.replacingOccurrences(of: "x", with: " by ")). The game will use \(used.replacingOccurrences(of: "x", with: " by ")). Choose a resolution in Configs",
                 size: .large, glass: true, action: model.openResolutionSettings) {
                Image(systemName: "exclamationmark.triangle").font(.system(size: 14, weight: .medium)).foregroundStyle(palette.accent)
                Text("This display can’t show \(asked) ·")
                Text("Choose…").fontWeight(.semibold).foregroundStyle(palette.accent)
            }
            .overlay(Capsule().strokeBorder(palette.selectionStroke, lineWidth: 1).allowsHitTesting(false))
            .disabled(!model.canNavigate)
            .help("The game will use \(SettingsText.resolution(used)) on this display.")
        }
    }

    private func attentionChip(_ item: HomeAttention) -> some View {
        Chip("\(item.title). \(item.fix)", size: .large, glass: true, action: { model.fix(item) }) {
            Image(systemName: "exclamationmark.triangle").font(.system(size: 14, weight: .medium)).foregroundStyle(palette.accent)
            Text(item.title + " ·")
            Text("Fix…").fontWeight(.semibold).foregroundStyle(palette.accent)
        }
        .overlay(Capsule().strokeBorder(palette.selectionStroke, lineWidth: 1).allowsHitTesting(false))
        .disabled(model.gameRunning || model.installingUpdate)
        .help(item.fix)
    }
}

/// The next game's loadout and frame cap. With loadouts, it opens a menu to pick the one the next game uses
/// or edit it in Configs; before loadouts exist the cap is a launcher setting, and the chip opens it.
private struct LoadoutChip: View {
    @ObservedObject var model: LauncherModel
    var face: (numeral: String?, text: String, spoken: String)
    @Environment(\.palette) private var palette
    @State private var anchor = PullDownAnchorBox()
    var body: some View {
        let choices = model.loadoutChoices
        if choices.isEmpty {
            Chip(face.spoken + ". Open Display settings", size: .large, glass: true, action: model.openFrameSettings) { capLabel }
        } else {
            Chip(face.spoken + ". Choose the loadout for your next game", size: .large, glass: true, action: { showMenu(choices) }) {
                capLabel
                Image(systemName: "chevron.down").font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.secondary).accessibilityHidden(true)
            }
            .background(PullDownAnchor(box: anchor))
            .help("Choose the loadout for your next game")
        }
    }
    @ViewBuilder private var capLabel: some View {
        if let numeral = face.numeral { Text(numeral).font(.stencil(20)).foregroundStyle(palette.accent) }
        Text(face.text).lineLimit(1)
    }
    private func showMenu(_ choices: [LoadoutChoice]) {
        guard let view = anchor.view else { return }
        let current = choices.first(where: \.inUse) ?? choices[0]
        // While the game runs, it keeps the loadout it started with: the choice applies next time, so it waits.
        let entries: [PullDownMenu.Entry] = [.header("Use for your next game")]
            + choices.map { choice in
                .item(choice.name, detail: [choice.cap.map { $0 > 0 ? "\($0) fps" : "No cap" }, choice.resolution.map(SettingsText.resolution)].compactMap { $0 }.joined(separator: " · "),
                      checked: choice.inUse, enabled: !model.gameRunning) {
                    model.useLoadoutForNextGame(choice.id)
                }
            }
            + [.separator, .item("Edit \(current.name) in Configs…", enabled: model.canNavigate, action: model.openFrameSettings)]
        PullDownMenu.show(entries, below: view)
    }
}

// MARK: - Cards

private struct HomeCards: View {
    @ObservedObject var model: LauncherModel
    var stacked: Bool
    var cardHeight: CGFloat
    /// Recent servers are listed: Jump back in takes the width its rows need, as on the Home board
    /// (508 : 298 : 266 at 1440 wide); with nothing played the cards keep the 1.22 : 0.9 : 1 grid.
    var returning: Bool
    var body: some View {
        if stacked {
            GeometryReader { proxy in
                let unit = (proxy.size.width - 18) / (1.22 + 0.9)
                HStack(alignment: .top, spacing: 18) {
                    JumpBackInCard(model: model).frame(width: unit * 1.22)
                    LastMatchCard(model: model).frame(width: unit * 0.9)
                }
            }
            .frame(height: cardHeight)
        } else {
            GeometryReader { proxy in
                let shares: [CGFloat] = returning ? [508, 298, 266] : [1.22, 0.9, 1]
                let unit = (proxy.size.width - 36) / shares.reduce(0, +)
                HStack(alignment: .top, spacing: 18) {
                    JumpBackInCard(model: model).frame(width: unit * shares[0])
                    LastMatchCard(model: model).frame(width: unit * shares[1])
                    DispatchesCard(model: model).frame(width: unit * shares[2])
                }
            }
            .frame(height: cardHeight)
        }
    }
}

/// A card's empty state: a stencil heading, a sentence and at most one button.
private struct CardEmpty: View {
    var heading: String
    var message: String
    var action: (title: String, run: () -> Void)?
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 8) {
            Text(heading.uppercased()).font(.stencil(20)).tracking(2.4).foregroundStyle(palette.chipIcon).multilineTextAlignment(.center)
                .accessibilityAddTraits(.isHeader)
            Text(message).font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            if let action { Button(action.title, action: action.run).glassAction().padding(.top, 2) }
        }
        .frame(maxWidth: .infinity)
        .padding(.horizontal, 10).padding(.top, 26)
    }
}

private struct JumpBackInCard: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }
    var body: some View {
        ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            PanelHeading(title: "Jump back in", action: "All Recent") { model.showRecentServers() }
                .disabled(!model.canNavigate)
            let recent = model.homeRecent
            if recent.isEmpty {
                CardEmpty(heading: "Nothing played yet", message: "Servers you join appear here, ready to rejoin.",
                          action: ("Find a Server", { model.navigate(to: .servers) }))
                    .disabled(!model.canNavigate)
            } else {
                VStack(spacing: 4) {
                    ForEach(recent) { server in RecentRow(model: model, server: server, image: artwork.image(for: server.map)) }
                }
                .padding(.top, 10).padding(.horizontal, -8)
            }
            Spacer(minLength: 0)
        }
        .frame(maxHeight: .infinity)
    }
}

/// A recent server: art, name, map · mode · version, when it was played and Launch. The row opens it in Servers.
private struct RecentRow: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer
    var image: NSImage?
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        let name = QuakeColors.plain(server.name)
        // A server joined by address and never listed: its version is unknown and Servers can't show it.
        let listed = model.isListed(server)
        let detail: [String] = [server.knownMap.map(MapCatalog.displayName), server.knownMode.map(GameModes.long),
                                listed ? (ServerFacts(server).isCoD2x ? "CoD2x" : "Original 1.3") : nil].compactMap { $0 }
        let subtitle = listed || !detail.isEmpty ? detail.joined(separator: " · ") : "Joined by address"
        let compactDetail = [server.knownMap.map(MapCatalog.displayName), server.knownMode.map(GameModes.short),
                             listed ? (ServerFacts(server).isCoD2x ? "CoD2x" : "Original 1.3") : nil].compactMap { $0 }.joined(separator: " · ")
        let played = model.lastPlayed(server.address)
        // Clicking the row shows the server in Servers; a server no list has shown, or a page change
        // while the game runs, leaves the row as plain text rather than a button that does nothing.
        let opens = listed && model.canNavigate
        let label = "\(name), \(subtitle)" + (played.map { ", played \(PlayedDate.spoken($0))" } ?? "")
        let content = HStack(spacing: 12) {
            MapArtFrame(map: server.knownMap ?? "mp_toujane", image: server.knownMap == nil ? nil : image, size: .card, radius: Concentric.radius(in: 16, inset: 7))
            VStack(alignment: .leading, spacing: 3) {
                Text(name).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                // One line: a wrapped detail would make this row taller than the others in a narrow card.
                Text(compactDetail.isEmpty ? subtitle : compactDetail).font(.system(size: 12)).foregroundStyle(palette.secondary).lineLimit(1)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            if let played {
                Text(PlayedDate.day(played)).font(.typewriter(12)).foregroundStyle(palette.tertiary).lineLimit(1).fixedSize()
            }
        }
        HStack(spacing: 12) {
            if opens {
                Button { model.showServer(server.address) } label: { content.contentShape(Rectangle()) }
                    .buttonStyle(.plain)
                    .accessibilityLabel(label)
                    .accessibilityHint("Shows this server in Servers")
            } else {
                content.accessibilityElement(children: .ignore).accessibilityLabel(label).accessibilityAddTraits(.isStaticText)
            }
            Button { model.connect(server) } label: { Label("Launch", systemImage: "play.fill").font(.system(size: 12, weight: .semibold)) }
                .glassAction().controlSize(.small)
                .accessibilityLabel("Launch on \(name)")
                .help("Launch and join \(name)")
        }
        // The Launch capsule sits as far from the row's end as from its top and bottom (art 48 + 2 × 7).
        .padding(.vertical, 7).padding(.leading, 7).padding(.trailing, (48 + 14 - ControlMetrics.height(.small)) / 2)
        .background(hovering && opens ? palette.hover : .clear, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .shapeAudit(.container, .rounded(16), "recent row")
        .onHover { hovering = $0 }
        .contextMenu {
            Button("Launch") { model.connect(server) }
            Button("Show in Servers") { model.showServer(server.address) }.disabled(!opens)
            Divider()
            Button(model.library.favorites.contains(server.address) ? "Remove from Favorites" : "Add to Favorites") { model.favorite(server.address) }
            Button("Copy Address") { model.copyServer(server) }
            Button("Copy Join Link") { model.copyJoinLink(server) }
        }
    }
}

private struct LastMatchCard: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var checkup: ConfigCheckupModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    init(model: LauncherModel) { self.model = model; checkup = model.studio.checkup; artwork = model.artwork }
    var body: some View {
        ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            PanelHeading(title: "Last match", action: "All Matches") { model.libraryTab = .matches; model.navigate(to: .library) }
                .disabled(!model.canNavigate)
            VStack(alignment: .leading, spacing: 0) { content }.padding(.top, 12)
        }
        .frame(maxHeight: .infinity)
    }
    @ViewBuilder private var content: some View {
        if !model.settings.matchSummariesEnabled {
            CardEmpty(heading: "Match summaries are off", message: "Turn them on to see how each match ran. They stay on this Mac.",
                      action: ("Turn On in Settings…", { model.openSettings(.privacy) }))
            Spacer(minLength: 0)
        } else if case .failed = checkup.matchState {
            CardEmpty(heading: "Couldn’t read matches", message: "Your match summaries couldn’t be read. Try again after your next match.",
                      action: ("Try Again", model.refreshLibraryMatches))
            Spacer(minLength: 0)
        } else if let last = model.storedMatches.last {
            let facts = MatchFacts(last, index: model.storedMatches.count - 1)
            VStack(alignment: .leading, spacing: 12) {
                HStack(spacing: 12) {
                    MapArtFrame(map: facts.map, image: artwork.image(for: facts.map), size: .card)
                    VStack(alignment: .leading, spacing: 3) {
                        Text(facts.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                        Text(facts.stamp(time: false)).font(.typewriter(12)).foregroundStyle(palette.tertiary).lineLimit(1)
                    }
                }
                .accessibilityElement(children: .combine)
                HStack(alignment: .top, spacing: 8) {
                    figure(facts.fps.map(String.init), caption: "fps, typical", spoken: facts.fps.map { "\($0) frames a second, typical" } ?? "Frame rate not measured")
                    figure(facts.ping.map(String.init), caption: "ms ping", spoken: facts.ping.map { "\($0) milliseconds ping" } ?? "Ping not measured")
                    VStack(alignment: .leading, spacing: 5) {
                        StatusLabel(facts.connection.kind, facts.connection.words).frame(height: 30)
                        Text("connection").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityElement(children: .ignore).accessibilityLabel("Connection \(facts.connection.words)").accessibilityAddTraits(.isStaticText)
                }
            }
            Spacer(minLength: 8)
            HStack {
                Text("Kept on this Mac.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                Spacer(minLength: 8)
                Button("Open Check-up") { model.openCheckup(match: facts.id) }.glassAction().controlSize(.small)
                    .disabled(!model.canNavigate)
                    .help("Open Check-up for this match")
            }
        } else {
            CardEmpty(heading: "Not measured yet", message: "After your first match, its frame rate and connection show up here.")
            Spacer(minLength: 0)
        }
    }
    private func figure(_ value: String?, caption: String, spoken: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(value ?? "—").font(.stencil(30)).foregroundStyle(palette.numeral).monospacedDigit().frame(height: 30)
            Text(caption).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineLimit(1)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .ignore).accessibilityLabel(spoken).accessibilityAddTraits(.isStaticText)
    }
}

private struct DispatchesCard: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            // The card never runs past its edge: See All opens every line, in full, as a dialog.
            PanelHeading(title: "What's New", action: "See All", perform: model.showNews)
            // As many lines as the card has room for: three, else two, else this version's alone, with
            // shorter text before fewer lines. The last choice always fits.
            ViewThatFits(in: .vertical) {
                rows(model.dispatches(rows: 3), summary: 3)
                rows(model.dispatches(rows: 2), summary: 3)
                rows(model.dispatches(rows: 2), summary: 2)
                rows(model.dispatches(rows: 1), summary: 3)
                rows(model.dispatches(rows: 1), summary: 0)
            }
            .padding(.top, 14)
            Spacer(minLength: 8)
            Text(model.dispatchesFoot).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineLimit(2)
        }
        .frame(maxHeight: .infinity)
    }
    private func rows(_ list: [Dispatch], summary: Int) -> some View {
        VStack(alignment: .leading, spacing: 14) {
            ForEach(Array(list.enumerated()), id: \.offset) { _, dispatch in
                if model.canOpen(dispatch) {
                    Button { model.open(dispatch) } label: { NewsLine(dispatch: dispatch, titleLines: 2, summaryLines: summary).contentShape(Rectangle()) }
                        .buttonStyle(.plain).accessibilityElement(children: .combine)
                } else {
                    NewsLine(dispatch: dispatch, titleLines: 2, summaryLines: summary).accessibilityElement(children: .combine)
                }
            }
        }
    }
}

/// One line of What's New. The date sits above the title: the card is 266 points wide at the smallest
/// window, and a date column beside the text would leave the sentence about 150. The card limits the
/// lines of text; the dialog shows all of it (`nil`).
private struct NewsLine: View {
    var dispatch: Dispatch
    var titleLines: Int?
    var summaryLines: Int?
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(dispatch.stamp).font(.typewriter(12)).foregroundStyle(palette.address)
            Text(dispatch.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(titleLines)
                .fixedSize(horizontal: false, vertical: true)
            if !dispatch.summary.isEmpty, summaryLines != 0 {
                Text(dispatch.summary).font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(2.6).lineLimit(summaryLines)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// What's New in full: every release and news line the launcher has, newest first, then what comes
/// next. Home's card and its chip at a narrow window open it.
struct HomeNewsSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let lines = model.allDispatches
        SheetScaffold(icon: .mark, title: "What’s New", message: model.dispatchesFoot,
                      actions: [SheetAction(title: "Release Notes", role: .leading, action: model.openReleaseNotes),
                                .primary("Done") { model.dismissSheet() }]) {
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    ForEach(Array(lines.enumerated()), id: \.offset) { index, dispatch in
                        HStack(alignment: .center, spacing: 12) {
                            NewsLine(dispatch: dispatch, titleLines: nil, summaryLines: nil).accessibilityElement(children: .combine)
                            if model.canOpen(dispatch) {
                                Button(model.openTitle(dispatch)) { model.dismissSheet(); model.open(dispatch) }.controlSize(.small).fixedSize()
                                    .accessibilityLabel("\(model.openTitle(dispatch)): \(dispatch.title)")
                            }
                        }
                        .padding(.vertical, 12)
                        .overlay(alignment: .top) { if index > 0 { Rectangle().fill(palette.separator).frame(height: 1) } }
                    }
                }
                .padding(.horizontal, 16)
            }
            .frame(maxHeight: 380)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
        }
    }
}
