import AppKit
import SwiftUI
import UniformTypeIdentifiers

// Owner: ship-configs. The Loadout menu, Configs' entry points other units call, and Config Studio's
// harness states. Stored state lives in `studio` (ConfigStudioModel) and `discover`.
@MainActor extension LauncherModel {
    // MARK: Menu commands (Loadout menu, File › Import, Paste and Export)
    /// File › Import Config… (⌘O): the file chooser, attached to the main window.
    func importConfig() {
        guard canImport else { return }
        showMainWindow(); navigate(to: .configs)
        discover.chooseFile(in: launcherWindow)
    }
    /// File › Paste Config (⇧⌘V).
    func pasteConfig() { guard canImport else { return }; showMainWindow(); navigate(to: .configs); discover.importPasteboard() }

    /// Every loadout, for Loadout › Use for Next Game › (the one in use carries the tick: `isActive`).
    var loadouts: [ConfigProfile] { studio.profiles }
    /// The loadout the next game starts with (nil before loadouts exist).
    var nextGameLoadout: ConfigProfile? { studio.activeProfile }
    /// The loadout the Loadout menu acts on: the one Configs shows (the one in use until another is chosen).
    var menuLoadout: ConfigProfile? { studio.current?.profile }

    /// Loadout › Use for Next Game ›, Run Check-up and Show History: there are loadouts to show.
    var canUseLoadouts: Bool { canNavigate && studio.session != nil && studio.current != nil }
    /// New Loadout… makes a loadout: not while the game runs (Configs are read-only then).
    var canNewLoadout: Bool { canNavigate && studio.session != nil && !studio.isReadOnly }
    var canDuplicateLoadout: Bool { canNewLoadout && studio.current != nil }
    var canRenameLoadout: Bool { canNewLoadout && studio.current != nil }
    var canExportLoadout: Bool { canNavigate && studio.current != nil && studio.catalog != nil }
    /// Never the loadout in use, never while the game runs.
    var canDeleteLoadout: Bool { canNavigate && studio.deleteBlock(studio.current?.profile.id) == nil }

