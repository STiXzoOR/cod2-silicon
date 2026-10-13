import AppKit
import ApplicationServices
import SwiftUI

// Review harness, never shipped. It hosts the real launcher views in a titled window
// configured like the app's (hidden title bar, unified toolbar) and captures it through
// the window server, so Liquid Glass, materials and toolbars render as they do on screen.
//
//   LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN] [--group configs]
//                            [--increase-contrast] [--reduce-transparency] [--reduce-motion] [--actions] [--minimum]
//                            [--accessibility] [--overlays]
//
// --data uses the player's own loading screens (private review only; never commit them).
// --fallback forces the macOS 13–25 material styling. --scale2 adds 2× renders of the same
// window for checking text, vectors and shapes (material styling; system glass draws flat there).
// --group configs renders only the Config Studio screens; --increase-contrast and
// --reduce-transparency set those accessibility environments for the capture, as --reduce-motion
// does for Reduce Motion (in the view environment only; the Mac's settings never change). --minimum
// uses the launcher's 1100×760 minimum window.
// --actions clicks Config Studio controls in the window (real hit-testing and actions) and checks
// the bytes and History rows written to a temporary home.
// --accessibility reads each Config Studio window's accessibility tree, as VoiceOver would, writes
// it in reading order to OUTPUT/accessibility, and fails on a control without a name.
// --overlays opens sheets, popovers and alerts with real clicks and captures them over the window.
// --navigation captures Expert's group rail in the active and an inactive window, with the window's
// sidebar hidden and shown by its own toolbar button, and resized between 1440×900 and 1100×760.
// Config Studio screens use synthetic loadouts in a private temporary home, removed afterwards.
// COD2_SNAPSHOT_INTERACTIVE_EXPORT=1 waits up to 120 seconds for a real Save click in the
// synthetic export panel. Use this when AppKit hosts the Save button in a remote service that
// does not accept in-process key events. Exact destination, delegate validation, exported bytes
// and Cancel assertions still run; the test fails if the operator does not complete Save.
// COD2_SNAPSHOT_GEOMETRY=1 logs AppKit, view and WindowServer geometry at each sizing step;
// COD2_SNAPSHOT_TRACE=1 logs each step of the window-driving modes to stderr.

/// An ordinary native window with optional geometry logging. The caller must run unlocked;
/// focus and appearance are read from AppKit, never forced by test overrides.
final class SnapshotWindow: NSWindow {
    override func constrainFrameRect(_ frameRect: NSRect, to screen: NSScreen?) -> NSRect {
        let result = super.constrainFrameRect(frameRect, to: screen)
        if ProcessInfo.processInfo.environment["COD2_SNAPSHOT_GEOMETRY"] == "1" {
            print("CONSTRAIN requested=\(frameRect) result=\(result) screen=\(String(describing: screen?.visibleFrame))")
        }
        return result
    }

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

/// The window's accessibility tree as VoiceOver reads it: the actual accessibility objects AppKit
/// and SwiftUI build for the window, its sheets and popovers, read in this process through the
/// public NSAccessibility getters. Querying our own process through the AX C API returns only the
/// application element on this Mac, so it isn't used; nothing here needs a permission or changes
/// a setting. Getters are asked by role: a menu-backed node is never asked for its title or value
/// (a SwiftUI Menu's title getter never returns), and a getter that stops answering fails the run
/// with the node it was reading. Objects no getter can read, cycles and the depth limit are reported.
@MainActor enum AccessibilityTree {
    struct Element {
        let role: String, subrole: String, label: String, value: String, help: String, selected: Bool
        /// Screen rectangle, top-left origin (Quartz), converted once from AppKit's screen space.
        let frame: CGRect
        let depth: Int
        let node: NSObject
        /// Whether VoiceOver can set the value; nil when the object couldn't be asked.
        let valueSettable: Bool?
        var line: String {
            String(repeating: "  ", count: depth) + role + (label.isEmpty ? "" : " “\(label)”") + (value.isEmpty ? "" : " = \(value)")
                + (selected ? " [selected]" : "") + (help.isEmpty ? "" : " (\(help))")
        }
    }
    struct Tree {
        var elements: [Element] = []
        /// Objects that couldn't be read, cycles and depth-limited branches, with where they were.
        var problems: [String] = []
        /// Proxies for elements another process draws (a title-bar button's system menu view):
        /// only the AX server can follow them, so they are listed, not read.
        var remote: [String] = []
    }
    static let controls: Set<String> = ["AXButton", "AXCheckBox", "AXRadioButton", "AXSlider", "AXTextField", "AXTextArea", "AXPopUpButton",
                                        "AXMenuButton", "AXComboBox", "AXLink", "AXDisclosureTriangle", "AXIncrementor", "AXSearchField"]
    /// Roles whose title and value getters open or read a menu.
    static let menuBacked: Set<String> = ["AXMenuButton", "AXPopUpButton", "AXMenu", "AXMenuBar", "AXMenuBarItem", "AXMenuItem"]
    static let maximumDepth = 80

    // MARK: Reading one object

    /// A getter's answer, or a note that the object has no getter of that name.
    enum Answer<T> { case value(T?), unsupported }

    static func responds(_ node: NSObject, _ name: String) -> Bool { node.responds(to: NSSelectorFromString(name)) }
    /// The pre-10.10 attribute getter, for objects (such as cell proxies) that only implement it.
    static func legacy(_ node: NSObject, _ attribute: String) -> Answer<Any> {
        let selector = NSSelectorFromString("accessibilityAttributeValue:")
        guard node.responds(to: selector) else { return .unsupported }
        return .value(node.perform(selector, with: attribute)?.takeUnretainedValue())
    }
    static func ask<T>(_ node: NSObject, _ getter: String, _ attribute: String, _ what: String, _ modern: (AnyObject) -> T?) -> Answer<Any> {
        Watchdog.shared.begin("\(what): \(getter)")
        defer { Watchdog.shared.end() }
        if responds(node, getter) { return .value(modern(node)) }
        return legacy(node, attribute)
    }
    static func string(_ answer: Answer<Any>) -> String {
        guard case .value(let raw?) = answer else { return "" }
        if let text = raw as? String { return text }
        if let text = raw as? NSAttributedString { return text.string }
        if let number = raw as? NSNumber { return number.stringValue }
        return ""
    }
    static func role(_ node: NSObject) -> Answer<Any> {
        ask(node, "accessibilityRole", "AXRole", String(describing: type(of: node))) { $0.accessibilityRole?()?.rawValue }
    }
    static func children(_ node: NSObject, _ what: String) -> Answer<Any> {
        ask(node, "accessibilityChildren", "AXChildren", what) { $0.accessibilityChildren?() ?? nil }
    }
    /// Whether VoiceOver sees the object itself; an ignored object's children stand in its place.
    static func isElement(_ node: NSObject) -> Bool {
        if responds(node, "isAccessibilityElement") { return (node as AnyObject).isAccessibilityElement?() ?? true }
        let selector = NSSelectorFromString("accessibilityIsIgnored")
        guard node.responds(to: selector) else { return true }
        typealias Ignored = @convention(c) (AnyObject, Selector) -> Bool
        return !unsafeBitCast(node.method(for: selector), to: Ignored.self)(node, selector)
    }
    /// AXUIElementIsAttributeSettable's own question, asked of the object; nil if it can't be asked.
    static func valueSettable(_ node: NSObject, _ what: String) -> Bool? {
        Watchdog.shared.begin("\(what): value settable")
        defer { Watchdog.shared.end() }
        let selector = NSSelectorFromString("accessibilityIsAttributeSettable:")
        if node.responds(to: selector) {
            typealias Settable = @convention(c) (AnyObject, Selector, NSString) -> Bool
            return unsafeBitCast(node.method(for: selector), to: Settable.self)(node, selector, "AXValue" as NSString)
        }
        guard responds(node, "isAccessibilitySelectorAllowed:") else { return nil }
        return (node as AnyObject).isAccessibilitySelectorAllowed?(NSSelectorFromString("setAccessibilityValue:"))
    }
    /// AppKit screen coordinates (bottom-left origin) to Quartz (top-left), once, here.
    static func quartz(_ rect: NSRect) -> CGRect {
        let primary = NSScreen.screens.first?.frame.maxY ?? 0
        return CGRect(x: rect.minX, y: primary - rect.maxY, width: rect.width, height: rect.height)
    }

    // MARK: Walking

    /// The window, then its sheets, child windows (popovers) and any visible window whose sheet
    /// parent it is: the windows VoiceOver reads with it.
    static func related(_ window: NSWindow) -> [NSWindow] {
        var result: [NSWindow] = [], queue = [window]
        while let next = queue.first {
            queue.removeFirst()
            guard !result.contains(where: { $0 === next }) else { continue }
            result.append(next)
            queue += next.sheets + (next.childWindows ?? []) + NSApp.windows.filter { $0.isVisible && $0.sheetParent === next }
        }
        return result
    }

    static func read(_ window: NSWindow?) -> Tree {
        var tree = Tree()
        guard let window else { tree.problems.append("no window to read"); return tree }
        var seen = Set<ObjectIdentifier>()
        func walk(_ node: NSObject, depth: Int, ancestors: [ObjectIdentifier], path: String) {
            let id = ObjectIdentifier(node)
            if ancestors.contains(id) { tree.problems.append("cycle: \(type(of: node)) is its own ancestor at \(path)"); return }
            // Reached again from another window or parent: read once.
            guard seen.insert(id).inserted else { return }
            guard depth < maximumDepth else { tree.problems.append("depth limit \(maximumDepth) reached at \(path) (\(type(of: node)))"); return }
            let kind = String(describing: type(of: node))
            if kind == "NSAccessibilityRemoteUIElement" { tree.remote.append("\(kind) at \(path)"); return }
            guard case .value(let rawRole) = role(node) else { tree.problems.append("unhandled object \(kind) at \(path): it answers no accessibility getter"); return }
            let role = (rawRole as? String) ?? ""
            let what = "\(kind) \(role) at \(path)"
            let shown = isElement(node)
            var nextDepth = depth
            if shown {
                func text(_ getter: String, _ attribute: String, _ modern: (AnyObject) -> Any?) -> String { string(ask(node, getter, attribute, what, modern)) }
                let menu = menuBacked.contains(role)
                let label = [text("accessibilityLabel", "AXDescription") { $0.accessibilityLabel?() ?? nil },
                             menu ? "" : text("accessibilityTitle", "AXTitle") { $0.accessibilityTitle?() ?? nil },
                             text("accessibilityPlaceholderValue", "AXPlaceholderValue") { $0.accessibilityPlaceholderValue?() ?? nil }]
                    .first { !$0.isEmpty } ?? ""
                var value = menu ? "" : text("accessibilityValue", "AXValue") { $0.accessibilityValue?() ?? nil }
                if value.count > 80 { value = String(value.prefix(80)) + "…" }
                let selected = string(ask(node, "isAccessibilitySelected", "AXSelected", what) { $0.isAccessibilitySelected?() }) == "1"
                var frame = CGRect.zero
                Watchdog.shared.begin("\(what): frame")
                if responds(node, "accessibilityFrame") { frame = quartz((node as AnyObject).accessibilityFrame?() ?? .zero) }
                else if case .value(let position?) = legacy(node, "AXPosition"), case .value(let size?) = legacy(node, "AXSize"),
                        let point = (position as? NSValue)?.pointValue, let extent = (size as? NSValue)?.sizeValue {
                    frame = quartz(NSRect(origin: point, size: extent))
                } else { tree.problems.append("no frame for \(what)") }
                Watchdog.shared.end()
                let controlText = ["AXTextArea", "AXTextField"].contains(role)
                tree.elements.append(Element(role: role, subrole: text("accessibilitySubrole", "AXSubrole") { $0.accessibilitySubrole?()?.rawValue },
                                             label: label, value: value.replacingOccurrences(of: "\n", with: " "),
                                             help: text("accessibilityHelp", "AXHelp") { $0.accessibilityHelp?() ?? nil }, selected: selected,
                                             frame: frame, depth: depth, node: node, valueSettable: controlText ? valueSettable(node, what) : nil))
                nextDepth = depth + 1
            }
            switch children(node, what) {
            case .unsupported: tree.problems.append("no children getter on \(what)")
            case .value(let raw):
                guard let raw else { return }
                guard let list = raw as? [Any] else { tree.problems.append("children of \(what) aren't a list (\(type(of: raw)))"); return }
                for (index, child) in list.enumerated() {
                    guard let object = child as? NSObject else { tree.problems.append("unhandled child \(type(of: child)) of \(what)"); continue }
                    walk(object, depth: nextDepth, ancestors: ancestors + [id], path: path + "/\(index)")
                }
            }
        }
        for (index, root) in related(window).enumerated() { walk(root, depth: 0, ancestors: [], path: "window\(index)") }
        return tree
    }
    /// Asks for the whole tree once, so SwiftUI builds the nodes a later read finds.
    static func wake(_ window: NSWindow) {
        // An AX API request to our own process (no permission needed) is what tells AppKit an
        // assistive client is asking; SwiftUI builds its nodes then. The answer itself isn't used.
        var ignored: CFTypeRef?
        _ = AXUIElementCopyAttributeValue(AXUIElementCreateApplication(getpid()), kAXWindowsAttribute as CFString, &ignored)
        _ = read(window)
    }
    /// The elements for an action to find its control in; problems are logged for diagnosis.
    static func elements(of window: NSWindow?) -> [Element] {
        let tree = read(window)
        for problem in tree.problems { fputs("A11Y problem: \(problem)\n", stderr) }
        return tree.elements
    }

