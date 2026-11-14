import AppKit
import SwiftUI

// Owner: ship-home-library. Home and Library: what Home shows about the last and the next game, the
// match summaries Library lists, and the demos and screenshots on disk.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.

/// A loadout in Home's loadout menu, with the frame cap it sets.
struct LoadoutChoice: Identifiable, Equatable {
    let id: String
    let name: String
    let cap: Int?
    let inUse: Bool
    /// The loadout's own resolution ("2560x1440"); nil until its first game gives it one.
    var resolution: String? = nil
}

/// Something that keeps the next game from running as it should, with the place that fixes it.
enum HomeAttention: String, CaseIterable, Identifiable {
    case data, key, shaders
    var id: String { rawValue }
    var title: String {
        switch self { case .data: "Game data not found"; case .key: "No CD key"; case .shaders: "Approximate shaders" }
    }
    /// What VoiceOver says the chip does.
    var fix: String {
        switch self {
        case .data: "Choose your game folder again"
        case .key: "Enter your CD key"
        case .shaders: "Build the original shaders"
        }
    }
}

extension GameServer {
    /// The map and mode the server reported; nil for an address only ever joined (never seen in a list).
    var knownMap: String? { fields["mapname"] }
    var knownMode: String? { fields["g_gametype"] ?? fields["gametype"] }
}

/// One stored match summary as Home and Library show it: typical frame rate, ping and the connection in a word.
struct MatchFacts: Identifiable {
    enum Connection { case steady, dips, notMeasured
        var words: String { switch self { case .steady: "Steady"; case .dips: "Some dips"; case .notMeasured: "Not measured" } }
        var kind: StatusKind { switch self { case .steady: .ready; case .dips: .needsChange; case .notMeasured: .notMeasured } }
    }
    /// Missing server updates from this share on read as dips (the game infers them from gaps in the update sequence).
    static let dipShare = 1.0
    let id: String
    let map: String
    let mode: String
    let ended: Date?
    let duration: String
    let fps: Int?
    let ping: Int?
    let connection: Connection

    init(_ summary: MatchSummary, index: Int) {
        id = "\(summary.endedAt)#\(index)"
        map = summary.map; mode = summary.mode
        ended = Self.date(summary.endedAt)
        duration = Self.duration(summary.durationSeconds)
        // Half the frames took at most the median frame time: the frame rate the match typically ran at.
        fps = summary.frameTimeP50MS.flatMap { $0 > 0 ? Int((1000 / $0).rounded()) : nil }
        ping = summary.pingSamples > 0 ? summary.pingAverageMS.map { Int($0.rounded()) } : nil
        if summary.packetsReceived > 0, let loss = summary.packetLossPercent { connection = loss < Self.dipShare ? .steady : .dips }
        else { connection = .notMeasured }
    }
    var title: String { "\(MapCatalog.displayName(map)) · \(GameModes.long(mode))" }
    /// “YESTERDAY 21:40 · 21 MIN” in Library, “YESTERDAY · 21 MIN” on Home.
    func stamp(time: Bool) -> String {
        let when = ended.map { time ? PlayedDate.dayAndTime($0) : PlayedDate.day($0) }
        return ([when, duration.uppercased()].compactMap { $0 }).joined(separator: " · ")
    }
    var spoken: String {
        let when = ended.map(PlayedDate.spoken) ?? "Date not recorded"
        let frames = fps.map { "\($0) frames a second typical" } ?? "frame rate not measured"
        let milliseconds = ping.map { "\($0) milliseconds ping" } ?? "ping not measured"
        return "\(MapCatalog.displayName(map)), \(GameModes.long(mode)), \(when), \(duration), \(frames), \(milliseconds), connection \(connection.words)"
    }
    /// “21 min”, “1 h 5 min”, “40 s” (as Check-up writes it), without trapping on any stored value.
    static func duration(_ seconds: Double) -> String {
        let total = seconds.isFinite && seconds > 0 ? UInt64(min(seconds.rounded(), 0x1p63)) : 0
        if total < 60 { return "\(total) s" }
        let minutes = total / 60
        return minutes < 60 ? "\(minutes) min" : "\(minutes / 60) h \(minutes % 60) min"
    }
    static func date(_ text: String) -> Date? {
        let formatter = ISO8601DateFormatter()
        if let date = formatter.date(from: text) { return date }
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter.date(from: text)
    }
}