    /// Loadout › Use for Next Game ›, the loadout row's context menu, and Home's loadout chip.
    func useLoadoutForNextGame(_ id: String) {
        guard studio.session != nil, !studio.isReadOnly else { return }
        studio.select(id); studio.useForNextGame()
        refreshDisplay()
    }
    /// Loadout › New Loadout… (⌘N): the New Loadout sheet over Configs.
    func newLoadout() {
        guard canNewLoadout else { return }
        showConfigs()
        sheet = .newLoadout
    }
    /// Loadout › Duplicate (⇧⌘D) and the row's context menu: a copy, named “Copy of …”, shown at once.
    func duplicateLoadout(_ id: String? = nil) {
        guard canNewLoadout else { return }
        if let id { studio.select(id) }
        guard studio.current != nil else { return }
        showConfigs()
        studio.duplicateLoadout()
    }
    /// Loadout › Rename… and the row's context menu.
    func renameLoadout(_ id: String? = nil) {
        guard canNewLoadout else { return }
        if let id { studio.select(id) }
        guard studio.current != nil else { return }
        showConfigs()
        sheet = .renameLoadout
    }
    /// File › Export Loadout… (⇧⌘E), More Actions › Export for Windows… and the row's Export…: a
    /// config_mp.cfg that Call of Duty 2 on Windows can load, through a save panel on the main window.
    func exportLoadout(_ id: String? = nil) {
        guard canNavigate else { return }
        if let id { studio.select(id) }
        guard let current = studio.current, studio.catalog != nil else { return }
        showConfigs()
        let panel = makeExportPanel()
        panel.nameFieldStringValue = "config_mp.cfg"
        panel.allowedContentTypes = [UTType(filenameExtension: "cfg") ?? .plainText]
        panel.message = "Save \(current.profile.displayName) for Call of Duty 2 on Windows. Copy it into that player profile’s folder."
        panel.prompt = "Export"
        // MARK: - ship-integrate: keep Export attached to the loadout's window.
        guard let window = launcherWindow ?? NSApp.keyWindow else {
            notice = "Open the launcher window, then choose Export Loadout again."
            return
        }
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.studio.binds.exportWindows(to: url)
        }
    }
    /// Loadout › Delete Loadout… and the row's context menu: the Delete Loadout alert. Never the
    /// loadout in use; the folder goes to the Trash, so it can be put back (Undo, or Finder).
    func requestDeleteLoadout(_ id: String? = nil) {
        guard canNavigate, let id = id ?? studio.current?.profile.id, studio.deleteBlock(id) == nil,
              let profile = studio.profiles.first(where: { $0.id.caseInsensitiveCompare(id) == .orderedSame }) else { return }
        showConfigs()
        // The question belongs to the main window, even when the menu was used over Settings.
        confirmation = deleteConfirmation(profile)
    }
    /// The Delete Loadout alert (AllSheets board), with the board's “can't be undone” replaced by
    /// where it goes: the Trash, so it can be put back.
    func deleteConfirmation(_ profile: ConfigProfile) -> ConfirmationRequest {
        ConfirmationRequest(title: "Delete the loadout “\(profile.displayName)”?",
                            message: "Its settings, binds and history go with it. Your other loadouts aren’t affected. You can put it back from the Trash.",
                            actionTitle: "Delete Loadout") { [weak self] in
            guard let self else { return }
            self.studio.moveToTrash(profile.id, undo: self.launcherWindow?.undoManager)
        }
    }
    /// More Actions and the row's context menu: the loadout's config in Finder.
    func showLoadoutInFinder(_ id: String) {
        guard let home = studio.session?.home, (try? ConfigProfileStore.validateIdentifier(id)) != nil else { return }
        NSWorkspace.shared.activateFileViewerSelecting([home.appendingPathComponent("main/players/\(id)/config_mp.cfg")])
    }
    /// Loadout › Run Check-up, Library › Matches and Home's Last match: Check-up for the loadout on
    /// screen, measured from `match` (a `MatchSummary.id`) when given, otherwise the latest match.
    func openCheckup(match: String? = nil) {
        guard canNavigate else { return }
        showConfigs()
        studio.tab = .checkup
        studio.checkup.load()
        studio.checkup.show(match: match)
    }
    /// Loadout › Show History.
    func showHistory() { guard canNavigate else { return }; showConfigs(); studio.tab = .history }

    /// View › Jump to… (⌘K) on Configs: the page's own Jump to Any Setting. True when it showed.
    func jumpToSetting() -> Bool {
        guard page == .configs, discover.page == nil, studio.session != nil, studio.current != nil, studio.catalog != nil else { return false }
        studio.jumpPresented = true
        return true
    }
    /// Edit › Find (⌘F) on Configs: the search of what's showing (Discover's player search, Binds'
    /// action search, or Expert's setting search).
    func findInConfigs() {
        if discover.page != nil { discover.requestFind(); return }
        guard studio.current != nil else { return }
        if studio.tab == .binds { studio.binds.layout = .list } else { studio.tab = .settings; studio.level = .expert }
        // After the switch above has put the search field on screen.
        DispatchQueue.main.async { [studio] in studio.requestFind() }
    }

    /// Brings the main window forward on Configs, out of Discover, with nothing else in the way.
    private func showConfigs() {
        if launcherWindow?.isVisible != true || settingsInFront { showMainWindow() }
        navigate(to: .configs)
        discover.landsOnHome = false
        discover.page = nil
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewConfigs(_ screen: String) -> Bool {
        switch screen {
        // The review harness attaches a session over a temporary home with synthetic loadouts.
        case "configs-guided", "configs-guided-rules", "configs-discover-hidden", "configs-delete-confirm":
            page = .configs; studio.level = .guided; studio.group = .frame
        case "configs-expert-rules": page = .configs; studio.level = .expert; studio.group = .net
        case "configs-guided-aim", "configs-running": page = .configs; studio.level = .guided; studio.group = .aim
        // Graphics, with Display resolution listing the preview display's sizes.
        case "configs-guided-picture": page = .configs; studio.level = .guided; studio.group = .gfx; refreshDisplay()
        case "configs-expert": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-text": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-history": page = .configs; studio.tab = .history
        case "configs-other-loadout": page = .configs; studio.level = .guided; studio.group = .frame
        case "configs-new-loadout-sheet": page = .configs; studio.level = .guided; studio.group = .frame; sheet = .newLoadout
        case "configs-rename-sheet": page = .configs; studio.level = .guided; studio.group = .frame; sheet = .renameLoadout
        // Discover and import: the harness supplies a signed fixture directory or a reviewed import.
        case "discover-unavailable", "discover-players", "configs-home", "configs-home-end", "configs-home-all", "configs-home-search", "configs-home-nomatch": page = .configs; discover.page = .home
        case "configs-checker-empty", "configs-checker", "configs-checker-fixed", "configs-checker-clean": page = .configs; discover.page = .checker
        case "discover-saved": page = .configs; discover.page = .saved
        case "discover-player": page = .configs
        case "configs-import", "configs-import-everything": page = .configs
        case "servers-ready", "servers-ready-fixed": page = .servers
        case "configs-binds-list", "configs-binds-visual", "configs-binds-advanced": page = .configs; studio.tab = .binds
        case "configs-checkup", "configs-checkup-match": page = .configs; studio.tab = .checkup
        // Share Your Setup (ConfigShareModel.swift).
        default: return previewShare(screen)
        }
        return true
    }
}
