import AppKit
import ApplicationServices
import SwiftUI

// Review harness, never shipped. It hosts the real launcher views in a titled window
// configured like the app's (hidden title bar, unified toolbar) and captures it through
// the window server, so Liquid Glass, materials and toolbars render as they do on screen.
//
//   LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN] [--group configs]
//                            [--increase-contrast] [--reduce-transparency] [--actions] [--minimum]
//                            [--accessibility] [--overlays]
//
// --data uses the player's own loading screens (private review only; never commit them).
// --fallback forces the macOS 13–25 material styling. --scale2 adds 2× renders of the same
// window for checking text, vectors and shapes (material styling; system glass draws flat there).
// --group configs renders only the Config Studio screens; --increase-contrast and
// --reduce-transparency set those accessibility environments for the capture. --minimum uses the
// launcher's 1100×760 minimum window.
// --actions clicks Config Studio controls in the window (real hit-testing and actions) and checks
// the bytes and History rows written to a temporary home.
// --accessibility reads each Config Studio window's accessibility tree, as VoiceOver would, writes
// it in reading order to OUTPUT/accessibility, and fails on a control without a name.
// --overlays opens sheets, popovers and alerts with real clicks and captures them over the window.
// Config Studio screens use synthetic loadouts in a private temporary home, removed afterwards.
// COD2_SNAPSHOT_GEOMETRY=1 logs AppKit, view and WindowServer geometry at each sizing step;
// COD2_SNAPSHOT_TRACE=1 logs each step of the window-driving modes to stderr.

/// The review host can't become the active app while the login session is locked, so the
/// window reports key and main appearance. Test-only: these overrides never ship.
final class SnapshotWindow: NSWindow {
    override func constrainFrameRect(_ frameRect: NSRect, to screen: NSScreen?) -> NSRect {
        let result = super.constrainFrameRect(frameRect, to: screen)
        if ProcessInfo.processInfo.environment["COD2_SNAPSHOT_GEOMETRY"] == "1" {
            print("CONSTRAIN requested=\(frameRect) result=\(result) screen=\(String(describing: screen?.visibleFrame))")
        }
        return result
    }
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
        guard let primary = NSScreen.screens.first,
              let handle = dlopen(nil, RTLD_NOW), let symbol = dlsym(handle, "CGWindowListCreateImage") else { return nil }
        let create = unsafeBitCast(symbol, to: Create.self)
        // A null capture rect uses WindowServer's framing bounds, which on macOS 27 are
        // inset from the AppKit canvas (1412×882 for a 1440×900 window). Capture the actual
        // frame without rescaling so the image and SwiftUI's shape-audit coordinates agree.
        // Quartz's screen origin is the primary display's top-left; AppKit's is bottom-left.
        let frame = window.frame
        let bounds = CGRect(x: frame.minX, y: primary.frame.maxY - frame.maxY, width: frame.width, height: frame.height)
        // kCGWindowListOptionIncludingWindow, kCGWindowImageBestResolution (shadow outside bounds excluded).
        return create(bounds, 1 << 3, UInt32(window.windowNumber), 1 << 3)?.takeRetainedValue()
    }

    private typealias CreateFromArray = @convention(c) (CGRect, CFArray, UInt32) -> Unmanaged<CGImage>?
    /// Our own windows composited front to back (a sheet, popover or alert over the window),
    /// cropped to the window: other apps' windows never enter the image.
    static func image(of windows: [NSWindow], in window: NSWindow) -> CGImage? {
        guard let primary = NSScreen.screens.first,
              let handle = dlopen(nil, RTLD_NOW), let symbol = dlsym(handle, "CGWindowListCreateImageFromArray") else { return nil }
        let create = unsafeBitCast(symbol, to: CreateFromArray.self)
        // The window plus anything that hangs past its edge, such as a popover.
        let frame = windows.reduce(window.frame) { $0.union($1.frame) }.intersection(primary.frame)
        let bounds = CGRect(x: frame.minX, y: primary.frame.maxY - frame.maxY, width: frame.width, height: frame.height)
        var ids: [UnsafeRawPointer?] = windows.map { UnsafeRawPointer(bitPattern: UInt($0.windowNumber)) }
        guard let array = CFArrayCreate(nil, &ids, ids.count, nil) else { return nil }
        return create(bounds, array, 1 << 3)?.takeRetainedValue()
    }
}

/// The window's accessibility tree as VoiceOver reads it, through the AX API. Asking our own
/// process needs no permission and changes no setting, and it makes SwiftUI build its nodes.
/// (Reading SwiftUI's nodes directly can raise inside a Menu; the AX API reports an error instead.)
@MainActor enum AccessibilityTree {
    struct Element {
        let role: String, subrole: String, label: String, value: String, help: String, selected: Bool
        /// Screen rectangle, top-left origin (Quartz).
        let frame: CGRect
        let depth: Int
        let element: AXUIElement
        var line: String {
            String(repeating: "  ", count: depth) + role + (label.isEmpty ? "" : " “\(label)”") + (value.isEmpty ? "" : " = \(value)")
                + (selected ? " [selected]" : "") + (help.isEmpty ? "" : " (\(help))")
        }
    }
    static let controls: Set<String> = ["AXButton", "AXCheckBox", "AXRadioButton", "AXSlider", "AXTextField", "AXTextArea", "AXPopUpButton",
                                        "AXMenuButton", "AXComboBox", "AXLink", "AXDisclosureTriangle", "AXIncrementor", "AXSearchField"]
    static var application: AXUIElement { AXUIElementCreateApplication(getpid()) }
    static func wake() { _ = attribute(application, kAXWindowsAttribute) }
    static func attribute(_ element: AXUIElement, _ name: String) -> CFTypeRef? {
        var value: CFTypeRef?
        return AXUIElementCopyAttributeValue(element, name as CFString, &value) == .success ? value : nil
    }
    static func text(_ element: AXUIElement, _ name: String) -> String { attribute(element, name) as? String ?? "" }
    static func frame(_ element: AXUIElement) -> CGRect {
        var point = CGPoint.zero, size = CGSize.zero
        if let value = attribute(element, kAXPositionAttribute), CFGetTypeID(value) == AXValueGetTypeID() { AXValueGetValue(value as! AXValue, .cgPoint, &point) }
        if let value = attribute(element, kAXSizeAttribute), CFGetTypeID(value) == AXValueGetTypeID() { AXValueGetValue(value as! AXValue, .cgSize, &size) }
        return CGRect(origin: point, size: size)
    }
    /// The AX window that is this NSWindow (same frame), with its sheets and popovers as children or siblings.
    static func windows(matching window: NSWindow?) -> [AXUIElement] {
        let all = attribute(application, kAXWindowsAttribute) as? [AXUIElement] ?? []
        guard let window, let primary = NSScreen.screens.first else { return all }
        let target = CGRect(x: window.frame.minX, y: primary.frame.maxY - window.frame.maxY, width: window.frame.width, height: window.frame.height)
        return all.filter { abs(frame($0).minX - target.minX) < 2 && abs(frame($0).minY - target.minY) < 2 && abs(frame($0).width - target.width) < 2 }
    }
    static func elements(of window: NSWindow?) -> [Element] {
        var result: [Element] = []
        func walk(_ element: AXUIElement, depth: Int) {
            guard depth < 80 else { return }
            let role = text(element, kAXRoleAttribute)
            let label = [text(element, kAXDescriptionAttribute), text(element, kAXTitleAttribute), text(element, kAXPlaceholderValueAttribute)].first { !$0.isEmpty } ?? ""
            var value = ""
            if let raw = attribute(element, kAXValueAttribute) {
                if let string = raw as? String { value = string } else if let number = raw as? NSNumber { value = number.stringValue }
            }
            if value.count > 80 { value = String(value.prefix(80)) + "…" }
            result.append(Element(role: role, subrole: text(element, kAXSubroleAttribute), label: label, value: value.replacingOccurrences(of: "\n", with: " "),
                                  help: text(element, kAXHelpAttribute), selected: (attribute(element, kAXSelectedAttribute) as? Bool) == true,
                                  frame: frame(element), depth: depth, element: element))
            for child in attribute(element, kAXChildrenAttribute) as? [AXUIElement] ?? [] { walk(child, depth: depth + 1) }
        }
        for root in windows(matching: window) { walk(root, depth: 0) }
        return result
    }
    /// Named elements with no role, which VoiceOver reads without saying what they are.
    static func roleless(_ elements: [Element]) -> [Element] {
        elements.filter { ($0.role == "AXUnknown" || $0.role.isEmpty) && !$0.label.isEmpty }
    }
    /// Controls VoiceOver would announce without a name.
    static func unnamed(_ elements: [Element]) -> [Element] {
        // The window's own buttons and scroll-bar parts are named by their subrole; selectable
        // read-only text is read by its value.
        let system: Set<String> = ["AXCloseButton", "AXMinimizeButton", "AXZoomButton", "AXFullScreenButton",
                                   "AXIncrementArrow", "AXDecrementArrow", "AXIncrementPage", "AXDecrementPage"]
        return elements.indices.compactMap { index in
            let element = elements[index]
            guard controls.contains(element.role), !system.contains(element.subrole), element.label.trimmingCharacters(in: .whitespaces).isEmpty else { return nil }
            if ["AXTextArea", "AXTextField"].contains(element.role), !settable(element.element) {
                // Read-only text is read by its contents, its own value or the text inside it.
                let inside = elements[(index + 1)...].prefix { $0.depth > element.depth }
                if !element.value.isEmpty || inside.contains(where: { $0.role == "AXStaticText" && !$0.value.isEmpty }) { return nil }
            }
            return element
        }
    }
    static func settable(_ element: AXUIElement) -> Bool {
        var settable: DarwinBoolean = false
        return AXUIElementIsAttributeSettable(element, kAXValueAttribute as CFString, &settable) == .success && settable.boolValue
    }
}

