import AppKit
import SwiftUI

// Review harness, owner ship-settings. Settings window screens (names starting “settings” or “about” render it) and
// the settings part of the page actions.

extension Snapshots {
    /// Settings window screens: every pane (SettingsWin, SetDisplay, SetGameData, SetPrivacy, SetUpdates, SetAdvanced,
    /// SetAbout), their states, this unit's sheets (captured as -sheet.png) and alerts. Light variants come with every run.
    static let settingsScreens = [
        "settings", "settings-running", "settings-original", "settings-original-missing",
        "settings-display", "settings-display-scale", "settings-display-opengl", "settings-display-windowed", "settings-display-restored",
        "settings-game-data", "settings-game-data-missing", "settings-game-data-building", "settings-set-up-again",
        "settings-cod2x-installed", "settings-cod2x-downloading", "settings-cod2x-error", "settings-sheet-replace-key",
        "settings-privacy", "settings-privacy-sent", "settings-reports", "settings-delete-summaries",
        "settings-updates", "settings-updates-never", "settings-updates-checking", "settings-updates-offline",
        "settings-update", "settings-update-installing", "settings-update-error", "settings-sheet-whats-new",
        "settings-advanced", "settings-reset",
        "about", "about-footer", "about-update", "settings-sheet-report", "settings-sheet-report-server", "settings-sheet-acknowledgement",
    ]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let settingsPageScreens = ["settings-invalid", "settings-save-error", "settings-legacy", "settings-legacy-custom", "settings-legacy-display-adjusted"]
    static let settingsAccessibilityScreens = ["settings", "settings-display", "about-footer", "settings-game-data", "settings-privacy", "settings-updates", "settings-advanced", "about"]
    static let settingsLabels: [String: [String]] = [
        "settings-invalid": ["Extra launch options", "Your last valid options still apply"],
        "settings-legacy": ["125 frames per second", "Custom frame cap", "Game volume"],
        "settings-legacy-custom": ["Custom frame cap, 0 to 1000 frames per second", "Game volume"],
        "settings-save-error": ["Problem: Your changes couldn’t be saved", "Try Again"],
        "settings": ["General", "Display", "Game Data", "Privacy", "Updates", "Advanced", "About", "Game version",
                     "Hide CoD2 Unbound while the game is running", "Appearance", "Refresh the server list when Servers opens", "Open Configs"],
        "settings-legacy-display-adjusted": ["Resolution changed from 6016 × 3384 to 5120 × 2880", "Larger sizes are sharper and slower"],
        "settings-display": ["Preview: the game fills the display", "Mode", "Resolution, vertical sync and picture quality belong to each loadout", "Open Configs", "Restore Defaults"],
        "settings-display-scale": ["Preview: the game fills the display", "Graphics", "Render scale", "Draws the 3D picture with fewer pixels, then scales it up to fit.",
                                   "The game has no control for these four"],
        "settings-display-opengl": ["Graphics", "Render scale", "Needs Metal. OpenGL always draws the 3D picture at full size.", "The game has no control for these four"],
        "settings-game-data": ["Show in Finder", "Change…", "Replace…", "Rebuild", "Set Up Again…", "Game versions installed"],
        "settings-privacy": ["Send crash reports to CoD2 Unbound", "See What’s Sent…", "Send CoD2x crash reports to the CoD2x project",
                             "Match summaries", "Delete Summaries…", "Master servers"],
        "settings-updates": ["CoD2 Unbound \(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "") is up to date", "Check Now", "Check for updates automatically", "Release notes for every version"],
        "settings-advanced": ["Extra launch options, one per line", "Show in Finder", "Copy Last Game Log", "Reset Warnings", "Reset Settings…"],
        "about-footer": ["Call of Duty is a registered trademark"],
        "about": ["CoD2 Unbound", "Copy Version Info", "Release Notes", "Website", "Report a Problem…", "Acknowledgements", "Check for Updates…"],
    ]
}

