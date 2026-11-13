import SwiftUI
import AppKit

// Owner: ship-shell. Jump to… (⌘K, HomeJump board): one field that finds servers in the list already
// in memory (never a network request), loadouts, every Settings pane and row by its visible name,
// and the menu bar's actions. ↑ ↓ move, ↩ opens, ⌘↩ launches on a server, Escape closes.

/// One result. `open` is ↩ (and a click); `launch` is ⌘↩, for servers only.
struct JumpItem: Identifiable {
    enum Section: String, CaseIterable { case servers = "Servers", loadouts = "Loadouts", settings = "Settings", actions = "Actions" }
    var id: String
    var section: Section
    var title: String
    var detail: String
    var symbol: String
    var keys: String = ""
    var serverAddress: String? = nil
    var open: @MainActor () -> Void
    var launch: (@MainActor () -> Void)? = nil
}

@MainActor enum JumpIndex {
    /// Settings rows by their visible names (SettingsWin board), each with the pane it opens.
    /// Keep these names aligned with the actual panes; conditional rows are filtered below.
    static let settingsRows: [(title: String, pane: SettingsPane)] = [
        ("Game version", .general), ("Hide CoD2 Silicon while the game is running", .general), ("Appearance", .general),
        ("Refresh the server list when Servers opens", .general),
        ("Display", .display), ("Mode", .display), ("Resolution", .display), ("Vertical sync", .display), ("Texture sharpness at a distance", .display),
        ("Game data", .gameData), ("CD key", .gameData), ("Shaders", .gameData), ("Loadouts, demos and screenshots", .gameData),
        ("Game versions installed", .gameData), ("CoD2x menus and icons", .gameData), ("Set up again", .gameData),
        ("Send crash reports to CoD2 Silicon", .privacy), ("Send CoD2x crash reports to the CoD2x project", .privacy), ("Match summaries", .privacy),
        ("Check for updates automatically", .updates),
        ("Extra launch options", .advanced), ("Logs", .advanced), ("Warnings you dismissed", .advanced), ("Launcher settings", .advanced),
        ("Acknowledgements", .about), ("Release notes", .about),
    ]
    static func items(_ model: LauncherModel, query raw: String) -> [JumpItem] {
        let query = raw.trimmingCharacters(in: .whitespaces)
        func matches(_ fields: String...) -> Bool { query.isEmpty || fields.contains { $0.localizedCaseInsensitiveContains(query) } }
        var result: [JumpItem] = []
        // Servers: what the list already holds; with nothing typed, the recent ones.
        let pool = query.isEmpty ? model.recentServers : model.servers
        let servers = pool.filter { matches(QuakeColors.plain($0.name), MapCatalog.displayName($0.map), $0.map, $0.address) }
            .sorted { query.isEmpty ? false : $0.ping < $1.ping }
            .prefix(query.isEmpty ? 3 : 5)
        for server in servers {
            let players = server.maxPlayers > 0 ? "\(server.playerCount)/\(server.maxPlayers)" : nil
            let detail = [MapCatalog.displayName(server.map), players, server.ping > 0 ? "\(server.ping) ms" : nil].compactMap { $0 }.joined(separator: " · ")
            result.append(JumpItem(id: "server-" + server.address, section: .servers, title: QuakeColors.plain(server.name), detail: detail,
                                   symbol: "dot.radiowaves.left.and.right", serverAddress: server.address,
                                   open: { model.showServer(server.address) }, launch: { model.connect(server) }))
        }
        // Loadouts.
        for profile in model.studio.profiles where matches(profile.displayName, "loadout") && !query.isEmpty {
            result.append(JumpItem(id: "loadout-" + profile.id, section: .loadouts, title: profile.displayName,
                                   detail: profile.isActive ? "Loadout · used for your next game" : "Loadout", symbol: "square.stack",
                                   open: { model.navigate(to: .configs); model.studio.select(profile.id) }))
        }
        // Settings: every pane and row by its visible name.
        if !query.isEmpty {
            for pane in SettingsPane.allCases where matches(pane.title) {
                result.append(JumpItem(id: "pane-" + pane.rawValue, section: .settings, title: pane.title, detail: "Settings",
                                       symbol: pane.symbol, open: { model.openSettings(pane) }))
            }
            for row in settingsRows where matches(row.title) && !(model.usesLoadouts && row.title == "Texture sharpness at a distance") {
                result.append(JumpItem(id: "row-" + row.title, section: .settings, title: row.title, detail: "Settings › \(row.pane.title)",
                                       symbol: "gearshape", open: { model.openSettings(row.pane) }))
            }
        }
        // Actions from the menu bar that can run now.
        for action in actions(model) where matches(action.title) {
            result.append(action)
        }
        // Keep each section short, so the first screen shows every kind of result.
        var counts: [JumpItem.Section: Int] = [:]
        return result.filter { item in
            counts[item.section, default: 0] += 1
            return counts[item.section]! <= (item.section == .actions ? (query.isEmpty ? 6 : 5) : 6)
        }
    }
    private static func actions(_ model: LauncherModel) -> [JumpItem] {
        var list: [JumpItem] = []
        func add(_ title: String, _ symbol: String, _ keys: String = "", _ enabled: Bool = true, _ run: @escaping @MainActor () -> Void) {
            guard enabled else { return }
            list.append(JumpItem(id: "action-" + title, section: .actions, title: title, detail: "", symbol: symbol, keys: keys, open: run))
        }
        add("Connect to Address…", "plus", "⌘L") { model.connectToAddress() }
        add("Refresh Server List", "arrow.clockwise", "⌘R", !model.refreshing) { model.navigate(to: .servers); model.refresh() }
        add(model.lastServer.map { "Launch on \(QuakeColors.plain($0.name))" } ?? "Launch", "play.fill", "⌘↩") { model.launch() }
        add("Open Game Menu", "gamecontroller", "⇧⌘↩") { model.openGameMenu() }
        add("Import Config…", "square.and.arrow.down", "⌘O", model.canImport) { model.importConfig() }
        add("Paste Config", "doc.on.clipboard", "⇧⌘V", model.canImport) { model.pasteConfig() }
        for page in LauncherPage.navigation { add(page.title, page.symbol, "⌘\(page.shortcut)") { model.navigate(to: page) } }
        add("Settings…", "gearshape", "⌘,") { model.openSettings() }
        add("Run Check-up", "stethoscope") { model.openCheckup() }
        add("Show History", "clock.arrow.circlepath", "", !model.studio.profiles.isEmpty) { model.showHistory() }
        add("Show Game Data in Finder", "folder", "", !model.dataPath.isEmpty) { model.showGameDataInFinder() }
        add("Show My Files in Finder", "folder") { model.showMyFilesInFinder() }
        add("Show Logs in Finder", "doc.text.magnifyingglass") { model.showLogsInFinder() }
        add("Check for Updates…", "arrow.down.circle", "", !model.checkingUpdates) { model.openSettings(.updates); model.checkUpdates() }
        add("Keyboard Shortcuts", "keyboard") { model.showKeyboardShortcuts() }
        add("Release Notes", "doc.richtext") { model.openReleaseNotes() }
        add("Report a Problem…", "ladybug") { model.reportProblem() }
        add("Clear Recent Servers", "clock.badge.xmark", "", !model.library.recent.isEmpty) { model.clearRecentServers() }
        return list
    }
}

