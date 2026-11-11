import AppKit
import SwiftUI

// Review harness, owner ship-home-library. Home and Library screens and the Library part of the page actions.

extension Snapshots {
    static let homeLibraryScreens = ["home", "home-first", "home-crash-sent", "home-crash-not-sent", "library-demos", "library-screenshots"]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let homeLibraryPageScreens = ["home-update", "home-checking", "home-current", "library-empty", "library-empty-screenshots", "library-reloaded",
                                         "library-error", "library-renaming", "library-rename-error", "library-trash", "library-running", "library-missing",
                                         "library-trashed", "home-update-error", "home-loadout", "home-loadout-unreadable"]
    static let homeLibraryAccessibilityScreens = ["home", "home-first", "library-demos", "library-screenshots"]
    static let homeLibraryLabels: [String: [String]] = [
        "home-first": ["Find a server", "Open game menu", "NOTHING PLAYED YET"],
        "home-update": ["Install Update", "View Release"],
        "home-checking": ["Checking GitHub Releases"],
        "home-current": ["You’re up to date"],
        "home-loadout": ["Frame cap 333 frames per second · Main loadout"],
        "home-loadout-unreadable": ["Frame cap unavailable"],
        "library-empty": ["Show Folder in Finder"],
        "library-empty-screenshots": ["Show Folder in Finder"],
        "library-renaming": ["Rename recording", "Recording name", "Cancel", "Rename"],
        "library-rename-error": ["Enter a name without slashes or colons", "Cancel", "Rename"],
        "library-trash": ["Move to Trash", "Cancel"],
        "library-trashed": ["Moved to Trash", "NO RECORDINGS", "Show Folder in Finder"],
    ]
}

/// Page actions, Library part: real media files, activation refresh, rename, playback failures, Quick Look.
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

        let screenshots = model.home.appendingPathComponent("main/screenshots")
        try FileManager.default.createDirectory(at: screenshots, withIntermediateDirectories: true)
        let picture = NSImage(size: NSSize(width: 64, height: 64))
        picture.lockFocus(); NSColor.systemOrange.setFill(); NSRect(x: 0, y: 0, width: 64, height: 64).fill(); picture.unlockFocus()
        guard let tiff = picture.tiffRepresentation, let bitmap = NSBitmapImageRep(data: tiff),
              let png = bitmap.representation(using: .png, properties: [:]) else { return false }
        try png.write(to: screenshots.appendingPathComponent("review-one.png"))
        try png.write(to: screenshots.appendingPathComponent("review-two.png"))
        model.libraryTab = .screenshots; model.loadMedia()
        try await Task.sleep(for: .milliseconds(500))
        guard let tile = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix(model.media[0].url.lastPathComponent) }),
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
