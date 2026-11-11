import AppKit
import SwiftUI

// Review harness, owner ship-home-library. Home and Library screens and the Library part of the page actions.

extension Snapshots {
    static let homeLibraryScreens = ["home", "home-first", "home-attention", "home-running", "home-crash", "home-matches-off",
                                     "library-matches", "library-demos", "library-screenshots"]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let homeLibraryPageScreens = ["home-update", "home-update-installing", "home-crash-sent", "home-crash-not-sent",
                                         "library-matches-empty", "library-matches-off", "library-matches-error", "library-demos-selected",
                                         "library-screenshots-selected", "library-empty", "library-empty-screenshots", "library-reloaded",
                                         "library-error", "library-renaming", "library-rename-error", "library-trash", "library-running", "library-missing",
                                         "library-trashed", "home-loadout", "home-loadout-unreadable"]
    static let homeLibraryAccessibilityScreens = ["home", "home-first", "home-attention", "home-running", "home-matches-off",
                                                  "library-matches", "library-demos", "library-screenshots"]
    static let homeLibraryLabels: [String: [String]] = [
        "home": ["Launch on Northern Lights | Public", "Find a Server", "Jump back in", "All Recent", "Launch on Sunday Rifles · EU",
                 "Last match", "All Matches", "Connection Steady", "Open Check-up", "Dispatches", "Release Notes"],
        "home-first": ["Find a Server", "Open Game Menu", "NOTHING PLAYED YET", "NOT MEASURED YET"],
        "home-attention": ["Approximate shaders. Build the original shaders"],
        "home-running": ["Return to Game", "Quit Game"],
        "home-matches-off": ["MATCH SUMMARIES ARE OFF", "Turn On in Settings…"],
        "home-update": ["What’s New", "Install Update"],
        "home-update-installing": ["Downloading version 0.4.0"],
        "home-loadout": ["Main loadout, 333 frames per second cap"],
        "home-loadout-unreadable": ["Frame cap unavailable"],
        "library-matches": ["Toujane, Search & Destroy", "Check-up for Toujane · Search & Destroy", "Kept on this Mac", "Delete Summaries…"],
        "library-matches-empty": ["NO MATCHES YET", "Find a Server"],
        "library-matches-off": ["MATCH SUMMARIES ARE OFF", "Turn On in Settings…"],
        "library-matches-error": ["MATCHES UNAVAILABLE", "Try Again"],
        "library-demos": ["Demo toujane_tdm_1110_2114.dm_1", "Play toujane_tdm_1110_2114.dm_1", "More actions for toujane_tdm_1110_2114.dm_1", "Show in Finder"],
        "library-screenshots": ["Screenshot, Toujane", "Show in Finder"],
        "library-empty": ["NO RECORDINGS", "Step 1", "Show Demos Folder"],
        "library-empty-screenshots": ["NO SCREENSHOTS", "Show Screenshots Folder"],
        "library-renaming": ["Rename recording", "Recording name", "Cancel", "Rename"],
        "library-rename-error": ["Enter a name without slashes or colons", "Cancel", "Rename"],
        "library-trash": ["Move to Trash", "Cancel"],
        "library-trashed": ["Moved “toujane_tdm_1110_2114.dm_1” to the Trash", "Undo"],
    ]
}

