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
        if ProcessInfo.processInfo.environment["COD2_SNAPSHOT_AX_TRACE"] == "1" { fputs("AX \(what): \(getter)\n", stderr) }
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
                    if let stuck { fputs("FAIL: accessibility query did not return within \(Int(Self.limit)) s: \(stuck)\n", stderr); Darwin.exit(1) }
                }
            }
        }
    }
    func end() { lock.lock(); current = nil; lock.unlock() }
}

@main struct Snapshots {
    /// LaunchServices gives the review app focus, but `open -W` does not relay its exit code.
    /// Keep a per-run result so the Python runner still fails on crashes and failed checks.
    static func writeResult(_ status: Int32) {
        let arguments = CommandLine.arguments
        guard let index = arguments.firstIndex(of: "--result-file"), arguments.count > index + 1 else { return }
        try? "\(getpid())\n\(status)\n".write(toFile: arguments[index + 1], atomically: true, encoding: .utf8)
    }
    @MainActor static var outputDirectory: URL?
    @MainActor static func finish(_ status: Int32) -> Never {
        writeResult(status)
        if let outputDirectory { try? String(status).write(to: outputDirectory.appendingPathComponent("exit-status"), atomically: true, encoding: .utf8) }
        Darwin.exit(status)
    }
    // MARK: Screens and suites
    // Each unit lists its screens, page screens, accessibility screens, expected labels and suites in
    // its own harness file (SnapshotsShell.swift, SnapshotsHomeLibrary.swift, SnapshotsServers.swift,
    // SnapshotsSettings.swift, SnapshotsConfigs.swift); these gather them. A screen's name picks its
    // window: “settings…” and “about…” render the Settings window, “foundations…” the building-block
    // gallery (SnapshotsFoundations.swift), everything else the main window. `model.preview(_:)` sets it up.
    static var screens: [String] { exampleScreens + shellScreens + homeLibraryScreens + serversScreens + settingsScreens + pageScreens + configScreens }
    /// Page screens: rendered by `--group pages` (the accessibility-preference variants) and read by `--accessibility`.
    static var pageScreens: [String] { homeLibraryPageScreens + serversPageScreens + settingsPageScreens }
    /// Page screens whose accessibility tree `--accessibility` reads alongside Config Studio's.
    static var accessibilityScreens: [String] {
        ["foundations", "sheet-keyboard-shortcuts"] + homeLibraryAccessibilityScreens + serversAccessibilityScreens + settingsAccessibilityScreens + pageScreens
    }
    /// Labels each screen must expose to VoiceOver, so an empty or unbuilt tree can't pass.
    @MainActor static var expectedLabels: [String: [String]] {
        [exampleLabels, homeLibraryLabels, serversLabels, settingsLabels, ConfigsPreview.expectedLabels].reduce(into: [:]) { all, unit in all.merge(unit) { first, _ in first } }
    }
    /// Shared examples: the Foundations gallery (also with a popover open), and a sheet over each window.
    static let exampleScreens = ["foundations", "foundations-popover", "sheet-keyboard-shortcuts", "settings-sheet-acknowledgements"]
    static let exampleLabels: [String: [String]] = [
        "foundations": ["Foundations", "Local match summaries", "Display mode", "Show in Finder", "Server address", "Search servers"],
        "sheet-keyboard-shortcuts": ["Keyboard shortcuts", "Done", "Command 1"],
    ]
    /// A window-driving check run by `--suite NAME`; snapshots.py runs every suite `--list-suites`
    /// prints, each with a private HOME below its output folder.
    struct Suite: Sendable { let name: String; let run: @MainActor @Sendable (URL) async -> Bool }
    static var suites: [Suite] { shellSuites + [Suite(name: "page-actions", run: PagesPreview.actions)] }