/// Dates as Home and Library write them: typewriter stamps (“YESTERDAY”, “08 NOV 21:14”) and sentences.
enum PlayedDate {
    static func day(_ date: Date, now: Date = Date(), calendar: Calendar = .current) -> String {
        if calendar.isDate(date, inSameDayAs: now) { return "TODAY" }
        if let yesterday = calendar.date(byAdding: .day, value: -1, to: now), calendar.isDate(date, inSameDayAs: yesterday) { return "YESTERDAY" }
        return StencilDate.day(date, calendar: calendar)
    }
    static func dayAndTime(_ date: Date, now: Date = Date(), calendar: Calendar = .current) -> String {
        day(date, now: now, calendar: calendar) + " " + time(date, calendar: calendar)
    }
    /// “Played yesterday at 21:14”.
    static func sentence(_ date: Date, now: Date = Date(), calendar: Calendar = .current) -> String {
        let clock = time(date, calendar: calendar)
        switch day(date, now: now, calendar: calendar) {
        case "TODAY": return "Played today at \(clock)"
        case "YESTERDAY": return "Played yesterday at \(clock)"
        default:
            let parts = calendar.dateComponents([.day, .month], from: date)
            let month = calendar.standaloneMonthSymbols[((parts.month ?? 1) - 1 + 12) % 12]
            return "Played on \(parts.day ?? 1) \(month) at \(clock)"
        }
    }
    static func spoken(_ date: Date) -> String {
        sentence(date).replacingOccurrences(of: "Played ", with: "")
    }
    private static func time(_ date: Date, calendar: Calendar) -> String {
        let parts = calendar.dateComponents([.hour, .minute], from: date)
        return String(format: "%02d:%02d", parts.hour ?? 0, parts.minute ?? 0)
    }
}

@MainActor extension LauncherModel {
    // MARK: Home: hero, chips and cards

    var dispatches: [Dispatch] { dispatches(rows: 3) }
    /// What’s New in `rows` lines. A card too short for three drops what comes next first, then older lines.
    func dispatches(rows: Int) -> [Dispatch] {
        HomeDispatches.list(version: version, releases: library.dispatches, news: library.news, next: rows >= 3 ? library.newsNext : nil, rows: rows)
    }
    /// Every line the launcher has, for the What’s New dialog.
    var allDispatches: [Dispatch] { dispatches(rows: 12) }
    /// Home’s What’s New card (See All) and its chip at a narrow window.
    func showNews() { present(.news) }
    /// The button beside a line in the dialog: where it leads.
    func openTitle(_ dispatch: Dispatch) -> String {
        switch dispatch.open {
        case "configs": "Open Configs"
        case "servers": "Open Servers"
        case "library": "Open Library"
        case "updates": "Open Updates"
        default: dispatch.version != nil ? "Open Updates" : "Read More"
        }
    }
    var dispatchesFoot: String { lastUpdateCheck.map { "Checked " + SettingsText.when($0) + "." } ?? "News appears after the first update check." }
    /// Whether a What’s New line leads anywhere: a place in the app, the Updates pane for a release, or a link.
    func canOpen(_ dispatch: Dispatch) -> Bool { dispatch.open != nil || dispatch.version != nil || dispatch.link != nil }
    func open(_ dispatch: Dispatch) {
        switch dispatch.open {
        case "configs": navigate(to: .configs)
        case "servers": navigate(to: .servers)
        case "library": navigate(to: .library)
        case "updates": openSettings(.updates)
        default:
            if dispatch.version != nil { openSettings(.updates) }
            else if let link = dispatch.link.flatMap(URL.init(string:)) { NSWorkspace.shared.open(link) }
        }
    }