    /// The application's focused element, as VoiceOver asks the app for it, and its name.
    static func focusedLabel() -> String {
        guard let focus = NSApp.accessibilityApplicationFocusedUIElement() as? NSObject else { return "" }
        return string(ask(focus, "accessibilityLabel", "AXDescription", "focus \(type(of: focus))") { $0.accessibilityLabel?() ?? nil })
    }
    /// VoiceOver's press.
    @discardableResult static func press(_ element: Element) -> Bool {
        Watchdog.shared.begin("press \(element.role) “\(element.label)”")
        defer { Watchdog.shared.end() }
        return (element.node as AnyObject).accessibilityPerformPress?() ?? false
    }
    /// VoiceOver setting a field's value.
    static func setValue(_ element: Element, _ text: String) {
        (element.node as AnyObject).setAccessibilityValue?(text)
    }

    /// Named elements with no role, which VoiceOver reads without saying what they are.
    static func roleless(_ elements: [Element]) -> [Element] {
        elements.filter { ($0.role == "AXUnknown" || $0.role.isEmpty) && !$0.label.isEmpty }
    }
    /// Controls VoiceOver would announce without a name, with why.
    static func unnamed(_ elements: [Element]) -> [(Element, String)] {
        // The window's own buttons and scroll-bar parts are named by their subrole; selectable
        // read-only text is read by its value.
        let system: Set<String> = ["AXCloseButton", "AXMinimizeButton", "AXZoomButton", "AXFullScreenButton",
                                   "AXIncrementArrow", "AXDecrementArrow", "AXIncrementPage", "AXDecrementPage"]
        return elements.indices.compactMap { index in
            let element = elements[index]
            guard controls.contains(element.role), !system.contains(element.subrole), element.label.trimmingCharacters(in: .whitespaces).isEmpty else { return nil }
            if ["AXTextArea", "AXTextField"].contains(element.role) {
                // An unanswered question is a failure, never a read-only field.
                guard let settable = element.valueSettable else { return (element, "has no name, and whether its value can be set couldn't be read") }
                if !settable {
                    // Read-only text is read by its contents, its own value or the text inside it.
                    let inside = elements[(index + 1)...].prefix { $0.depth > element.depth }
                    if !element.value.isEmpty || inside.contains(where: { $0.role == "AXStaticText" && !$0.value.isEmpty }) { return nil }
                }
            }
            return (element, "has no name")
        }
    }
}

/// Fails the run, naming the getter, when an accessibility query on the main thread stops answering.
final class Watchdog: @unchecked Sendable {
    static let shared = Watchdog()
    static let limit: TimeInterval = 20
    private let lock = NSLock()
    private var current: (what: String, since: Date)?
    private var started = false
    func begin(_ what: String) {
        lock.lock(); defer { lock.unlock() }
        current = (what, Date())
        if !started {
            started = true
            Thread.detachNewThread { [self] in
                while true {
                    Thread.sleep(forTimeInterval: 1)
                    lock.lock(); let stuck = current.flatMap { Date().timeIntervalSince($0.since) > Self.limit ? $0.what : nil }; lock.unlock()
                    if let stuck { fputs("FAIL: accessibility query did not return within \(Int(Self.limit)) s: \(stuck)\n", stderr); exit(1) }
                }
            }
        }
    }
    func end() { lock.lock(); current = nil; lock.unlock() }
}

@main struct Snapshots {
    static let screens = ["home", "home-first", "servers", "servers-empty", "settings", "library-demos", "library-screenshots", "about",
                          "setup-data", "setup-data-missing", "setup-key", "setup-shaders", "setup-ready"] + configScreens
    static let configScreens = ["configs-guided", "configs-guided-aim", "configs-expert", "configs-text", "configs-history", "configs-running",
                                "discover-unavailable", "discover-players", "discover-saved", "discover-player", "configs-import",
                                "configs-import-everything", "servers-ready", "servers-ready-fixed",
                                "configs-binds-list", "configs-binds-advanced", "configs-binds-visual", "configs-checkup",
                                "configs-guided-rules", "configs-expert-rules",
                                "configs-binds-verify", "configs-binds-learn", "configs-expert-player",
                                "configs-checkup-measured", "configs-checkup-off", "configs-checkup-error",
                                "configs-import-link", "configs-import-windows", "configs-import-loading",
                                "discover-saved-removed", "configs-save-failure", "configs-import-windows-profiles", "configs-catalog-loading"]

