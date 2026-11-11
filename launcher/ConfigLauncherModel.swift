import AppKit
import SwiftUI

// Owner: ship-configs. The Loadout menu, Configs' entry points other units call, and Config Studio's
// harness states. Stored state lives in `studio` (ConfigStudioModel) and `discover`.
@MainActor extension LauncherModel {
    // MARK: Menu commands (Loadout menu, File › Import and Paste)
    /// File › Import Config… (⌘O).
    func importConfig() { guard canImport else { return }; showMainWindow(); navigate(to: .configs); discover.chooseFile() }
    /// File › Paste Config (⇧⌘V).
    func pasteConfig() { guard canImport else { return }; showMainWindow(); navigate(to: .configs); discover.importPasteboard() }
    /// Loadout › Use for Next Game, and Home's loadout chip.
    func useLoadoutForNextGame(_ id: String) { studio.select(id); studio.useForNextGame() }
    /// The loadout the next game starts with (nil before loadouts exist).
    var nextGameLoadout: ConfigProfile? { studio.activeProfile }
    /// Loadout › Run Check-up, and Library › Matches / Home's Last match (`match` selects it once ship-configs adds that).
    func openCheckup(match: String? = nil) { navigate(to: .configs); studio.tab = .checkup }
    /// Loadout › Show History.
    func showHistory() { navigate(to: .configs); studio.tab = .history }
    /// Loadout › New Loadout… (⌘N): ship-configs moves its sheet behind this.
    func newLoadout() {}
    /// Loadout › Duplicate (⇧⌘D): ship-configs.
    func duplicateLoadout() {}
    /// Loadout › Rename…: ship-configs.
    func renameLoadout() {}
    /// File › Export Loadout… (⇧⌘E): ship-configs.
    func exportLoadout() {}
    /// Loadout › Delete Loadout…, behind its alert: ship-configs adds deletion.
    func requestDeleteLoadout() {}

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewConfigs(_ screen: String) -> Bool {
        switch screen {
        // The review harness attaches a session over a temporary home with synthetic loadouts.
        case "configs-guided", "configs-guided-rules": page = .configs; studio.level = .guided; studio.group = .frame
        case "configs-expert-rules": page = .configs; studio.level = .expert; studio.group = .net
        case "configs-guided-aim", "configs-running": page = .configs; studio.level = .guided; studio.group = .aim
        case "configs-expert": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-text": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-history": page = .configs; studio.tab = .history
        // Discover and import: the harness supplies a signed fixture directory or a reviewed import.
        case "discover-unavailable", "discover-players": page = .configs; discover.page = .players
        case "discover-saved": page = .configs; discover.page = .saved
        case "discover-player": page = .configs
        case "configs-import", "configs-import-everything": page = .configs
        case "servers-ready", "servers-ready-fixed": page = .servers
        case "configs-binds-list", "configs-binds-visual", "configs-binds-advanced": page = .configs; studio.tab = .binds
        case "configs-checkup": page = .configs; studio.tab = .checkup
        default: return false
        }
        return true
    }
}