    /// The screens `--group pages` renders: the page screens and every Home, Servers, Library, Settings and About screen.
    static func inPagesGroup(_ screen: String) -> Bool {
        pageScreens.contains(screen) || screen.hasPrefix("home") || screen.hasPrefix("settings") || screen.hasPrefix("about")
            || ["servers", "servers-empty", "library-demos", "library-screenshots", "servers-ready"].contains(screen)
    }
    /// The window a screen renders in.
    enum Surface: String { case main, settings, gallery }
    static func surface(for screen: String?) -> Surface {
        guard let screen else { return .main }
        if screen.hasPrefix("settings") || screen.hasPrefix("about") { return .settings }
        return screen.hasPrefix("foundations") ? .gallery : .main
    }
    /// The Settings window is 900 × 700 (900 × 560 at its minimum); the others use `windowSize`.
    @MainActor static func size(of surface: Surface) -> NSSize {
        guard surface == .settings else { return windowSize }
        return NSSize(width: SettingsWindowMetrics.width, height: windowSize == NSSize(width: 1440, height: 900) ? SettingsWindowMetrics.height : SettingsWindowMetrics.minimumHeight)
    }
    /// The size of the window being captured, for the shape audit's coordinates.
    @MainActor static var captureSize = NSSize(width: 1440, height: 900)