    @MainActor static func main() {
        let arguments = CommandLine.arguments
        guard arguments.count >= 2 else { fputs("usage: LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]\n", stderr); exit(2) }
        let output = URL(fileURLWithPath: arguments[1])
        let data = arguments.firstIndex(of: "--data").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let fallback = arguments.contains("--fallback"), scale2 = arguments.contains("--scale2")
        let only = arguments.firstIndex(of: "--only").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let configsOnly = arguments.firstIndex(of: "--group").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil } == "configs"
        if arguments.contains("--minimum") { windowSize = NSSize(width: 1100, height: 760) }
        let accessibility = Accessibility(increaseContrast: arguments.contains("--increase-contrast"), reduceTransparency: arguments.contains("--reduce-transparency"),
                                          reduceMotion: arguments.contains("--reduce-motion"))
        let app = NSApplication.shared
        // Match the shipping app: accessory activation can leave a real window inactive.
        app.setActivationPolicy(.regular)
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
            Task { @MainActor in
                let passed = if let only { await ConfigsPreview.action(only) } else { await ConfigsPreview.actions() }
                exit(passed ? 0 : 1)
            }
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
        if arguments.contains("--navigation") {
            Task { @MainActor in exit(await ConfigsPreview.navigation(output: output) ? 0 : 1) }
            app.run()
        }
        Task { @MainActor in
            var written = 0, checked = 0
            var findings: [String] = [], report: [[String: Any]] = []
            for dark in [true, false] {
                for screen in screens where (only == nil || only == screen) && (!configsOnly || configScreens.contains(screen)) {
                    let gate = screen == "configs-catalog-loading" ? PreviewCatalogGate() : nil
                    defer { gate?.release() }
                    let model = LauncherModel(snapshot: true, catalog: { try gate?.load() ?? ConfigStudioModel.bundledCatalog() })
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

    struct Accessibility { var increaseContrast = false; var reduceTransparency = false; var reduceMotion = false }

    @MainActor static func root(_ model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil, accessibility: Accessibility = .init()) -> some View {
        LauncherRoot(model: model)
            .environment(\.shapeAudit, audit)
            .environment(\.usesGlass, glass && LiquidGlass.available)
            .environment(\.colorScheme, dark ? .dark : .light)
            // Test-only overrides of the system settings, without changing them for the Mac.
            .environment(\._colorSchemeContrast, accessibility.increaseContrast ? .increased : .standard)
            .environment(\._accessibilityReduceTransparency, accessibility.reduceTransparency)
            .environment(\._accessibilityReduceMotion, accessibility.reduceMotion)
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
        if #available(macOS 14.0, *) { NSApp.activate() } else { NSApp.activate(ignoringOtherApps: true) }
        window.makeKeyAndOrderFront(nil)
        geometry(window, "order-front")
        try? await Task.sleep(for: .milliseconds(1400))
        geometry(window, "first-settled")
        window.setFrame(NSRect(origin: NSPoint(x: 40, y: 40), size: windowSize), display: true)
        geometry(window, "second-set-frame")
        try? await Task.sleep(for: .milliseconds(400))
        geometry(window, "settled")
        let keyTarget = window.attachedSheet ?? window
        guard NSApp.isActive, NSApp.keyWindow === keyTarget, keyTarget.isKeyWindow else {
            fputs("FAIL: the real snapshot window or its attached sheet did not become active and key; no focus or material proof is possible.\n", stderr)
            exit(1)
        }
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
        let byID = Dictionary(uniqueKeysWithValues: items.map { ($0.id, $0) })
        func parent(of item: Item) -> Item? {
            item.parentID.flatMap { byID[$0] }
        }
        func visible(_ item: Item) -> Bool {
            guard CGRect(origin: .zero, size: Snapshots.windowSize).contains(item.frame) else { return false }
            // A scrolled-out child must not be measured against a different panel that happens
            // to occupy its layout coordinates. Only its actual containing surface counts.
            guard let container = parent(of: item) else { return true }
            return container.frame.insetBy(dx: -0.5, dy: -0.5).contains(item.frame)
        }
        for item in items where item.role == .control {
            if case .rounded = item.form { out.append("\(describe(item)) is a rounded rectangle; buttons are capsules or circles") }
            if visible(item), item.name == "prominent", let seen = pixels?.shape(of: item.frame), seen != "capsule" {
                out.append("\(describe(item)) draws as a \(seen), not a capsule")
            }
        }
        for item in items {
            guard visible(item), let container = parent(of: item) else { continue }
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
        let rowItems = items.filter { ($0.role == .control || $0.role == .field) && visible($0) }
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

/// Holds the existing injected catalog source, so the real asynchronous loading path
/// stays visible for a capture. Every caller releases it in cleanup; the deadline also
/// bounds a failed capture without adding any loading-state override to production.
final class PreviewCatalogGate: Sendable {
    private let semaphore = DispatchSemaphore(value: 0)
    func load() throws -> ConfigCatalog {
        guard semaphore.wait(timeout: .now() + 60) == .success else {
            throw NSError(domain: "LauncherSnapshot", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "The catalog capture did not release its loading fixture."])
        }
        return try ConfigStudioModel.bundledCatalog()
    }
    func release() { semaphore.signal() }
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
                              ("MOUSE1", "+attack"), ("MOUSE2", "toggleads"), ("MWHEELDOWN", "weapprev"), ("MWHEELUP", "weapnext")] {
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
        // The loading fixture supplies a bounded catalog source; onAppear starts the real load.
        if screen != "configs-catalog-loading" { model.studio.loadCatalogNow() }
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
        case "configs-guided-rules", "configs-expert-rules": model.studio.checkup.rulesetIDs = ["zpam-4.08"]
        case "configs-checkup", "configs-checkup-measured", "configs-checkup-off", "configs-checkup-error":
            // Check-up reads the isolated home's own summaries, as the app reads its home after a game.
            model.page = .configs; model.studio.tab = .checkup
            let checkup = model.studio.checkup
            checkup.rulesetIDs = ["zpam-4.08"]
            if screen == "configs-checkup-measured" { try writeMatches(home) }
            if screen == "configs-checkup-off" { checkup.matchSummariesEnabled = false }
            if screen == "configs-checkup-error" { checkup.readMatches = { _ in throw CocoaError(.fileReadNoPermission) } }
            checkup.refreshMatches(home: home)
        case "configs-save-failure":
            // A real failed write: the loadout's folder refuses it, then is writable again for cleanup.
            model.page = .configs; model.studio.level = .guided; model.studio.group = .aim
            let folder = home.appendingPathComponent("main/players/Main")
            try FileManager.default.setAttributes([.posixPermissions: 0o500], ofItemAtPath: folder.path)
            defer { try? FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: folder.path) }
            if let speed = model.studio.spec(named: "sensitivity") { model.studio.set(speed, to: "3.1") }
        case "configs-import-link", "configs-import-windows", "configs-import-loading", "discover-saved-removed", "configs-import-windows-profiles",
             "configs-catalog-loading": model.page = .configs
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

    /// Two synthetic matches (documentation-range server, made-up numbers): the latest is shown.
    static func writeMatches(_ home: URL) throws {
        let row = #"{"schemaVersion":1,"endedAt":"2026-10-06T18:40:00Z","endReason":"map_change","map":"mp_toujane","mode":"sd","durationSeconds":1260,"serverAddress":"203.0.113.24:28960","ruleHints":{"g_competitive":"1"},"frameCount":151200,"frameTimeP50MS":3,"frameTimeP99MS":5.2,"frameTimeMaxMS":41,"frameTimeHistogramLimitMS":250,"framesUnderCap":149688,"capSampleFrames":151200,"capFPS":250,"pingAverageMS":38,"pingMaximumMS":71,"pingSamples":24120,"packetsReceived":24120,"packetsLost":36,"packetLossPercent":0.14903129657228018,"snapshotsReceived":24120}"#
        let older = row.replacingOccurrences(of: "mp_toujane", with: "mp_carentan").replacingOccurrences(of: "2026-10-06T18:40:00Z", with: "2026-10-05T21:10:00Z")
        let stats = home.appendingPathComponent("main/stats")
        try FileManager.default.createDirectory(at: stats, withIntermediateDirectories: true)
        try PrivateFile.write(Data((older + "\n" + row + "\n").utf8), to: stats.appendingPathComponent("matches.jsonl"))
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
        case "discover-saved-removed":
            // A bookmark whose player the signed index no longer lists.
            try await fixture()
            discover.setSaved("player-2", true)
            let saved = home.appendingPathComponent("directory/index.json.saved.json")
            try PrivateFile.write(Data(#"{"ids":["player-2","player-retired"],"schemaVersion":1}"#.utf8), to: saved)
            await discover.reload()
            discover.page = .saved
        case "configs-import-link": discover.step = .link("https://example.com/configs/config_mp.cfg")
        case "configs-import-windows":
            // A Windows profile folder made here, never a mounted drive.
            let players = home.appendingPathComponent("windows-preview/players")
            let file = players.appendingPathComponent("Morrow/config_mp.cfg")
            try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
            try PrivateFile.write(config(["sensitivity": "2.2", "com_maxfps": "250", "cg_drawFPS": "Simple", "name": "Morrow"]).serialized(), to: file)
            discover.importWindows(WindowsProfileCandidate(profileName: "Morrow", configURL: file, playersURL: players, isActive: true, isBackup: false))
            await waitReview()
        case "configs-import-windows-profiles":
            // The Windows step's real picker over known synthetic candidates; no drive is scanned
            // and no profile is read unless one is reviewed.
            let players = home.appendingPathComponent("windows-preview/players")
            func candidate(_ name: String, active: Bool = false, backup: Bool = false) -> WindowsProfileCandidate {
                WindowsProfileCandidate(profileName: name, configURL: players.appendingPathComponent(name + "/config_mp.cfg"), playersURL: players,
                                        isActive: active, isBackup: backup)
            }
            let found = WindowsProfileDiscovery(profiles: [candidate("Morrow", active: true), candidate("Scrim"), candidate("Morrow", backup: true)],
                                                truncated: false, unreadableDirectories: 1)
            discover.findWindowsProfiles = { _ in (found, searched: ["Windows (preview)"]) }
            discover.step = .windows
        case "configs-import-loading":
            // Held at reading, so the progress state can be seen; nothing is read.
            discover.beginImport(displayName: "Pasted config", provenance: .init(source: "Pasted text", symbol: "doc.on.clipboard")) { _, _ in
                try await Task.sleep(for: .seconds(3600)); throw CancellationError()
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
            // VoiceOver reads focus); the arrow keys then move along the drawing.
            guard let (visualModel, visual, visualHome) = await open("configs-binds-visual", dark: true) else { return false }
            defer { visual.orderOut(nil); visual.close(); try? FileManager.default.removeItem(at: visualHome) }
            guard let keyE = AccessibilityTree.elements(of: visual).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("E, ") }) else {
                print("FAIL: key E isn't in the accessibility tree"); return false
            }
            visual.makeKey()
            click(visual, element: keyE)
            try? await Task.sleep(for: .milliseconds(400))
            let focusLabel = AccessibilityTree.focusedLabel()
            guard visualModel.studio.binds.selectedVirtualKey == 14, focusLabel.hasPrefix("E, ") else {
                print("FAIL: clicking E selected \(String(describing: visualModel.studio.binds.selectedVirtualKey)) with focus on “\(focusLabel)”"); return false
            }
            print("PASS: clicking E in Visual selected it and gave it the keyboard focus")
            // Arrow keys arrive as AppKit sends them (function-key characters), so move commands reach SwiftUI.
            // Independent ANSI fixture positions, not the production neighbor helper.
            var at: UInt16 = 14
            for (code, direction, next) in [(124, ConfigKeyboardDirection.right, 15), (125, .down, 3), (123, .left, 2), (126, .up, 14)] as [(UInt16, ConfigKeyboardDirection, UInt16)] {
                func scrolls() -> String { views(NSClipView.self, in: visual.contentView!).map { NSStringFromPoint($0.bounds.origin) }.joined(separator: " ") }
                trace("arrow \(direction): before responder \(visual.firstResponder.map { String(describing: type(of: $0)) } ?? "none") scroll \(scrolls())")
                pressArrow(visual, keyCode: code)
                try? await Task.sleep(for: .milliseconds(400))
                trace("arrow \(direction): after responder \(visual.firstResponder.map { String(describing: type(of: $0)) } ?? "none") scroll \(scrolls()) selected \(String(describing: visualModel.studio.binds.selectedVirtualKey))")
                let focused = AccessibilityTree.focusedLabel()
                let name = BindsLabel.drawnName(next)
                guard visualModel.studio.binds.selectedVirtualKey == next, focused.hasPrefix(name + ", ") else {
                    print("FAIL: arrow \(direction) from \(at) selected \(String(describing: visualModel.studio.binds.selectedVirtualKey)) with focus on “\(focused)”, not \(next)"); return false
                }
                at = next
            }
            print("PASS: the arrow keys moved selection and focus along the drawing (right, down, left, up) from E")
            guard await deviceSetupActions(model: visualModel, window: visual) else { return false }
        }
        guard await groupRailAction() else { return false }
        guard await compareAction() else { return false }
        guard await loadoutMenuActions() else { return false }
        guard await importAction() else { return false }
        guard await readyAction() else { return false }
        // Last, so a window that keeps synthesized keys from SwiftUI can't hide the checks above.
        return await tabExitAction()
    }

    /// One later check on its own (`--actions --only NAME`), for iterating on it; the full run
    /// above is what the matrix requires.
    static func action(_ name: String) async -> Bool {
        switch name {
        case "group-rail": return await groupRailAction()
        case "compare": return await compareAction()
        case "loadout-menu": return await loadoutMenuActions()
        case "import": return await importAction()
        case "ready": return await readyAction()
        case "tab-exit": return await tabExitAction()
        case "review-controls": return await reviewControlsAction()
        case "device-setup":
            guard let (model, window, home) = await open("configs-binds-visual", dark: true) else { return false }
            defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
            return await deviceSetupActions(model: model, window: window)
        default: print("FAIL: no action check named \(name)"); return false
        }
    }

    /// A Mouse speed keyboard step or typed value belongs to the loadout shown when it began: choosing
    /// Scrim (or a replacement session, Escape, or Main → Scrim → Main) first never writes it to Scrim or late to Main. Show Controls
    /// with a text draft asks first. Each case has a positive save, so disabled controls can't pass.
    static func reviewControlsAction() async -> Bool {
        let mainConfig = "main/players/Main/config_mp.cfg", mainHistory = "main/players/Main/history/"
        func files(_ home: URL) -> [String: Data] {
            var all: [String: Data] = [:]
            let root = home.standardizedFileURL.path + "/"
            guard let walk = FileManager.default.enumerator(at: home, includingPropertiesForKeys: [.isRegularFileKey]) else { return all }
            for case let url as URL in walk
            where (try? url.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                all[String(url.standardizedFileURL.path.dropFirst(root.count))] = try? Data(contentsOf: url)
            }
            return all
        }
        func changed(_ before: [String: Data], _ after: [String: Data]) -> [String] { Set(before.keys).union(after.keys).filter { before[$0] != after[$0] }.sorted() }
        func sensitivity(_ data: Data?) -> Double? {
            guard let data, let text = String(data: data, encoding: .isoLatin1),
                  let match = text.range(of: #"seta sensitivity "[^"]*""#, options: .regularExpression) else { return nil }
            return Double(text[match].dropFirst("seta sensitivity \"".count).dropLast())
        }
        /// One edit to Main: its config and one new History row, and Mouse speed now `value`.
        func oneMainEdit(_ what: String, _ before: [String: Data], _ after: [String: Data], value: Double) -> Bool {
            let paths = changed(before, after)
            let added = paths.filter { $0.hasPrefix(mainHistory) && before[$0] == nil }
            guard paths.allSatisfy({ $0.hasPrefix("main/players/Main/") || $0 == "main/players/.studio.lock" }), paths.contains(mainConfig), added.count == 1,
                  let saved = sensitivity(after[mainConfig]), abs(saved - value) < 0.0001 else {
                print("FAIL: \(what) did not save exactly one Main edit of Mouse speed to \(value) (changed \(paths), saved \(String(describing: sensitivity(after[mainConfig]))))"); return false
            }
            return true
        }
        func element(_ window: NSWindow, _ role: String, _ label: String) -> AccessibilityTree.Element? {
            AccessibilityTree.elements(of: window).first { $0.role == role && ($0.label == label || $0.label.hasPrefix(label + ",")) }
        }
        /// A real click on the Mouse speed knob, where the slider's own compare marker places it (it
        /// doesn't move the value). True when that gave the slider the keyboard focus; without system
        /// Full Keyboard Access a click doesn't, and the step comes from VoiceOver's increment instead.
        func focusSlider(_ window: NSWindow, model: LauncherModel) -> Bool? {
            guard let element = element(window, "AXSlider", "Mouse speed"), let shown = Double(element.value),
                  let range = model.studio.specs.first(where: { $0.name == "sensitivity" })?.slideRange else { print("FAIL: the Mouse speed slider or its range isn't available"); return nil }
            let primary = NSScreen.screens.first?.frame.maxY ?? 0
            let fraction: Double = (shown - range.lowerBound) / (range.upperBound - range.lowerBound)
            let x: CGFloat = element.frame.minX + 8 + (element.frame.width - 16) * CGFloat(fraction)
            window.makeKey()
            click(window, at: window.convertPoint(fromScreen: NSPoint(x: x, y: primary - element.frame.midY)))
            return AccessibilityTree.focusedLabel() == "Mouse speed"
        }
        /// One keyboard step: the Right arrow to the focused slider, or VoiceOver's increment on its node.
        /// Both reach the slider's own binding, never the model directly. Only the arrow reaches the pause
        /// before saving; the increment ends its own edit and saves at once, so it doesn't cover the pause.
        func step(_ window: NSWindow, focused: Bool) -> String {
            if focused { pressArrow(window, keyCode: 124); return "Right arrow" }
            guard let node = element(window, "AXSlider", "Mouse speed")?.node else { return "nothing" }
            _ = (node as AnyObject).accessibilityPerformIncrement?()
            return "VoiceOver increment"
        }
        func value(_ window: NSWindow, _ role: String) -> String { element(window, role, "Mouse speed")?.value ?? "" }

        // Keyboard step, then Scrim before the 500 ms pause ends; then a replacement session; then the positive save.
        // VoiceOver's increment ends its own edit and saves at once, so only a focused slider's arrow reaches the pause.
        for variant in ["Scrim", "replacement session", "positive"] {
            guard let (model, window, home) = await open("configs-expert", dark: true, setup: { $0.studio.group = .aim }) else { return false }
            defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
            guard let scrim = element(window, "AXButton", "Scrim") else { print("FAIL: \(variant): the Scrim loadout isn't on screen"); return false }
            guard let focused = focusSlider(window, model: model) else { return false }
            try? await Task.sleep(for: .milliseconds(700))
            let before = files(home), shown = Double(value(window, "AXSlider")) ?? .nan
            let start = ContinuousClock.now
            let route = step(window, focused: focused)
            let afterStep = files(home)
            guard let number = Double(value(window, "AXSlider")), abs(number - shown) > 0.001 else {
                print("FAIL: \(variant): a \(route) did not change the shown Mouse speed (\(shown))"); return false
            }
            if focused {
                guard afterStep == before else { print("FAIL: \(variant): a Right arrow saved before the pause (changed \(changed(before, afterStep)))"); return false }
            } else {
                guard oneMainEdit("a VoiceOver increment", before, afterStep, value: number) else { return false }
            }
            switch variant {
            case "Scrim":
                click(window, element: scrim)
            case "replacement session":
                do {
                    let session = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: { [] })
                    try session.refresh()
                    model.studio.attach(session)
                } catch { print("FAIL: replacement session: \(error)"); return false }
            default: break
            }
            let elapsed = ContinuousClock.now - start
            try? await Task.sleep(for: .milliseconds(700))
            let after = files(home)
            let saved = focused ? "" : "; the increment itself saved to Main at once (the pause needs keyboard focus, which a click gives only with Full Keyboard Access)"
            if variant == "positive" {
                guard focused ? oneMainEdit("a Right arrow on Mouse speed", before, after, value: number) : after == afterStep else {
                    print("FAIL: a \(route) on Mouse speed wrote again later (changed \(changed(afterStep, after)))"); return false
                }
                print("PASS: a \(route) on the Mouse speed slider saved \(ConfigValues.format(number)) to Main as one History row\(focused ? " after the pause" : saved)")
                continue
            }
            let expected = variant == "Scrim" ? "Scrim" : "Main", shownNow = Double(value(window, "AXSlider")) ?? .nan
            let expectedValue = variant == "Scrim" ? 2.1 : focused ? shown : number
            // Only the arrow's pending save has a deadline; the increment already saved, so its switch may take longer.
            guard !focused || elapsed < .milliseconds(500), after == afterStep, model.studio.current?.profile.id == expected, abs(shownNow - expectedValue) < 0.001 else {
                print("FAIL: \(variant): a \(route) was written late or carried over (after \(elapsed), changed \(changed(afterStep, after)), "
                      + "shown \(model.studio.current?.profile.id ?? "none") with Mouse speed \(shownNow))"); return false
            }
            print("PASS: a \(route) on Mouse speed, then the \(variant) \(elapsed) later: nothing more written to either loadout or History; \(expected) shows \(ConfigValues.format(shownNow))\(saved)")
        }

        // A typed Mouse speed belongs to the focus and loadout it began in. Without Return: Scrim, Escape,
        // a replacement session, and Main → Scrim → Main save nothing late; a later fresh edit still saves.
        // Then the positive saves: Return, and more typing in the same focus saved by leaving with Tab.
        for variant in ["Scrim", "Escape", "replacement session", "Main, Scrim, Main", "Return"] {
            guard let (model, window, home) = await open("configs-expert", dark: true, setup: { $0.studio.group = .aim }) else { return false }
            defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
            guard let scrim = element(window, "AXButton", "Scrim"), let main = element(window, "AXButton", "Main") else {
                print("FAIL: \(variant): the Main or Scrim loadout isn't on screen"); return false
            }
            /// Types `text` into Mouse speed as the field editor's own input; a fresh click first unless still editing.
            func enter(_ text: String, fresh: Bool) async -> Bool {
                if fresh {
                    guard let field = element(window, "AXTextField", "Mouse speed") else { print("FAIL: \(variant): the Mouse speed field isn't on screen"); return false }
                    window.makeKey()
                    click(window, element: field)
                    try? await Task.sleep(for: .milliseconds(300))
                }
                guard let editor = window.firstResponder as? NSTextView else { print("FAIL: \(variant): the Mouse speed field isn't editing"); return false }
                let before = files(home)
                editor.insertText(text, replacementRange: NSRange(location: 0, length: (editor.string as NSString).length))
                try? await Task.sleep(for: .milliseconds(200))
                guard value(window, "AXTextField") == text, files(home) == before else { print("FAIL: \(variant): typing \(text) didn't show as an unsaved draft"); return false }
                return true
            }
            /// After a canceled draft, a fresh click, typing and Tab still save once to Main.
            func freshEdit(_ what: String) async -> Bool {
                let before = files(home)
                guard await enter("5.3", fresh: true) else { return false }
                press(window, keyCode: 48, characters: "\t")
                try? await Task.sleep(for: .milliseconds(600))
                return oneMainEdit("a fresh 5.3 and Tab after \(what)", before, files(home), value: 5.3)
            }
            let before = files(home)
            guard await enter("4.7", fresh: true) else { return false }
            switch variant {
            case "Return":
                press(window, keyCode: 36, characters: "\r")
                try? await Task.sleep(for: .milliseconds(600))
                let returned = files(home)
                guard oneMainEdit("typing 4.7 and Return", before, returned, value: 4.7) else { return false }
                guard await enter("4.9", fresh: false) else { return false }
                press(window, keyCode: 48, characters: "\t")
                try? await Task.sleep(for: .milliseconds(600))
                guard oneMainEdit("typing 4.9 in the same focus and Tab", returned, files(home), value: 4.9) else { return false }
                print("PASS: typing 4.7 in Mouse speed and pressing Return saved it to Main as one History row; typing 4.9 in the same focus and leaving with Tab saved one more")
            case "Escape":
                press(window, keyCode: 53, characters: "\u{1b}")
                try? await Task.sleep(for: .milliseconds(700))
                let paths = changed(before, files(home)), shown = value(window, "AXTextField")
                guard paths.isEmpty, model.studio.current?.profile.id == "Main", shown == "2.6" else {
                    print("FAIL: Escape after typing 4.7 saved or kept it (changed \(paths), shown \(model.studio.current?.profile.id ?? "none") with \(shown))"); return false
                }
                guard await freshEdit("Escape") else { return false }
                print("PASS: typing 4.7 in Mouse speed, then Escape: nothing saved and Main shows 2.6; a fresh edit then saved once")
            case "replacement session":
                do {
                    let session = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: { [] })
                    try session.refresh()
                    model.studio.attach(session)
                } catch { print("FAIL: replacement session: \(error)"); return false }
                try? await Task.sleep(for: .milliseconds(700))
                let paths = changed(before, files(home)), shown = value(window, "AXTextField")
                guard paths.isEmpty, model.studio.current?.profile.id == "Main", shown == "2.6" else {
                    print("FAIL: a typed Mouse speed was saved across a replacement session (changed \(paths), shown \(model.studio.current?.profile.id ?? "none") with \(shown))"); return false
                }
                guard await freshEdit("a replacement session") else { return false }
                print("PASS: typing 4.7 in Mouse speed, then a replacement session for Main: nothing saved and Main shows 2.6; a fresh edit then saved once")
            case "Main, Scrim, Main":
                click(window, element: scrim)
                try? await Task.sleep(for: .milliseconds(150))
                guard model.studio.current?.profile.id == "Scrim" else { print("FAIL: Main, Scrim, Main: clicking Scrim didn't show Scrim"); return false }
                click(window, element: main)
                try? await Task.sleep(for: .milliseconds(700))
                let after = files(home), paths = changed(before, after), shown = value(window, "AXTextField")
                let mainOnly = paths.isEmpty || oneMainEdit("typing 4.7 before Main, Scrim, Main", before, after, value: 4.7)
                guard mainOnly, model.studio.current?.profile.id == "Main", shown == (paths.isEmpty ? "2.6" : "4.7") else {
                    print("FAIL: Main, Scrim, Main carried or saved a stale Mouse speed (changed \(paths), shown \(model.studio.current?.profile.id ?? "none") with \(shown))"); return false
                }
                guard await freshEdit("Main, Scrim, Main") else { return false }
                print("PASS: typing 4.7 in Mouse speed, then Scrim and back to Main: Scrim and its History unchanged; Main \(paths.isEmpty ? "unchanged" : "saved the typed value first") and shows \(shown); a fresh edit then saved once")
            default:
                click(window, element: scrim)
                try? await Task.sleep(for: .milliseconds(700))
                let after = files(home), paths = changed(before, after), shown = value(window, "AXTextField")
                let mainOnly = paths.isEmpty || oneMainEdit("typing 4.7 before choosing Scrim", before, after, value: 4.7)
                guard mainOnly, model.studio.current?.profile.id == "Scrim", shown == "2.1" else {
                    print("FAIL: a typed Mouse speed was carried to Scrim (changed \(paths), shown \(model.studio.current?.profile.id ?? "none") with \(shown))"); return false
                }
                print("PASS: typing 4.7 in Mouse speed, then clicking Scrim: Scrim and its History unchanged and showing 2.1; Main \(paths.isEmpty ? "unchanged" : "saved the typed value first")")
            }
        }

        // Show Controls with a text draft: Keep Editing, Apply Edits, and Discard Edits.
        guard let (model, window, home) = await open("configs-expert", dark: true) else { return false }
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
        let studio = model.studio
        func pressButton(_ label: String) async -> Bool {
            guard let button = element(window, "AXButton", label) else { print("FAIL: \(label) isn't on screen"); return false }
            window.makeKey()
            click(window, element: button)
            try? await Task.sleep(for: .milliseconds(700))
            return true
        }
        func draft(_ line: String) async -> String? {
            guard await pressButton("Edit as Text"), studio.textMode,
                  let editor = views(NSTextView.self, in: window.contentView!).first(where: { $0.accessibilityLabel() == "Loadout as text" }) else {
                print("FAIL: Edit as Text didn't show the loadout's text"); return nil
            }
            window.makeFirstResponder(editor)
            editor.setSelectedRange(NSRange(location: 0, length: 0))
            editor.insertText(line + "\n", replacementRange: NSRange(location: 0, length: 0))
            try? await Task.sleep(for: .milliseconds(300))
            guard studio.textEdited, studio.text.hasPrefix(line + "\n") else { print("FAIL: the text editor didn't take the draft line"); return nil }
            return studio.text
        }
        func confirm(_ choice: String) async -> Bool {
            guard await pressButton("Show Controls") else { return false }
            guard let sheet = window.attachedSheet, let content = sheet.contentView,
                  let button = views(NSButton.self, in: content).first(where: { $0.title == choice }),
                  ["Apply Edits", "Discard Edits", "Keep Editing"].allSatisfy({ title in views(NSButton.self, in: content).contains { $0.title == title } }) else {
                print("FAIL: Show Controls with a text draft didn't ask to Apply, Discard or Keep Editing"); return false
            }
            let frame = button.convert(button.bounds, to: nil)
            sheet.makeKey()
            click(sheet, at: NSPoint(x: frame.midX, y: frame.midY))
            for _ in 0..<40 where window.attachedSheet != nil { try? await Task.sleep(for: .milliseconds(50)) }
            try? await Task.sleep(for: .milliseconds(400))
            return window.attachedSheet == nil
        }
        guard let kept = await draft("// keep this draft") else { return false }
        let baseline = files(home)
        guard await confirm("Keep Editing"), studio.textMode, studio.textEdited, studio.text == kept, files(home) == baseline else {
            print("FAIL: Keep Editing didn't leave the exact draft open and unsaved"); return false
        }
        print("PASS: Show Controls with a text draft asked first; Keep Editing left the exact draft open and nothing saved")
        guard await confirm("Apply Edits"), !studio.textMode else { print("FAIL: Apply Edits didn't show the controls"); return false }
        let applied = files(home), paths = changed(baseline, applied)
        guard applied[mainConfig] == kept.data(using: .isoLatin1), paths.filter({ $0.hasPrefix(mainHistory) && baseline[$0] == nil }).count == 1,
              paths.allSatisfy({ $0.hasPrefix("main/players/Main/") || $0 == "main/players/.studio.lock" }) else {
            print("FAIL: Apply Edits didn't save the draft's exact bytes as one History row (changed \(paths))"); return false
        }
        print("PASS: Apply Edits saved the draft's exact bytes to Main as one History row and showed the controls")
        guard await draft("// discard this draft") != nil else { return false }
        guard await confirm("Discard Edits"), !studio.textMode, files(home) == applied else {
            print("FAIL: Discard Edits saved something or kept the text open"); return false
        }
        guard await pressButton("Edit as Text"), studio.textMode, !studio.textEdited, studio.text.data(using: .isoLatin1) == applied[mainConfig] else {
            print("FAIL: after Discard Edits, Edit as Text didn't show the saved text"); return false
        }
        print("PASS: Discard Edits saved nothing and showed the controls; Edit as Text then showed the saved bytes")
        return true
    }

    /// The drawing is one Tab stop: after a click on E, Tab leaves the keyboard instead of
    /// visiting each key. Read back the way VoiceOver reads focus.
    static func tabExitAction() async -> Bool {
        guard let (model, window, home) = await open("configs-binds-visual", dark: true) else { return false }
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
        func focusedPosition() -> (elements: [AccessibilityTree.Element], drawing: Range<Int>, focus: Int)? {
            // Read synchronously on MainActor: compare actual provider objects, never guessed
            // spoken-name/frame matches. A provider that cannot establish membership fails closed.
            let tree = AccessibilityTree.read(window)
            guard tree.problems.isEmpty,
                  let group = tree.elements.firstIndex(where: { $0.label.hasSuffix("layout. Each key says what it does.") }),
                  let focus = NSApp.accessibilityApplicationFocusedUIElement() as? NSObject else { return nil }
            let index: Int
            if let exact = tree.elements.firstIndex(where: { $0.node === focus }) { index = exact }
            else {
                let equal = tree.elements.indices.filter { tree.elements[$0].node.isEqual(focus) }
                guard equal.count == 1 else { return nil }
                index = equal[0]
            }
            let depth = tree.elements[group].depth
            let end = tree.elements.indices.dropFirst(group + 1).first { tree.elements[$0].depth <= depth } ?? tree.elements.endIndex
            return (tree.elements, group..<end, index)
        }
        let initial = AccessibilityTree.read(window)
        guard initial.problems.isEmpty,
              let group = initial.elements.firstIndex(where: { $0.label.hasSuffix("layout. Each key says what it does.") }) else {
            print("FAIL: keyboard drawing is not identifiable in the native accessibility tree"); return false
        }
        let descendants = initial.elements.dropFirst(group + 1).prefix { $0.depth > initial.elements[group].depth }
        guard let keyE = descendants.first(where: { $0.role == "AXButton" && $0.label.hasPrefix("E, ") }) else {
            print("FAIL: key E is not in the keyboard drawing's accessibility subtree"); return false
        }
        window.makeKey()
        click(window, element: keyE)
        try? await Task.sleep(for: .milliseconds(400))
        guard model.studio.binds.selectedVirtualKey == 14, let before = focusedPosition(),
              before.drawing.contains(before.focus), before.elements[before.focus].label.hasPrefix("E, ") else {
            print("FAIL: clicking E did not select it and give its native accessibility element focus"); return false
        }
        press(window, keyCode: 48, characters: "\t")
        try? await Task.sleep(for: .milliseconds(400))
        guard let after = focusedPosition() else {
            print("FAIL: Tab focus could not be identified in the current native accessibility tree"); return false
        }
        let focused = after.elements[after.focus]
        guard !after.drawing.contains(after.focus) else {
            print("FAIL: Tab did not leave the whole keyboard drawing (focus on “\(focused.label)”, \(focused.role))"); return false
        }
        // Native split-view pane proxies can own focus without being a named control.
        // Require a real, identified outside element and a reverse traversal back to E.
        press(window, keyCode: 48, characters: "\u{19}", modifiers: .shift)
        try? await Task.sleep(for: .milliseconds(400))
        guard let returned = focusedPosition(), returned.drawing.contains(returned.focus),
              returned.elements[returned.focus].label.hasPrefix("E, "), model.studio.binds.selectedVirtualKey == 14 else {
            print("FAIL: Shift-Tab did not return to the keyboard drawing's selected E key"); return false
        }
        print("PASS: Tab left the keyboard drawing in one step and Shift-Tab returned to its selected E key")
        return true
    }

    /// Queue a click through the application's normal event pump, where Binds' local monitor
    /// listens. Dequeuing mouse-down also establishes NSApp.currentEvent for SwiftUI controls;
    /// dispatching it inside a task leaves the previous event current and can lose the click.
    /// Callers yield afterward; the following mouse-up is available to a control's tracking loop.
    static func appClick(_ window: NSWindow, element: AccessibilityTree.Element) {
        let primary = NSScreen.screens.first?.frame.maxY ?? 0
        let point = window.convertPoint(fromScreen: NSPoint(x: element.frame.midX, y: primary - element.frame.midY))
        func event(_ type: NSEvent.EventType) -> NSEvent? {
            NSEvent.mouseEvent(with: type, location: point, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime,
                               windowNumber: window.windowNumber, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)
        }
        guard let down = event(.leftMouseDown), let up = event(.leftMouseUp) else { return }
        NSApp.postEvent(down, atStart: false)
        NSApp.postEvent(up, atStart: false)
    }

    /// Press keys to verify with real key presses through the window's monitor, and a real click
    /// on I Don’t Have This Key (clicks pass through); Learn button positions with a real click
    /// that is consumed rather than pressing the control under it, and Return to finish.
    static func deviceSetupActions(model: LauncherModel, window: NSWindow) async -> Bool {
        let binds = model.studio.binds
        window.makeKey()
        // Start through the visible device controls, as a person does. In particular, do not
        // insert the setup panel by mutating the store while a keyboard cap owns focus.
        guard let keyboardChip = AccessibilityTree.elements(of: window).first(where: {
            $0.role == "AXButton" && $0.label.hasPrefix("Magic Keyboard ·")
        }) else { print("FAIL: keyboard device control is not on screen"); return false }
        appClick(window, element: keyboardChip)
        try? await Task.sleep(for: .milliseconds(400))
        guard let verify = AccessibilityTree.elements(of: window).first(where: {
            $0.role == "AXButton" && $0.label == "Press Keys to Verify…"
        }) else {
            print("FAIL: keyboard popover did not offer Press Keys to Verify"); return false
        }
        guard let panel = AccessibilityTree.related(window).first(where: { candidate in
            candidate !== window && candidate.frame.contains(NSPoint(x: verify.frame.midX,
                y: (NSScreen.screens.first?.frame.maxY ?? 0) - verify.frame.midY))
        }) else { print("FAIL: keyboard verification control has no visible popover window"); return false }
        appClick(panel, element: verify)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.verification?.step == .besideShift else { print("FAIL: Press keys to verify did not start (\(model.studio.failure?.title ?? "no failure"))"); return false }
        press(window, keyCode: 50, characters: "`")
        press(window, keyCode: 58, characters: "")
        try? await Task.sleep(for: .milliseconds(400))
        for index in 0..<2 {
            let expectedBefore: KeyboardVerification.Step = index == 0 ? .keypad : .navigation
            guard binds.verification?.step == expectedBefore else {
                print("FAIL: unexpected keyboard step before Skip: \(String(describing: binds.verification?.step))"); return false
            }
            guard let skip = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "I Don’t Have This Key" }) else {
                print("FAIL: I Don’t Have This Key is not on screen (step \(String(describing: binds.verification?.step)))"); return false
            }
            appClick(window, element: skip)
            try? await Task.sleep(for: .milliseconds(400))
            let expectedAfter: KeyboardVerification.Step = index == 0 ? .functionRow : .size([.percent75])
            guard binds.verification?.step == expectedAfter else {
                print("FAIL: Skip did not advance keyboard verification: \(String(describing: binds.verification?.step))"); return false
            }
            if index == 0 { press(window, keyCode: 122, characters: "") }
            try? await Task.sleep(for: .milliseconds(300))
        }
        // The keys only narrow the size; nothing is verified until the person chooses it.
        guard binds.verification?.step == .size([.percent75]), let keyboard = binds.keyboard, binds.devices.overrides[keyboard.id]?.verified != true,
              let size = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "My keyboard is 75%" }) else {
            print("FAIL: the answers did not stop at choosing the size (\(String(describing: binds.verification?.step)))"); return false
        }
        appClick(window, element: size)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.verification == nil, binds.devices.overrides[keyboard.id] == InputDeviceOverride(shape: .iso, size: .percent75, modifiers: .pc, verified: true) else {
            print("FAIL: choosing 75% did not verify ISO, 75%, PC labels (\(String(describing: binds.devices.overrides[keyboard.id])))"); return false
        }
        print("PASS: pressing the key beside Shift, Option, F1, clicking I Don’t Have This Key twice and choosing 75% verified ISO, 75%, PC labels")

