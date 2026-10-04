import AppKit
import SwiftUI

// Review harness, never shipped. It hosts the real launcher views in a titled window
// configured like the app's (hidden title bar, unified toolbar) and captures it through
// the window server, so Liquid Glass, materials and toolbars render as they do on screen.
//
//   LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]
//
// --data uses the player's own loading screens (private review only; never commit them).
// --fallback forces the macOS 13–25 material styling. --scale2 adds 2× offscreen renders
// for checking text and vector crispness (system materials draw flat in that path).

/// The review host can't become the active app while the login session is locked, so the
/// window reports key and main appearance. Test-only: these overrides never ship.
final class SnapshotWindow: NSWindow {
    override var isKeyWindow: Bool { true }
    override var isMainWindow: Bool { true }
    override var canBecomeKey: Bool { true }
    @objc func _hasActiveAppearance() -> Bool { true }
    @objc func _hasActiveAppearanceIgnoringKeyFocus() -> Bool { true }
    @objc func _hasKeyAppearance() -> Bool { true }
    @objc func _hasMainAppearance() -> Bool { true }
}

@MainActor enum WindowCapture {
    private typealias Create = @convention(c) (CGRect, UInt32, UInt32, UInt32) -> Unmanaged<CGImage>?
    /// CGWindowListCreateImage of our own window: no screen-recording permission is needed for it.
    static func image(of window: NSWindow) -> CGImage? {
        guard let handle = dlopen(nil, RTLD_NOW), let symbol = dlsym(handle, "CGWindowListCreateImage") else { return nil }
        let create = unsafeBitCast(symbol, to: Create.self)
        // kCGWindowListOptionIncludingWindow, kCGWindowImageBoundsIgnoreFraming | kCGWindowImageBestResolution
        return create(.null, 1 << 3, UInt32(window.windowNumber), (1 << 0) | (1 << 3))?.takeRetainedValue()
    }
}

@main struct Snapshots {
    static let screens = ["home", "home-first", "servers", "servers-empty", "settings", "library-demos", "library-screenshots", "about",
                          "setup-data", "setup-data-missing", "setup-key", "setup-shaders", "setup-ready"]