/// Page actions, settings part: auto-save to JSON and the game config, invalid advanced text, +set lines, flush on
/// quit, Reset Settings, Delete Summaries, the last game log, and the Settings window taking each pane's height.
@MainActor enum SettingsChecks {
    static func pageActions(_ model: LauncherModel) async throws -> Bool {
        // MARK: - ship-play: preserve pre-launcher choices until a named loadout takes ownership.
        let legacyHome = model.home.appendingPathComponent("legacy-display-fixture")
        defer { try? FileManager.default.removeItem(at: legacyHome) }
        let legacyMain = legacyHome.appendingPathComponent("main")
        try FileManager.default.createDirectory(at: legacyMain, withIntermediateDirectories: true)
        try "seta r_mode \"1280x720\"\nseta r_monitor \"2\"\n".write(to: legacyMain.appendingPathComponent("config_mp.cfg"), atomically: true, encoding: .utf8)
        let legacy = ConfigDocument.launchLoadout(home: legacyHome, profileID: nil)
        guard legacy?.effectiveSetting(named: "r_mode") == "1280x720", legacy?.effectiveSetting(named: "r_monitor") == "2" else { return false }
        let active = legacyMain.appendingPathComponent("players/Main")
        try FileManager.default.createDirectory(at: active, withIntermediateDirectories: true)
        try "Main".write(to: active.deletingLastPathComponent().appendingPathComponent("active.txt"), atomically: true, encoding: .utf8)
        try "seta r_mode \"2560x1440\"\n".write(to: active.appendingPathComponent("config_mp.cfg"), atomically: true, encoding: .utf8)
        guard ConfigDocument.launchLoadout(home: legacyHome, profileID: nil)?.effectiveSetting(named: "r_mode") == "2560x1440" else { return false }
        print("PASS: legacy resolution and monitor survive import; the selected named loadout takes precedence")
        let saved = model.home.appendingPathComponent("launcher-settings.json")
        let cfg = model.home.appendingPathComponent("main/launcher.cfg")
        model.settings.fullscreen = "windowed"
        try await Task.sleep(for: .milliseconds(500))
        let settings = try JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved))
        guard settings.fullscreen == "windowed", try String(contentsOf: cfg, encoding: .utf8).contains("seta r_fullscreen \"0\"") else {
            print("FAIL: auto-save did not persist the same value to JSON and game config"); return false
        }
        let validJSON = try Data(contentsOf: saved), validConfig = try Data(contentsOf: cfg)
        model.editAdvancedOptions("name invalid;quit")
        try await Task.sleep(for: .milliseconds(350))
        guard model.advancedError != nil, model.settings.advanced.isEmpty,
              try Data(contentsOf: saved) == validJSON, try Data(contentsOf: cfg) == validConfig else {
            print("FAIL: invalid advanced text replaced the last valid settings"); return false
        }
        model.editAdvancedOptions("net_port 29980\n+set com_hunkMegs 384")
        try await Task.sleep(for: .milliseconds(350))
        let config = try String(contentsOf: cfg, encoding: .utf8)
        guard model.advancedError == nil, config.contains("seta net_port \"29980\""), config.contains("seta com_hunkMegs \"384\""),
              model.settings.advanced == "net_port 29980\n+set com_hunkMegs 384" else {
            print("FAIL: extra launch options were not saved as typed, or a +set line was refused"); return false
        }
        // Quitting before the debounce fires still persists the new setting.
        model.settings.vsync = false
        NotificationCenter.default.post(name: NSApplication.willTerminateNotification, object: NSApp)
        guard try !JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved)).vsync else { return false }
        print("PASS: debounced settings save to JSON and game config; invalid text keeps both last-valid files; +set lines saved as typed; termination flushes pending changes")

        // MARK: - ship-play: hot-plug/monitor change normalizes the same saved and visible choice.
        let oldDisplay = model.mainDisplay
        model.mainDisplay = MainDisplay(name: "5K fixture", nativeWidth: 5120, nativeHeight: 2880, modes: [.init(3008,1692), .init(6016,3384)])
        model.settings.resolution = "6016x3384"; model.normalizeResolution(); model.flushSettings()
        guard model.settings.resolution == "5120x2880", model.resolutionAdjustment?.contains("5120 × 2880") == true,
              !model.resolutionOptions.contains(where: { $0.value == "6016x3384" }),
              try JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved)).resolution == "5120x2880" else {
            print("FAIL: changed display did not normalize the saved, shown and launch resolution together"); return false
        }
        model.mainDisplay = MainDisplay(name: "1080p fixture", nativeWidth: 1920, nativeHeight: 1080)
        model.normalizeResolution(); model.flushSettings()
        guard model.settings.resolution == "1920x1080" else { return false }
        model.mainDisplay = oldDisplay; model.refreshDisplay(); model.resolutionAdjustment = nil
        print("PASS: display change clamps and persists the effective resolution, removes oversized choices and explains the change")

        // Reset Settings: everything in the window goes back; the player's files stay.
        let demo = model.home.appendingPathComponent("main/demos/kept.dm_1")
        try FileManager.default.createDirectory(at: demo.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data("demo".utf8).write(to: demo)
        model.settings.hideWhilePlaying = false; model.settings.refreshServersOnOpen = true
        // MARK: - ship-integrate: Settings choices drive the shell and server join flow.
        guard !model.hidesLauncherWhilePlaying, model.refreshServersOnOpen else {
            print("FAIL: shell or server browser ignored the Settings choice"); return false
        }
        model.alwaysUseServerVersion = true
        model.flushSettings()
        guard model.isWarningDismissed(DismissibleWarning.switchVersion),
              try JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved)).dismissedWarnings.contains(DismissibleWarning.switchVersion) else {
            print("FAIL: the server version choice was not saved with Settings"); return false
        }
        model.resetWarnings()
        guard !model.alwaysUseServerVersion else { print("FAIL: Reset Warnings did not restore the version question"); return false }
        model.dismissWarning(DismissibleWarning.switchVersion)
        print("PASS: hide during play, refresh on opening Servers and Reset Warnings share the Settings choices")
        model.resetSettings()
        model.flushSettings()
        let reset = try JSONDecoder().decode(GameSettings.self, from: Data(contentsOf: saved))
        guard reset == model.defaultSettings, reset.hideWhilePlaying, !reset.refreshServersOnOpen, reset.dismissedWarnings.isEmpty, reset.advanced.isEmpty,
              FileManager.default.fileExists(atPath: demo.path), model.settingsFeedback?.pane == .advanced else {
            print("FAIL: Reset Settings did not restore the defaults, or touched the player's files"); return false
        }
        // The library checks that follow count the demos folder.
        try FileManager.default.removeItem(at: demo)
        print("PASS: Reset Settings restores every launcher setting and keeps the player's files")

        // Delete Summaries removes the local file and Check-up's count.
        let summaries = model.home.appendingPathComponent("main/stats/matches.jsonl")
        try FileManager.default.createDirectory(at: summaries.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data("{}\n".utf8).write(to: summaries)
        model.deleteSummaries()
        guard !FileManager.default.fileExists(atPath: summaries.path), model.matchSummaryCount == 0, model.settingsFeedback?.failed == false else {
            print("FAIL: Delete Summaries left the match summaries"); return false
        }
        print("PASS: Delete Summaries removes the local match summaries")

        // The last game log is the newest file the game wrote (the clipboard is never touched here).
        let log = model.home.appendingPathComponent("main/console_mp.log")
        try Data("----- Console log -----\n".utf8).write(to: log)
        guard model.lastGameLog()?.standardizedFileURL == log.standardizedFileURL else { print("FAIL: the last game log was not found"); return false }
        try FileManager.default.removeItem(at: log)
        let session = try GameSessionLog(home: model.home)
        try session.pipe.fileHandleForWriting.write(contentsOf: Data("Game started\n".utf8)); session.finish()
        guard model.lastGameLog()?.standardizedFileURL == session.url.standardizedFileURL else { print("FAIL: retained session not used by Logs"); return false }
        try FileManager.default.removeItem(at: session.url)
        print("PASS: the last game log is the game's newest log")

        // In the app, the Settings window takes each pane's height.
        model.settingsPane = .general
        let window = await Snapshots.show(model: model, dark: true, glass: true, screen: "settings")
        defer { window.orderOut(nil); window.close() }
        var heights: [SettingsPane: CGFloat] = [:]
        // The harness sizes the window itself while showing it, so each pane is opened after that.
        for pane in [SettingsPane.about, .general] {
            model.settingsPane = pane
            try await Task.sleep(for: .milliseconds(900))
            heights[pane] = window.contentLayoutRect.height
        }
        guard let general = heights[.general], let about = heights[.about], general < SettingsWindowMetrics.height, about > general + 40 else {
            print("FAIL: the Settings window did not fit its pane (General \(heights[.general] ?? 0), About \(heights[.about] ?? 0))"); return false
        }
        model.settingsPane = .general
        print("PASS: the Settings window fits each pane (General \(Int(general)) pt, About \(Int(about)) pt)")
        return true
    }
}