    /// The server behind a recent address: what the launcher last saw of it, or for one it never listed
    /// (joined by address), a bare entry with the map and mode of its newest match summary, if any.
    func recentServer(_ address: String) -> GameServer {
        if let known = knownServer((try? LaunchLink.canonicalAddress(address)) ?? address) { return known }
        var fields: [String: String] = [:]
        let wanted = (try? LaunchLink.canonicalAddress(address)) ?? address
        if let match = storedMatches.last(where: { $0.serverAddress == address || (try? LaunchLink.canonicalAddress($0.serverAddress)) == wanted }) {
            fields["mapname"] = match.map; fields["g_gametype"] = match.mode
        }
        return GameServer(address: address, fields: fields, ping: 0, players: [])
    }
    /// Home's hero and Jump back in: the most recent joins, newest first, including servers joined by address.
    var homeRecent: [GameServer] { library.recent.prefix(3).map(recentServer) }
    var homeLastServer: GameServer? { library.recent.first.map(recentServer) }
    /// Whether the launcher has seen `server` in a server list (so its name, map and version are known).
    func isListed(_ server: GameServer) -> Bool { knownServer((try? LaunchLink.canonicalAddress(server.address)) ?? server.address) != nil }

    /// When `address` was last joined: the launcher's record, else the end of the newest match summary from it.
    func lastPlayed(_ address: String) -> Date? {
        if let date = library.played[address] { return date }
        let wanted = (try? LaunchLink.canonicalAddress(address)) ?? address
        return storedMatches.last { $0.serverAddress == address || (try? LaunchLink.canonicalAddress($0.serverAddress)) == wanted }
            .flatMap { MatchFacts.date($0.endedAt) }
    }
    /// The server the running game was sent to by this launcher (nil when it was started on its own menu).
    var serverInPlay: GameServer? {
        guard gameRunning, let server = homeLastServer, let date = library.played[server.address], date >= gameStartedAt.addingTimeInterval(-2) else { return nil }
        return server
    }
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
    /// The next game's resolution. It belongs to the loadout; one that has none yet starts from the
    /// size chosen for this display. `used` is set when the display can't show the loadout's size:
    /// the game then takes the largest it offers that fits inside.
    var nextGameResolution: (value: String, used: String?) {
        guard usesLoadouts, let session = studio.session ?? configStudio else { return (settings.resolution, nil) }
        let connection = serverConnection.address == lastServer?.address
        let profile = (try? (connection ? serverConnection.profileID : nil).map(session.readProfile) ?? session.readActive()) ?? nil
        guard let raw = profile?.document.effectiveSetting(named: "r_mode")?.trimmingCharacters(in: .whitespaces), DisplayResolution(raw) != nil else { return (settings.resolution, nil) }
        guard let sizes = mainDisplay?.resolutions, !sizes.isEmpty, !sizes.contains(where: { $0.value == raw }) else { return (raw, nil) }
        return (raw, DisplayResolutions.fitting(raw, in: sizes))
    }
    /// Home's loadout menu: every loadout with its frame cap and resolution, the one in use ticked. Empty before loadouts exist.
    var loadoutChoices: [LoadoutChoice] { usesLoadouts ? loadoutCards : [] }
    /// Every loadout with its frame cap and resolution, for Configs · Home.
    var loadoutCards: [LoadoutChoice] {
        guard let session = studio.session ?? configStudio else { return [] }
        return session.profiles.map { profile in
            let document = (try? session.readProfile(profile.id))?.document
            let cap = document?.effectiveSetting(named: "com_maxfps").flatMap { Int($0.trimmingCharacters(in: .whitespaces)) }
            let size = document?.effectiveSetting(named: "r_mode").map { $0.trimmingCharacters(in: .whitespaces) }.flatMap { DisplayResolution($0) != nil ? $0 : nil }
            return LoadoutChoice(id: profile.id, name: profile.displayName, cap: cap, inUse: profile.isActive, resolution: size)
        }
    }
    /// Choose…: the next game's loadout in Configs, open at Display resolution.
    func openResolutionSettings() {
        guard usesLoadouts else { openSettings(.display); return }
        openFrameSettings()
        guard page == .configs else { return }
        studio.group = .gfx; studio.search = "Display resolution"
    }
    /// Edit … in Configs: the next game's loadout, open at its frame settings (Settings › Display before loadouts exist).
    func openFrameSettings() {
        guard usesLoadouts else { openSettings(.display); return }
        navigate(to: .configs)
        guard page == .configs else { return }
        discover.landsOnHome = false; discover.page = nil
        studio.loadCatalogNow()
        let target = (serverConnection.address == lastServer?.address ? serverConnection.profileID : nil) ?? studio.activeProfile?.id
        if let target { studio.select(target) }
        studio.tab = .settings; studio.group = .frame; studio.search = ""; studio.filter = nil
    }
    /// The chips under the hero that something needs fixing, in the order they block a game.
    var homeAttention: [HomeAttention] {
        var items: [HomeAttention] = []
        if dataPath.isEmpty || dataMissing { items.append(.data) }
        if !keyVerified { items.append(.key) }
        if approximate { items.append(.shaders) }
        return items
    }
    /// Each attention chip goes straight to its fix: setup's game folder or shader step, or the Replace Key sheet.
    func fix(_ item: HomeAttention) {
        switch item {
        case .data: restartSetup(at: 0)
        case .key: presentReplaceKey()
        case .shaders: restartSetup(at: 2)
        }
    }
    /// Looks again for what Home's attention chips report: game data that went missing, a CD key that was removed.
    func refreshAttention() {
        guard !snapshot, !onboard, !gameRunning else { return }
        dataMissing = !dataPath.isEmpty && LauncherFindData(dataPath) == nil
        keyVerified = KeyStore.hasValidKey(at: preferences)
    }
    /// Home's Quit Game (and the Quit alert's Quit Both): asks the game to quit the way the Dock's Quit does,
    /// so it saves its own settings. The game monitor brings the launcher back when it has closed.
    func quitGame() {
        guard let pid = activeGamePID, !quittingGame else { return }
        quittingGame = true
        if NSRunningApplication(processIdentifier: pid)?.terminate() != true { engine?.terminate() }
        Task { [weak self] in
            for _ in 0..<40 {
                try? await Task.sleep(for: .milliseconds(250))
                guard let self else { return }
                if !self.gameRunning { self.quittingGame = false; return }
            }
            guard let self, self.gameRunning else { return }
            self.quittingGame = false
            self.notice = "The game didn’t quit. Switch to it and quit from its own menu."
        }
    }