    @MainActor static func main() {
        let arguments = CommandLine.arguments
        guard arguments.count >= 2 else { fputs("usage: LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]\n", stderr); exit(2) }
        let output = URL(fileURLWithPath: arguments[1])
        let data = arguments.firstIndex(of: "--data").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let fallback = arguments.contains("--fallback"), scale2 = arguments.contains("--scale2")
        let only = arguments.firstIndex(of: "--only").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let app = NSApplication.shared
        app.setActivationPolicy(.accessory)
        LauncherFonts.register()
        guard NSFont(name: LauncherFonts.stencilHeavy, size: 12) != nil, NSFont(name: LauncherFonts.typewriter, size: 12) != nil else {
            fputs("Bundled fonts did not register.\n", stderr); exit(1)
        }
        do { try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true) } catch { fputs("\(error)\n", stderr); exit(1) }
        if arguments.contains("--contrast") {
            guard let data else { fputs("--contrast needs --data GAME_DIR\n", stderr); exit(2) }
            exit(contrastReport(data: data, output: output) ? 0 : 1)
        }
        Task { @MainActor in
            var written = 0
            for dark in [true, false] {
                for screen in screens where only == nil || only == screen {
                    let model = LauncherModel(snapshot: true)
                    model.preview(screen)
                    if let data {
                        let maps = model.servers.map(\.map) + model.media.compactMap { $0.facts.map.map { "mp_" + $0 } } + ["mp_toujane"]
                        model.artwork.loadPrivatePreviewSynchronously(dataPath: data, maps: maps)
                    }
                    let name = "\(screen)-\(dark ? "dark" : "light")"
                    if let image = await capture(model: model, dark: dark, fallback: fallback) {
                        try? write(image, to: output.appendingPathComponent(name + ".png")); written += 1
                    } else { fputs("Capture failed: \(name)\n", stderr) }
                    if scale2, let image = render2x(model: model, dark: dark) {
                        try? write(image, to: output.appendingPathComponent(name + "@2x.png")); written += 1
                    }
                }
            }
            print("Rendered \(written) launcher review images to \(output.path)\(fallback ? " (material fallback)" : "")\(data == nil ? "" : " with private game artwork")")
            exit(written > 0 ? 0 : 1)
        }
        app.run()
    }

    @MainActor static func root(_ model: LauncherModel, dark: Bool, glass: Bool) -> some View {
        LauncherRoot(model: model)
            .environment(\.usesGlass, glass && LiquidGlass.available)
            .environment(\.colorScheme, dark ? .dark : .light)
            .environment(\.controlActiveState, .key)
    }


    @MainActor static func capture(model: LauncherModel, dark: Bool, fallback: Bool) async -> CGImage? {
        let appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        NSApp.appearance = appearance
        let controller = NSHostingController(rootView: root(model, dark: dark, glass: !fallback))
        controller.sizingOptions = []
        if #available(macOS 14.0, *) { controller.sceneBridgingOptions = [.toolbars] }
        let window = SnapshotWindow(contentRect: NSRect(x: 0, y: 0, width: 1440, height: 900),
                                    styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView], backing: .buffered, defer: false)
        window.appearance = appearance
        window.titlebarAppearsTransparent = true
        window.titleVisibility = .hidden
        window.toolbarStyle = .unified
        window.isReleasedWhenClosed = false
        window.contentViewController = controller
        window.setFrame(NSRect(x: 40, y: 40, width: 1440, height: 900), display: true)
        window.orderFrontRegardless()
        try? await Task.sleep(for: .milliseconds(1400))
        window.setFrame(NSRect(x: 40, y: 40, width: 1440, height: 900), display: true)
        try? await Task.sleep(for: .milliseconds(400))
        let image = WindowCapture.image(of: window)
        window.orderOut(nil); window.close()
        return image
    }

    /// 2× offscreen render of the material styling, for text and vector crispness checks.
    @MainActor static func render2x(model: LauncherModel, dark: Bool) -> CGImage? {
        let view = NSHostingView(rootView: root(model, dark: dark, glass: false))
        view.frame = NSRect(x: 0, y: 0, width: 1440, height: 900)
        view.appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        view.layoutSubtreeIfNeeded()
        guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 2880, pixelsHigh: 1800, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                                         isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else { return nil }
        rep.size = view.bounds.size
        RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.3))
        view.cacheDisplay(in: view.bounds, to: rep)
        return rep.cgImage
    }

    /// Measures text contrast over the player's own loading screens without looking at, or writing,
    /// any of their pixels: each hero backdrop renders offscreen without its text, and only the
    /// contrast numbers are kept. The text boxes are where Home sets its eyebrow, title, meta and hint.
    @MainActor static func contrastReport(data: String, output: URL) -> Bool {
        let maps = MapCatalog.stockKeys.map { "mp_" + $0 }
        let store = MapArtworkStore(home: URL(fileURLWithPath: "/Preview/CoD2 Silicon"))
        store.loadPrivatePreviewSynchronously(dataPath: data, maps: maps)
        let leading: CGFloat = 232
        // (name, window rect, ink index in HeroInk)
        let boxes: [(String, CGRect, Int)] = [("eyebrow", CGRect(x: 300, y: 150, width: 420, height: 18), 2),
                                              ("title", CGRect(x: 300, y: 182, width: 760, height: 114), 0),
                                              ("meta", CGRect(x: 300, y: 316, width: 560, height: 22), 1),
                                              ("hint", CGRect(x: 650, y: 388, width: 120, height: 16), 3)]
        var rows: [[String: Any]] = [], worst = Double.infinity, measured = 0
        for dark in [true, false] {
            for map in maps {
                guard let image = store.image(for: map) else { continue }
                let view = HeroBackdrop(map: map, image: image, leading: leading)
                    .environment(\.palette, dark ? Palette.dark : Palette.light)
                    .frame(width: 1440 - leading, height: 620)
                let renderer = ImageRenderer(content: view)
                renderer.scale = 1
                guard let cg = renderer.cgImage, let space = CGColorSpace(name: CGColorSpace.sRGB) else { continue }
                let width = cg.width, height = cg.height
                var pixels = [UInt8](repeating: 0, count: width * height * 4)
                let drawn = pixels.withUnsafeMutableBytes { raw -> Bool in
                    guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space,
                                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return false }
                    context.draw(cg, in: CGRect(x: 0, y: 0, width: width, height: height))
                    return true
                }
                guard drawn else { continue }
                var row: [String: Any] = ["map": map, "appearance": dark ? "dark" : "light"]
                for (name, box, ink) in boxes {
                    var values: [Double] = []
                    for y in Int(box.minY)..<Int(box.maxY) where y < height {
                        for x in Int(box.minX - leading)..<Int(box.maxX - leading) where x >= 0 && x < width {
                            let o = (y * width + x) * 4   // CGContext rows run top-down in memory
                            values.append(ScrimMath.luminance(pixels[o], pixels[o + 1], pixels[o + 2]))
                        }
                    }
                    values.sort()
                    let background = dark ? values[values.count * 98 / 100] : values[values.count * 2 / 100]
                    let text = HeroInk.luminance((dark ? HeroInk.dark : HeroInk.light)[ink])
                    let ratio = ScrimMath.contrast(text, background)
                    row[name] = (ratio * 100).rounded() / 100
                    worst = min(worst, ratio)
                }
                rows.append(row); measured += 1
            }
        }
        let report: [String: Any] = ["measured": measured, "worst": (worst * 100).rounded() / 100, "percentile": "98th brightest (dark) / 2nd darkest (light) background pixel", "maps": rows]
        if let json = try? JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys]) {
            try? json.write(to: output.appendingPathComponent("contrast.json"))
        }
        print("Contrast over \(measured) private loading-screen heroes: worst \(String(format: "%.2f", worst)):1 (target 4.5:1)")
        return measured > 0 && worst >= 4.5
    }

    static func write(_ image: CGImage, to url: URL) throws {
        guard let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) else { throw LauncherError(message: "PNG encoding failed") }
        try png.write(to: url)
    }
}
