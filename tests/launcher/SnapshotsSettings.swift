import AppKit
import SwiftUI

// Review harness, owner ship-settings. Settings window screens (names starting “settings” or “about” render it) and
// the settings part of the page actions.

extension Snapshots {
    /// Settings window screens, one pane each plus the states the old Settings and About pages had.
    static let settingsScreens = ["settings", "settings-display", "settings-game-data", "settings-privacy", "settings-updates", "settings-advanced", "about",
                                  "settings-original", "settings-original-missing", "settings-cod2x-installed", "settings-cod2x-downloading",
                                  "settings-cod2x-error", "settings-reports", "settings-update", "settings-update-installing", "settings-update-error", "about-update"]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let settingsPageScreens = ["settings-invalid", "settings-save-error", "settings-legacy", "settings-legacy-custom"]
    static let settingsAccessibilityScreens = ["settings", "settings-game-data", "about"]
    static let settingsLabels: [String: [String]] = [
        "settings-invalid": ["Extra launch options", "Your last valid options still apply"],
        "settings-legacy": ["125 frames per second", "Custom frame cap", "Game volume"],
        "settings-legacy-custom": ["Custom frame cap, 0 to 1000 frames per second", "Game volume"],
        "settings": ["General", "Display", "Game Data", "Privacy", "Updates", "Advanced", "About"],
        "settings-game-data": ["Show game data in Finder", "Set Up Again", "Original shaders verified"],
        "about": ["Credits", "Licence", "Full credits"],
    ]
}

/// Page actions, settings part: auto-save to JSON and the game config, invalid advanced text, flush on quit.
@MainActor enum SettingsChecks {
    static func pageActions(_ model: LauncherModel) async throws -> Bool {
        let saved = model.home.appendingPathComponent("launcher-settings.json")
        let cfg = model.home.appendingPathComponent("main/launcher.cfg")
        model.settings.vsync = true
        try await Task.sleep(for: .milliseconds(500))
        let settings = try JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved))
        guard settings.vsync, try String(contentsOf: cfg, encoding: .utf8).contains("seta r_swapInterval \"1\"") else {
            print("FAIL: auto-save did not persist the same value to JSON and game config"); return false
        }
        let validJSON = try Data(contentsOf: saved), validConfig = try Data(contentsOf: cfg)
        model.editAdvancedOptions("name invalid;quit")
        try await Task.sleep(for: .milliseconds(350))
        guard model.advancedError != nil, model.settings.advanced.isEmpty,
              try Data(contentsOf: saved) == validJSON, try Data(contentsOf: cfg) == validConfig else {
            print("FAIL: invalid advanced text replaced the last valid settings"); return false
        }
        model.editAdvancedOptions("net_port 29980")
        try await Task.sleep(for: .milliseconds(350))
        guard model.advancedError == nil,
              try String(contentsOf: cfg, encoding: .utf8).contains("seta net_port \"29980\"") else { return false }
        // Quitting before the debounce fires still persists the new setting.
        model.settings.vsync = false
        NotificationCenter.default.post(name: NSApplication.willTerminateNotification, object: NSApp)
        guard try !JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved)).vsync else { return false }
        print("PASS: debounced settings save to JSON and game config; invalid text keeps both last-valid files; termination flushes pending changes")
        return true
    }
}