struct JumpToPalette: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    private var query: String { model.jumpQuery }
    @State private var selection = 0
    @State private var monitor: Any?
    @FocusState private var focused: Bool
    var body: some View {
        let items = JumpIndex.items(model, query: query)
        ZStack(alignment: .top) {
            (palette.dark ? Color.black.opacity(0.42) : palette.ground.opacity(0.55))
                .ignoresSafeArea()
                .contentShape(Rectangle())
                .onTapGesture { close() }
                .accessibilityHidden(true)
            VStack(spacing: 0) {
                field
                Rectangle().fill(palette.separator).frame(height: 1)
                results(items)
                footer
            }
            .frame(width: 640)
            .glassSurface(RoundedRectangle(cornerRadius: 26, style: .continuous))
            .padding(.top, 72)
            .accessibilityElement(children: .contain)
            .accessibilityLabel("Jump to")
            .accessibilityAddTraits(.isModal)
        }
        .transition(reduceMotion ? .opacity : .opacity.combined(with: .scale(scale: 0.98, anchor: .top)))
        .onAppear { focused = true; selection = 0; install() }
        .onDisappear { remove(); model.jumpServer = nil }
        .onChange(of: model.jumpQuery) { _, _ in selection = 0; publish(JumpIndex.items(model, query: query)) }
        .onChange(of: selection, initial: true) { _, _ in publish(items) }
    }
    private var field: some View {
        HStack(spacing: 12) {
            Image(systemName: "magnifyingglass").font(.system(size: 17, weight: .medium)).foregroundStyle(palette.secondary).accessibilityHidden(true)
            TextField("Jump to", text: $model.jumpQuery, prompt: Text("Servers, loadouts, settings and actions").foregroundStyle(palette.tertiary))
                .textFieldStyle(.plain).font(.system(size: 17)).foregroundStyle(palette.text)
                .focused($focused)
                .accessibilityLabel("Jump to")
            KeyCap("esc").accessibilityLabel("Escape closes")
        }
        .padding(.horizontal, 18).frame(height: 58)
    }
    @ViewBuilder private func results(_ items: [JumpItem]) -> some View {
        if items.isEmpty {
            Text("Nothing matches “\(query)”. Try a server, map, loadout or setting name.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary).frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 20).padding(.vertical, 18)
                .accessibilityAddTraits(.isStaticText)
        } else {
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(JumpItem.Section.allCases, id: \.self) { section in
                            let rows = items.enumerated().filter { $0.element.section == section }
                            if !rows.isEmpty {
                                Text(section.rawValue).font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.secondary)
                                    .padding(.horizontal, 12).padding(.top, 8).padding(.bottom, 4)
                                    .accessibilityAddTraits(.isHeader)
                                ForEach(rows, id: \.element.id) { index, item in row(item, selected: index == selection).id(index) }
                            }
                        }
                    }
                    .padding(8)
                }
                .frame(maxHeight: 380)
                .fixedSize(horizontal: false, vertical: true)
                .onChange(of: selection) { _, value in proxy.scrollTo(value) }
            }
        }
    }
    private func row(_ item: JumpItem, selected: Bool) -> some View {
        Button { run(item, launch: false) } label: {
            HStack(spacing: 12) {
                Image(systemName: item.symbol).font(.system(size: 15, weight: .medium)).foregroundStyle(palette.chipIcon).frame(width: 20)
                Text(item.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                if !item.detail.isEmpty { Text(item.detail).font(.system(size: 13)).foregroundStyle(palette.secondary).lineLimit(1) }
                Spacer(minLength: 8)
                if selected && item.launch != nil {
                    Text("Launch ⌘↩").font(.system(size: 12)).foregroundStyle(palette.secondary)
                } else if !item.keys.isEmpty {
                    Text(item.keys).font(.system(size: 12)).foregroundStyle(palette.tertiary)
                }
            }
            .padding(.horizontal, 12).frame(height: 40)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(item.detail.isEmpty ? item.title : "\(item.title), \(item.detail)")
        .accessibilityAddTraits(selected ? .isSelected : [])
        .accessibilityHint(item.launch != nil ? "Return shows it in Servers. Command Return launches on it." : "")
    }
    private var footer: some View {
        HStack(spacing: 14) {
            HStack(spacing: 5) { KeyCap("↑"); KeyCap("↓"); Text("to move") }
            HStack(spacing: 5) { KeyCap("↩"); Text("to open") }
            HStack(spacing: 5) { ShortcutKeys(["⌘", "↩"]); Text("to launch on a server") }
            Spacer(minLength: 0)
        }
        .font(.system(size: 12)).foregroundStyle(palette.secondary)
        .padding(.horizontal, 14).frame(height: 44)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }
    private func publish(_ items: [JumpItem]) {
        let address = items.indices.contains(selection) ? items[selection].serverAddress : nil
        if model.jumpServer != address { model.jumpServer = address }
    }
    private func run(_ item: JumpItem, launch: Bool) {
        close()
        if launch, let go = item.launch { go() } else { item.open() }
    }
    private func close() { model.jumpPresented = false }
    /// ↑ ↓ ↩ ⌘↩ and Escape go to the palette while it is open, before the field or the menus see them.
    private func install() {
        guard monitor == nil else { return }
        monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { event in
            let code = event.keyCode, command = event.modifierFlags.contains(.command), number = event.windowNumber
            let consumed = MainActor.assumeIsolated { handle(code, command: command, window: number) }
            return consumed ? nil : event
        }
    }
    /// True when the key was the palette's.
    private func handle(_ code: UInt16, command: Bool, window: Int) -> Bool {
        guard model.jumpPresented, model.launcherWindow.map({ $0.windowNumber == window }) ?? true else { return false }
        let items = JumpIndex.items(model, query: query)
        switch code {
        case 125: if !items.isEmpty { selection = min(items.count - 1, selection + 1) }
        case 126: if !items.isEmpty { selection = max(0, selection - 1) }
        case 53: close()
        case 36, 76:
            guard items.indices.contains(selection) else { return true }
            let item = items[selection]
            run(item, launch: command && item.launch != nil)
        default: return false
        }
        return true
    }
    private func remove() { if let monitor { NSEvent.removeMonitor(monitor) }; monitor = nil }
}