    @MainActor static func main() {
        writeResult(-1)
        setbuf(stdout, nil)
        let arguments = CommandLine.arguments
        // `--list` prints each screen and its window, `--list-suites` the suites; no window opens.
        if arguments.count >= 2, arguments[1] == "--list" {
            for screen in screens {
                print(screen, surface(for: screen).rawValue, configScreens.contains(screen) ? "configs" : "-", inPagesGroup(screen) ? "pages" : "-")
            }
            Darwin.exit(0)
        }
        if arguments.count >= 2, arguments[1] == "--list-suites" { suites.forEach { print($0.name) }; Darwin.exit(0) }
        guard arguments.count >= 2 else { fputs("usage: LauncherSnapshots OUTPUT [--data GAME_DIR] [--fallback] [--scale2] [--only SCREEN]\n", stderr); Snapshots.finish(2) }
        let output = URL(fileURLWithPath: arguments[1])
        outputDirectory = output
        let data = arguments.firstIndex(of: "--data").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let fallback = arguments.contains("--fallback"), scale2 = arguments.contains("--scale2")
        let only = arguments.firstIndex(of: "--only").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let group = arguments.firstIndex(of: "--group").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
        let configsOnly = group == "configs", pagesOnly = group == "pages"
        if arguments.contains("--minimum") { windowSize = NSSize(width: 1100, height: 760) }
        let accessibility = Accessibility(increaseContrast: arguments.contains("--increase-contrast"), reduceTransparency: arguments.contains("--reduce-transparency"),
                                          reduceMotion: arguments.contains("--reduce-motion"))
        let app = NSApplication.shared
        // Match the shipping app: accessory activation can leave a real window inactive.
        app.setActivationPolicy(.regular)
        LauncherFonts.register()
        guard NSFont(name: LauncherFonts.stencilHeavy, size: 12) != nil, NSFont(name: LauncherFonts.typewriter, size: 12) != nil else {
            fputs("Bundled fonts did not register.\n", stderr); Snapshots.finish(1)
        }
        do { try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true) } catch { fputs("\(error)\n", stderr); Snapshots.finish(1) }
        if arguments.contains("--contrast") {
            guard let data else { fputs("--contrast needs --data GAME_DIR\n", stderr); Snapshots.finish(2) }
            Snapshots.finish(contrastReport(data: data, output: output) ? 0 : 1)
        }
        // `--suite NAME`, or the older `--shell-actions`, `--shell-accessibility` and `--page-actions`.
        let suiteName = arguments.firstIndex(of: "--suite").flatMap { arguments.count > $0 + 1 ? arguments[$0 + 1] : nil }
            ?? suites.map(\.name).first { arguments.contains("--" + $0) }
        if let suiteName {
            guard let suite = suites.first(where: { $0.name == suiteName }) else { fputs("No suite named \(suiteName).\n", stderr); Snapshots.finish(2) }
            Task { @MainActor in Snapshots.finish(await suite.run(output) ? 0 : 1) }
            app.run()
        }
        if arguments.contains("--actions") {
            Task { @MainActor in
                let passed = if let only { await ConfigsPreview.action(only) } else { await ConfigsPreview.actions() }
                Snapshots.finish(passed ? 0 : 1)
            }
            app.run()
        }
        if arguments.contains("--accessibility") {
            Task { @MainActor in Snapshots.finish(await ConfigsPreview.accessibility(output: output, only: only) ? 0 : 1) }
            app.run()
        }
        if arguments.contains("--overlays") {
            Task { @MainActor in Snapshots.finish(await ConfigsPreview.overlays(output: output, only: only) ? 0 : 1) }
            app.run()
        }
        if arguments.contains("--navigation") {
            Task { @MainActor in Snapshots.finish(await ConfigsPreview.navigation(output: output) ? 0 : 1) }
            app.run()
        }
        Task { @MainActor in
            var written = 0, checked = 0
            var findings: [String] = [], report: [[String: Any]] = []
            for dark in [true, false] {
                for screen in screens where (only == nil || only == screen) && (!configsOnly || configScreens.contains(screen)) && (!pagesOnly || inPagesGroup(screen)) {
                    let gate = screen == "configs-catalog-loading" ? PreviewCatalogGate() : nil
                    defer { gate?.release() }
                    let model = LauncherModel(snapshot: true, catalog: { try gate?.load() ?? ConfigStudioModel.bundledCatalog() })
                    model.preview(screen)
                    var home: URL?
                    if configScreens.contains(screen) || screen.hasPrefix("home-loadout") || screen.hasPrefix("settings") {
                        do { home = try ConfigsPreview.seed(model, screen: screen); try await ConfigsPreview.prepare(model, screen: screen, home: home!) }
                        catch { fputs("Config Studio preview failed for \(screen): \(error)\n", stderr); Snapshots.finish(1) }
                    }
                    defer { if let home { try? FileManager.default.removeItem(at: home) } }
                    if let data {
                        let maps = model.servers.map(\.map) + model.media.compactMap { $0.facts.map.map { "mp_" + $0 } } + ["mp_toujane"]
                        model.artwork.loadPrivatePreviewSynchronously(dataPath: data, maps: maps)
                    }
                    let name = "\(screen)-\(dark ? "dark" : "light")"
                    let audit = ShapeAudit()
                    let captured = await capture(model: model, dark: dark, fallback: fallback, audit: audit, accessibility: accessibility, screen: screen)
                    if let sheet = captured.sheet { try? write(sheet, to: output.appendingPathComponent(name + "-sheet.png")); written += 1 }
                    if let image = captured.window {
                        try? write(image, to: output.appendingPathComponent(name + ".png")); written += 1
                        let items = Array(audit.items.values)
                        let found = ShapeCheck.findings(items, image: image).map { "\(name): \($0)" }
                        checked += items.count; findings += found
                        report.append(["screen": name, "items": items.map(ShapeCheck.json), "findings": found])
                    } else { fputs("Capture failed: \(name)\n", stderr) }
                    if scale2 {
                        let rendered = await render2x(model: model, dark: dark, accessibility: accessibility, screen: screen)
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
            Snapshots.finish(written > 0 && findings.isEmpty ? 0 : 1)
        }
        app.run()
    }

    struct Accessibility { var increaseContrast = false; var reduceTransparency = false; var reduceMotion = false }

    @MainActor static func root(_ model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil, accessibility: Accessibility = .init(),
                                screen: String? = nil) -> some View {
        content(model, screen: screen)
            .environment(\.shapeAudit, audit)
            .environment(\.usesGlass, glass && LiquidGlass.available)
            .environment(\.colorScheme, dark ? .dark : .light)
            // Test-only overrides of the system settings, without changing them for the Mac.
            .environment(\._colorSchemeContrast, accessibility.increaseContrast ? .increased : .standard)
            .environment(\._accessibilityReduceTransparency, accessibility.reduceTransparency)
            .environment(\._accessibilityReduceMotion, accessibility.reduceMotion)
    }


    /// The window's root view for a screen: the launcher, the Settings window or the Foundations gallery.
    @MainActor @ViewBuilder static func content(_ model: LauncherModel, screen: String?) -> some View {
        switch surface(for: screen) {
        case .main: LauncherRoot(model: model)
        case .settings: SettingsWindowRoot(model: model)
        case .gallery: FoundationsGallery(model: model, popover: screen == "foundations-popover")
        }
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

    /// The window, and an attached sheet (the import review) as its own image. A popover the screen
    /// opens is composited over the window.
    @MainActor static func capture(model: LauncherModel, dark: Bool, fallback: Bool, audit: ShapeAudit, accessibility: Accessibility = .init(), screen: String? = nil) async -> (window: CGImage?, sheet: CGImage?) {
        let window = await show(model: model, dark: dark, glass: !fallback, audit: audit, accessibility: accessibility, screen: screen)
        await preparePageWindow(window, screen: screen)
        let popovers = NSApp.windows.filter { $0 !== window && $0.isVisible && String(describing: type(of: $0)).contains("Popover") }
        let image = popovers.isEmpty ? WindowCapture.image(of: window) : WindowCapture.image(of: popovers + [window], in: window)
        let sheet = window.attachedSheet.flatMap { WindowCapture.image(of: $0) }
        geometry(window, "capture", image: image)
        window.attachedSheet.map { window.endSheet($0) }
        popovers.forEach { $0.orderOut(nil) }
        window.orderOut(nil); window.close()
        return (image, sheet)
    }

    /// Reach validation through the actual sheet controls, without a production preview override.
    @MainActor static func preparePageWindow(_ window: NSWindow, screen: String?) async {
        if screen?.hasPrefix("settings-legacy") == true, let content = window.contentView,
           let scroll = ConfigsPreview.views(NSScrollView.self, in: content).max(by: {
               ($0.documentView?.bounds.height ?? 0) - $0.contentSize.height < ($1.documentView?.bounds.height ?? 0) - $1.contentSize.height
           }), let document = scroll.documentView {
            scroll.contentView.scroll(to: NSPoint(x: 0, y: max(0, document.bounds.height - scroll.contentSize.height)))
            scroll.reflectScrolledClipView(scroll.contentView)
            try? await Task.sleep(for: .milliseconds(300))
            return
        }
        guard screen == "library-rename-error" else { return }
        AccessibilityTree.wake(window)
        let elements = AccessibilityTree.elements(of: window)
        guard let field = elements.first(where: { $0.role == "AXTextField" && $0.label == "Recording name" }),
              let rename = elements.first(where: { $0.role == "AXButton" && $0.label == "Rename" }) else {
            fputs("FAIL: Rename validation fixture is missing its field or action.\n", stderr); finish(1)
        }
        AccessibilityTree.setValue(field, "invalid/name")
        try? await Task.sleep(for: .milliseconds(100))
        guard AccessibilityTree.press(rename) else { finish(1) }
        try? await Task.sleep(for: .milliseconds(300))
        guard AccessibilityTree.elements(of: window).contains(where: { $0.label.contains("without slashes") || $0.value.contains("without slashes") }) else {
            fputs("FAIL: Rename did not show its inline validation error.\n", stderr); finish(1)
        }
    }

    /// 1440×900, or the launcher's 1100×760 minimum with --minimum.
    @MainActor static var windowSize = NSSize(width: 1440, height: 900)

    /// The app-like window, shown and laid out at `windowSize` (the Settings window at its own size).
    @MainActor static func show(model: LauncherModel, dark: Bool, glass: Bool, audit: ShapeAudit? = nil, accessibility: Accessibility = .init(),
                                screen: String? = nil) async -> NSWindow {
        let appearance = NSAppearance(named: dark ? .darkAqua : .aqua)
        NSApp.appearance = appearance
        let surface = surface(for: screen)
        let windowSize = size(of: surface)
        captureSize = windowSize
        let controller = NSHostingController(rootView: root(model, dark: dark, glass: glass, audit: audit, accessibility: accessibility, screen: screen))
        controller.sizingOptions = []
        if #available(macOS 14.0, *) { controller.sceneBridgingOptions = surface == .settings ? [.title, .toolbars] : [.toolbars] }
        // The Settings window keeps its title (the pane's name) like the app's; it isn't resizable sideways.
        let style: NSWindow.StyleMask = surface == .settings ? [.titled, .closable, .miniaturizable, .fullSizeContentView]
            : [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView]
        let window = SnapshotWindow(contentRect: NSRect(origin: .zero, size: windowSize), styleMask: style, backing: .buffered, defer: false)
        geometry(window, "init")
        window.appearance = appearance
        if surface == .settings {
            window.title = model.settingsPane.title
        } else {
            window.titlebarAppearsTransparent = true
            window.titleVisibility = .hidden
            window.toolbarStyle = .unified
        }
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
        // Launch Services can finish activation after the initial layout pass. Keep the
        // actual focus assertion, allowing the normal activation request time to settle.
        for _ in 0..<10 {
            if NSApp.isActive, NSApp.keyWindow === keyTarget, keyTarget.isKeyWindow { break }
            NSApp.activate(); keyTarget.makeKeyAndOrderFront(nil)
            try? await Task.sleep(for: .milliseconds(200))
        }
        guard NSApp.isActive, NSApp.keyWindow === keyTarget, keyTarget.isKeyWindow else {
            print("Focus diagnostic: active=\(NSApp.isActive), targetKey=\(keyTarget.isKeyWindow), key=\(String(describing: NSApp.keyWindow)), front=\(NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? "none")")
            fputs("FAIL: the real snapshot window or its attached sheet did not become active and key; no focus or material proof is possible.\n", stderr)
            Snapshots.finish(1)
        }
        return window
    }

    /// 2× render of the same window with the material styling, toolbar included, for text, vector
    /// and shape checks. A bare hosting view has no window, so the split view wouldn't lay out.
    @MainActor static func render2x(model: LauncherModel, dark: Bool, accessibility: Accessibility = .init(), screen: String? = nil) async -> (window: CGImage?, sheet: CGImage?) {
        let window = await show(model: model, dark: dark, glass: false, accessibility: accessibility, screen: screen)
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
            guard CGRect(origin: .zero, size: Snapshots.captureSize).contains(item.frame) else { return false }
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
            width = image.width; height = image.height; scale = CGFloat(image.width) / Snapshots.captureSize.width
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

/// Page regressions run with an explicitly private HOME supplied by snapshots.py. Each unit's part
/// lives in its harness file (SettingsChecks, ServersChecks, HomeLibraryChecks); they share one model and window.
@MainActor enum PagesPreview {
    static func actions(output: URL) async -> Bool {
        guard let privateHome = ProcessInfo.processInfo.environment["CFFIXED_USER_HOME"],
              privateHome.hasPrefix(output.deletingLastPathComponent().path + "/"), NSHomeDirectory() == privateHome else {
            print("FAIL: page actions require a private home below the test output"); return false
        }
        let model = LauncherModel()
        model.onboard = false
        do {
            guard try await SettingsChecks.pageActions(model) else { return false }
            let window = await Snapshots.show(model: model, dark: true, glass: true)
            defer { window.orderOut(nil); window.close() }
            AccessibilityTree.wake(window)
            guard try await ServersChecks.pageActions(model, window: window) else { return false }
            guard try await HomeLibraryChecks.pageActions(model, window: window) else { return false }
            return true
        } catch { print("FAIL: page actions: \(error)"); return false }
    }
}
