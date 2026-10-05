import AppKit
import SwiftUI

// Review harness, never shipped. It hosts the real launcher views in a titled window
// configured like the app's (hidden title bar, unified toolbar) and captures it through
// the window server, so Liquid Glass, materials and toolbars render as they do on screen.
//
//   LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]
//
// --data uses the player's own loading screens (private review only; never commit them).
// --fallback forces the macOS 13–25 material styling. --scale2 adds 2× renders of the same
// window for checking text, vectors and shapes (material styling; system glass draws flat there).

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
            var written = 0, checked = 0
            var findings: [String] = [], report: [[String: Any]] = []
            for dark in [true, false] {
                for screen in screens where only == nil || only == screen {
                    let model = LauncherModel(snapshot: true)
                    model.preview(screen)
                    if let data {
                        let maps = model.servers.map(\.map) + model.media.compactMap { $0.facts.map.map { "mp_" + $0 } } + ["mp_toujane"]
                        model.artwork.loadPrivatePreviewSynchronously(dataPath: data, maps: maps)
                    }
                    let name = "\(screen)-\(dark ? "dark" : "light")"
                    let audit = ShapeAudit()
                    if let image = await capture(model: model, dark: dark, fallback: fallback, audit: audit) {
                        try? write(image, to: output.appendingPathComponent(name + ".png")); written += 1
                        let items = Array(audit.items.values)
                        let found = ShapeCheck.findings(items, image: image).map { "\(name): \($0)" }
                        checked += items.count; findings += found
                        report.append(["screen": name, "items": items.map(ShapeCheck.json), "findings": found])
                    } else { fputs("Capture failed: \(name)\n", stderr) }
                    if scale2, let image = await render2x(model: model, dark: dark) {
                        try? write(image, to: output.appendingPathComponent(name + "@2x.png")); written += 1
                    }
                }
            }
            print("Rendered \(written) launcher review images to \(output.path)\(fallback ? " (material fallback)" : "")\(data == nil ? "" : " with private game artwork")")
            if let json = try? JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys]) {
                try? json.write(to: output.appendingPathComponent("shape-audit.json"))
            }
            findings.forEach { print("SHAPE: \($0)") }
            print("Shape audit: \(checked) shapes checked, \(findings.count) findings")
            exit(written > 0 && findings.isEmpty ? 0 : 1)
        }
        app.run()
    }

    @MainActor static func root(_ model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil) -> some View {
        LauncherRoot(model: model)
            .environment(\.shapeAudit, audit)
            .environment(\.usesGlass, glass && LiquidGlass.available)
            .environment(\.colorScheme, dark ? .dark : .light)
            .environment(\.controlActiveState, .key)
    }


    @MainActor static func capture(model: LauncherModel, dark: Bool, fallback: Bool, audit: ShapeAudit) async -> CGImage? {
        let window = await show(model: model, dark: dark, glass: !fallback, audit: audit)
        let image = WindowCapture.image(of: window)
        window.orderOut(nil); window.close()
        return image
    }

    /// The app-like window, shown and laid out at 1440×900.
    @MainActor static func show(model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil) async -> NSWindow {
        let appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        NSApp.appearance = appearance
        let controller = NSHostingController(rootView: root(model, dark: dark, glass: glass, audit: audit))
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
        return window
    }

    /// 2× render of the same window with the material styling, toolbar included, for text, vector
    /// and shape checks. A bare hosting view has no window, so the split view wouldn't lay out.
    @MainActor static func render2x(model: LauncherModel, dark: Bool) async -> CGImage? {
        let window = await show(model: model, dark: dark, glass: false)
        defer { window.orderOut(nil); window.close() }
        guard let frame = window.contentView?.superview,
              let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 2880, pixelsHigh: 1800, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                                         isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else { return nil }
        rep.size = frame.bounds.size
        frame.cacheDisplay(in: frame.bounds, to: rep)
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

/// The shape system, checked on every captured screen from the frames the views report:
/// - a button is a capsule or a circle (and a prominent one is seen to be one in the pixels);
/// - a shape that reaches into a container's corner is concentric with it: equal insets to both
///   edges, and a radius of the container's radius minus that inset;
/// - a shape centred in a row sits as far from the row's end as from its top and bottom;
/// - controls and fields side by side share one height.
@MainActor enum ShapeCheck {
    typealias Item = ShapeAudit.Item
    static let tolerance: CGFloat = 0.75

    static func findings(_ items: [Item], image: CGImage) -> [String] {
        var out: [String] = []
        let pixels = Pixels(image)
        let containers = items.filter { $0.role == .container }
        func area(_ item: Item) -> CGFloat { item.frame.width * item.frame.height }
        func parent(of item: Item) -> Item? {
            containers.filter { $0 != item && area($0) > area(item) && $0.frame.insetBy(dx: -0.5, dy: -0.5).contains(item.frame) }
                .min { area($0) < area($1) }
        }
        for item in items where item.role == .control {
            if case .rounded = item.form { out.append("\(describe(item)) is a rounded rectangle; buttons are capsules or circles") }
            if item.name == "prominent", let seen = pixels?.shape(of: item.frame), seen != "capsule" {
                out.append("\(describe(item)) draws as a \(seen), not a capsule")
            }
        }
        for item in items {
            guard let container = parent(of: item) else { continue }
            let f = item.frame, k = container.frame
            let top = f.minY - k.minY, bottom = k.maxY - f.maxY, leading = f.minX - k.minX, trailing = k.maxX - f.maxX
            let outer = radius(container), inner = radius(item)
            let corners = [("top-leading", top, leading), ("top-trailing", top, trailing), ("bottom-leading", bottom, leading), ("bottom-trailing", bottom, trailing)]
            var inCorner = false
            for (corner, a, b) in corners where a < outer - tolerance && b < outer - tolerance {
                inCorner = true
                if abs(a - b) > tolerance {
                    out.append("\(describe(item)) in \(describe(container)): \(corner) insets \(fmt(a)) and \(fmt(b)) differ")
                } else if abs(inner - (outer - a)) > tolerance {
                    out.append("\(describe(item)) in \(describe(container)): radius \(fmt(inner)) is not concentric (\(fmt(outer)) − \(fmt(a)) = \(fmt(outer - a)))")
                }
            }
            if !inCorner, abs(top - bottom) <= tolerance {
                for (side, inset) in [("leading", leading), ("trailing", trailing)] where inset < 2 * top && abs(inset - top) > tolerance {
                    out.append("\(describe(item)) in \(describe(container)): \(side) inset \(fmt(inset)) but vertical inset \(fmt(top))")
                }
            }
        }
        // Side by side: controls and fields whose vertical ranges overlap, in the same container and close together.
        let rowItems = items.filter { $0.role == .control || $0.role == .field }
        for (index, a) in rowItems.enumerated() {
            for b in rowItems[(index + 1)...] where parent(of: a) == parent(of: b) {
                let gap = max(b.frame.minX - a.frame.maxX, a.frame.minX - b.frame.maxX)
                let overlap = min(a.frame.maxY, b.frame.maxY) - max(a.frame.minY, b.frame.minY)
                if gap >= 0, gap < 40, overlap > 0, abs(a.frame.height - b.frame.height) > tolerance {
                    out.append("\(describe(a)) and \(describe(b)) sit side by side at different heights")
                }
            }
        }
        return out
    }

    static func radius(_ item: Item) -> CGFloat {
        let short = min(item.frame.width, item.frame.height) / 2
        switch item.form {
        case .capsule, .circle: return short
        case .rounded(let value): return min(value, short)
        }
    }
    static func fmt(_ value: CGFloat) -> String { String(format: value == value.rounded() ? "%.0f" : "%.1f", value) }
    static func describe(_ item: Item) -> String {
        let f = item.frame
        return "\(item.name) [\(fmt(f.minX)),\(fmt(f.minY)) \(fmt(f.width))×\(fmt(f.height))]"
    }
    static func json(_ item: Item) -> [String: Any] {
        let form: String
        switch item.form {
        case .capsule: form = "capsule"
        case .circle: form = "circle"
        case .rounded(let value): form = "rounded \(fmt(value))"
        }
        return ["role": item.role.rawValue, "name": item.name, "form": form,
                "frame": [item.frame.minX, item.frame.minY, item.frame.width, item.frame.height].map { Double($0) }]
    }

    /// The captured window's pixels, addressed in window points.
    struct Pixels {
        var data: [UInt8], width: Int, height: Int, scale: CGFloat
        init?(_ image: CGImage) {
            width = image.width; height = image.height; scale = CGFloat(image.width) / 1440
            data = [UInt8](repeating: 0, count: width * height * 4)
            let (w, h) = (width, height)
            let drawn = data.withUnsafeMutableBytes { raw -> Bool in
                guard let space = CGColorSpace(name: CGColorSpace.sRGB),
                      let context = CGContext(data: raw.baseAddress, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4, space: space,
                                              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return false }
                context.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
                return true
            }
            if !drawn { return nil }
        }
        func color(_ x: CGFloat, _ y: CGFloat) -> [Int] {
            let px = min(width - 1, max(0, Int(x * scale))), py = min(height - 1, max(0, Int(y * scale)))
            let o = (py * width + px) * 4
            return [Int(data[o]), Int(data[o + 1]), Int(data[o + 2])]
        }
        static func distance(_ a: [Int], _ b: [Int]) -> Int { zip(a, b).map { abs($0 - $1) }.reduce(0, +) }
        /// "capsule" when the point 2 pt inside the top-leading corner matches the background beside the
        /// button, "rounded rectangle" when it matches the fill; nil when fill and background are too alike.
        func shape(of frame: CGRect) -> String? {
            let fill = color(frame.minX + 4, frame.midY), outside = color(frame.minX - 3, frame.midY)
            let corner = color(frame.minX + 2, frame.minY + 2)
            guard Self.distance(fill, outside) > 90 else { return nil }
            return Self.distance(corner, outside) < Self.distance(corner, fill) ? "capsule" : "rounded rectangle"
        }
    }
}