    // MARK: Library: matches

    /// The game's local match summaries as Check-up last read them, oldest first.
    var storedMatches: [MatchSummary] { if case .read(let all) = studio.checkup.matchState { return all }; return [] }
    /// Reads the summaries again (Library opens, the window comes forward, the game quits).
    func refreshLibraryMatches() {
        guard !snapshot else { return }
        studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        studio.checkup.refreshMatches(home: home)
        loadLibraryArtwork()
    }

    // MARK: Library: demos and screenshots

    func libraryFolder(_ tab: LibraryTab) -> URL? {
        switch tab {
        case .matches: nil
        case .demos: home.appendingPathComponent("main/demos")
        case .screenshots: home.appendingPathComponent("main/screenshots")
        }
    }
    /// Library's current tab, read again from disk: Library opens, the window comes forward, the game quits.
    func reloadLibrary() {
        if libraryTab == .matches { refreshLibraryMatches() } else { loadMedia() }
    }
    func loadMedia() {
        guard !snapshot, let folder = libraryFolder(libraryTab) else { return }
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
            // A reload clears a failure; what an action reported (copied, moved to the Trash) stays.
            if mediaError { mediaMessage = nil; mediaError = false }
            if !media.contains(where: { $0.url == selectedMedia }) { selectedMedia = nil }
            loadLibraryArtwork()
        } catch { mediaError = true; mediaMessage = "Your library couldn’t be read. Check the folder in Finder, then try again." }
    }
    /// Loading screens for everything the launcher shows: servers, recordings and matches (one request, so
    /// none of them cancels the others).
    func loadLibraryArtwork() {
        guard !snapshot, !dataPath.isEmpty else { return }
        var maps = homeRecent.map(\.map) + recentServers.map(\.map) + visibleServers.prefix(32).map(\.map)
        if let selected { maps.append(selected.map) }
        maps += media.compactMap(\.facts.map) + storedMatches.suffix(24).map(\.map)
        let path = dataPath
        Task { await artwork.load(dataPath: path, maps: maps) }
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
    /// Screenshot › Copy Image: the picture, and the file for pasting in Finder.
    func copyMedia(_ entry: MediaEntry) {
        guard mediaFileExists(entry.url), !snapshot else { return }
        guard let image = NSImage(contentsOf: entry.url) else { mediaError = true; mediaMessage = "This screenshot couldn’t be copied. Show it in Finder and try again."; return }
        NSPasteboard.general.clearContents(); NSPasteboard.general.writeObjects([image, entry.url as NSURL])
        report("Screenshot copied.")
    }
    /// Demo › Copy: the file, for pasting in Finder or a message.
    func copyDemo(_ entry: MediaEntry) {
        guard mediaFileExists(entry.url), !snapshot else { return }
        NSPasteboard.general.clearContents(); NSPasteboard.general.writeObjects([entry.url as NSURL])
        report("Copied “\(entry.url.lastPathComponent)”. Paste it in Finder to make a copy.")
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
            selectedMedia = target; loadMedia(); report("Renamed to “\(target.lastPathComponent)”.")
            return nil
        } catch { return "This recording couldn’t be renamed. Check that it’s still in the folder and try again." }
    }
    func trashMedia(_ entry: MediaEntry) {
        guard !snapshot else { return }
        do {
            var resulting: NSURL?
            try FileManager.default.trashItem(at: entry.url, resultingItemURL: &resulting)
            trashedMedia = (resulting as URL?).map { (original: entry.url, trashed: $0) }
            loadMedia()
            report("Moved “\(entry.url.lastPathComponent)” to the Trash.")
        } catch { mediaError = true; mediaMessage = "This file couldn’t be moved to the Trash. Show it in Finder and try again." }
    }
    /// The banner's Undo after Move to Trash: puts the file back where it was.
    func undoTrash() {
        guard let item = trashedMedia, !snapshot else { return }
        trashedMedia = nil
        do {
            guard !FileManager.default.fileExists(atPath: item.original.path) else { throw CocoaError(.fileWriteFileExists) }
            try FileManager.default.moveItem(at: item.trashed, to: item.original)
            mediaMessage = nil; mediaError = false
            selectedMedia = item.original; loadMedia()
            studio.announce("Put back “\(item.original.lastPathComponent)”.")
        } catch {
            mediaError = true
            mediaMessage = "“\(item.original.lastPathComponent)” couldn’t be put back. Open the Trash in Finder and choose Put Back."
        }
    }
    func openMediaFolder() {
        guard !snapshot, let folder = libraryFolder(libraryTab) else { return }
        do { try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true); NSWorkspace.shared.open(folder) }
        catch { mediaError = true; mediaMessage = "The folder couldn’t be opened. Check that your CoD2 Silicon folder is writable." }
    }
    /// What an action did, in the banner at the top of the list and to VoiceOver.
    private func report(_ message: String) {
        mediaError = false; mediaMessage = message; studio.announce(message)
    }

    // MARK: Confirmations this unit owns
    /// Library: Move to Trash (⌘⌫), behind its alert; the file can be put back from the Trash.
    func requestTrash(_ entry: MediaEntry) {
        confirm(ConfirmationRequest(title: "Move “\(entry.url.lastPathComponent)” to the Trash?",
                                    message: "You can put it back from the Trash until you empty it.", actionTitle: "Move to Trash") { [weak self] in
            self?.trashMedia(entry)
        })
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewHomeLibrary(_ screen: String) -> Bool {
        guard screen.hasPrefix("home") || screen.hasPrefix("library") else { return false }
        previewPlayed()
        if screen != "home-news-first" { previewNews() }
        // Home learns the display when the app becomes active, which happens once per review run: a
        // later screen that needs the preview's display (the resolution it can't show) asks for it here.
        if screen == "home-loadout-resolution" { refreshDisplay() }
        switch screen {
        case "home-news-first": page = .home; previewMatches()
        case "home-news-long", "home-news-all":
            // Long news, as a release title and a paragraph from the network can be: the card shows what
            // fits, the dialog everything.
            page = .home; previewMatches()
            let long = "Hosting a server from the launcher, player setups in Configs, a config checker, a loadout’s own resolution and vertical sync, and What’s New from the network. Read the full notes for what was tested and what was not."
            library.dispatches = library.dispatches.map { var line = $0; line.title = "CoD2 Silicon \(version): hosting, player setups and the config checker"; line.summary = long; return line }
                + [Dispatch(stamp: "13 OCT", title: "CoD2 Silicon 0.2.1", summary: "Fixes the resolution on 6K displays and a freeze when the server list refreshes while a game starts.",
                            link: "https://github.com/STiXzoOR/cod2-silicon/releases", date: Date(timeIntervalSince1970: 1_791_849_600), version: "0.2.1")]
            library.news = library.news.map { var line = $0; line.summary = "New in Configs: fourteen setups from the 2008 and 2009 seasons, each credited to the page it was published on and reviewed line by line before anything is imported."; return line }
            if screen == "home-news-all" { present(.news) }
        case "home", "home-attention", "home-update", "home-crash", "home-crash-log", "home-crash-sent", "home-crash-not-sent", "home-running", "home-matches-off":
            page = .home; previewMatches()
            if screen == "home-attention" { approximate = true }
            if screen == "home-update" { previewUpdateOffer() }
            if screen == "home-running" { gameRunning = true; gameStartedAt = Date().addingTimeInterval(-12 * 60); library.played[library.recent[0]] = gameStartedAt.addingTimeInterval(20) }
            if screen == "home-matches-off" { settings.matchSummariesEnabled = false; studio.checkup.matchSummariesEnabled = false }
            if screen.hasPrefix("home-crash") {
                crashReport = home.appendingPathComponent(screen == "home-crash-log" ? "logs/game-2026-11-13T12-00-00Z-preview.log" : "cod2_crash_4242.txt")
                notice = "The game closed unexpectedly. Your settings and loadouts are safe."
                if screen == "home-crash-sent" { notice += " " + CrashReporting.Status.sent.message }
                if screen == "home-crash-not-sent" { notice += " " + CrashReporting.Status.notSent(.offline).message }
            }
        case "home-first": page = .home; library.recent = []; library.played = [:]
        case "home-address":
            // Last joined by address: a server no list has shown, with no match summary from it.
            page = .home; previewMatches()
            let address = "203.0.113.77:28960"
            library.played[address] = Date().addingTimeInterval(-50 * 60); library.recent.insert(address, at: 0)
        case "home-loadout", "home-loadout-unreadable": page = .home
        case "home-update-installing": page = .home; previewUpdateOffer(); installingUpdate = true; updateMessage = AppUpdate.Message.downloading("0.4.0")
        case "library-matches": page = .library; libraryTab = .matches; previewMatches()
        case "library-matches-empty": page = .library; libraryTab = .matches; previewMatches([])
        case "library-matches-off": page = .library; libraryTab = .matches; previewMatches([]); settings.matchSummariesEnabled = false; studio.checkup.matchSummariesEnabled = false
        case "library-matches-off-kept": page = .library; libraryTab = .matches; previewMatches(); settings.matchSummariesEnabled = false; studio.checkup.matchSummariesEnabled = false
        case "library-matches-error": page = .library; libraryTab = .matches
            studio.checkup.readMatches = { _ in throw CocoaError(.fileReadNoPermission) }; studio.checkup.refreshMatches(home: home)
        case "library-demos": page = .library; libraryTab = .demos
        case "library-demos-selected": page = .library; libraryTab = .demos; selectedMedia = media.first?.url
        case "library-empty": page = .library; libraryTab = .demos; media = []
        case "library-empty-screenshots": page = .library; libraryTab = .screenshots; media = []
        case "library-error": page = .library; libraryTab = .demos; media = []; mediaError = true; mediaMessage = "Your library couldn’t be read. Check the folder in Finder, then try again."
        case "library-reloaded":
            page = .library; libraryTab = .demos
            let name = "new-match.dm_1"
            media.insert(MediaEntry(url: home.appendingPathComponent("main/demos/" + name), facts: MediaFacts(name: name, date: Date(), bytes: 540_000)), at: 0)
            selectedMedia = media.first?.url
        case "library-renaming", "library-rename-error": page = .library; libraryTab = .demos; renamingMedia = media.first
        case "library-trash": page = .library; libraryTab = .demos; if let entry = media.first { requestTrash(entry) }
        case "library-trashed":
            page = .library; libraryTab = .demos
            let gone = media.removeFirst()
            trashedMedia = (original: gone.url, trashed: URL(fileURLWithPath: "/Preview/.Trash").appendingPathComponent(gone.url.lastPathComponent))
            mediaMessage = "Moved “\(gone.url.lastPathComponent)” to the Trash."
        case "library-running": page = .library; libraryTab = .demos; gameRunning = true; mediaError = true; mediaMessage = "Close the game before playing a recording."
        case "library-missing": page = .library; libraryTab = .demos; mediaError = true; mediaMessage = "This recording is no longer in the folder. Refresh your library to see what’s available."
        case "library-screenshots", "library-screenshots-selected":
            page = .library; libraryTab = .screenshots
            let calendar = Calendar(identifier: .gregorian)
            media = [("toujane_1110_2117.jpg", 10, 21, 17), ("carentan_1108_2231.jpg", 8, 22, 31), ("leningrad_1104_1944.jpg", 4, 19, 44),
                     ("matmata_1104_1902.jpg", 4, 19, 2), ("dawnville_1102_2015.jpg", 2, 20, 15), ("stalingrad_1101_1850.jpg", 1, 18, 50)].map { name, day, hour, minute in
                MediaEntry(url: home.appendingPathComponent("main/screenshots/" + name),
                           facts: MediaFacts(name: name, date: calendar.date(from: DateComponents(year: 2026, month: 11, day: day, hour: hour, minute: minute)), bytes: 412_000))
            }
            if screen == "library-screenshots-selected" { selectedMedia = media.first?.url }
        default: return false
        }
        return true
    }
    /// The three recent servers' join dates: yesterday evening, then two earlier nights.
    /// What’s New after a check: a note from the news file, this version’s release, and what comes next.
    private func previewNews() {
        let calendar = Calendar(identifier: .gregorian)
        func day(_ day: Int) -> Date { calendar.date(from: DateComponents(year: 2026, month: 11, day: day, hour: 12)) ?? Date() }
        library.dispatches = [Dispatch(stamp: StencilDate.day(day(18)), title: "CoD2 Silicon \(version)", summary: HomeDispatches.builtIn(version: "0.3.0").summary,
                                       link: "https://github.com/STiXzoOR/cod2-silicon/releases", date: day(18), version: version)]
        library.news = [Dispatch(stamp: StencilDate.day(day(21)), title: "14 more player setups", summary: "New in Configs: setups from the 2008 and 2009 seasons.", date: day(21), open: "configs")]
        library.newsNext = Dispatch(stamp: "NEXT", title: "A Metal renderer", summary: "In the works. The original look stays until it matches.",
                                    link: "https://github.com/STiXzoOR/cod2-silicon/blob/main/docs/ROADMAP.md")
        library.dispatchesCheckedAt = Calendar.current.date(bySettingHour: 14, minute: 2, second: 0, of: Date())
    }
    private func previewPlayed() {
        let calendar = Calendar(identifier: .gregorian)
        let yesterday = calendar.date(byAdding: .day, value: -1, to: Date()) ?? Date()
        let dates = [calendar.date(bySettingHour: 21, minute: 14, second: 0, of: yesterday) ?? yesterday,
                     calendar.date(from: DateComponents(year: 2026, month: 11, day: 8, hour: 21, minute: 30)) ?? yesterday,
                     calendar.date(from: DateComponents(year: 2026, month: 11, day: 6, hour: 19, minute: 2)) ?? yesterday]
        for (address, date) in zip(library.recent, dates) { library.played[address] = date }
        // The demos the core seeds, dated like the board's.
        media = [("toujane_tdm_1110_2114.dm_1", 10, 21, 14, 3_100_000), ("carentan_tdm_1108_2240.dm_1", 8, 22, 40, 4_800_000),
                 ("leningrad_tdm_1104_1918.dm_1", 4, 19, 18, 2_000_000)].map { name, day, hour, minute, bytes in
            MediaEntry(url: home.appendingPathComponent("main/demos/" + name),
                       facts: MediaFacts(name: name, date: calendar.date(from: DateComponents(year: 2026, month: 11, day: day, hour: hour, minute: minute)), bytes: bytes))
        }
    }
    /// Stored match summaries for the harness (invented, documentation-range server addresses), oldest first.
    private func previewMatches(_ rows: [MatchSummary]? = nil) {
        let calendar = Calendar(identifier: .gregorian)
        let yesterday = calendar.date(byAdding: .day, value: -1, to: Date()) ?? Date()
        func stamp(_ date: Date?) -> String { ISO8601DateFormatter().string(from: date ?? yesterday) }
        func summary(_ map: String, _ mode: String, _ ended: Date?, minutes: Double, fps: Double?, ping: Double?, loss: Double?, address: String) -> MatchSummary {
            let frames: UInt64 = fps == nil ? 0 : UInt64(minutes * 60 * 240)
            let p50 = fps.map { 1000 / $0 }
            let received: UInt64 = loss == nil ? 0 : UInt64(minutes * 60 * 20)
            let lost: UInt64 = loss.map { UInt64((Double(received) * $0 / (100 - $0)).rounded()) } ?? 0
            return MatchSummary(schemaVersion: 1, endedAt: stamp(ended), endReason: "disconnect", map: map, mode: mode, durationSeconds: minutes * 60,
                                serverAddress: address, ruleHints: [:], frameCount: frames, frameTimeP50MS: p50, frameTimeP99MS: p50.map { $0 * 2.2 },
                                frameTimeMaxMS: p50.map { $0 * 6 }, frameTimeHistogramLimitMS: 250, framesUnderCap: nil, capSampleFrames: 0, capFPS: 250,
                                pingAverageMS: ping, pingMaximumMS: ping.map { $0 * 1.6 }, pingSamples: ping == nil ? 0 : 400,
                                packetsReceived: received, packetsLost: lost,
                                packetLossPercent: loss == nil ? nil : 100 * Double(lost) / Double(received + lost), packetLossSource: loss == nil ? nil : "snapshot-sequence",
                                snapshotsReceived: received)
        }
        func date(_ day: Int, _ hour: Int, _ minute: Int) -> Date? { calendar.date(from: DateComponents(year: 2026, month: 11, day: day, hour: hour, minute: minute)) }
        let stored = rows ?? [
            summary("mp_leningrad", "tdm", date(4, 21, 14), minutes: 29, fps: 246, ping: 68, loss: nil, address: "192.0.2.10:28960"),
            summary("mp_stalingrad", "dm", date(6, 19, 18), minutes: 15, fps: 250, ping: 73, loss: 0.2, address: "198.51.100.88:28961"),
            summary("mp_matmata", "sd", date(8, 20, 2), minutes: 26, fps: 197, ping: 44, loss: 2.4, address: "203.0.113.90:28962"),
            summary("mp_carentan", "tdm", date(8, 22, 10), minutes: 34, fps: 250, ping: 31, loss: 0.1, address: "198.51.100.7:28960"),
            summary("mp_toujane", "sd", calendar.date(bySettingHour: 21, minute: 40, second: 0, of: yesterday), minutes: 21, fps: 248, ping: 38, loss: 0.3, address: "203.0.113.24:28960"),
        ]
        studio.checkup.readMatches = { _ in stored }
        studio.checkup.refreshMatches(home: home)
    }
}