        // No keypad, F1 and Home fit Tenkeyless or 75%: both are offered, Cancel verifies neither.
        let earlier = binds.devices.overrides[keyboard.id]
        binds.beginVerification()
        try? await Task.sleep(for: .milliseconds(300))
        press(window, keyCode: 6, characters: "z"); press(window, keyCode: 58, characters: "")
        try? await Task.sleep(for: .milliseconds(300))
        binds.skipVerificationStep()
        press(window, keyCode: 122, characters: ""); press(window, keyCode: 115, characters: "")
        try? await Task.sleep(for: .milliseconds(400))
        let offered = AccessibilityTree.elements(of: window).filter { $0.role == "AXButton" && $0.label.hasPrefix("My keyboard is ") }.map(\.label)
        guard binds.verification?.step == .size([.tkl, .percent75]), offered == ["My keyboard is Tenkeyless", "My keyboard is 75%"],
              let cancel = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "Cancel" }) else {
            print("FAIL: F1 and Home did not offer Tenkeyless and 75% (\(offered))"); return false
        }
        appClick(window, element: cancel)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.verification == nil, binds.devices.overrides[keyboard.id] == earlier else {
            print("FAIL: Cancel at the size choice changed the keyboard (\(String(describing: binds.devices.overrides[keyboard.id])))"); return false
        }
        print("PASS: F1 and Home offered Tenkeyless or 75%; clicking Cancel verified neither and kept the earlier answer")

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
        // The panel's own buttons take real clicks; the click isn't an answer.
        guard let skip = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "I Don’t Have This Button" }) else {
            print("FAIL: I Don’t Have This Button is not on screen"); return false
        }
        appClick(window, element: skip)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.learning?.step == .position(.wheel), binds.learning?.learned[.right] == nil, binds.learning?.learned.count == 1 else {
            print("FAIL: clicking I Don’t Have This Button was recorded as an answer (\(String(describing: binds.learning?.step)))"); return false
        }
        guard let cancel = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "Cancel" }) else {
            print("FAIL: Cancel is not on screen while learning"); return false
        }
        let learnedBefore = binds.devices.learnedButtons
        appClick(window, element: cancel)
        try? await Task.sleep(for: .milliseconds(400))
        guard binds.learning == nil, binds.devices.learnedButtons == learnedBefore else {
            print("FAIL: clicking Cancel did not stop learning without saving"); return false
        }
        print("PASS: clicking I Don’t Have This Button skipped the right button and clicking Cancel stopped learning; neither was recorded")
        binds.beginLearning()
        try? await Task.sleep(for: .milliseconds(300))
        guard let findAgain = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("Find by Pressing") }) else { return false }
        appClick(window, element: findAgain)
        try? await Task.sleep(for: .milliseconds(400))
        for _ in 0..<5 { press(window, keyCode: 36, characters: "\r"); try? await Task.sleep(for: .milliseconds(150)) }
        guard binds.learning == nil, binds.pointerButtonDetail(EngineKey(spelling: "mouse1")!) == "Left button",
              binds.pointerButtonDetail(EngineKey(spelling: "mouse2")!) == "Not on your mouse" else {
            print("FAIL: Return did not finish learning with only the left button (\(String(describing: binds.learning?.step)))"); return false
        }
        print("PASS: a click while learning was recorded as the left button without pressing Find by Pressing; Return finished")
        return true
    }

    /// The group rail's row whose name starts the label, and whether the row reports itself selected.
    static func railRow(_ title: String, in window: NSWindow) -> (element: AccessibilityTree.Element, selected: Bool)? {
        let elements = AccessibilityTree.elements(of: window)
        guard let rail = elements.firstIndex(where: { $0.label == "Setting groups" }) else { return nil }
        let inside = elements[(rail + 1)...].prefix { $0.depth > elements[rail].depth }
        // Native static text carries its words as the value and leaves the label empty.
        func text(_ element: AccessibilityTree.Element) -> String {
            element.label.isEmpty && element.role == "AXStaticText" ? element.value : element.label
        }
        guard let index = inside.firstIndex(where: { text($0).hasPrefix(title + ", ") }) else { return nil }
        let element = elements[index]
        // The row, or the cell inside it, carries the native selected state: read the row's own ancestors.
        var ancestors: [AccessibilityTree.Element] = [], depth = element.depth
        for candidate in elements[(rail + 1)..<index].reversed() where candidate.depth < depth { ancestors.append(candidate); depth = candidate.depth }
        return (element, element.selected || ancestors.contains { $0.selected && ["AXRow", "AXCell"].contains($0.role) })
    }

    /// Navigation states of a real window: active and inactive, the sidebar hidden and shown with its
    /// own toolbar button, and the window resized to the minimum and back. The rail's selection and
    /// the chosen group survive each; images are written for review. No setting of the Mac changes.
    static func navigation(output: URL) async -> Bool {
        try? FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
        let large = NSSize(width: 1440, height: 900), small = NSSize(width: 1100, height: 760)
        var written = 0
        for dark in [true, false] {
            let appearance = dark ? "dark" : "light"
            guard let (model, window, home) = await open("configs-expert", dark: dark, setup: { $0.studio.group = .aim }) else { return false }
            defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
            let aim = model.studio.catalogTitle(.aim)
            func capture(_ step: String) -> Bool {
                guard let image = WindowCapture.image(of: window) else { print("FAIL: navigation \(step)-\(appearance): capture failed"); return false }
                do { try Snapshots.write(image, to: output.appendingPathComponent("navigation-\(step)-\(appearance).png")); written += 1 } catch { return false }
                return true
            }
            // The native sidebar and toolbar animate their Liquid Glass after a toggle or resize; a
            // capture taken sooner can show a half-collapsed sidebar or the old toolbar outline.
            let settle = Duration.seconds(2)
            func resize(_ size: NSSize) async {
                window.setFrame(NSRect(origin: window.frame.origin, size: size), display: true)
                try? await Task.sleep(for: settle)
            }
            func toggleButton() -> AccessibilityTree.Element? {
                AccessibilityTree.elements(of: window).first { $0.role == "AXButton" && $0.label.localizedCaseInsensitiveContains("sidebar") }
            }
            /// The window's own split view controller, found through public AppKit only: the content view
            /// controller's children, the delegate of a split view in the window, or the toggle's target.
            func splitController() -> NSSplitViewController? {
                func controllers(_ controller: NSViewController?) -> [NSViewController] {
                    guard let controller else { return [] }
                    return [controller] + controller.children.flatMap { controllers($0) }
                }
                let delegates = window.contentView.map { views(NSSplitView.self, in: $0).compactMap { $0.delegate as? NSViewController } } ?? []
                let target = ((toggleButton()?.node as? NSControl)?.target as? NSViewController).map { [$0] } ?? []
                return (controllers(window.contentViewController) + delegates + target).lazy.compactMap { $0 as? NSSplitViewController }
                    .first { $0.splitViewItems.contains { $0.behavior == .sidebar } && $0.view.window === window }
            }
            /// The native sidebar's state as AppKit reports it: hidden, shown, or nil (with a reason) when
            /// the item, its view and the toggle's label disagree or can't be read. Never sets anything.
            func sidebarHidden(_ step: String) -> Bool? {
                guard let split = splitController(), let item = split.splitViewItems.first(where: { $0.behavior == .sidebar }) else {
                    print("FAIL: navigation \(step)-\(appearance): the window's native sidebar split view item could not be found"); return nil
                }
                let view = item.viewController.view
                let visible = view.isHiddenOrHasHiddenAncestor ? NSRect.zero : view.visibleRect
                let viewShown = view.window === window && visible.width >= 1 && visible.height >= 1
                let label = toggleButton()?.label ?? ""
                let labelHidden: Bool? = label.localizedCaseInsensitiveContains("show sidebar") ? true
                    : label.localizedCaseInsensitiveContains("hide sidebar") ? false : nil
                print("navigation \(step)-\(appearance): native sidebar isCollapsed=\(item.isCollapsed) viewHidden=\(view.isHiddenOrHasHiddenAncestor) visibleRect=\(NSStringFromRect(visible)) toggle=“\(label)”")
                guard item.isCollapsed != viewShown, labelHidden.map({ $0 == item.isCollapsed }) ?? true else {
                    print("FAIL: navigation \(step)-\(appearance): the native sidebar state is inconsistent (isCollapsed=\(item.isCollapsed), view shown=\(viewShown), toggle=“\(label)”)"); return nil
                }
                return item.isCollapsed
            }
            func expectSidebar(_ step: String, hidden: Bool) -> Bool {
                guard let actual = sidebarHidden(step) else { return false }
                guard actual == hidden else {
                    print("FAIL: navigation \(step)-\(appearance): the native sidebar is \(actual ? "hidden" : "shown"), expected \(hidden ? "hidden" : "shown")"); return false
                }
                return true
            }
            /// Measured in screen points from the accessibility frames and AppKit's own views: the loadout
            /// column's content starts 32 points (20 of page margin, 12 of its own inset) past the sidebar's
            /// visible edge, or the window's edge when hidden, so the editor takes the released width; and
            /// the Configs title clears every titlebar control.
            func layout(_ step: String) -> Bool {
                let primary = NSScreen.screens.first?.frame.maxY ?? 0
                func screen(_ view: NSView) -> CGRect { AccessibilityTree.quartz(window.convertToScreen(view.convert(view.bounds, to: nil))) }
                let elements = AccessibilityTree.elements(of: window)
                let win = CGRect(x: window.frame.minX, y: primary - window.frame.maxY, width: window.frame.width, height: window.frame.height)
                guard let item = splitController()?.splitViewItems.first(where: { $0.behavior == .sidebar }),
                      let column = elements.first(where: { $0.label == "Loadouts" && $0.frame.width > 200 })?.frame,
                      let title = elements.first(where: { ($0.label == "Configs" || $0.value == "Configs") && column.contains(CGPoint(x: $0.frame.midX, y: $0.frame.midY)) })?.frame else {
                    print("FAIL: navigation \(step)-\(appearance): the sidebar item, loadout column or Configs title could not be measured"); return false
                }
                let sidebar = item.viewController.view
                let edge = item.isCollapsed || sidebar.isHiddenOrHasHiddenAncestor ? win.minX : screen(sidebar).intersection(win).maxX
                let controls = [NSWindow.ButtonType.closeButton, .miniaturizeButton, .zoomButton].compactMap { window.standardWindowButton($0) }
                    .filter { !$0.isHiddenOrHasHiddenAncestor }.map(screen) + (toggleButton().map { [$0.frame] } ?? [])
                let gap = column.minX - edge
                print("navigation \(step)-\(appearance): loadout column x=\(Int(column.minX - win.minX)) gap=\(Int(gap)) editor width=\(Int(win.maxX - column.minX)) title=\(NSStringFromRect(title.offsetBy(dx: -win.minX, dy: -win.minY)))")
                guard abs(gap - 32) <= 2, column.maxX <= win.maxX else {
                    print("FAIL: navigation \(step)-\(appearance): the loadout column starts \(Int(gap)) points past the sidebar's edge, not 32"); return false
                }
                guard controls.count >= 4, !controls.contains(where: { $0.intersects(title) }) else {
                    print("FAIL: navigation \(step)-\(appearance): the Configs title overlaps a titlebar control (\(controls.count) controls measured)"); return false
                }
                return true
            }
            /// At 1440 points the rail shows, with Aim selected; at the minimum a menu replaces it. With the
            /// sidebar hidden at the minimum, either is right, depending on the room the editor gets.
            func check(_ step: String, rail: Bool?) -> Bool {
                guard model.page == .configs, model.studio.group == .aim, model.studio.filter == nil else {
                    print("FAIL: navigation \(step)-\(appearance): the page or group changed (\(model.page), \(model.studio.group))"); return false
                }
                let row = railRow(aim, in: window)
                if rail == true || (rail == nil && row != nil) {
                    guard row?.selected == true else { print("FAIL: navigation \(step)-\(appearance): \(aim) is not the rail's selected row"); return false }
                } else {
                    guard AccessibilityTree.elements(of: window).contains(where: { $0.role == "AXPopUpButton" && $0.label == "Setting group" }) else {
                        print("FAIL: navigation \(step)-\(appearance): the Setting group menu does not replace the rail"); return false
                    }
                }
                return layout(step) && capture(step)
            }
            /// One real click on the sidebar's own toolbar button, through the normal event pump, then the
            /// settled native state must be the expected one. No retry: a lost click fails here.
            func toggleSidebar(_ step: String, hidden: Bool) async -> Bool {
                guard let toggle = toggleButton() else { print("FAIL: navigation \(step)-\(appearance): the sidebar's toolbar button is not in the accessibility tree"); return false }
                guard let before = sidebarHidden("\(step) (before click)") else { return false }
                guard before != hidden else {
                    print("FAIL: navigation \(step)-\(appearance): the native sidebar is already \(hidden ? "hidden" : "shown") before the click"); return false
                }
                window.makeKey()
                appClick(window, element: toggle)
                try? await Task.sleep(for: settle)
                return expectSidebar(step, hidden: hidden)
            }
            await resize(large)
            guard expectSidebar("active", hidden: false) else { return false }
            guard window.frame.size == large, check("active", rail: true) else { return false }
            // Another of our windows takes key: the rail's selection draws as in an inactive window.
            let other = NSWindow(contentRect: NSRect(x: window.frame.minX + 40, y: window.frame.minY + 40, width: 260, height: 120),
                                 styleMask: [.titled], backing: .buffered, defer: false)
            other.isReleasedWhenClosed = false
            other.makeKeyAndOrderFront(nil)
            try? await Task.sleep(for: .milliseconds(700))
            guard !window.isKeyWindow, other.isKeyWindow else { print("FAIL: navigation inactive-\(appearance): the window stayed key"); other.close(); return false }
            let inactive = expectSidebar("inactive", hidden: false) && check("inactive", rail: true)
            other.orderOut(nil); other.close()
            window.makeKeyAndOrderFront(nil)
            try? await Task.sleep(for: .milliseconds(600))
            guard inactive, window.isKeyWindow else { return false }
            guard await toggleSidebar("sidebar-hidden", hidden: true), check("sidebar-hidden", rail: true),
                  await toggleSidebar("sidebar-shown", hidden: false), check("sidebar-shown", rail: true) else { return false }
            await resize(small)
            guard window.frame.size == small, expectSidebar("minimum", hidden: false), check("minimum", rail: false) else { return false }
            guard await toggleSidebar("minimum-sidebar-hidden", hidden: true), check("minimum-sidebar-hidden", rail: nil),
                  await toggleSidebar("minimum-sidebar-shown", hidden: false), check("minimum-sidebar-shown", rail: false) else { return false }
            await resize(large)
            guard window.frame.size == large, expectSidebar("resized-back", hidden: false), check("resized-back", rail: true) else { return false }
        }
        print("PASS: navigation: \(written) captures; the rail kept \(ConfigPurposeGroup.aim) selected while inactive, with the sidebar hidden and shown, and across 1440x900 and 1100x760")
        return written == 16
    }

    /// Expert's group rail is a native selection list: a real click selects a group, the down arrow
    /// moves to the next group, a click on Changed selects that filter and a click on a group
    /// clears it. The selected row reports itself selected; nothing is written.
    static func groupRailAction() async -> Bool {
        guard let (model, window, home) = await open("configs-expert", dark: true) else { return false }
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home) }
        let studio = model.studio
        let file = home.appendingPathComponent("main/players/Main/config_mp.cfg")
        let before = try? Data(contentsOf: file)
        func row(_ title: String) -> (element: AccessibilityTree.Element, selected: Bool)? { railRow(title, in: window) }
        let aim = studio.catalogTitle(.aim)
        guard let aimRow = row(aim) else { print("FAIL: the \(aim) row is not in the Setting groups list"); return false }
        window.makeKey()
        appClick(window, element: aimRow.element)
        try? await Task.sleep(for: .milliseconds(500))
        guard studio.group == .aim, studio.filter == nil, row(aim)?.selected == true else {
            print("FAIL: clicking \(aim) did not select it (group \(studio.group), selected \(String(describing: row(aim)?.selected)))"); return false
        }
        print("PASS: clicking \(aim) in the group rail selected it, and its row reports itself selected")
        let groups = ConfigPurposeGroup.allCases
        let next = groups[groups.firstIndex(of: .aim)! + 1]
        pressArrow(window, keyCode: 125)
        try? await Task.sleep(for: .milliseconds(500))
        guard studio.group == next, row(studio.catalogTitle(next))?.selected == true else {
            print("FAIL: the down arrow did not move the rail to \(studio.catalogTitle(next)) (group \(studio.group), first responder \(window.firstResponder.map { String(describing: type(of: $0)) } ?? "none"))"); return false
        }
        print("PASS: the down arrow moved the rail's selection to \(studio.catalogTitle(next))")
        func changedToggle() -> AccessibilityTree.Element? {
            AccessibilityTree.elements(of: window).first { $0.role == "AXCheckBox" && $0.label == ConfigExpertFilter.changed.title }
        }
        guard let changed = changedToggle() else { print("FAIL: the Changed checkbox is not in the rail"); return false }
        appClick(window, element: changed)
        try? await Task.sleep(for: .milliseconds(500))
        guard studio.filter == .changed, changedToggle()?.value == "1", row(studio.catalogTitle(next))?.selected == false else {
            print("FAIL: clicking Changed did not check the filter alone (filter \(String(describing: studio.filter)))"); return false
        }
        guard let checked = changedToggle() else { return false }
        appClick(window, element: checked)
        try? await Task.sleep(for: .milliseconds(500))
        guard studio.filter == nil, changedToggle()?.value == "0" else {
            print("FAIL: clicking Changed again did not clear the filter"); return false
        }
        guard let unchecked = changedToggle() else { return false }
        appClick(window, element: unchecked)
        try? await Task.sleep(for: .milliseconds(500))
        guard let back = row(aim) else { return false }
        appClick(window, element: back.element)
        try? await Task.sleep(for: .milliseconds(500))
        guard studio.group == .aim, studio.filter == nil, studio.search.isEmpty, (try? Data(contentsOf: file)) == before else {
            print("FAIL: clicking \(aim) after Changed did not clear the filter, or the rail wrote to the loadout"); return false
        }
        print("PASS: Changed toggled on and off; clicking \(aim) cleared it; the loadout's bytes are unchanged")
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
    static func open(_ screen: String, dark: Bool, catalogGate: PreviewCatalogGate? = nil, setup: (LauncherModel) -> Void = { _ in }) async -> (LauncherModel, NSWindow, URL)? {
        let model = LauncherModel(snapshot: true, catalog: { try catalogGate?.load() ?? ConfigStudioModel.bundledCatalog() })
        model.preview(screen)
        guard let home = try? seed(model, screen: screen) else { print("FAIL: preview home for \(screen)"); return nil }
        // Discover, import and ready-check screens load their fixtures the way the snapshot run does.
        do { try await prepare(model, screen: screen, home: home) } catch { print("FAIL: preview for \(screen): \(error)"); return nil }
        setup(model)
        trace("open \(screen): seeded")
        let window = await Snapshots.show(model: model, dark: dark, glass: true)
        trace("open \(screen): shown")
        // SwiftUI builds its accessibility nodes once something asks for them.
        AccessibilityTree.wake(window)
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
        else { AccessibilityTree.press(element) }
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
        "configs-checkup-measured": ["Your last match: Toujane, Search & Destroy", "Movement: 5.2 MS", "Connection: 38 MS", "Refresh match summaries"],
        "configs-checkup-off": ["Match summaries are off", "Refresh match summaries"],
        "configs-checkup-error": ["Couldn’t read your match summaries", "Connection: no value, Not measured"],
        "configs-import-link": ["Config link", "Review"],
        "configs-import-windows": ["Review Morrow from Windows", "From Windows"],
        "configs-import-loading": ["Reading and checking every line"],
        "discover-saved-removed": ["Remove this saved setup"],
        "configs-save-failure": ["Couldn’t save: Changed Mouse speed"],
        "configs-import-windows-profiles": ["Bring in your Windows setup", "Morrow", "Scrim", "Choose Folder…"],
        "configs-catalog-loading": ["Loading settings"],
    ]

    /// Every Config Studio screen's accessibility tree in reading order, with unnamed controls as failures.
    static func accessibility(output: URL, only: String?) async -> Bool {
        let folder = output.appendingPathComponent("accessibility")
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        var ok = true
        for screen in Snapshots.configScreens where only == nil || only == screen {
            let gate = screen == "configs-catalog-loading" ? PreviewCatalogGate() : nil
            defer { gate?.release() }
            guard let (_, window, home) = await open(screen, dark: true, catalogGate: gate) else { return false }
            // Everything the checks use (settability included) is read while the window is open.
            let tree = AccessibilityTree.read(window)
            let elements = tree.elements
            trace("\(screen): \(elements.count) elements read")
            let controls = elements.filter { AccessibilityTree.controls.contains($0.role) }
            let unnamed = AccessibilityTree.unnamed(elements)
            // Static text reads its words as its value.
            let missing = (expectedLabels[screen] ?? []).filter { expected in !elements.contains { $0.label.hasPrefix(expected) || $0.value.hasPrefix(expected) } }
            let roleless = AccessibilityTree.roleless(elements)
            window.orderOut(nil); window.close()
            try? FileManager.default.removeItem(at: home)
            try? (elements.map(\.line).joined(separator: "\n") + "\n").write(to: folder.appendingPathComponent(screen + ".txt"), atomically: true, encoding: .utf8)
            for (element, why) in unnamed { print("A11Y: \(screen): \(element.role) (\(type(of: element.node))) at \(NSStringFromRect(element.frame)) \(why)") }
            for element in roleless { print("A11Y: \(screen): “\(element.label)” has no role") }
            for label in missing { print("A11Y: \(screen): no element named “\(label)…”") }
            for problem in tree.problems { print("A11Y: \(screen): \(problem)") }
            for proxy in tree.remote { print("A11Y: \(screen): not read, drawn by another process: \(proxy)") }
            print("A11Y \(screen): \(elements.count) elements, \(controls.count) controls, \(unnamed.count) without a name, \(roleless.count) without a role, \(tree.problems.count) unreadable")
            ok = ok && unnamed.isEmpty && roleless.isEmpty && missing.isEmpty && tree.problems.isEmpty && controls.count > 5
        }
        print(ok ? "PASS: every Config Studio control has a name and every named element a role VoiceOver can read" : "FAIL: accessibility tree problems above")
        return ok
    }

    struct Overlay {
        let name: String, screen: String
        var prepare: @MainActor (LauncherModel) -> Void = { _ in }
        let target: @MainActor (AccessibilityTree.Element) -> Bool
        var after: @MainActor (NSWindow) async -> Void = { _ in }
        /// VoiceOver's press, for a control inside a sheet (a click would land on the window below).
        var press = false
        /// The click opens an alert on the window's sheet; the sheet alone isn't the overlay.
        var nested = false
    }
    static let overlayCases: [Overlay] = [
        Overlay(name: "configs-jump", screen: "configs-guided", target: { $0.role == "AXButton" && $0.label == "Jump to Any Setting" }, after: { window in
            // Type “fps” the way VoiceOver would set a field's value.
            if let field = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "Setting name or console name" }) {
                AccessibilityTree.setValue(field, "fps")
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
        // Import Everything asks once more before writing; the button is in the review sheet.
        Overlay(name: "configs-import-confirm", screen: "configs-import-everything",
                target: { $0.role == "AXButton" && $0.label.hasPrefix("Import All") }, press: true, nested: true),
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
                if overlay.press { AccessibilityTree.press(target) } else { activate(window, element: target) }
                try? await Task.sleep(for: .milliseconds(900))
                await overlay.after(window)
                // Sheets, alerts and popovers are windows of their own; popovers aren't in orderedWindows.
                // The capture takes them front to back, so a window nested deeper (an alert on a sheet)
                // comes before the one it hangs from, then by level.
                func depth(_ other: NSWindow) -> Int {
                    var depth = 0, node = other
                    while depth < 8, let next = node.sheetParent ?? node.parent { depth += 1; node = next }
                    return depth
                }
                let others = NSApp.windows.filter { $0 !== window && $0.isVisible && $0.frame.intersects(window.frame) }
                    .sorted { (depth($0), $0.level.rawValue) > (depth($1), $1.level.rawValue) }
                // The alert must be the real one on the sheet, in front of it: the sheet alone doesn't count.
                var alertInFront = false
                if let sheet = window.attachedSheet, let alert = sheet.attachedSheet as? NSPanel,
                   let alertIndex = others.firstIndex(where: { $0 === alert }), let sheetIndex = others.firstIndex(where: { $0 === sheet }) {
                    alertInFront = alertIndex < sheetIndex
                }
                if others.isEmpty {
                    print("FAIL: \(name): clicking “\(target.label)” opened nothing"); ok = false
                } else if overlay.nested, !alertInFront {
                    print("FAIL: \(name): clicking “\(target.label)” opened no alert in front of its sheet (\(others.map { String(describing: type(of: $0)) }.joined(separator: ", ")))"); ok = false
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

    /// An arrow key as the keyboard sends it: a function-key character on the numeric pad, which
    /// AppKit turns into a move command.
    static func pressArrow(_ window: NSWindow, keyCode: UInt16) {
        guard NSApp.isActive, NSApp.keyWindow === window, window.isKeyWindow, window.firstResponder != nil else {
            fputs("FAIL: key event target is not the actual active key window with a first responder.\n", stderr); exit(1)
        }
        let scalar: UInt32 = [123: 0xF702, 124: 0xF703, 125: 0xF701, 126: 0xF700][keyCode] ?? 0xF703
        let characters = String(Character(UnicodeScalar(scalar)!))
        for type in [NSEvent.EventType.keyDown, .keyUp] {
            if let event = NSEvent.keyEvent(with: type, location: .zero, modifierFlags: [.numericPad, .function], timestamp: ProcessInfo.processInfo.systemUptime,
                                            windowNumber: window.windowNumber, context: nil, characters: characters,
                                            charactersIgnoringModifiers: characters, isARepeat: false, keyCode: keyCode) {
                NSApp.sendEvent(event)
            }
        }
    }
    enum BindsLabel {
        /// The name the ABC drawing speaks first for a letter or digit key.
        @MainActor static func drawnName(_ virtualKey: UInt16) -> String {
            PreviewInputProvider().inputSource().label(virtualKey: virtualKey).uppercased()
        }
    }

    /// What the More Actions menu did while it tracked.
    @MainActor final class MenuDriver {
        var menu: NSMenu?, index: Int?, titles: [String] = [], ended = false, timedOut = false, actionCompleted = false
    }
    /// Services the real panel only after it accepts the exact synthetic destination. A delegate
    /// rejects every other URL even if the panel's selection changes between the check and Save.
    @MainActor final class SavePanelService: NSObject, NSOpenSavePanelDelegate {
        let target: URL
        let saving: Bool
        private let acceptedTargets: Set<URL>
        private(set) var seen = false, timedOut = false, destinationVerified = false, destinationValidated = false
        private var ticks = 0, seenAt = 0, finished = false, timer: Timer?
        private let interactive = ProcessInfo.processInfo.environment["COD2_SNAPSHOT_INTERACTIVE_EXPORT"] == "1"
        private var deadline = 0.0
        private var deadlineWork: DispatchWorkItem?
        private weak var ownedPanel: NSSavePanel?
        init(target: URL, saving: Bool = true) {
            let original = target.standardizedFileURL
            // Only this already-created, owned synthetic directory is resolved. Panel URLs
            // are compared lexically; no candidate outside it is inspected on disk.
            let resolved = original.deletingLastPathComponent().resolvingSymlinksInPath().appendingPathComponent(original.lastPathComponent)
            self.target = resolved
            self.acceptedTargets = [original, resolved]
            self.saving = saving
            super.init()
        }
        private func accepts(_ url: URL?) -> Bool {
            url.map { acceptedTargets.contains($0.standardizedFileURL) } ?? false
        }
        private func acceptsDirectory(_ url: URL?) -> Bool {
            guard let url else { return false }
            return acceptedTargets.contains { $0.deletingLastPathComponent() == url.standardizedFileURL }
        }
        func makePanel() -> NSSavePanel {
            let panel = NSSavePanel()
            // AppKit permits these properties to be set only before presentation.
            panel.directoryURL = target.deletingLastPathComponent()
            panel.nameFieldStringValue = target.lastPathComponent
            panel.delegate = self
            ownedPanel = panel
            return panel
        }
        func panel(_ sender: Any, validate url: URL) throws {
            guard let panel = sender as? NSSavePanel, panel === ownedPanel, accepts(url) else {
                throw NSError(domain: "LauncherSnapshot", code: 1,
                              userInfo: [NSLocalizedDescriptionKey: "The export must stay in the synthetic test folder."])
            }
            destinationValidated = true
        }
        nonisolated private static func deadlineExpired() {
            FileHandle.standardError.write(Data("FAIL: synthetic export panel exceeded its deadline.\n".utf8))
            exit(1)
        }
        func start() {
            let timeout: TimeInterval = interactive && saving ? 120 : 12
            deadline = ProcessInfo.processInfo.systemUptime + timeout
            // A native nested error panel can stop main-run-loop timers. Bound this test host
            // independently; cancelling this work item prevents it from affecting later checks.
            let work = DispatchWorkItem(block: Self.deadlineExpired)
            deadlineWork = work
            DispatchQueue.global().asyncAfter(deadline: .now() + timeout, execute: work)
            let timer = Timer(timeInterval: 0.1, repeats: true) { [weak self] _ in MainActor.assumeIsolated { self?.tick() } }
            RunLoop.main.add(timer, forMode: .common)
            self.timer = timer
        }
        func stop() { timer?.invalidate(); timer = nil; deadlineWork?.cancel(); deadlineWork = nil }
        private func tick() {
            if ProcessInfo.processInfo.systemUptime >= deadline {
                timedOut = true
                ownedPanel?.cancel(nil)
                stop()
                // Exit only this synthetic host, even if an owned child modal intercepted
                // Cancel. Never abort an unrelated application's modal interaction.
                fputs("FAIL: synthetic export panel exceeded its deadline.\n", stderr)
                exit(1)
            }
            ticks += 1
            guard let panel = NSApp.modalWindow as? NSSavePanel else { return }
            if !seen {
                guard panel === ownedPanel else { return }
                seen = true; seenAt = ticks
            } else if panel === ownedPanel, !finished, ticks >= seenAt + 6,
                      NSApp.isActive, NSApp.keyWindow === panel, panel.isKeyWindow,
                      panel.delegate === self, acceptsDirectory(panel.directoryURL), accepts(panel.url) {
                destinationVerified = true; finished = true
                if saving && interactive {
                    fputs("EXPORT READY: click Save in the real panel; exact synthetic destination verified.\n", stderr)
                } else if saving {
                    ConfigsPreview.press(panel, keyCode: 36, characters: "\r")
                } else { panel.cancel(nil) }
            }
        }
    }

    /// Opens the loadout's More Actions menu with VoiceOver's press, finds the item in the actual
    /// menu while it tracks and closes the menu; the item's action is then sent the way a chosen
    /// item's is, once its menu has closed. Task jobs can't run inside a menu or modal loop entered
    /// from a task, so the menu (and a save panel the item opens) are serviced from timers in the
    /// common run-loop modes, each with a bound.
    static func chooseMoreAction(_ window: NSWindow, _ prefix: String, panel: SavePanelService? = nil) async -> Bool {
        guard let more = AccessibilityTree.elements(of: window).first(where: { $0.label.hasPrefix("More Actions for This Loadout") }) else {
            print("FAIL: More Actions for This Loadout is not in the accessibility tree"); return false
        }
        let driver = MenuDriver(), center = NotificationCenter.default
        let began = center.addObserver(forName: NSMenu.didBeginTrackingNotification, object: nil, queue: nil) { note in
            // Posted on the main thread, inside the menu's own tracking.
            nonisolated(unsafe) let menu = note.object as? NSMenu
            MainActor.assumeIsolated { if driver.menu == nil { driver.menu = menu } }
        }
        let ended = center.addObserver(forName: NSMenu.didEndTrackingNotification, object: nil, queue: nil) { note in
            // Posted on the main thread, inside the menu's own tracking.
            nonisolated(unsafe) let menu = note.object as? NSMenu
            MainActor.assumeIsolated { if menu != nil, menu === driver.menu { driver.ended = true } }
        }
        defer { center.removeObserver(began); center.removeObserver(ended) }
        let start = Date()
        let timer = Timer(timeInterval: 0.1, repeats: true) { _ in
            MainActor.assumeIsolated {
                guard let menu = driver.menu, driver.index == nil, !driver.ended, !driver.timedOut else { return }
                driver.titles = menu.items.map(\.title)
                if let index = menu.items.firstIndex(where: { $0.title.hasPrefix(prefix) && $0.isEnabled }) {
                    driver.index = index; menu.cancelTracking()
                } else if Date().timeIntervalSince(start) > 8 {
                    driver.timedOut = true; menu.cancelTracking()
                }
            }
        }
        RunLoop.main.add(timer, forMode: .common)
        defer { timer.invalidate() }
        AccessibilityTree.press(more)
        // The menu tracks inside the press or just after it.
        for _ in 0..<100 where !driver.ended { try? await Task.sleep(for: .milliseconds(100)) }
        timer.invalidate()
        guard driver.menu != nil, let index = driver.index, driver.ended else {
            print("FAIL: the More Actions menu \(driver.menu == nil ? "did not open" : driver.ended ? "has no enabled “\(prefix)…” item (\(driver.titles))" : "did not close")")
            return false
        }
        // A synchronous modal panel entered from this Swift task cannot service all
        // MainActor continuations. Enter the actual menu action from the native event
        // loop, while this task yields, just as a user choosing the menu item would.
        let actionTimer = Timer(timeInterval: 0.01, repeats: false) { _ in
            MainActor.assumeIsolated {
                panel?.start()
                driver.menu?.performActionForItem(at: index)
                panel?.stop()
                driver.actionCompleted = true
            }
        }
        RunLoop.main.add(actionTimer, forMode: .common)
        defer { actionTimer.invalidate() }
        for _ in 0..<1300 where !driver.actionCompleted { try? await Task.sleep(for: .milliseconds(100)) }
        guard driver.actionCompleted else { print("FAIL: the native menu action did not complete"); return false }
        if let panel, !panel.seen || panel.timedOut || !panel.destinationVerified || (panel.saving && !panel.destinationValidated) {
            print("FAIL: “\(prefix)…” \(panel.seen ? "left its save panel open" : "did not open a save panel")"); return false
        }
        return true
    }

    /// Rename and Export for Windows, opened from the loadout's More Actions menu: renaming writes
    /// only the display name; Cancel writes nothing. Export writes exactly the Windows file into a
    /// private folder outside the game home; Cancel writes nothing.
    static func loadoutMenuActions() async -> Bool {
        guard let (model, window, home) = await open("configs-guided", dark: true) else { return false }
        let exports = FileManager.default.temporaryDirectory.appendingPathComponent("cod2-configs-export-\(UUID().uuidString.prefix(8))")
        defer { window.orderOut(nil); window.close(); try? FileManager.default.removeItem(at: home); try? FileManager.default.removeItem(at: exports) }
        guard let session = model.studio.session else { return false }
        let players = home.appendingPathComponent("main/players")
        func disk() -> [Data?] { ["Main/config_mp.cfg", "Main/studio.json", "active.txt"].map { try? Data(contentsOf: players.appendingPathComponent($0)) } }
        func sheetElement(_ role: String, _ label: String) -> AccessibilityTree.Element? {
            AccessibilityTree.elements(of: window).first { $0.role == role && $0.label == label }
        }
        window.makeKey()

        // Rename, then the same sheet cancelled.
        let before = disk(), count = session.history.count
        guard await chooseMoreAction(window, "Rename “Main”") else { return false }
        try? await Task.sleep(for: .milliseconds(900))
        guard window.attachedSheet != nil, let field = sheetElement("AXTextField", "Name"), let rename = sheetElement("AXButton", "Rename") else {
            print("FAIL: Rename “Main”… did not open the rename sheet"); return false
        }
        AccessibilityTree.setValue(field, "Scrim Night")
        try? await Task.sleep(for: .milliseconds(400))
        AccessibilityTree.press(rename)
        try? await Task.sleep(for: .milliseconds(900))
        let renamed = disk()
        guard model.studio.metadata?.displayName == "Scrim Night", renamed[0] == before[0], renamed[2] == before[2], renamed[1] != before[1],
              session.history.count == count + 1, session.history.first?.summary == "Renamed to Scrim Night", window.attachedSheet == nil else {
            print("FAIL: Rename did not change only the display name (\(model.studio.metadata?.displayName ?? "none"))"); return false
        }
        print("PASS: More Actions › Rename wrote only the display name, Scrim Night, as one History row; config and active loadout unchanged")
        guard await chooseMoreAction(window, "Rename “Scrim Night”") else { return false }
        try? await Task.sleep(for: .milliseconds(900))
        guard let again = sheetElement("AXTextField", "Name"), let cancel = sheetElement("AXButton", "Cancel") else { print("FAIL: the rename sheet did not reopen"); return false }
        AccessibilityTree.setValue(again, "Discarded")
        try? await Task.sleep(for: .milliseconds(300))
        AccessibilityTree.press(cancel)
        try? await Task.sleep(for: .milliseconds(900))
        guard disk() == renamed, session.history.count == count + 1, window.attachedSheet == nil else { print("FAIL: cancelling Rename wrote something"); return false }
        print("PASS: cancelling Rename closed the sheet and wrote nothing")

        // Export for Windows through the save panel, into a private folder; then cancelled.
        do { try FileManager.default.createDirectory(at: exports, withIntermediateDirectories: true) } catch { return false }
        let target = exports.appendingPathComponent("config_mp.cfg")
        guard let current = session.current, let expected = try? ConfigChains.windowsExport(read: current) else { print("FAIL: no Windows export to compare"); return false }
        let save = SavePanelService(target: target)
        model.makeExportPanel = { save.makePanel() }
        defer { model.makeExportPanel = { NSSavePanel() } }
        let exported = await chooseMoreAction(window, "Export for Windows", panel: save)
        try? await Task.sleep(for: .milliseconds(600))
        let targetExists = FileManager.default.fileExists(atPath: target.path)
        let bytesMatch = (try? Data(contentsOf: target)) == expected
        let gameUnchanged = disk() == renamed
        guard exported, targetExists, bytesMatch, gameUnchanged, model.studio.failure == nil else {
            print("FAIL: Export for Windows: panel=\(save.seen), destinationVerified=\(save.destinationVerified), actionCompleted=\(exported), targetExists=\(targetExists), bytesMatch=\(bytesMatch), gameUnchanged=\(gameUnchanged), modelFailure=\(model.studio.failure != nil)")
            return false
        }
        print("PASS: More Actions › Export for Windows opened the save panel and wrote exactly the Windows config in a private folder")
        try? FileManager.default.removeItem(at: target)
        let cancelExport = SavePanelService(target: target, saving: false)
        model.makeExportPanel = { cancelExport.makePanel() }
        let cancelled = await chooseMoreAction(window, "Export for Windows", panel: cancelExport)
        try? await Task.sleep(for: .milliseconds(600))
        guard cancelled, !FileManager.default.fileExists(atPath: target.path), disk() == renamed, model.studio.failure == nil else {
            print("FAIL: cancelling the export panel wrote something"); return false
        }
        print("PASS: cancelling the export save panel wrote nothing")
        return true
    }

    /// A key press delivered through the application, where Binds' local monitor listens.
    static func press(_ window: NSWindow, keyCode: UInt16, characters: String, modifiers: NSEvent.ModifierFlags = [], charactersIgnoringModifiers: String? = nil) {
        guard NSApp.isActive, NSApp.keyWindow === window, window.isKeyWindow, window.firstResponder != nil else {
            fputs("FAIL: key event target is not the actual active key window with a first responder.\n", stderr); exit(1)
        }
        for type in [NSEvent.EventType.keyDown, .keyUp] {
            if let event = NSEvent.keyEvent(with: type, location: .zero, modifierFlags: modifiers, timestamp: ProcessInfo.processInfo.systemUptime,
                                            windowNumber: window.windowNumber, context: nil, characters: characters,
                                            charactersIgnoringModifiers: charactersIgnoringModifiers ?? characters, isARepeat: false, keyCode: keyCode) {
                NSApp.sendEvent(event)
            }
        }
    }
}
