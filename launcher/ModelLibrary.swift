import AppKit
import SwiftUI

// Owner: ship-home-library. Home and Library: what Home shows about the next game, and demos and screenshots on disk.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.
@MainActor extension LauncherModel {
    var dispatches: [Dispatch] { library.dispatches.isEmpty ? Dispatch.bundled : Array(library.dispatches.prefix(2)) }
    var nextGameFrameCap: (value: Int?, name: String?) {
        guard usesLoadouts else { return (settings.fps, nil) }
        guard let session = studio.session ?? configStudio else { return (nil, nil) }
        do {
            let connection = serverConnection.address == lastServer?.address
            let profile = try (connection ? serverConnection.profileID : nil).map(session.readProfile) ?? session.readActive()
            guard let profile else { return (nil, nil) }
            let raw = (connection ? serverConnection.overrides["com_maxfps"] : nil) ?? profile.document.effectiveSetting(named: "com_maxfps")
            return (raw.flatMap(Int.init), profile.profile.displayName)
        } catch { return (nil, nil) }
    }
    func openFrameSettings() {
        guard usesLoadouts else { openSettings(.display); return }
        page = .configs
        studio.loadCatalogNow()
        let target = (serverConnection.address == lastServer?.address ? serverConnection.profileID : nil) ?? studio.activeProfile?.id
        if let target { studio.select(target) }
        studio.tab = .settings; studio.group = .frame; studio.search = ""; studio.filter = nil
    }
    func loadMedia() {
        guard !snapshot else { return }
        let folder = home.appendingPathComponent(libraryTab == .demos ? "main/demos" : "main/screenshots")
        let extensions = libraryTab == .demos ? ["dm_1"] : ["jpg", "jpeg", "png", "tga"]
        let keys: [URLResourceKey] = [.contentModificationDateKey, .fileSizeKey, .isRegularFileKey]
        do {
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
            media = try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: keys)
            .filter { extensions.contains($0.pathExtension.lowercased()) && (try? $0.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true }
            .map { url -> MediaEntry in
                let values = try? url.resourceValues(forKeys: Set(keys))
                return MediaEntry(url: url, facts: MediaFacts(name: url.lastPathComponent, date: values?.contentModificationDate, bytes: values?.fileSize))
            }
            .sorted {
                let a = $0.facts.modifiedAt ?? .distantPast, b = $1.facts.modifiedAt ?? .distantPast
                return a == b ? $0.url.lastPathComponent < $1.url.lastPathComponent : a > b
            }
            mediaMessage = nil; mediaError = false
            if !media.contains(where: { $0.url == selectedMedia }) { selectedMedia = nil }
            loadArtwork()
        } catch { mediaError = true; mediaMessage = "Your library couldn’t be read. Check the folder in Finder, then try again." }
    }
    func mediaFileExists(_ url: URL) -> Bool {
        guard !snapshot else { return true }
        guard FileManager.default.fileExists(atPath: url.path) else {
            mediaError = true; mediaMessage = "“\(url.lastPathComponent)” is no longer in this folder. Refresh your library to see what’s available."
            return false
        }
        return true
    }
    func openMedia(_ entry: MediaEntry) {
        guard mediaFileExists(entry.url), !snapshot else { return }
        if !NSWorkspace.shared.open(entry.url) { mediaError = true; mediaMessage = "This screenshot couldn’t be opened. Show it in Finder to choose an app." }
    }
    func revealMedia(_ entry: MediaEntry) {
        guard mediaFileExists(entry.url), !snapshot else { return }
        NSWorkspace.shared.activateFileViewerSelecting([entry.url])
    }
    func copyMedia(_ entry: MediaEntry) {
        guard mediaFileExists(entry.url), !snapshot else { return }
        guard let image = NSImage(contentsOf: entry.url) else { mediaError = true; mediaMessage = "This screenshot couldn’t be copied. Show it in Finder and try again."; return }
        NSPasteboard.general.clearContents(); NSPasteboard.general.writeObjects([image, entry.url as NSURL])
        mediaError = false; mediaMessage = "Screenshot copied."; studio.announce("Screenshot copied.")
    }
    func renameMedia(_ entry: MediaEntry, to name: String) -> String? {
        let name = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.hasPrefix(".") else { return "Choose a name that doesn’t start with a dot, so the recording stays visible." }
        guard !name.isEmpty, ![".", ".."].contains(name), !name.contains("/"), !name.contains(":"),
              !name.unicodeScalars.contains(where: { $0.value < 32 }) else { return "Enter a name without slashes or colons." }
        let target = entry.url.deletingLastPathComponent().appendingPathComponent(name).appendingPathExtension(entry.url.pathExtension)
        if target == entry.url { return nil }
        guard !FileManager.default.fileExists(atPath: target.path) else { return "A recording already has that name. Choose another." }
        guard !snapshot else { return nil }
        do {
            try FileManager.default.moveItem(at: entry.url, to: target)
            selectedMedia = target; loadMedia(); studio.announce("Recording renamed.")
            return nil
        } catch { return "This recording couldn’t be renamed. Check that it’s still in the folder and try again." }
    }
    func trashMedia(_ entry: MediaEntry) {
        guard !snapshot else { return }
        do {
            try FileManager.default.trashItem(at: entry.url, resultingItemURL: nil)
            loadMedia(); mediaMessage = "Moved to Trash. You can put it back using Finder."; studio.announce("Moved to Trash.")
        } catch { mediaError = true; mediaMessage = "This file couldn’t be moved to Trash. Show it in Finder and try again." }
    }
    func openMediaFolder() {
        guard !snapshot else { return }
        let folder = home.appendingPathComponent(libraryTab == .demos ? "main/demos" : "main/screenshots")
        do { try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true); NSWorkspace.shared.open(folder) }
        catch { mediaError = true; mediaMessage = "The folder couldn’t be opened. Check that your CoD2 Silicon folder is writable." }
    }

    // MARK: Confirmations this unit owns
    /// Library: Move to Trash…, behind its alert (recoverable from the Trash).
    func requestTrash(_ entry: MediaEntry) {
        confirm(ConfirmationRequest(title: "Move “\(entry.url.lastPathComponent)” to Trash?",
                                    message: "You can restore this file from the Trash in Finder.", actionTitle: "Move to Trash") { [weak self] in
            self?.trashMedia(entry)
        })
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewHomeLibrary(_ screen: String) -> Bool {
        switch screen {
        case "home-checking": updateWasChecked = true; checkingUpdates = true; updateMessage = "Checking GitHub Releases…"
        case "home-current": updateWasChecked = true; updateMessage = "You’re up to date · \(version)"
        case "home-update-error": updateWasChecked = true; updateError = true; updateMessage = "Updates couldn’t be checked. Check your internet connection and try again."
        case "library-empty": page = .library; media = []
        case "library-empty-screenshots": page = .library; libraryTab = .screenshots; media = []
        case "library-error": page = .library; media = []; mediaError = true; mediaMessage = "Your library couldn’t be read. Check the folder in Finder, then try again."
        case "library-reloaded":
            page = .library
            let name = "new-match.dm_1"
            media.insert(MediaEntry(url: home.appendingPathComponent("main/demos/" + name), facts: MediaFacts(name: name, date: Date(), bytes: 540_000)), at: 0)
            selectedMedia = media.first?.url
        case "library-renaming", "library-rename-error": page = .library; renamingMedia = media.first
        case "library-trash": page = .library; if let entry = media.first { requestTrash(entry) }
        case "library-trashed": page = .library; media = []; mediaMessage = "Moved to Trash. You can put it back using Finder."
        case "library-running": page = .library; gameRunning = true; mediaError = true; mediaMessage = "Close the game before playing a recording."
        case "library-missing": page = .library; mediaError = true; mediaMessage = "This recording is no longer in the folder. Refresh your library to see what’s available."
        case "home-update": page = .home; previewUpdateOffer()
        case "home-crash-sent":
            crashReport = home.appendingPathComponent("cod2_crash_4242.txt")
            notice = "The game closed unexpectedly. Your launcher is still open. " + CrashReporting.Status.sent.message
        case "home-crash-not-sent":
            crashReport = home.appendingPathComponent("cod2_crash_4242.txt")
            notice = "The game closed unexpectedly. Your launcher is still open. " + CrashReporting.Status.notSent(.offline).message
        case "library-demos": page = .library
        case "library-screenshots": page = .library; libraryTab = .screenshots
            media = ["toujane-dusk.jpg", "carentan-rooftops.jpg", "leningrad-snow.jpg"].enumerated().map { index, name in
                MediaEntry(url: home.appendingPathComponent("main/screenshots/" + name),
                           facts: MediaFacts(name: name, date: Calendar(identifier: .gregorian).date(from: DateComponents(year: 2026, month: 10, day: 4 - min(index, 1), hour: 21 - index * 2, minute: 17 + index * 13)), bytes: 412_000))
            }
        case "home-first": library.recent = []
        default: return false
        }
        return true
    }
}