/// Page actions, Library part: real media files, activation refresh, rename, playback failures, Command-Delete,
/// Move to Trash and Undo, stored match summaries, Quick Look.
@MainActor enum HomeLibraryChecks {
    static func pageActions(_ model: LauncherModel, window: NSWindow) async throws -> Bool {
        let demos = model.home.appendingPathComponent("main/demos")
        try FileManager.default.createDirectory(at: demos, withIntermediateDirectories: true)
        let older = demos.appendingPathComponent("z-older.dm_1"), newer = demos.appendingPathComponent("a-newer.dm_1")
        try Data("older".utf8).write(to: older); try Data("newer".utf8).write(to: newer)
        try FileManager.default.setAttributes([.modificationDate: Date(timeIntervalSince1970: 100)], ofItemAtPath: older.path)
        try FileManager.default.setAttributes([.modificationDate: Date(timeIntervalSince1970: 200)], ofItemAtPath: newer.path)
        model.libraryTab = .demos; model.loadMedia()
        guard model.media.map(\.url) == [newer, older] else { print("FAIL: media is not newest first"); return false }
        model.page = .library
        try await Task.sleep(for: .milliseconds(400))
        let latest = demos.appendingPathComponent("latest.dm_1")
        try Data("returned match".utf8).write(to: latest)
        NotificationCenter.default.post(name: NSApplication.didBecomeActiveNotification, object: NSApp)
        try await Task.sleep(for: .milliseconds(300))
        guard model.media.first?.url == latest else { print("FAIL: returning to the app did not refresh media"); return false }
        guard let entry = model.media.first, model.renameMedia(entry, to: "renamed-match") == nil,
              model.media.first?.url.lastPathComponent == "renamed-match.dm_1" else { print("FAIL: rename did not update disk and library"); return false }
        guard model.renameMedia(model.media[0], to: "../escape") != nil else { return false }
        guard model.renameMedia(model.media[0], to: ".hidden") != nil else { return false }
        model.gameRunning = true; model.play(demo: model.media[0].url)
        guard model.mediaMessage?.contains("Close the game") == true else { return false }
        model.gameRunning = false
        let gone = model.media[0].url
        try FileManager.default.removeItem(at: gone)
        model.play(demo: gone)
        guard model.mediaMessage?.contains("no longer") == true else { return false }
        print("PASS: real media files sort by date, appear on app activation, rename safely, and missing/running-game playback failures stay in Library")

        // Command-Delete asks about the selected demo only; with nothing selected it does nothing.
        model.loadMedia()
        try await Task.sleep(for: .milliseconds(300))
        guard let target = model.media.first,
              let row = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix("Demo " + target.url.lastPathComponent) }),
              AccessibilityTree.press(row) else { print("FAIL: a demo row has no accessibility action to select it"); return false }
        try await Task.sleep(for: .milliseconds(200))
        guard model.selectedMedia == target.url else { print("FAIL: pressing a demo row did not select it"); return false }
        ConfigsPreview.press(window, keyCode: 51, characters: "\u{7f}", modifiers: .command)
        try await Task.sleep(for: .milliseconds(300))
        guard model.confirmation?.title == "Move “\(target.url.lastPathComponent)” to the Trash?" else {
            print("FAIL: Command-Delete did not ask to move the selected demo to the Trash (asked: \(model.confirmation?.title ?? "nothing"))"); return false
        }
        model.confirmation = nil
        try await Task.sleep(for: .milliseconds(300))
        model.selectedMedia = nil
        ConfigsPreview.press(window, keyCode: 51, characters: "\u{7f}", modifiers: .command)
        try await Task.sleep(for: .milliseconds(300))
        guard model.confirmation == nil else { print("FAIL: Command-Delete with nothing selected asked: \(model.confirmation?.title ?? "")"); model.confirmation = nil; return false }
        print("PASS: Command-Delete asks to move the selected demo to the Trash, and does nothing without a selection")

        // Move to Trash, then Undo puts the file back where it was.
        let trashed = model.media[model.media.count - 1]
        model.trashMedia(trashed)
        let inTrash = model.trashedMedia?.trashed
        defer { if let inTrash, FileManager.default.fileExists(atPath: inTrash.path) { try? FileManager.default.removeItem(at: inTrash) } }
        guard inTrash != nil, !FileManager.default.fileExists(atPath: trashed.url.path), !model.media.contains(where: { $0.url == trashed.url }),
              model.mediaMessage == "Moved “\(trashed.url.lastPathComponent)” to the Trash." else { print("FAIL: Move to Trash did not move the file and say so"); return false }
        model.undoTrash()
        guard FileManager.default.fileExists(atPath: trashed.url.path), model.media.contains(where: { $0.url == trashed.url }), model.selectedMedia == trashed.url,
              model.trashedMedia == nil else { print("FAIL: Undo did not put the demo back"); return false }
        print("PASS: Move to Trash moves the demo and says so; Undo puts it back and selects it")

        // Stored match summaries: Matches lists them newest first.
        let stats = model.home.appendingPathComponent("main/stats")
        try FileManager.default.createDirectory(at: stats, withIntermediateDirectories: true)
        func line(_ ended: String, _ map: String, _ mode: String, _ lost: Int) -> String {
            #"{"schemaVersion":1,"endedAt":"\#(ended)","endReason":"disconnect","map":"\#(map)","mode":"\#(mode)","durationSeconds":600,"serverAddress":"198.51.100.7:28960","ruleHints":{},"frameCount":1000,"frameTimeP50MS":4,"frameTimeP99MS":8,"frameTimeMaxMS":20,"frameTimeHistogramLimitMS":250,"capSampleFrames":0,"capFPS":250,"pingAverageMS":30,"pingMaximumMS":50,"pingSamples":10,"packetsReceived":\#(1000 - lost),"packetsLost":\#(lost),"packetLossPercent":\#(Double(lost) / 10),"packetLossSource":"snapshot-sequence","snapshotsReceived":\#(1000 - lost)}"#
        }
        try Data((line("2026-11-09T20:00:00Z", "mp_carentan", "tdm", 20) + "\n" + line("2026-11-10T21:00:00Z", "mp_toujane", "sd", 0) + "\n").utf8)
            .write(to: stats.appendingPathComponent("matches.jsonl"))
        model.libraryTab = .matches
        try await Task.sleep(for: .milliseconds(500))
        let rows = AccessibilityTree.elements(of: window).map(\.label)
        guard model.storedMatches.count == 2,
              let toujane = rows.firstIndex(where: { $0.hasPrefix("Toujane, Search & Destroy") && $0.hasSuffix("connection Steady") }),
              let carentan = rows.firstIndex(where: { $0.hasPrefix("Carentan, Team Deathmatch") && $0.hasSuffix("connection Some dips") }), toujane < carentan else {
            print("FAIL: Matches did not list the stored summaries newest first with their connection (\(model.storedMatches.count) read)"); return false
        }
        print("PASS: Matches lists the stored summaries newest first: frame rate, ping and the connection in a word")

        let screenshots = model.home.appendingPathComponent("main/screenshots")
        try FileManager.default.createDirectory(at: screenshots, withIntermediateDirectories: true)
        let picture = NSImage(size: NSSize(width: 64, height: 64))
        picture.lockFocus(); NSColor.systemOrange.setFill(); NSRect(x: 0, y: 0, width: 64, height: 64).fill(); picture.unlockFocus()
        guard let tiff = picture.tiffRepresentation, let bitmap = NSBitmapImageRep(data: tiff),
              let png = bitmap.representation(using: .png, properties: [:]) else { return false }
        try png.write(to: screenshots.appendingPathComponent("review-one.png"))
        try png.write(to: screenshots.appendingPathComponent("review-two.png"))
        model.libraryTab = .screenshots
        try await Task.sleep(for: .milliseconds(500))
        let first = "Screenshot, " + model.media[0].url.deletingPathExtension().lastPathComponent
        guard let tile = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix(first) }),
              AccessibilityTree.press(tile) else { print("FAIL: screenshot selection has no accessibility action"); return false }
        try await Task.sleep(for: .milliseconds(200))
        ConfigsPreview.pressArrow(window, keyCode: 124)
        try await Task.sleep(for: .milliseconds(200))
        guard model.selectedMedia == model.media[1].url else {
            print("FAIL: the screenshot Right arrow did not move selection (selected: \(model.selectedMedia?.lastPathComponent ?? "none"); responder: \(String(describing: window.firstResponder)))")
            return false
        }
        ConfigsPreview.press(window, keyCode: 49, characters: " ")
        try await Task.sleep(for: .milliseconds(600))
        guard let panel = NSApp.keyWindow, panel !== window, panel.isVisible,
              String(describing: type(of: panel)).contains("QLPreviewPanel") else { print("FAIL: Space did not open native Quick Look"); return false }
        ConfigsPreview.press(panel, keyCode: 53, characters: "\u{1b}")
        try await Task.sleep(for: .milliseconds(300))
        guard NSApp.keyWindow === window else { print("FAIL: Escape did not return from Quick Look"); return false }
        print("PASS: accessible screenshot selection accepts arrow keys; Space opens the native Quick Look panel and Escape returns")
        return true
    }
}