@main struct Snapshots {
    static let screens = ["home", "home-first", "servers", "servers-empty", "settings", "library-demos", "library-screenshots", "about",
                          "setup-data", "setup-data-missing", "setup-key", "setup-shaders", "setup-ready"] + configScreens
    static let configScreens = ["configs-guided", "configs-guided-aim", "configs-expert", "configs-text", "configs-history", "configs-running",
                                "discover-unavailable", "discover-players", "discover-saved", "discover-player", "configs-import",
                                "configs-import-everything", "servers-ready", "servers-ready-fixed",
                                "configs-binds-list", "configs-binds-advanced", "configs-binds-visual", "configs-checkup",
                                "configs-guided-rules", "configs-expert-rules",
                                "configs-binds-verify", "configs-binds-learn", "configs-expert-player"]

    @MainActor static func main() {
        let arguments = CommandLine.arguments
        guard arguments.count >= 2 else { fputs("usage: LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]\n", stderr); exit(2) }
        let output = URL(fileURLWithPath: arguments[1])
        let data = arguments.firstIndex(of: "--data").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let fallback = arguments.contains("--fallback"), scale2 = arguments.contains("--scale2")
        let only = arguments.firstIndex(of: "--only").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let configsOnly = arguments.firstIndex(of: "--group").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil } == "configs"
        if arguments.contains("--minimum") { windowSize = NSSize(width: 1100, height: 760) }
        let accessibility = Accessibility(increaseContrast: arguments.contains("--increase-contrast"), reduceTransparency: arguments.contains("--reduce-transparency"))
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
        if arguments.contains("--actions") {
            Task { @MainActor in exit(await ConfigsPreview.actions() ? 0 : 1) }
            app.run()
        }
        if arguments.contains("--accessibility") {
            Task { @MainActor in exit(await ConfigsPreview.accessibility(output: output, only: only) ? 0 : 1) }
            app.run()
        }
        if arguments.contains("--overlays") {
            Task { @MainActor in exit(await ConfigsPreview.overlays(output: output, only: only) ? 0 : 1) }
            app.run()
        }
        Task { @MainActor in
            var written = 0, checked = 0
            var findings: [String] = [], report: [[String: Any]] = []
            for dark in [true, false] {
                for screen in screens where (only == nil || only == screen) && (!configsOnly || configScreens.contains(screen)) {
                    let model = LauncherModel(snapshot: true)
                    model.preview(screen)
                    var home: URL?
                    if configScreens.contains(screen) {
                        do { home = try ConfigsPreview.seed(model, screen: screen); try await ConfigsPreview.prepare(model, screen: screen, home: home!) }
                        catch { fputs("Config Studio preview failed for \(screen): \(error)\n", stderr); exit(1) }
                    }
                    defer { if let home { try? FileManager.default.removeItem(at: home) } }
                    if let data {
                        let maps = model.servers.map(\.map) + model.media.compactMap { $0.facts.map.map { "mp_" + $0 } } + ["mp_toujane"]
                        model.artwork.loadPrivatePreviewSynchronously(dataPath: data, maps: maps)
                    }
                    let name = "\(screen)-\(dark ? "dark" : "light")"
                    let audit = ShapeAudit()
                    let captured = await capture(model: model, dark: dark, fallback: fallback, audit: audit, accessibility: accessibility)
                    if let sheet = captured.sheet { try? write(sheet, to: output.appendingPathComponent(name + "-sheet.png")); written += 1 }
                    if let image = captured.window {
                        try? write(image, to: output.appendingPathComponent(name + ".png")); written += 1
                        let items = Array(audit.items.values)
                        let found = ShapeCheck.findings(items, image: image).map { "\(name): \($0)" }
                        checked += items.count; findings += found
                        report.append(["screen": name, "items": items.map(ShapeCheck.json), "findings": found])
                    } else { fputs("Capture failed: \(name)\n", stderr) }
                    if scale2 {
                        let rendered = await render2x(model: model, dark: dark, accessibility: accessibility)
                        if let image = rendered.window { try? write(image, to: output.appendingPathComponent(name + "@2x.png")); written += 1 }
                        if let sheet = rendered.sheet { try? write(sheet, to: output.appendingPathComponent(name + "-sheet@2x.png")); written += 1 }
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

    struct Accessibility { var increaseContrast = false; var reduceTransparency = false }

    @MainActor static func root(_ model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil, accessibility: Accessibility = .init()) -> some View {
        LauncherRoot(model: model)
            .environment(\.shapeAudit, audit)
            .environment(\.usesGlass, glass && LiquidGlass.available)
            .environment(\.colorScheme, dark ? .dark : .light)
            .environment(\.controlActiveState, .key)
            // Test-only overrides of the system settings, without changing them for the Mac.
            .environment(\._colorSchemeContrast, accessibility.increaseContrast ? .increased : .standard)
            .environment(\._accessibilityReduceTransparency, accessibility.reduceTransparency)
    }


    @MainActor static func geometry(_ window: NSWindow, _ step: String, image: CGImage? = nil) {
        guard ProcessInfo.processInfo.environment["COD2_SNAPSHOT_GEOMETRY"] == "1" else { return }
        var row: [String: Any] = ["step": step, "frame": NSStringFromRect(window.frame),
            "contentLayoutRect": NSStringFromRect(window.contentLayoutRect), "scale": window.backingScaleFactor,
            "screenFrame": NSStringFromRect(window.screen?.frame ?? .zero),
            "screenVisibleFrame": NSStringFromRect(window.screen?.visibleFrame ?? .zero)]
        var views: [[String: String]] = []
        var view = window.contentView
        while let current = view {
            views.append(["class": String(describing: type(of: current)), "frame": NSStringFromRect(current.frame), "bounds": NSStringFromRect(current.bounds)])
            view = current.superview
        }
        row["views"] = views
        row["windowServer"] = CGWindowListCopyWindowInfo([.optionIncludingWindow], CGWindowID(window.windowNumber)) as Any
        if let image { row["image"] = [image.width, image.height] }
        if let data = try? JSONSerialization.data(withJSONObject: row, options: [.sortedKeys]), let json = String(data: data, encoding: .utf8) { print("GEOMETRY " + json) }
    }

    /// The window, and an attached sheet (the import review) as its own image.
    @MainActor static func capture(model: LauncherModel, dark: Bool, fallback: Bool, audit: ShapeAudit, accessibility: Accessibility = .init()) async -> (window: CGImage?, sheet: CGImage?) {
        let window = await show(model: model, dark: dark, glass: !fallback, audit: audit, accessibility: accessibility)
        let image = WindowCapture.image(of: window)
        let sheet = window.attachedSheet.flatMap { WindowCapture.image(of: $0) }
        geometry(window, "capture", image: image)
        window.attachedSheet.map { window.endSheet($0) }
        window.orderOut(nil); window.close()
        return (image, sheet)
    }

    /// 1440×900, or the launcher's 1100×760 minimum with --minimum.
    @MainActor static var windowSize = NSSize(width: 1440, height: 900)

    /// The app-like window, shown and laid out at `windowSize`.
    @MainActor static func show(model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil, accessibility: Accessibility = .init()) async -> NSWindow {
        let appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        NSApp.appearance = appearance
        let controller = NSHostingController(rootView: root(model, dark: dark, glass: glass, audit: audit, accessibility: accessibility))
        controller.sizingOptions = []
        if #available(macOS 14.0, *) { controller.sceneBridgingOptions = [.toolbars] }
        let window = SnapshotWindow(contentRect: NSRect(origin: .zero, size: windowSize),
                                    styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView], backing: .buffered, defer: false)
        geometry(window, "init")
        window.appearance = appearance
        window.titlebarAppearsTransparent = true
        window.titleVisibility = .hidden
        window.toolbarStyle = .unified
        window.isReleasedWhenClosed = false
        window.contentViewController = controller
        geometry(window, "content-controller")
        window.setFrame(NSRect(origin: NSPoint(x: 40, y: 40), size: windowSize), display: true)
        geometry(window, "first-set-frame")
        window.orderFrontRegardless()
        geometry(window, "order-front")
        try? await Task.sleep(for: .milliseconds(1400))
        geometry(window, "first-settled")
        window.setFrame(NSRect(origin: NSPoint(x: 40, y: 40), size: windowSize), display: true)
        geometry(window, "second-set-frame")
        try? await Task.sleep(for: .milliseconds(400))
        geometry(window, "settled")
        return window
    }

    /// 2× render of the same window with the material styling, toolbar included, for text, vector
    /// and shape checks. A bare hosting view has no window, so the split view wouldn't lay out.
    @MainActor static func render2x(model: LauncherModel, dark: Bool, accessibility: Accessibility = .init()) async -> (window: CGImage?, sheet: CGImage?) {
        let window = await show(model: model, dark: dark, glass: false, accessibility: accessibility)
        defer { window.attachedSheet.map { window.endSheet($0) }; window.orderOut(nil); window.close() }
        func render(_ view: NSView?) -> CGImage? {
            guard let view, let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(view.bounds.width) * 2, pixelsHigh: Int(view.bounds.height) * 2,
                                                       bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                                                       bytesPerRow: 0, bitsPerPixel: 0) else { return nil }
            rep.size = view.bounds.size
            view.cacheDisplay(in: view.bounds, to: rep)
            return rep.cgImage
        }
        let image = render(window.contentView?.superview)
        geometry(window, "render2x", image: image)
        return (image, render(window.attachedSheet?.contentView?.superview))
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
        @MainActor init?(_ image: CGImage) {
            width = image.width; height = image.height; scale = CGFloat(image.width) / Snapshots.windowSize.width
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

/// A fixed ABC keyboard and a mouse, so Binds renders the same everywhere and never
/// shows the reviewer's own devices.
@MainActor final class PreviewInputProvider: InputDeviceProvider {
    func inputSource() -> InputSourceSnapshot {
        var labels: [UInt16: String] = [:]
        let keys: [(UInt16, String)] = [(0, "a"), (1, "s"), (2, "d"), (3, "f"), (4, "h"), (5, "g"), (6, "z"), (7, "x"), (8, "c"), (9, "v"), (11, "b"),
            (12, "q"), (13, "w"), (14, "e"), (15, "r"), (16, "y"), (17, "t"), (31, "o"), (32, "u"), (34, "i"), (35, "p"), (37, "l"), (38, "j"),
            (40, "k"), (45, "n"), (46, "m"), (18, "1"), (19, "2"), (20, "3"), (21, "4"), (23, "5"), (22, "6"), (26, "7"), (28, "8"), (25, "9"),
            (29, "0"), (27, "-"), (24, "="), (33, "["), (30, "]"), (42, "\\"), (41, ";"), (39, "'"), (43, ","), (47, "."), (44, "/"), (50, "`")]
        for (key, label) in keys { labels[key] = label }
        return InputSourceSnapshot(id: "com.apple.keylayout.ABC", name: "ABC", shape: .ansi, labels: labels)
    }
    func devices() -> [InputDevice] {
        [InputDevice(id: "1", name: "Magic Keyboard", vendorID: 0x05ac, kind: .keyboard, keyboardShape: .ansi),
         InputDevice(id: "2", name: "USB Mouse", vendorID: nil, kind: .mouse, buttonCount: 5)]
    }
    func startObserving(_ changed: @escaping @MainActor () -> Void) {}
    func stopObserving() {}
}

/// Holds Press keys to verify or Learn button positions open at a chosen step for a still
/// capture. It never installs an event monitor, so it never hears the reviewer's input.
@MainActor final class PreviewCaptureSource: BindingCaptureEventSource {
    private var handler: (@MainActor (BindingCaptureEvent) -> BindingCaptureDisposition)?
    func start(_ handler: @escaping @MainActor (BindingCaptureEvent) -> BindingCaptureDisposition) { self.handler = handler }
    func stop() { handler = nil }
    func send(_ event: BindingCaptureEvent) { _ = handler?(event) }
}

/// Synthetic loadouts for the Config Studio screens, written through the real store into a
/// private temporary home. Test preview data only: generic loadout names, no real players.
@MainActor enum ConfigsPreview {
    static let dates = ["2026-09-28T19:40:00", "2026-10-02T20:02:00", "2026-10-02T20:03:00", "2026-10-04T21:20:00",
                        "2026-10-05T22:31:00", "2026-10-06T17:55:00", "2026-10-06T18:42:00", "2026-10-06T18:44:00",
                        "2026-10-06T18:50:00", "2026-10-06T18:52:00"]

    /// A canonical game-written config: header, sorted binds, then sorted settings.
    static func config(_ values: [String: String]) -> ConfigDocument {
        var text = "// generated by Call of Duty, do not modify\nunbindall\nunbindall\n"
        for (key, action) in [("TAB", "+scores"), ("SPACE", "+gostand"), ("4", "+smoke"), ("a", "+moveleft"), ("c", "togglecrouch"), ("d", "+moveright"),
                              ("e", "+leanright"), ("q", "+leanleft"), ("r", "+reload"), ("s", "+back"), ("t", "chatmodepublic"), ("v", "+melee"),
                              ("w", "+forward"), ("y", "chatmodeteam"), ("z", "mp_quickmessage"), ("CTRL", "toggleprone"), ("SHIFT", "+holdbreath"),
                              ("MOUSE1", "+attack"), ("MOUSE2", "+toggleads_throw"), ("MWHEELDOWN", "weapprev"), ("MWHEELUP", "weapnext")] {
            text += "bind \(key) \"\(action)\"\n"
        }
        for name in values.keys.sorted(by: { $0.lowercased() < $1.lowercased() }) { text += "seta \(name) \"\(values[name]!)\"\n" }
        return ConfigDocument(data: Data(text.utf8), dialect: .cod2x)
    }

    @discardableResult
    static func seed(_ model: LauncherModel, screen: String) throws -> URL {
        let home = FileManager.default.temporaryDirectory.appendingPathComponent("cod2-configs-preview-\(screen)-\(UUID().uuidString.prefix(8))")
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        var values = ["cg_chatHeight": "8", "cg_drawFPS": "Simple", "cl_maxpackets": "30", "com_maxfps": "333", "in_rawmouse": "1",
                      "name": "^3Sgt. ^7Morrow", "r_gamma": "1", "r_picmip": "1", "rate": "25000", "sensitivity": "2.8", "snaps": "30", "snd_volume": "0.8"]
        var meta = ConfigStudioMetadata(displayName: "Main")
        meta.origin = ConfigOrigin(kind: .migration, source: "main/config_mp.cfg"); meta.dpi = 800
        var main = try store.create(id: "Main", displayName: "Main", document: config(values), metadata: meta)
        try store.select(id: "Main", expectedActive: nil)
        func change(_ edits: [String: String], _ summary: String, metadata: ConfigStudioMetadata? = nil) throws {
            values.merge(edits) { $1 }
            try store.update(id: "Main", document: config(values), expected: main.version, metadata: metadata, summary: summary)
            main = try store.read(id: "Main")
        }
        try change(["r_gamma": "1.4"], "Changed Brightness")
        meta.locks = ["r_gamma"]
        try change([:], "Locked Brightness", metadata: meta)
        try change(["cl_maxpackets": "100"], "Changed Updates you send")
        try change(["r_picmip": "2"], "Changed Texture detail")
        // The game saved a change while it ran.
        let before = main.version
        values["cg_chatHeight"] = "4"
        try PrivateFile.write(config(values).serialized(), to: home.appendingPathComponent("main/players/Main/config_mp.cfg"))
        try store.recordGameSession(id: "Main", before: before)
        main = try store.read(id: "Main")
        try change(["sensitivity": "2.6", "cg_drawFPS": "Off"], "Changed 2 settings")
        meta.notes = ["sensitivity": "Matched to my old mouse pad. Leave it."]
        try change([:], "Noted Mouse speed", metadata: meta)
        var scrim = try store.duplicate(id: "Main", as: "Scrim", displayName: "Scrim", expected: main.version)
        var scrimValues = values
        scrimValues.merge(["sensitivity": "2.1", "com_maxfps": "250", "r_picmip": "3", "cg_drawFPS": "Simple", "snd_volume": "0.6"]) { $1 }
        scrim = try store.read(id: "Scrim")
        try store.update(id: "Scrim", document: config(scrimValues), expected: scrim.version, summary: "Changed 5 settings")
        _ = try store.create(id: "Pub", displayName: "Pub", document: config(["sensitivity": "3", "com_maxfps": "125", "name": "^3Sgt. ^7Morrow"]))
        if screen.hasPrefix("configs-binds") {
            // Two advanced binds made through B2: a toggle and a cycle of messages.
            main = try store.read(id: "Main")
            let chains = [ConfigChain(id: "fps_counter", key: "m", behavior: .settingCycle(name: "cg_drawFPS", values: ["Off", "Simple"]), name: "Frame counter"),
                          ConfigChain(id: "greetings", key: "n", behavior: .commandCycle(steps: [["say gl"], ["say hf"], ["say gg"]]))]
            try ConfigChains.apply(try ConfigChains.prepare(chains: chains, read: main), to: store)
        }
        try spreadDates(home.appendingPathComponent("main/players/Main/history"))

        let running = screen == "configs-running"
        let session = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: { running ? [4242] : [] })
        try session.refresh()
        model.studio.deviceProvider = { PreviewInputProvider() }
        let setup = PreviewCaptureSource()
        if screen == "configs-binds-verify" || screen == "configs-binds-learn" { model.studio.captureSource = { _ in setup } }
        model.studio.attach(session)
        model.studio.loadCatalogNow()
        model.studio.checkup.load()
        switch screen {
        case "configs-expert": model.studio.compareID = "Scrim"
        case "configs-text":
            model.studio.toggleText()
            model.studio.editText(model.studio.text + "seta r_gamma \"1.6\n")
        case "configs-history":
            model.studio.snapshotID = session.history.first { $0.summary == "Changed Texture detail" }?.id
        case "configs-running": try session.reconcileGames([4242])
        case "configs-binds-visual": model.studio.binds.layout = .visual; model.studio.binds.selectedKey = EngineKey(spelling: "w")
        case "configs-binds-advanced": model.studio.binds.selectChain("greetings"); model.studio.binds.showLines = true
        case "configs-checkup", "configs-guided-rules", "configs-expert-rules": model.studio.checkup.rulesetIDs = ["zpam-4.08"]
        case "configs-binds-verify":
            // Second question, after a key that isn't the answer: the note shows beside it.
            model.page = .configs; model.studio.tab = .binds; model.studio.binds.layout = .visual
            model.studio.binds.selectedKey = EngineKey(spelling: "w")
            model.studio.binds.beginVerification()
            setup.send(.key(virtualKey: 6, pressed: true)); setup.send(.key(virtualKey: 6, pressed: false))
            setup.send(.key(virtualKey: 0, pressed: true)); setup.send(.key(virtualKey: 0, pressed: false))
        case "configs-binds-learn":
            // Left and right learned; the wheel is next.
            model.page = .configs; model.studio.tab = .binds; model.studio.binds.layout = .visual
            model.studio.binds.selectedKey = EngineKey(spelling: "w")
            model.studio.binds.beginLearning()
            for button in [0, 1] { setup.send(.mouse(button: button, pressed: true)); setup.send(.mouse(button: button, pressed: false)) }
        case "configs-expert-player": model.page = .configs
        default: break
        }
        return home
    }

    /// Discover, import and ready-check states. Players come from the signed synthetic fixture
    /// (placeholder names, preview only); “discover-unavailable” is the production directory.
    static func prepare(_ model: LauncherModel, screen: String, home: URL) async throws {
        let discover = model.discover
        func fixture() async throws {
            discover.use(try DirectoryFixture.directory(cacheURL: home.appendingPathComponent("directory/index.json")))
            await discover.reload()
        }
        func waitReview() async {
            for _ in 0..<200 {
                guard let review = discover.review, case .loading = review.phase else { return }
                try? await Task.sleep(for: .milliseconds(20))
            }
        }
        switch screen {
        case "discover-unavailable": await discover.reload()
        case "discover-players": try await fixture()
        case "discover-saved":
            try await fixture()
            discover.setSaved("player-2", true); discover.setSaved("player-4", true)
        case "discover-player", "configs-expert-player":
            try await fixture()
            discover.open(.player("player-1"))
            for _ in 0..<200 {
                guard case .loading? = discover.detail?.phase else { break }
                try? await Task.sleep(for: .milliseconds(20))
            }
            if screen == "configs-expert-player", let entry = discover.entry("player-1") {
                // Compare from the player's page: Expert, Aim & input, with the player's markers.
                discover.compareInExpert(entry)
                model.studio.group = .aim
            }
        case "configs-import", "configs-import-everything":
            try await fixture()
            guard let entry = discover.entry("player-1") else { return }
            discover.tryEntry(entry)
            await waitReview()
            if screen == "configs-import" { discover.review?.setTarget("Main") } else { discover.review?.setMode(.everything) }
        case "servers-ready", "servers-ready-fixed":
            // A game-written config saves every archived setting; the rules also read these three.
            let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
            let main = try store.read(id: "Main")
            var document = main.document
            for (name, value) in [("m_filter", "0"), ("cl_mouseAccel", "0"), ("r_picmip_manual", "0")] { try document.setSetting(named: name, value: value) }
            try store.update(id: "Main", document: document, expected: main.version, summary: "Changed 3 settings")
            try model.studio.session?.refresh()
            let server = GameServer(address: "203.0.113.200:28960", fields: ["sv_hostname": "^3Desert ^7Fox ^1League", "mapname": "mp_matmata",
                "g_gametype": "sd", "clients": "8", "sv_maxclients": "10", "protocol": "120", "shortversion": "1.4.6.8", "fs_game": "", "g_competitive": "1"],
                ping: 42, players: (0..<8).map { ServerPlayer(id: $0, score: 30 - $0 * 3, ping: 20 + $0 * 6, name: ["Aurora", "North", "Moss", "Echo", "Scout", "River", "Harbor", "Quill"][$0]) },
                observedAt: Date())
            model.servers.insert(server, at: 0)
            model.selectedServer = server.address
            if screen == "servers-ready-fixed" { try model.fixForServerOnly(server.address) }
        default: break
        }
    }

    /// Spreads History rows over the board's dates, keeping their order.
    static func spreadDates(_ folder: URL) throws {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withFullDate, .withTime, .withColonSeparatorInTime, .withDashSeparatorInDate]
        formatter.timeZone = .current
        let files = try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil).filter { $0.pathExtension == "json" }
        var rows = try files.map { url -> (URL, [String: Any]) in (url, try JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]) }
        rows.sort { ($0.1["date"] as! Double) < ($1.1["date"] as! Double) }
        for (index, row) in rows.enumerated() {
            var object = row.1
            object["date"] = formatter.date(from: dates[min(index, dates.count - 1)])!.timeIntervalSinceReferenceDate
            try PrivateFile.write(JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]), to: row.0)
        }
    }

    /// A real click: hit-testing, the control's own tracking and its action. The mouse-up is
    /// queued first so a control's tracking loop finds it. `point` is in window base coordinates.
    static func click(_ window: NSWindow, at point: NSPoint) {
        func event(_ type: NSEvent.EventType) -> NSEvent? {
            NSEvent.mouseEvent(with: type, location: point, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime,
                               windowNumber: window.windowNumber, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)
        }
        guard let down = event(.leftMouseDown), let up = event(.leftMouseUp) else { return }
        NSApp.postEvent(up, atStart: false)
        window.sendEvent(down)
    }
    static func views<T: NSView>(_ type: T.Type, in view: NSView) -> [T] {
        ((view as? T).map { [$0] } ?? []) + view.subviews.flatMap { views(type, in: $0) }
    }

    /// Real controls, real store: clicking the Show crosshair switch writes one exact edit, and
    /// clicking History's Restore button restores a snapshot as a new transaction.
    static func actions() async -> Bool {
        let model = LauncherModel(snapshot: true)
        model.preview("configs-guided")
        model.studio.group = .hud
        let home: URL
        do { home = try seed(model, screen: "configs-actions") } catch { print("FAIL: preview home: \(error)"); return false }
        defer { try? FileManager.default.removeItem(at: home) }
        guard let session = model.studio.session else { return false }
        let audit = ShapeAudit()
        let window = await Snapshots.show(model: model, dark: true, glass: true, audit: audit)
        defer { window.orderOut(nil); window.close() }
        let file = home.appendingPathComponent("main/players/Main/config_mp.cfg")
        // Guided HUD lists Show crosshair first; the sidebar and loadout column have no switches.
        let switches = views(NSSwitch.self, in: window.contentView!).map { ($0, $0.convert($0.bounds, to: nil)) }
            .filter { $0.1.minX > 700 }.sorted { $0.1.maxY > $1.1.maxY }
        guard let before = try? Data(contentsOf: file), let toggle = switches.first else {
            print("FAIL: the Show crosshair switch is not on screen"); return false
        }
        var expected = ConfigDocument(data: before, dialect: .cod2x)
        do { try expected.setSetting(named: "cg_drawCrosshair", value: "0") } catch { return false }
        click(window, at: NSPoint(x: toggle.1.midX, y: toggle.1.midY))
        try? await Task.sleep(for: .milliseconds(600))
        guard (try? Data(contentsOf: file)) == expected.serialized(), session.history.first?.summary == "Changed Show crosshair" else {
            print("FAIL: clicking Show crosshair did not write exactly one edit (history: \(session.history.first?.summary ?? "none"))"); return false
        }
        print("PASS: clicking the Show crosshair switch wrote `seta cg_drawCrosshair \"0\"` in canonical order and one History row")

        guard let target = session.history.first(where: { $0.summary == "Changed Texture detail" }) else { return false }
        let count = session.history.count
        model.studio.tab = .history
        model.studio.snapshotID = target.id
        try? await Task.sleep(for: .milliseconds(900))
        // The History inspector's one prominent action is Restore This Snapshot.
        guard let restore = audit.items.values.first(where: { $0.name == "prominent" && $0.frame.minX > 1000 })?.frame else {
            print("FAIL: Restore button not on screen"); return false
        }
        click(window, at: NSPoint(x: restore.midX, y: window.frame.height - restore.midY))
        try? await Task.sleep(for: .milliseconds(600))
        guard (try? Data(contentsOf: file)) == target.data, session.history.count == count + 1, session.history.first?.source == .restore,
              session.history.first?.beforeData == expected.serialized() else {
            print("FAIL: Restore did not write the snapshot as a new transaction"); return false
        }
        print("PASS: clicking Restore This Snapshot wrote the snapshot's exact bytes as a new History transaction")

        // Binds: find Throw frag grenade, click its primary key, then press G on the keyboard.
        model.studio.tab = .binds
        model.studio.binds.layout = .list
        model.studio.binds.query = "frag"
        try? await Task.sleep(for: .milliseconds(900))
        guard let chip = audit.items.values.first(where: { $0.name == "bind key +frag primary" })?.frame else { print("FAIL: frag key chip not on screen"); return false }
        let beforeBind = (try? Data(contentsOf: file)) ?? Data()
        click(window, at: NSPoint(x: chip.midX, y: window.frame.height - chip.midY))
        try? await Task.sleep(for: .milliseconds(300))
        guard model.studio.binds.target == .slot(action: "+frag", slot: .primary) else {
            print("FAIL: clicking the key chip at \(chip) did not start listening (window \(model.studio.binds.window != nil), "
                  + "capture \(String(describing: model.studio.binds.capture?.state)), failure \(model.studio.failure?.title ?? "none"))")
            return false
        }
        press(window, keyCode: 5, characters: "g")
        try? await Task.sleep(for: .milliseconds(500))
        var bound = ConfigDocument(data: beforeBind, dialect: .cod2x)
        do { try bound.setBind(key: "g", binding: "+frag") } catch { return false }
        guard (try? Data(contentsOf: file)) == bound.serialized(), session.history.first?.summary == "Changed Throw frag grenade binding",
              model.studio.binds.capture?.isMonitoring == false else {
            print("FAIL: pressing G did not save exactly one bind"); return false
        }
        print("PASS: clicking a key chip and pressing G saved `bind g \"+frag\"` as one History row and stopped listening")


        // Check-up: Apply the top suggestion, then Undo it from its card.
        model.studio.checkup.rulesetIDs = ["zpam-4.08"]
        model.studio.tab = .checkup
        try? await Task.sleep(for: .milliseconds(900))
        guard let apply = audit.items.values.first(where: { $0.name == "checkup apply competitive-frame-cap" })?.frame else { print("FAIL: Apply not on screen"); return false }
        let beforeApply = (try? Data(contentsOf: file)) ?? Data()
        click(window, at: NSPoint(x: apply.midX, y: window.frame.height - apply.midY))
        try? await Task.sleep(for: .milliseconds(600))
        var capped = ConfigDocument(data: beforeApply, dialect: .cod2x)
        do { try capped.setSetting(named: "com_maxfps", value: "250") } catch { return false }
        guard (try? Data(contentsOf: file)) == capped.serialized(), session.history.first?.source == .checkup else {
            print("FAIL: Apply did not write the suggested cap"); return false
        }
        guard let undoButton = audit.items.values.first(where: { $0.name == "checkup undo competitive-frame-cap" })?.frame else { print("FAIL: Undo not on screen"); return false }
        click(window, at: NSPoint(x: undoButton.midX, y: window.frame.height - undoButton.midY))
        try? await Task.sleep(for: .milliseconds(600))
        guard (try? Data(contentsOf: file)) == beforeApply, session.history.first?.source == .restore else {
            print("FAIL: Undo did not restore the exact bytes"); return false
        }
        print("PASS: clicking Apply wrote `seta com_maxfps \"250\"` as a Check-up transaction and clicking Undo restored the exact bytes")

        // One window at a time from here, so the keyboard focus is unambiguous.
        window.orderOut(nil)
        do {
            // Visual: clicking a key selects it and hands it the keyboard focus (read back the way
            // VoiceOver reads focus). The arrow keys' move commands are not checked here: in this
            // window they never reach SwiftUI, although the same structure moves in a minimal one.
            guard let (visualModel, visual, visualHome) = await open("configs-binds-visual", dark: true) else { return false }
            defer { visual.orderOut(nil); visual.close(); try? FileManager.default.removeItem(at: visualHome) }
            guard let keyE = AccessibilityTree.elements(of: visual).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("E, ") }) else {
                print("FAIL: key E isn't in the accessibility tree"); return false
            }
            visual.makeKey()
            click(visual, element: keyE)
            try? await Task.sleep(for: .milliseconds(400))
            let focus = AccessibilityTree.attribute(AccessibilityTree.application, kAXFocusedUIElementAttribute).map { $0 as! AXUIElement }
            let focusLabel = focus.map { AccessibilityTree.text($0, kAXDescriptionAttribute) } ?? ""
            guard visualModel.studio.binds.selectedVirtualKey == 14, focusLabel.hasPrefix("E, ") else {
                print("FAIL: clicking E selected \(String(describing: visualModel.studio.binds.selectedVirtualKey)) with focus on “\(focusLabel)”"); return false
            }
            print("PASS: clicking E in Visual selected it and gave it the keyboard focus")
            guard await deviceSetupActions(model: visualModel, window: visual) else { return false }
        }
        guard await compareAction() else { return false }
        guard await importAction() else { return false }
        guard await readyAction() else { return false }
        // Last, so a window that keeps synthesized keys from SwiftUI can't hide the checks above.
        return await tabExitAction()
    }

    /// The drawing is one Tab stop: after a click on E, Tab leaves the keyboard instead of
    /// visiting each key. Read back the way VoiceOver reads focus.
    static func tabExitAction() async -> Bool {
        guard let (model, window, home) = await open("configs-binds-visual", dark: true) else { return false }
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
        let keys = AccessibilityTree.elements(of: window).filter { $0.role == "AXButton" && $0.label.contains(", ") && KeyLabels.drawn.contains(where: $0.label.hasPrefix) }
        guard let keyE = keys.first(where: { $0.label.hasPrefix("E, ") }) else { print("FAIL: key E isn't in the accessibility tree"); return false }
        window.makeKey()
        click(window, element: keyE)
        try? await Task.sleep(for: .milliseconds(400))
        guard model.studio.binds.selectedVirtualKey == 14 else { print("FAIL: clicking E did not select it"); return false }
        press(window, keyCode: 48, characters: "\t")
        try? await Task.sleep(for: .milliseconds(400))
        let focus = AccessibilityTree.attribute(AccessibilityTree.application, kAXFocusedUIElementAttribute).map { $0 as! AXUIElement }
        let label = focus.map { AccessibilityTree.text($0, kAXDescriptionAttribute) } ?? ""
        guard !label.isEmpty, !keys.contains(where: { $0.label == label }) else {
            print("FAIL: Tab from E left the focus on “\(label)”, still on the keyboard drawing"); return false
        }
        print("PASS: Tab from the keyboard drawing left it in one step (focus on “\(label)”)")
        return true
    }

    /// Key names the ANSI drawing speaks first in each key's label (“E, Not bound”).
    enum KeyLabels {
        static let drawn: [String] = "QWERTYUIOPASDFGHJKLZXCVBNM1234567890".map { "\($0), " }
            + ["Space, ", "Command, ", "Option, ", "Control, ", "Shift, ", "Right ", "tab, ", "caps, ", "return, ", "delete, ", "esc, ", "fn, ", "F"]
    }

    /// A click delivered through the application, where Binds' local monitor listens (a window's
    /// own sendEvent would bypass it). The mouse-up is queued first for a control's tracking loop.
    static func appClick(_ window: NSWindow, element: AccessibilityTree.Element) {
        let primary = NSScreen.screens.first?.frame.maxY ?? 0
        let point = window.convertPoint(fromScreen: NSPoint(x: element.frame.midX, y: primary - element.frame.midY))
        func event(_ type: NSEvent.EventType) -> NSEvent? {
            NSEvent.mouseEvent(with: type, location: point, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime,
                               windowNumber: window.windowNumber, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)
        }
        guard let down = event(.leftMouseDown), let up = event(.leftMouseUp) else { return }
        NSApp.postEvent(up, atStart: false)
        NSApp.sendEvent(down)
    }

    /// Press keys to verify with real key presses through the window's monitor, and a real click
    /// on I Don’t Have This Key (clicks pass through); Learn button positions with a real click
    /// that is consumed rather than pressing the control under it, and Return to finish.
    static func deviceSetupActions(model: LauncherModel, window: NSWindow) async -> Bool {
        let binds = model.studio.binds
        window.makeKey()
        binds.beginVerification()
        try? await Task.sleep(for: .milliseconds(300))
        guard binds.verification?.step == .besideShift else { print("FAIL: Press keys to verify did not start (\(model.studio.failure?.title ?? "no failure"))"); return false }
        press(window, keyCode: 50, characters: "`")
        press(window, keyCode: 58, characters: "")
        try? await Task.sleep(for: .milliseconds(400))
        for _ in 0..<2 {
            guard let skip = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "I Don’t Have This Key" }) else {
                print("FAIL: I Don’t Have This Key is not on screen (step \(String(describing: binds.verification?.step)))"); return false
            }
            appClick(window, element: skip)
            try? await Task.sleep(for: .milliseconds(400))
            if binds.verification?.step == .functionRow { press(window, keyCode: 122, characters: "") }
            try? await Task.sleep(for: .milliseconds(300))
        }
        guard binds.verification == nil, let keyboard = binds.keyboard,
              binds.devices.overrides[keyboard.id] == InputDeviceOverride(shape: .iso, size: .percent75, modifiers: .pc, verified: true) else {
            print("FAIL: verifying by pressing keys did not set ISO, 75%, PC labels (\(String(describing: binds.keyboard.flatMap { binds.devices.overrides[$0.id] })))"); return false
        }
        print("PASS: pressing the key beside Shift, Option, F1 and clicking I Don’t Have This Key twice verified ISO, 75%, PC labels")

        binds.beginLearning()
        try? await Task.sleep(for: .milliseconds(300))
        guard let find = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("Find by Pressing") }) else {
            print("FAIL: Find by Pressing is not on screen"); return false
        }
        appClick(window, element: find)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.learning?.step == .position(.right), binds.target == nil else {
            print("FAIL: a learning click was not recorded as the left button, or pressed the control under it"); return false
        }
        for _ in 0..<5 { press(window, keyCode: 36, characters: "\r"); try? await Task.sleep(for: .milliseconds(150)) }
        guard binds.learning == nil, binds.pointerButtonDetail(EngineKey(spelling: "mouse1")!) == "Left button",
              binds.pointerButtonDetail(EngineKey(spelling: "mouse2")!) == "Not on your mouse" else {
            print("FAIL: Return did not finish learning with only the left button (\(String(describing: binds.learning?.step)))"); return false
        }
        print("PASS: a click while learning was recorded as the left button without pressing Find by Pressing; Return finished")
        return true
    }

    /// From a player's page: a real click on Compare in Expert, then on Take Theirs for the
    /// frame-rate cap, writes exactly that one setting as one History row.
    static func compareAction() async -> Bool {
        guard let (model, window, home) = await open("discover-player", dark: true) else { return false }
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
        guard let session = model.studio.session else { return false }
        let file = home.appendingPathComponent("main/players/Main/config_mp.cfg")
        guard let compare = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "Compare in Expert" }) else {
            print("FAIL: Compare in Expert is not on screen"); return false
        }
        activate(window, element: compare)
        try? await Task.sleep(for: .milliseconds(900))
        guard model.studio.player?.name == "[Player 1]", model.studio.level == .expert, model.discover.page == nil else {
            print("FAIL: Compare in Expert did not open Expert with [Player 1]"); return false
        }
        model.studio.group = .frame
        try? await Task.sleep(for: .milliseconds(700))
        guard let before = try? Data(contentsOf: file),
              let take = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("Take [Player 1]’s Frame rate cap") }) else {
            print("FAIL: Take Theirs for Frame rate cap is not on screen"); return false
        }
        activate(window, element: take)
        try? await Task.sleep(for: .milliseconds(700))
        var expected = ConfigDocument(data: before, dialect: .cod2x)
        do { try expected.setSetting(named: "com_maxfps", value: "250") } catch { return false }
        guard (try? Data(contentsOf: file)) == expected.serialized(), session.history.first?.summary == "Took Frame rate cap from [Player 1]" else {
            print("FAIL: Take Theirs did not write exactly the player's frame-rate cap"); return false
        }
        print("PASS: clicking Compare in Expert and Take Theirs wrote `seta com_maxfps \"250\"` from [Player 1] as one History row")
        return true
    }

    /// A real click on the import review's Apply button (the sheet's bottom-trailing control) writes
    /// exactly the reviewed lines into a new loadout as one Import transaction, leaving Main alone.
    static func importAction() async -> Bool {
        let model = LauncherModel(snapshot: true)
        model.preview("configs-import")
        let home: URL
        do { home = try seed(model, screen: "configs-actions-import"); try await prepare(model, screen: "configs-import-everything", home: home) }
        catch { print("FAIL: import preview: \(error)"); return false }
        defer { try? FileManager.default.removeItem(at: home) }
        guard let review = model.discover.review, review.session != nil else { print("FAIL: the import review did not load"); return false }
        review.setMode(.review)
        guard let expected = try? review.session?.preview().document.serialized() else { print("FAIL: no reviewed bytes"); return false }
        let players = home.appendingPathComponent("main/players")
        let main = try? Data(contentsOf: players.appendingPathComponent("Main/config_mp.cfg")), active = try? Data(contentsOf: players.appendingPathComponent("active.txt"))
        let window = await Snapshots.show(model: model, dark: true, glass: true)
        defer { window.orderOut(nil); window.close() }
        guard let sheet = window.attachedSheet else { print("FAIL: the import review sheet is not attached"); return false }
        sheet.makeKey()
        click(sheet, at: NSPoint(x: sheet.frame.width - 22 - 28, y: 16 + 18))
        try? await Task.sleep(for: .milliseconds(800))
        let written = try? Data(contentsOf: players.appendingPathComponent("Player_1/config_mp.cfg"))
        guard written == expected, (try? ConfigProfileStore(homeURL: home, dialect: .cod2x).history(id: "Player_1"))?.map(\.source) == [.import],
              (try? Data(contentsOf: players.appendingPathComponent("Main/config_mp.cfg"))) == main,
              (try? Data(contentsOf: players.appendingPathComponent("active.txt"))) == active, model.discover.review == nil else {
            print("FAIL: clicking Apply did not import exactly the reviewed lines (\(review.applyError ?? "no error shown"))"); return false
        }
        print("PASS: clicking the review's Apply button imported exactly the ticked lines into a new loadout as one Import transaction; Main and the active loadout are unchanged")
        return true
    }

    /// Real clicks on Fix for This Server Only, its confirmation and Undo stage and clear a
    /// per-connection override without writing the loadout, its sidecar or the active selection.
    static func readyAction() async -> Bool {
        let model = LauncherModel(snapshot: true)
        model.preview("servers-ready")
        let home: URL
        do { home = try seed(model, screen: "servers-actions"); try await prepare(model, screen: "servers-ready", home: home) }
        catch { print("FAIL: ready-check preview: \(error)"); return false }
        defer { try? FileManager.default.removeItem(at: home) }
        let players = home.appendingPathComponent("main/players")
        func disk() -> [Data?] { ["Main/config_mp.cfg", "Main/studio.json", "active.txt"].map { try? Data(contentsOf: players.appendingPathComponent($0)) } }
        let before = disk()
        let audit = ShapeAudit()
        let window = await Snapshots.show(model: model, dark: true, glass: true, audit: audit)
        defer { window.orderOut(nil); window.close() }
        func cardButton() -> CGRect? {
            guard let deploy = audit.items.values.first(where: { $0.name == "prominent" && $0.frame.minX > 1000 })?.frame else { return nil }
            return audit.items.values.filter { $0.name == "glass button" && $0.form == .capsule && $0.frame.minX > 1000 && $0.frame.maxY < deploy.minY }
                .map(\.frame).min { $0.minY < $1.minY }
        }
        guard let fix = cardButton() else { print("FAIL: Fix for This Server Only is not on screen"); return false }
        click(window, at: NSPoint(x: fix.midX, y: window.frame.height - fix.midY))
        try? await Task.sleep(for: .milliseconds(800))
        guard let alert = window.attachedSheet, let content = alert.contentView,
              let confirm = views(NSButton.self, in: content).first(where: { $0.title == "Fix for This Server Only" }) else {
            print("FAIL: the fix confirmation did not appear"); return false
        }
        let frame = confirm.convert(confirm.bounds, to: nil)
        alert.makeKey()
        click(alert, at: NSPoint(x: frame.midX, y: frame.midY))
        try? await Task.sleep(for: .milliseconds(800))
        guard model.serverConnection.overrides == ["com_maxfps": "250"], disk() == before else {
            print("FAIL: the fix was not staged for this connection only (\(model.serverConnection.overrides))"); return false
        }
        print("PASS: clicking Fix for This Server Only and confirming staged com_maxfps 250 for this connection; Main's config, sidecar and the active selection are unchanged")
        // The confirmation closes asynchronously; the window takes clicks again once it has gone.
        for _ in 0..<40 where window.attachedSheet != nil { try? await Task.sleep(for: .milliseconds(50)) }
        try? await Task.sleep(for: .milliseconds(400))
        window.makeKey()
        guard let undo = cardButton() else { print("FAIL: Undo is not on screen"); return false }
        // After the confirmation window had key focus, the first click can only bring this window forward.
        for _ in 0..<2 where !model.serverConnection.overrides.isEmpty {
            click(window, at: NSPoint(x: undo.midX, y: window.frame.height - undo.midY))
            try? await Task.sleep(for: .milliseconds(600))
        }
        guard model.serverConnection.overrides.isEmpty, model.serverConnection.profileID == nil, disk() == before else {
            print("FAIL: Undo did not clear the per-connection fix (clicked \(undo), sheet \(window.attachedSheet.map { "\($0)" } ?? "none"))"); return false
        }
        print("PASS: clicking Undo cleared the per-connection fix; nothing on disk changed")
        return true
    }
    /// The window for one review screen, seeded like the snapshot run; nil if seeding failed.
    static func open(_ screen: String, dark: Bool, setup: (LauncherModel) -> Void = { _ in }) async -> (LauncherModel, NSWindow, URL)? {
        let model = LauncherModel(snapshot: true)
        model.preview(screen)
        guard let home = try? seed(model, screen: screen) else { print("FAIL: preview home for \(screen)"); return nil }
        // Discover, import and ready-check screens load their fixtures the way the snapshot run does.
        do { try await prepare(model, screen: screen, home: home) } catch { print("FAIL: preview for \(screen): \(error)"); return nil }
        setup(model)
        trace("open \(screen): seeded")
        let window = await Snapshots.show(model: model, dark: dark, glass: true)
        trace("open \(screen): shown")
        AccessibilityTree.wake()
        trace("open \(screen): woke")
        try? await Task.sleep(for: .milliseconds(500))
        trace("open \(screen): ready")
        return (model, window, home)
    }
    static func trace(_ text: String) { if ProcessInfo.processInfo.environment["COD2_SNAPSHOT_TRACE"] == "1" { fputs(text + "\n", stderr) } }
    /// Clicks the centre of an accessibility element (screen coordinates) in the window.
    /// A real click when the control is on screen; otherwise VoiceOver's press, which
    /// works for controls scrolled out of view.
    static func activate(_ window: NSWindow, element: AccessibilityTree.Element) {
        let primary = NSScreen.screens.first?.frame.maxY ?? 0
        let visible = CGRect(x: window.frame.minX, y: primary - window.frame.maxY, width: window.frame.width, height: window.frame.height)
        if visible.contains(CGPoint(x: element.frame.midX, y: element.frame.midY)) { click(window, element: element) }
        else { _ = AXUIElementPerformAction(element.element, kAXPressAction as CFString) }
    }
    static func click(_ window: NSWindow, element: AccessibilityTree.Element) {
        let primary = NSScreen.screens.first?.frame.maxY ?? 0
        click(window, at: window.convertPoint(fromScreen: NSPoint(x: element.frame.midX, y: primary - element.frame.midY)))
    }

    /// Labels each screen must expose, so an empty or unbuilt tree can't pass.
    static let expectedLabels: [String: [String]] = [
        "configs-guided": ["Jump to Any Setting", "Frame rate cap"],
        "configs-binds-list": ["Primary key for Throw frag grenade", "Find an action or key", "Advanced binds"],
        "configs-binds-advanced": ["Key for this bind", "Save Bind", "Step 1"],
        "configs-binds-visual": ["W, Move forward", "Find by Pressing", "Selected key"],
        "configs-checkup": ["Movement: 333 FPS", "Apply", "Show Me", "Not Now", "Rules to check against"],
        "configs-history": ["Restore This Snapshot"],
        "configs-binds-verify": ["Verify your keyboard", "Press the key just to the left of the space bar", "Cancel"],
        "configs-binds-learn": ["Learn your mouse buttons", "Press down on the scroll wheel", "I Don’t Have This Button"],
        "configs-expert-player": ["Compare markers show [Player 1]’s values", "Take [Player 1]’s Mouse speed"],
        "discover-player": ["Compare in Expert", "Try This Setup"],
    ]

    /// Every Config Studio screen's accessibility tree in reading order, with unnamed controls as failures.
    static func accessibility(output: URL, only: String?) async -> Bool {
        let folder = output.appendingPathComponent("accessibility")
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        var ok = true
        for screen in Snapshots.configScreens where only == nil || only == screen {
            guard let (_, window, home) = await open(screen, dark: true) else { return false }
            let elements = AccessibilityTree.elements(of: window)
            trace("\(screen): \(elements.count) elements read")
            window.orderOut(nil); window.close()
            try? FileManager.default.removeItem(at: home)
            try? (elements.map(\.line).joined(separator: "\n") + "\n").write(to: folder.appendingPathComponent(screen + ".txt"), atomically: true, encoding: .utf8)
            let controls = elements.filter { AccessibilityTree.controls.contains($0.role) }
            let unnamed = AccessibilityTree.unnamed(elements)
            // Static text reads its words as its value.
            let missing = (expectedLabels[screen] ?? []).filter { expected in !elements.contains { $0.label.hasPrefix(expected) || $0.value.hasPrefix(expected) } }
            let roleless = AccessibilityTree.roleless(elements)
            for element in unnamed { print("A11Y: \(screen): \(element.role) at \(NSStringFromRect(element.frame)) has no name") }
            for element in roleless { print("A11Y: \(screen): “\(element.label)” has no role") }
            for label in missing { print("A11Y: \(screen): no element named “\(label)…”") }
            print("A11Y \(screen): \(elements.count) elements, \(controls.count) controls, \(unnamed.count) without a name, \(roleless.count) without a role")
            ok = ok && unnamed.isEmpty && roleless.isEmpty && missing.isEmpty && controls.count > 5
        }
        print(ok ? "PASS: every Config Studio control has a name and every named element a role VoiceOver can read" : "FAIL: accessibility tree problems above")
        return ok
    }

    struct Overlay {
        let name: String, screen: String
        var prepare: @MainActor (LauncherModel) -> Void = { _ in }
        let target: @MainActor (AccessibilityTree.Element) -> Bool
        var after: @MainActor (NSWindow) async -> Void = { _ in }
    }
    static let overlayCases: [Overlay] = [
        Overlay(name: "configs-jump", screen: "configs-guided", target: { $0.role == "AXButton" && $0.label == "Jump to Any Setting" }, after: { window in
            // Type “fps” the way VoiceOver would set a field's value.
            if let field = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "Setting name or console name" }) {
                _ = AXUIElementSetAttributeValue(field.element, kAXValueAttribute as CFString, "fps" as CFString)
                try? await Task.sleep(for: .milliseconds(500))
            }
        }),
        Overlay(name: "configs-new-loadout", screen: "configs-guided", target: { $0.role == "AXButton" && $0.label == "New Loadout" }),
        Overlay(name: "configs-expert-note", screen: "configs-expert", target: { $0.role == "AXButton" && $0.label == "Edit the note on Mouse speed" }),
        Overlay(name: "configs-binds-keyboard", screen: "configs-binds-visual", target: { $0.role == "AXButton" && $0.label.hasPrefix("Magic Keyboard") }),
        Overlay(name: "configs-binds-pointer", screen: "configs-binds-visual", target: { $0.role == "AXButton" && $0.label.hasPrefix("Mouse ·") }),
        Overlay(name: "configs-binds-confirm", screen: "configs-binds-advanced",
                prepare: { model in model.studio.binds.updateDraft { $0.steps[0].text = "good luck" } },
                target: { $0.role == "AXButton" && $0.label == "Save Bind" }),
    ]

    /// Sheets, popovers and alerts opened by real clicks, captured over their window.
    static func overlays(output: URL, only: String?) async -> Bool {
        var ok = true, written = 0
        for overlay in overlayCases where only == nil || only == overlay.name {
            for dark in [true, false] {
                guard let (_, window, home) = await open(overlay.screen, dark: dark, setup: overlay.prepare) else { return false }
                defer { try? FileManager.default.removeItem(at: home) }
                let name = "\(overlay.name)-\(dark ? "dark" : "light")"
                guard let target = AccessibilityTree.elements(of: window).first(where: overlay.target) else {
                    print("FAIL: \(name): the control that opens it isn't in the accessibility tree"); ok = false
                    window.orderOut(nil); window.close(); continue
                }
                activate(window, element: target)
                try? await Task.sleep(for: .milliseconds(900))
                await overlay.after(window)
                // Sheets, alerts and popovers are windows of their own; popovers aren't in orderedWindows.
                let others = NSApp.windows.filter { $0 !== window && $0.isVisible && $0.frame.intersects(window.frame) }
                    .sorted { ($0.attachedSheet == nil ? 0 : 1, $0.level.rawValue) > ($1.attachedSheet == nil ? 0 : 1, $1.level.rawValue) }
                if others.isEmpty {
                    print("FAIL: \(name): clicking “\(target.label)” opened nothing"); ok = false
                } else if let image = WindowCapture.image(of: others + [window], in: window) {
                    try? Snapshots.write(image, to: output.appendingPathComponent("overlay-" + name + ".png")); written += 1
                    print("OVERLAY \(name): \(others.map { String(describing: type(of: $0)) }.joined(separator: ", ")) over the window")
                } else {
                    print("FAIL: \(name): capture failed"); ok = false
                }
                if let sheet = window.attachedSheet { window.endSheet(sheet) }
                for other in others { other.orderOut(nil) }
                window.orderOut(nil); window.close()
                try? await Task.sleep(for: .milliseconds(200))
            }
        }
        print(ok ? "PASS: \(written) sheet, popover and alert captures opened by real clicks" : "FAIL: overlay problems above")
        return ok && written > 0
    }

    /// A key press delivered through the application, where Binds' local monitor listens.
    static func press(_ window: NSWindow, keyCode: UInt16, characters: String) {
        for type in [NSEvent.EventType.keyDown, .keyUp] {
            if let event = NSEvent.keyEvent(with: type, location: .zero, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime,
                                            windowNumber: window.windowNumber, context: nil, characters: characters,
                                            charactersIgnoringModifiers: characters, isARepeat: false, keyCode: keyCode) {
                NSApp.sendEvent(event)
            }
        }
    }
}
