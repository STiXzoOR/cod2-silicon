import AppKit
import SwiftUI
import IOKit

// Owner: ship-settings. The Settings window's model: saving settings, appearance, Display's facts about this
// Mac, Game Data's folder, key and shaders, Privacy, Advanced, checking for and installing updates, About and
// Report a Problem. Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.

/// Which step of an update failed, so Updates says which and Try Again repeats it.
enum UpdateStage: Equatable, Sendable { case check, install }

// MARK: - ship-play
/// The display the game uses. CoreGraphics display order follows SDL's macOS enumeration.
struct MainDisplay: Equatable, Sendable {
    var name: String
    var nativeWidth: Int
    var nativeHeight: Int
    var modes: [DisplayResolution] = []
    var nativeKnown = true
    var native: DisplayResolution { .init(nativeWidth, nativeHeight) }
    var resolutions: [DisplayResolution] { DisplayResolutions.offered(native: native, modes: modes) }
    @MainActor static func current(monitor: Int = 0) -> MainDisplay? {
        var ids = [CGDirectDisplayID](repeating: 0, count: 32), count: UInt32 = 0
        guard CGGetActiveDisplayList(32, &ids, &count) == .success, count > 0 else { return nil }
        let index = monitor > 0 && monitor <= Int(count) ? monitor - 1 : 0
        let id = ids[index]
        let screen = NSScreen.screens.first { ($0.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.uint32Value == id }
        let options = [kCGDisplayShowDuplicateLowResolutionModes as String: true] as CFDictionary
        let modes = CGDisplayCopyAllDisplayModes(id, options) as? [CGDisplayMode] ?? []
        let rotated = [90, 270].contains(Int(CGDisplayRotation(id)))
        func size(_ width: Int, _ height: Int) -> DisplayResolution { rotated ? .init(height, width) : .init(width, height) }
        // No native flag: use desktop coordinates conservatively, never a scaled backing or largest mode.
        let native = modes.filter { $0.ioFlags & 0x0200_0000 != 0 }
            .map { size($0.pixelWidth, $0.pixelHeight) }.max()
        let desktop = CGDisplayBounds(id).size
        let limit = native ?? .init(Int(desktop.width), Int(desktop.height))
        return MainDisplay(name: screen?.localizedName ?? "Display \(index + 1)", nativeWidth: limit.width, nativeHeight: limit.height,
                           modes: modes.filter { $0.isUsableForDesktopGUI() }.map { size($0.width, $0.height) }, nativeKnown: native != nil)
    }
}

/// Game Data's facts, read when the pane opens (never on every redraw).
struct GameDataFacts: Equatable {
    var found = true
    /// The stored CD key's last four characters; nil when no valid key is stored.
    var keyEnding: String?
    /// Built from the player's game files (true), taken from a Mac copy (false), or unknown.
    var shadersFromGameFiles: Bool?
    var shadersBuilt: Date?
    var myFilesBytes: Int64?
}

/// The Mac this runs on, as Report a Problem and Copy Version Info show it.
enum MacFacts {
    /// “MacBook Pro (14-inch, 2024), Apple M4 Pro” from the device tree and the chip's name; the model identifier otherwise.
    static let model: String = {
        var parts: [String] = []
        let entry = IORegistryEntryFromPath(kIOMainPortDefault, "IODeviceTree:/product")
        if entry != 0 {
            if let data = IORegistryEntryCreateCFProperty(entry, "product-name" as CFString, kCFAllocatorDefault, 0)?.takeRetainedValue() as? Data {
                let name = String(decoding: data.prefix { $0 != 0 }, as: UTF8.self).trimmingCharacters(in: .whitespaces)
                if !name.isEmpty { parts.append(name) }
            }
            IOObjectRelease(entry)
        }
        if parts.isEmpty { parts.append(sysctl("hw.model") ?? "Mac") }
        if let chip = sysctl("machdep.cpu.brand_string"), !chip.isEmpty { parts.append(chip) }
        return parts.joined(separator: ", ")
    }()
    static var macOS: String {
        let os = ProcessInfo.processInfo.operatingSystemVersion
        return "\(os.majorVersion).\(os.minorVersion)" + (os.patchVersion > 0 ? ".\(os.patchVersion)" : "")
    }
    private static func sysctl(_ name: String) -> String? {
        var size = 0
        guard sysctlbyname(name, nil, &size, nil, 0) == 0, size > 1 else { return nil }
        var bytes = [CChar](repeating: 0, count: size)
        guard sysctlbyname(name, &bytes, &size, nil, 0) == 0 else { return nil }
        return String(decoding: bytes.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
    }
}

extension KeyStore {
    /// The stored key's last four characters, for Settings. The key itself is never shown or copied.
    static func lastFour(at path: URL) -> String? {
        guard let text = try? String(contentsOf: path, encoding: .utf8) else { return nil }
        for line in text.components(separatedBy: "\n") where line.hasPrefix("codkey=") {
            let key = normalize(String(line.dropFirst(7)))
            if valid(key) { return String(key.suffix(4)) }
        }
        return nil
    }
}

@MainActor extension LauncherModel {
    // MARK: Saving
    func scheduleSettingsSave() {
        settingsSaveTask?.cancel()
        settingsSaveTask = Task { [weak self] in
            do { try await Task.sleep(for: .milliseconds(250)) } catch { return }
            self?.saveSettings()
        }
        // Every change comes through here: appearance applies at once, and the daily check follows its switch.
        applyAppearance()
        if booted, settings.checkUpdatesAutomatically != (automaticUpdateTask != nil) { scheduleAutomaticUpdateCheck() }
    }
    func flushSettings() {
        settingsSaveTask?.cancel()
        if settings != savedSettings { saveSettings() }
    }
    /// Invalid text remains editable, while launches and saved settings keep the last valid options.
    func editAdvancedOptions(_ text: String) {
        advancedDraft = text
        var candidate = settings
        for (index, line) in text.components(separatedBy: "\n").enumerated() where !line.trimmingCharacters(in: .whitespaces).isEmpty {
            candidate.advanced = line
            guard (try? candidate.launchConfig(migrated: configStudio?.migrationCompleted == true)) != nil else {
                if GameSettings.advancedPair(line)?.0.lowercased() == "r_mode" { advancedError = usesLoadouts ? "The resolution belongs to each loadout. Change it in Configs." : "Choose the resolution in Settings › Display."; return }
                advancedError = "Line \(index + 1) can’t be used. Enter one setting and its value, without quotes or semicolons. Paths, passwords and your CD key are set by the app."
                return
            }
        }
        candidate.advanced = text
        guard (try? candidate.launchConfig(migrated: configStudio?.migrationCompleted == true)) != nil else {
            advancedError = "These options are too long. Remove some lines and try again."
            return
        }
        advancedError = nil
        settings = candidate
    }
    /// Lines that are valid but that the game takes from the loadout instead, once loadouts are in use.
    // MARK: - ship-integrate
    var advancedIgnoredLines: [Int] {
        guard usesLoadouts else { return [] }
        return settings.advanced.components(separatedBy: "\n").enumerated().compactMap { index, line in
            guard let name = GameSettings.advancedPair(line)?.0,
                  !GameSettings.machineDvarNames.contains(name.lowercased()) else { return nil }
            return index + 1
        }
    }
    func saveSettings() {
        do {
            let text = try settings.launchConfig(migrated: configStudio?.migrationCompleted == true)
            guard !snapshot else { savedSettings = settings; settingsSaveError = nil; return }
            try PrivateFile.write(Data(text.utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try PrivateFile.write(JSONEncoder().encode(settings), to: home.appendingPathComponent("launcher-settings.json"))
            savedSettings = settings; settingsSaveError = nil
            studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        } catch { settingsSaveError = "Your changes couldn’t be saved. Check that your CoD2 Unbound folder is writable, then try again." }
    }

    // MARK: Start-up (boot calls this once)
    /// Applies the saved appearance, reopens Settings on the pane it was left on, and starts the daily check.
    func startSettings() {
        onUpdateReady = { [weak self] version, installsOnQuit in
            self?.notifyUpdateReady(version: version, installsOnQuit: installsOnQuit)
        }
        if let raw = UserDefaults.standard.string(forKey: Self.settingsPaneKey), let pane = SettingsPane(rawValue: raw) { settingsPane = pane }
        applyAppearance()
        scheduleAutomaticUpdateCheck()
    }
    static let settingsPaneKey = "CoD2UnboundSettingsPane"
    /// The Settings window remembers its pane between launches.
    func rememberSettingsPane() {
        guard !snapshot else { return }
        UserDefaults.standard.set(settingsPane.rawValue, forKey: Self.settingsPaneKey)
    }
    /// General › Appearance on every window, alerts and panels included. The review harness sets its own.
    func applyAppearance() {
        guard !snapshot, appliedAppearance != settings.appearance else { return }
        appliedAppearance = settings.appearance
        switch settings.appearance {
        case .system: NSApp.appearance = nil
        case .light: NSApp.appearance = NSAppearance(named: .aqua)
        case .dark: NSApp.appearance = NSAppearance(named: .darkAqua)
        }
    }

    // MARK: General
    var engineOptions: [(value: GameEngine, title: String)] {
        [(.cod2x, CoD2xFiles.engineTitle), (.original, "Original 1.3")]
    }
    /// The board's pointer: personal settings live in loadouts.
    func openConfigs() { showMainWindow(); navigate(to: .configs) }

    // MARK: Display
    // MARK: - ship-play
    /// r_monitor is a loadout setting; an explicit fullscreen monitor follows that loadout.
    var gameMonitor: Int {
        guard (try? settings.dvars())?.last(where: { $0.0.lowercased() == "r_fullscreen" })?.1 != "0" else { return 0 }
        let loadout = ConfigDocument.launchLoadout(home: home, profileID: serverConnection.profileID)
        let advanced = settings.advanced.components(separatedBy: "\n").compactMap(GameSettings.advancedPair)
            .last { $0.0.lowercased() == "r_monitor" }?.1
        let value = usesLoadouts ? loadout?.effectiveSetting(named: "r_monitor") : advanced ?? loadout?.effectiveSetting(named: "r_monitor")
        return Int(value ?? "0") ?? 0
    }
    func refreshDisplay() {
        if snapshot {
            if mainDisplay == nil { mainDisplay = MainDisplay(name: "Built-in Liquid Retina XDR Display", nativeWidth: 3456, nativeHeight: 2234, modes: [.init(1728, 1117)]) }
        } else { mainDisplay = MainDisplay.current(monitor: gameMonitor) }
        normalizeResolution()
        // Config Studio's Display resolution control lists what this display offers.
        studio.setDisplayResolutions(mainDisplay?.resolutions.map(\.value) ?? [], native: mainDisplay?.nativeKnown == true ? mainDisplay?.native.value : nil,
                                     firstRun: settings.resolution)
        guard !snapshot, displayObserver == nil else { return }
        displayObserver = NotificationCenter.default.addObserver(forName: NSApplication.didChangeScreenParametersNotification, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { self?.refreshDisplay() }
        }
    }
    func normalizeResolution() {
        guard let display = mainDisplay else { return }
        let previous = settings.resolution
        let actual = DisplayResolutions.fitting(previous, in: display.resolutions)
        guard previous != actual else { return }
        settings.resolution = actual
        // With loadouts this is only the value a loadout without a resolution starts from: nothing to announce.
        guard !usesLoadouts else { return }
        resolutionAdjustment = "Resolution changed from \(SettingsText.resolution(previous)) to \(SettingsText.resolution(actual)) to fit this display."
    }
    var resolutionOptions: [(value: String, title: String)] {
        (mainDisplay?.resolutions ?? [.init(640, 480)]).map { ($0.value, resolutionTitle($0.value)) }
    }
    func resolutionTitle(_ value: String) -> String {
        SettingsText.resolution(value) + (mainDisplay?.nativeKnown == true && value == mainDisplay?.native.value ? " · Native" : "")
    }
    var resolutionHelp: String {
        let own = mainDisplay.map { $0.nativeKnown ? " This display’s own size is \(SettingsText.resolution($0.native.value)) pixels." : " macOS hasn’t reported this display’s native size." } ?? ""
        return "Larger sizes are sharper and slower; anti-aliasing multiplies the work." + own
    }
    static let displayModes: [(value: String, title: String)] = [("borderless", "Borderless"), ("exclusive", "Full Screen"), ("spaces", "macOS Full Screen"), ("windowed", "Window")]
    static func displayModeDetail(_ mode: String) -> String {
        switch mode {
        case "borderless": "Borderless fills the screen and switches apps instantly."
        case "spaces": "macOS full screen gives the game its own Space, which Game Mode needs."
        case "windowed": "A window at the chosen size, beside your other apps."
        default: "Full Screen shows the chosen size pixel for pixel; switching apps takes a moment. Borderless stretches it to the desktop."
        }
    }
    /// What a new install starts with (this app's own defaults, then the built-in ones).
    var defaultSettings: GameSettings {
        var defaults = GameSettings()
        if let display = mainDisplay { defaults.resolution = DisplayResolutions.initial(native: display.native) }
        if let mode = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultFullscreen") as? String { defaults.fullscreen = mode }
        return defaults
    }
    /// Display › Restore Defaults: everything the Display pane shows. Returns what it replaced, for Undo.
    @discardableResult func restoreDisplayDefaults() -> GameSettings {
        let previous = settings, defaults = defaultSettings
        resolutionAdjustment = nil
        var restored = settings
        restored.fullscreen = defaults.fullscreen
        if !usesLoadouts {
            restored.resolution = defaults.resolution; restored.vsync = defaults.vsync; restored.anisotropy = defaults.anisotropy
            restored.fps = defaults.fps; restored.rawMouse = defaults.rawMouse; restored.sensitivity = defaults.sensitivity
            restored.dpi = defaults.dpi; restored.volume = defaults.volume
        }
        settings = restored
        return previous
    }

    // MARK: Game Data
    /// Reads what Game Data shows: the folder, the key's last four, the shaders' source and date, the size of the player's files.
    func refreshGameData() {
        guard !snapshot else {
            // Filled once; a harness state's own changes stay.
            if gameData.myFilesBytes == nil {
                gameData = GameDataFacts(found: true, keyEnding: keyVerified ? "7K2D" : nil, shadersFromGameFiles: approximate ? nil : true,
                                         shadersBuilt: Self.previewDate, myFilesBytes: 214_000_000)
            }
            return
        }
        var facts = gameData
        facts.found = !dataPath.isEmpty && LauncherFindData(dataPath) != nil
        facts.keyEnding = KeyStore.lastFour(at: preferences)
        let manifest = home.appendingPathComponent("shaders/manifest.json")
        if !approximate, let data = try? Data(contentsOf: manifest), let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
            facts.shadersFromGameFiles = json["generator"] is String
            facts.shadersBuilt = try? manifest.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
        } else { facts.shadersFromGameFiles = nil; facts.shadersBuilt = nil }
        gameData = facts
        let folder = home
        Task {
            let bytes = await Task.detached(priority: .utility) { Self.folderSize(folder) }.value
            gameData.myFilesBytes = bytes
        }
    }
    nonisolated static func folderSize(_ folder: URL) -> Int64 {
        let keys: [URLResourceKey] = [.totalFileAllocatedSizeKey, .isRegularFileKey]
        guard let walker = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: keys) else { return 0 }
        var total: Int64 = 0
        for case let url as URL in walker {
            guard let values = try? url.resourceValues(forKeys: Set(keys)), values.isRegularFile == true else { continue }
            total += Int64(values.totalFileAllocatedSize ?? 0)
        }
        return total
    }
    static let previewDate = Calendar(identifier: .gregorian).date(from: DateComponents(year: 2026, month: 11, day: 11, hour: 14, minute: 2)) ?? Date()
    /// What the shaders row says.
    var shaderSummary: String {
        if shaderBusy { return "Building from your game files… \(shaderCount) of \(Self.shaderTotal)" }
        if approximate { return "Approximate. Rebuild to get the original lighting, sky and water." }
        let source = gameData.shadersFromGameFiles == false ? "Original, taken from your Mac copy of the game" : "Original, built on this Mac"
        return source + (gameData.shadersBuilt.map { " on " + Self.shortDay($0) } ?? "")
    }
    static func shortDay(_ date: Date) -> String {
        let day = DateFormatter()
        day.setLocalizedDateFormatFromTemplate(Calendar.current.isDate(date, equalTo: Date(), toGranularity: .year) ? "dMMM" : "dMMMyyyy")
        return day.string(from: date)
    }
    /// Game Data › Change…: a folder chosen in a sheet on the Settings window, checked before it's used.
    func changeGameData() {
        guard !snapshot, !gameRunning, !shaderBusy, let window = settingsWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Use Folder"
        panel.message = "Choose your Call of Duty 2 folder. Its main folder must contain all 16 game archives, iw_00.iwd through iw_15.iwd."
        if !dataPath.isEmpty { panel.directoryURL = URL(fileURLWithPath: dataPath).deletingLastPathComponent() }
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.useGameData(url)
        }
    }
    func useGameData(_ url: URL) {
        guard let valid = LauncherFindData(url.path) else {
            dataChangeError = "“\(url.lastPathComponent)” is incomplete, so the game still uses \(dataPathDisplay.isEmpty ? "no folder" : dataPathDisplay). Choose the folder whose main folder has all 16 game archives."
            return
        }
        let previous = dataPath
        dataPath = valid
        do { try rememberData() } catch { dataPath = previous; dataChangeError = error.localizedDescription; return }
        confirmedDataPath = valid; dataChangeError = nil
        refreshGameData()
        loadArtwork()
        if approximate { refreshShaders() }
    }
    /// Game Data › Rebuild: builds the shaders again from the game files. The builder keeps a current cache,
    /// so the old one is set aside first and put back if the new build fails.
    func rebuildShaders() {
        guard !snapshot, !gameRunning, !shaderBusy, shaderRefresh == nil, !dataPath.isEmpty else { return }
        let cache = home.appendingPathComponent("shaders"), aside = home.appendingPathComponent("shaders-previous")
        let files = FileManager.default
        try? files.removeItem(at: aside)
        let hadCache = files.fileExists(atPath: cache.path)
        if hadCache { do { try files.moveItem(at: cache, to: aside) } catch { shaderRebuildMessage = "The shaders couldn’t be rebuilt: their folder is in use. Try again."; return } }
        shaderRebuildMessage = nil
        prepareShaders()
        let task = setupTask
        Task {
            await task?.value
            if approximate, hadCache, !files.fileExists(atPath: cache.path), (try? files.moveItem(at: aside, to: cache)) != nil {
                approximate = !LauncherVerifyShaders()
                shaderRebuildMessage = "The rebuild didn’t finish, so your previous shaders are back."
            } else {
                try? files.removeItem(at: aside)
                shaderRebuildMessage = approximate ? "The original shaders couldn’t be built. The game uses approximate ones." : "Shaders rebuilt."
            }
            refreshGameData()
        }
    }
    /// Game Data › Set Up Again…, behind its alert.
    func requestSetUpAgain() {
        confirm(ConfirmationRequest(title: "Run setup again?",
                                    message: "You’ll go through game data, CD key and shaders. Your loadouts, demos and screenshots stay, and you can cancel at any step.",
                                    actionTitle: "Set Up Again", destructive: false) { [weak self] in self?.setUpAgain() })
    }
    var installedEngineNames: [String] {
        GameEngine.allCases.filter { installedEngines.contains($0) }.map { $0 == .cod2x ? CoD2xFiles.engineTitle : "Original 1.3" }
    }

    /// What the app home keeps of CoD2x's files now.
    func refreshCoD2xFiles() {
        guard !snapshot, !cod2xFilesBusy else { return }
        let status = CoD2xFiles.status(home: home, dataPath: dataPath)
        cod2xFilesInstalled = status.installed; cod2xFilesMessage = status.message
    }
    /// Only when the player asks: CoD2x's pinned release from GitHub, of which only the archive is kept.
    func downloadCoD2xFiles() {
        getCoD2xFiles(CoD2xFiles.Message.downloading) { home in
            let release = try await CoD2xFiles.download()
            try Task.checkCancellation()
            try CoD2xFiles.install(release: release, home: home)
        }
    }
    /// Without a download: a Windows game folder with CoD2x installed, read and never changed.
    func chooseCoD2xFolder() {
        guard !snapshot, !cod2xFilesBusy, let window = settingsWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Copy Files"
        panel.message = "Choose a Windows Call of Duty 2 folder that has CoD2x installed. It is only read, never changed."
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let folder = panel.url else { return }
            self?.getCoD2xFiles(CoD2xFiles.Message.copying) { home in try CoD2xFiles.install(folder: folder, home: home) }
        }
    }
    func getCoD2xFiles(_ progress: String, _ work: @escaping @Sendable (URL) async throws -> Void) {
        guard !snapshot, !cod2xFilesBusy else { return }
        cod2xFilesBusy = true; cod2xFilesMessage = progress
        let home = self.home
        cod2xFilesTask = Task {
            var failure: String?
            do { try await work(home) }
            catch { if !Task.isCancelled { failure = CoD2xFiles.message(error) } }
            cod2xFilesBusy = false
            refreshCoD2xFiles()
            if let failure { cod2xFilesMessage = failure }
            if Task.isCancelled { cod2xFilesMessage = "Download cancelled. You can try again whenever you’re ready." }
            studio.announce(cod2xFilesMessage)
        }
    }
    func cancelCoD2xFiles() { cod2xFilesTask?.cancel() }
    func removeCoD2xFiles() {
        guard !snapshot, !cod2xFilesBusy else { return }
        do { try CoD2xFiles.remove(home: home); refreshCoD2xFiles() } catch { cod2xFilesMessage = CoD2xFiles.message(error) }
    }

    // MARK: Privacy
    /// The fields a crash report really carries (CrashReporting.envelope), with this Mac's values.
    var crashReportFields: [(name: String, value: String)] {
        let facts = snapshot ? CrashReporting.Facts(release: version, macOS: "27.0", model: "Mac16,7") : CrashReporting.Facts.current(release: version)
        return [("What failed", "The report’s first reason line, such as the signal that stopped the game"),
                ("When", "The time the report was written"),
                ("CoD2 Unbound", facts.release),
                ("Game version", "CoD2x or Original 1.3, whichever crashed"),
                ("macOS", facts.macOS),
                ("Mac model", facts.model),
                ("The report", "The crash or freeze report the game wrote, with your Mac’s user name replaced by ~ and every command line removed")]
    }
    var matchSummaryCount: Int { studio.checkup.matchCount }
    func refreshMatchCount() { if !snapshot { studio.checkup.refreshMatches(home: home) } }
    /// Settings › Privacy and Library › Matches: Delete Summaries…, behind its alert.
    func requestDeleteSummaries() {
        guard !gameRunning else { notice = "Quit the game before deleting match summaries."; return }
        refreshMatchCount()
        confirm(deleteSummariesRequest(count: matchSummaryCount))
    }
    func deleteSummariesRequest(count: Int) -> ConfirmationRequest {
        ConfirmationRequest(title: count == 1 ? "Delete your match summary?" : count > 1 ? "Delete all \(count) match summaries?" : "Delete match summaries?",
                            message: "Check-up loses its history and starts again from your next match. This can’t be undone.",
                            actionTitle: "Delete Summaries") { [weak self] in self?.deleteSummaries() }
    }
    func deleteSummaries() {
        guard !snapshot, !gameRunning else { return }
        let file = home.appendingPathComponent("main/stats/matches.jsonl")
        do {
            if FileManager.default.fileExists(atPath: file.path) { try FileManager.default.removeItem(at: file) }
            settingsFeedback = .init(pane: .privacy, text: "Match summaries deleted.")
        } catch { settingsFeedback = .init(pane: .privacy, text: "The match summaries couldn’t be deleted. Check that your CoD2 Unbound folder is writable.", failed: true) }
        studio.checkup.refreshMatches(home: home)
        if !settingsInFront, let feedback = settingsFeedback { notice = feedback.text }
    }

    // MARK: Advanced
    /// The newest retained session, falling back to reports from older builds.
    func lastGameLog() -> URL? {
        if let session = GameSessionLog.latest(home: home) { return session }
        let files = [home.appendingPathComponent("main/console_mp.log")] + CrashReports.all(in: home, since: .distantPast)
        func modified(_ url: URL) -> Date? {
            guard let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .contentModificationDateKey]), values.isRegularFile == true else { return nil }
            return values.contentModificationDate
        }
        return files.compactMap { url in modified(url).map { (url, $0) } }.max { $0.1 < $1.1 }?.0
    }
    /// Copies the last game log, shortened to its last 256 KB, with user names and command lines taken out.
    /// Returns what happened, in words.
    @discardableResult func copyLastGameLog() -> (text: String, copied: Bool) {
        guard !snapshot else { return ("Copied the crash report from today at 14:02.", true) }
        guard let log = lastGameLog(), let shortened = Self.shortened(log) else {
            return ("There’s no game log yet. Launch the game once, then try again.", false)
        }
        guard let text = shortened.text else {
            return ("It wasn’t copied: it looks like it holds your CD key. Open it from Show in Finder to check it yourself.", false)
        }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        let modified = shortened.modified
        let kind = log.lastPathComponent.hasPrefix("game-") ? "the game log" : log.lastPathComponent.hasPrefix("console") ? "the game’s console log" : log.lastPathComponent.contains("freeze") ? "the freeze report" : "the crash report"
        return ("Copied \(kind) from \(SettingsText.when(modified)).", true)
    }
    /// A log's last 256 KB with user names and command lines taken out. `text` is nil when what is left
    /// still looks like it holds a CD key; the whole result is nil when the file can't be read.
    static func shortened(_ log: URL) -> (text: String?, modified: Date)? {
        guard let handle = try? FileHandle(forReadingFrom: log) else { return nil }
        defer { try? handle.close() }
        let size = (try? handle.seekToEnd()) ?? 0
        try? handle.seek(toOffset: size > 262_144 ? size - 262_144 : 0)
        let raw = String(decoding: (try? handle.readToEnd()) ?? Data(), as: UTF8.self)
        let modified = (try? log.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? Date()
        return ((try? CrashReporting.redact(raw)), modified)
    }
    /// The log of the hosted server that was started last, if any server here has been started.
    func lastServerLog() -> URL? { hosting.latestLog() }
    /// Report a Problem's logs: the last game log, the last hosted server's log, or both under a heading
    /// each, on the clipboard. Nothing is copied unless every chosen log can be.
    @discardableResult func copyReportLogs(game: Bool, server: Bool) -> (text: String, copied: Bool) {
        guard !snapshot else { return ("Copied.", true) }
        var parts: [(title: String, text: String)] = []
        for (wanted, title, log, missing) in [(game, "Game log", lastGameLog(), "There’s no game log yet. Launch the game once, then try again."),
                                              (server, "Server log", lastServerLog(), "There’s no server log yet. Start a server once, then try again.")] where wanted {
            guard let log, let shortened = Self.shortened(log) else { return (missing, false) }
            guard let text = shortened.text else {
                return ("Nothing was copied: \(title == "Game log" ? "the game log" : "the server log") looks like it holds your CD key. Open it from Show in Finder to check it yourself.", false)
            }
            parts.append((title, text))
        }
        guard !parts.isEmpty else { return ("", true) }
        let text = parts.count == 1 ? parts[0].text : parts.map { "===== \($0.title) =====\n\($0.text)" }.joined(separator: "\n\n")
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        return (parts.count == 1 ? "Copied \(parts[0].title == "Game log" ? "the game log" : "the server log")." : "Copied both logs.", true)
    }
    /// Settings › Advanced › Reset Settings…, behind its alert.
    func requestResetSettings() {
        confirm(ConfirmationRequest(title: "Reset launcher settings?",
                                    message: "Everything in the Settings window goes back to how it started. Loadouts, demos, screenshots and your CD key stay.",
                                    actionTitle: "Reset Settings") { [weak self] in self?.resetSettings() })
    }
    func resetSettings() {
        advancedDraft = nil; advancedError = nil
        settings = defaultSettings
        settingsFeedback = .init(pane: .advanced, text: "Settings are back to how they started.")
    }

    // MARK: Updates
    /// HTTPS request to GitHub Releases for the release list. Nothing is downloaded or installed until Install.
    /// An automatic check is quiet when it fails: the pane keeps what it said before.
    func checkUpdates() { checkUpdates(automatic: false) }
    func checkUpdates(automatic: Bool) {
        guard !snapshot, !checkingUpdates, !installingUpdate else { return }
        let before = (message: updateMessage, error: updateError, stage: updateFailedStage)
        if !automatic { updateWasChecked = true; updateError = false; updateFailedStage = nil }
        checkingUpdates = true; updateMessage = "Checking GitHub Releases…"
        if !automatic { studio.announce(updateMessage) }
        updateTask = Task {
            defer { checkingUpdates = false; if !automatic { studio.announce(updateMessage) } }
            do {
                var request = URLRequest(url: URL(string: "https://api.github.com/repos/cod2unbound/cod2-unbound/releases?per_page=10")!)
                request.timeoutInterval = 15; request.setValue("CoD2-Unbound-Launcher", forHTTPHeaderField: "User-Agent")
                request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let response = response as? HTTPURLResponse, response.statusCode == 200 else { throw LauncherError(message: "Release service is unavailable. Try again later.") }
                let feed = try ReleaseNotes.parse(data)
                if !feed.dispatches.isEmpty { library.dispatches = feed.dispatches }
                if let news = await Self.fetchNews() { library.news = news.items; library.newsNext = news.next }
                library.dispatchesCheckedAt = Date(); saveLibrary()
                updateWasChecked = true; updateError = false; updateFailedStage = nil
                if let latest = feed.latestVersion, ReleaseNotes.isNewer(latest, than: version) {
                    let announced = availableVersion
                    updateMessage = "Version \(latest) is available."
                    releaseURL = feed.dispatches.first?.link.flatMap(URL.init(string:)) ?? AppUpdate.releases
                    updateOffer = AppUpdate.offer(version: latest, assets: feed.latestAssets)
                    availableVersion = latest
                    updateNotes = feed.latestNotes
                    // Integration hook (ship-shell's notifyUpdateReady): a version the automatic check finds, once per version.
                    // Nothing installs at quit, so installsOnQuit is always false.
                    if automatic, announced != latest { onUpdateReady?(latest, false) }
                } else { updateMessage = "You’re up to date · \(version)"; releaseURL = nil; updateOffer = nil; availableVersion = nil; updateNotes = [] }
            } catch {
                if automatic { updateMessage = before.message; updateError = before.error; updateFailedStage = before.stage; return }
                updateMessage = "The update server didn’t answer. Check your connection and try again. Your current version keeps working."
                updateError = true; updateFailedStage = .check
            }
        }
    }
    /// The project’s news file, asked for with every update check. A missing file means no news; any
    /// other failure keeps what Home already shows.
    nonisolated static func fetchNews() async -> NewsFeed.Feed? {
        var request = URLRequest(url: NewsFeed.url)
        request.timeoutInterval = 15; request.cachePolicy = .reloadIgnoringLocalCacheData
        request.setValue("CoD2-Unbound-Launcher", forHTTPHeaderField: "User-Agent")
        guard let (data, response) = try? await URLSession.shared.data(for: request), let code = (response as? HTTPURLResponse)?.statusCode else { return nil }
        if code == 404 { return NewsFeed.Feed() }
        return code == 200 ? try? NewsFeed.parse(data) : nil
    }
    /// Updates › Check for updates automatically: at most once a day while the launcher is open. Never in the
    /// review harness or the automated lifecycle tests (COD2_SETUP_NONINTERACTIVE).
    func scheduleAutomaticUpdateCheck() {
        automaticUpdateTask?.cancel(); automaticUpdateTask = nil
        guard !snapshot, booted, settings.checkUpdatesAutomatically,
              ProcessInfo.processInfo.environment["COD2_SETUP_NONINTERACTIVE"] != "1" else { return }
        automaticUpdateTask = Task { [weak self] in
            do { try await Task.sleep(for: .seconds(8)) } catch { return }
            while !Task.isCancelled {
                guard let self else { return }
                let attempted = UserDefaults.standard.object(forKey: Self.automaticCheckKey) as? Date ?? .distantPast
                let last = max(attempted, self.lastUpdateCheck ?? .distantPast)
                let due = last.addingTimeInterval(24 * 3600)
                if Date() >= due, !self.gameRunning, !self.checkingUpdates, !self.installingUpdate {
                    UserDefaults.standard.set(Date(), forKey: Self.automaticCheckKey)
                    self.checkUpdates(automatic: true)
                }
                let wait = min(3600, max(300, due.timeIntervalSinceNow))
                do { try await Task.sleep(for: .seconds(wait)) } catch { return }
            }
        }
    }
    static let automaticCheckKey = "CoD2UnboundLastAutomaticUpdateCheck"
    /// Install and Relaunch: only on the player's click; refused while a game runs or where the app can't replace
    /// itself (then the Releases page opens). Cancel stops it until the new app is about to take this one's place.
    func installUpdate() {
        guard !snapshot, !installingUpdate, !checkingUpdates, let offer = updateOffer else { return }
        let app = Bundle.main.bundleURL, bundleID = Bundle.main.bundleIdentifier ?? ""
        if let refusal = AppUpdate.refusal(app: app, gameRunning: gameRunning) { updateFailed(refusal); return }
        updateError = false; updateFailedStage = nil
        installingUpdate = true; updateMessage = AppUpdate.Message.downloading(offer.version)
        studio.announce(updateMessage)
        updateTask = Task {
            var folder: URL?
            do {
                let staged = try await AppUpdate.download(offer, beside: app)
                folder = staged.folder
                try Task.checkCancellation()
                updateMessage = AppUpdate.Message.checking
                let candidate = try await Task.detached {
                    try AppUpdate.verify(staged.archive, size: offer.archiveSize, sha256: staged.sha256)
                    let unpacked = try AppUpdate.unpack(staged.archive, in: staged.folder)
                    try AppUpdate.check(unpacked, bundleID: bundleID, version: offer.version, team: AppUpdate.teamIdentifier(of: app))
                    return unpacked
                }.value
                try Task.checkCancellation()
                if let refusal = AppUpdate.refusal(app: app, gameRunning: gameRunning) { throw refusal }
                updateMessage = AppUpdate.Message.installing(offer.version)
                try AppUpdate.swap(app, with: candidate)
                try? FileManager.default.removeItem(at: staged.folder)
                updateMessage = AppUpdate.Message.restarting
                relaunch(app)
            } catch {
                if let folder { try? FileManager.default.removeItem(at: folder) }
                installingUpdate = false
                if Task.isCancelled { updateMessage = "Update cancelled. Nothing was changed."; studio.announce(updateMessage) }
                else { updateFailed(error) }
            }
        }
    }
    /// Stops a download or check in progress (not the moment the app is replaced).
    func cancelUpdate() { updateTask?.cancel() }
    var canCancelUpdate: Bool { checkingUpdates || (installingUpdate && updateMessage != AppUpdate.Message.restarting && !updateMessage.hasPrefix("Installing")) }
    func updateFailed(_ error: Error) {
        updateError = true; updateFailedStage = .install
        updateMessage = AppUpdate.message(error)
        studio.announce(updateMessage)
        if (error as? AppUpdate.Failure)?.opensReleases == true { NSWorkspace.shared.open(AppUpdate.releases) }
    }
    /// Starts the new app once this one has quit.
    func relaunch(_ app: URL) {
        let helper = Process()
        helper.executableURL = URL(fileURLWithPath: "/bin/sh")
        helper.arguments = ["-c", "while /bin/kill -0 \"$1\" 2>/dev/null; do /bin/sleep 0.2; done; /usr/bin/open \"$2\"",
                            "cod2-unbound-update", String(getpid()), app.path]
        do { try helper.run() } catch {
            installingUpdate = false; updateMessage = "Version \(updateOffer?.version ?? "") is installed. Quit and open CoD2 Unbound to use it."; return
        }
        NSApp.terminate(nil)
    }
    /// Try Again repeats what failed.
    func retryUpdate() { if updateFailedStage == .install, updateOffer != nil { installUpdate() } else { checkUpdates() } }

    // MARK: State other units read
    /// “Don’t ask again” prompts (keys in `DismissibleWarning`): whether the player turned one off,
    /// turning one off, and Settings › Advanced › Reset Warnings. Saved with the other settings.
    func isWarningDismissed(_ key: String) -> Bool { settings.dismissedWarnings.contains(key) }
    func dismissWarning(_ key: String) { settings.dismissedWarnings.insert(key) }
    func resetWarnings() {
        settings.dismissedWarnings.removeAll()
        settingsFeedback = .init(pane: .advanced, text: "Every “Don’t ask again” prompt will ask again.")
    }
    /// Where updates stand: Home's pill, Settings › Updates and the app menu read this.
    var updateState: UpdateState {
        if installingUpdate { return .installing(message: updateMessage) }
        if checkingUpdates { return .checking }
        if updateError { return .failed(message: updateMessage) }
        if let offer = updateOffer { return .ready(version: offer.version, installable: true) }
        if releaseURL != nil, updateWasChecked { return .ready(version: availableVersion ?? "", installable: false) }
        return updateWasChecked ? .upToDate(version: version) : .notChecked
    }
    /// When the release list was last fetched (nil before the first check).
    var lastUpdateCheck: Date? { library.dispatchesCheckedAt }
    /// The fetched release notes, newest first (the bundled ones before any check).
    var releaseNotes: [Dispatch] { library.dispatches.isEmpty ? Dispatch.bundled : library.dispatches }

    // MARK: About and Report a Problem
    /// The build number, shown only when releases set one (the app bundle's default is 1).
    var build: String {
        let value = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? ""
        return value == "1" || value == version ? "" : value
    }
    var reportDetails: ReportDetails {
        let mode = Self.displayModes.first { $0.value == settings.fullscreen }?.title.lowercased() ?? settings.fullscreen
        return ReportDetails(version: version, build: build, engines: installedEngineNames,
                             macOS: snapshot ? "27.0" : MacFacts.macOS, model: snapshot ? "MacBook Pro, Apple M4 Pro" : MacFacts.model,
                             shaders: approximate ? "approximate" : "original", display: SettingsText.resolution(settings.resolution) + " " + mode)
    }
    /// About › Copy Version Info: the lines Report a Problem shows.
    func copyVersionInfo() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(reportDetails.text, forType: .string)
    }
    /// Report a Problem › Open GitHub: the prefilled bug form in the browser. The app itself sends nothing.
    func openIssue() {
        if let url = reportDetails.issueURL(), !snapshot { NSWorkspace.shared.open(url) }
    }
    func openWebsite() { NSWorkspace.shared.open(URL(string: "https://github.com/cod2unbound/cod2-unbound")!) }

    // MARK: Sheets and confirmations this unit owns
    /// Player card and Settings › Game Data: Replace…
    func presentReplaceKey() { present(.replaceKey) }
    /// Saves a replacement CD key from the Replace sheet; the error to show inline, or nil when saved.
    func saveReplacementKey(_ key: String) -> String? {
        guard KeyStore.valid(key) else { return "That isn’t a valid Call of Duty 2 key. Check each character and try again." }
        do { if !snapshot { try KeyStore.save(key, at: preferences) } } catch { return "The key couldn’t be saved. Check that your home folder is writable, then try again." }
        keyVerified = true
        refreshGameData()
        settingsFeedback = .init(pane: .gameData, text: "CD key replaced.")
        return nil
    }
    /// Home's update pill and Settings › Updates.
    func showWhatsNew() { present(.whatsNew) }
    /// Settings › About.
    func showAcknowledgements() { present(.acknowledgements) }
    /// After Set Up Again's alert (the menu may call it directly): setup runs in the main window, so Settings closes first.
    func setUpAgain() {
        guard !gameRunning else { notice = "Quit the game before running setup again."; return }
        settingsWindow?.orderOut(nil)
        showMainWindow()
        restartSetup()
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them. Screens named
    /// `settings…` and `about…` render the Settings window (the harness picks the window by name).
    func previewSettings(_ screen: String) -> Bool {
        guard screen.hasPrefix("settings") || screen.hasPrefix("about") else { return false }
        let now = Date()
        library.dispatchesCheckedAt = Calendar.current.date(bySettingHour: 14, minute: 2, second: 0, of: now)
        refreshDisplay(); refreshGameData()
        switch screen {
        case "settings": settingsPane = .general
        case "settings-legacy-display-adjusted":
            settingsPane = .display
            mainDisplay = MainDisplay(name: "5K display", nativeWidth: 5120, nativeHeight: 2880, modes: [.init(3008, 1692), .init(6016, 3384)])
            settings.resolution = "6016x3384"; normalizeResolution(); savedSettings = settings
        case "settings-display": settingsPane = .display; settings.resolution = "2560x1440"; savedSettings = settings
        case "settings-display-restored": settingsPane = .display; settingsFeedback = .init(pane: .display, text: "Defaults restored.", undo: true)
        case "settings-display-windowed": settingsPane = .display; settings.fullscreen = "windowed"; settings.resolution = "1920x1080"; savedSettings = settings
        case "settings-game-data": settingsPane = .gameData
        case "settings-game-data-missing": settingsPane = .gameData; gameData.found = false; gameData.keyEnding = nil; keyVerified = false; approximate = true
            dataChangeError = "“CoD2 Demo” is incomplete, so the game still uses ~/Games/CoD2. Choose the folder whose main folder has all 16 game archives."
        case "settings-game-data-building": settingsPane = .gameData; shaderBusy = true; shaderCount = 412
        case "settings-set-up-again": settingsPane = .gameData; settingsConfirmation = ConfirmationRequest(title: "Run setup again?",
            message: "You’ll go through game data, CD key and shaders. Your loadouts, demos and screenshots stay, and you can cancel at any step.", actionTitle: "Set Up Again", destructive: false) {}
        case "settings-privacy": settingsPane = .privacy
        case "settings-privacy-sent": settingsPane = .privacy; privacyDetailsShown = true
        case "settings-delete-summaries": settingsPane = .privacy; settingsConfirmation = deleteSummariesRequest(count: 37)
        case "settings-updates": settingsPane = .updates; updateWasChecked = true; updateMessage = "You’re up to date · \(version)"
        case "settings-updates-never": settingsPane = .updates; library.dispatchesCheckedAt = nil
        case "settings-updates-checking": settingsPane = .updates; updateWasChecked = true; checkingUpdates = true; updateMessage = "Checking GitHub Releases…"
        case "settings-updates-offline": settingsPane = .updates; updateWasChecked = true; updateError = true; updateFailedStage = .check
            library.dispatchesCheckedAt = Calendar.current.date(byAdding: .day, value: -1, to: Calendar.current.date(bySettingHour: 21, minute: 30, second: 0, of: now) ?? now)
            updateMessage = "The update server didn’t answer. Check your connection and try again. Your current version keeps working."
        case "settings-update", "about-update", "settings-update-installing", "settings-update-error", "settings-sheet-whats-new":
            settingsPane = screen.hasPrefix("about") ? .about : .updates
            previewUpdateOffer()
            if screen == "settings-update-installing" { installingUpdate = true; updateMessage = AppUpdate.Message.downloading("0.3.1") }
            if screen == "settings-update-error" { updateError = true; updateFailedStage = .install; updateMessage = AppUpdate.message(AppUpdate.Failure.mismatch) }
            if screen == "settings-sheet-whats-new" { settingsSheet = .whatsNew }
        case "settings-advanced": settingsPane = .advanced; settings.advanced = "+set com_hunkMegs 512\ncl_maxpackets 100"
            settings.dismissedWarnings = [DismissibleWarning.switchVersion]; savedSettings = settings
        case "settings-reset": settingsPane = .advanced; settingsConfirmation = ConfirmationRequest(title: "Reset launcher settings?",
            message: "Everything in the Settings window goes back to how it started. Loadouts, demos, screenshots and your CD key stay.", actionTitle: "Reset Settings") {}
        case "about", "about-footer": settingsPane = .about
        case "settings-sheet-replace-key": settingsPane = .gameData; settingsSheet = .replaceKey
        case "settings-sheet-report": settingsPane = .about; settingsSheet = .reportProblem
        case "settings-sheet-report-server":
            // A server has been hosted here: the sheet offers its log too.
            hosting.preview(servers: [HostedServer.new(id: "friends", name: "Friday Night Rifles")], selection: nil)
            settingsPane = .about; settingsSheet = .reportProblem
        case "settings-sheet-acknowledgement": settingsPane = .about; settingsSheet = .acknowledgements; selectedAcknowledgement = "vkd3d 2.1"
        case "settings-running": settingsPane = .general; gameRunning = true
        case "settings-legacy": settingsPane = .display
        case "settings-legacy-custom": settingsPane = .display; settings.fps = 144
        case "settings-invalid": settingsPane = .advanced; editAdvancedOptions("name invalid;quit")
        case "settings-save-error": settingsPane = .general; settingsSaveError = "Your changes couldn’t be saved. Check that your CoD2 Unbound folder is writable, then try again."
        case "settings-original": settingsPane = .general; settings.engine = .original; savedSettings = settings
        case "settings-original-missing": settingsPane = .general; installedEngines = [.cod2x]
        case "settings-cod2x-installed": settingsPane = .gameData; cod2xFilesInstalled = true; cod2xFilesMessage = CoD2xFiles.Message.installed
        case "settings-cod2x-downloading": settingsPane = .gameData; cod2xFilesBusy = true; cod2xFilesMessage = CoD2xFiles.Message.downloading
        case "settings-cod2x-error": settingsPane = .gameData; cod2xFilesMessage = CoD2xFiles.message(CoD2xFiles.Failure.noCoD2x)
        case "settings-reports": settingsPane = .privacy; settings.sendCrashReports = true; settings.sendCoD2xReports = true; savedSettings = settings
        default: return false
        }
        return true
    }
    /// A synthetic newer release for the harness (never fetched).
    func previewUpdateOffer() {
        updateWasChecked = true
        releaseURL = AppUpdate.releases
        availableVersion = "0.3.1"
        updateOffer = AppUpdate.Offer(version: "0.3.1", archive: AppUpdate.releases, archiveSize: 48_213_514, sums: AppUpdate.releases)
        updateMessage = "Version 0.3.1 is available."
        updateNotes = [ReleaseNoteItem(tag: "New", title: "Connect to an address", detail: "Join a server by address or cod2x:// link, with ⌘L."),
                       ReleaseNoteItem(tag: "Fixed", title: "Map art is no longer stretched", detail: "Loading screens are shown at their real 4:3 shape everywhere."),
                       ReleaseNoteItem(tag: "Fixed", title: "Setup remembers your CD key", detail: "Going Back no longer asks you to type it again.")]
    }
}

/// A short confirmation shown where the person acted, in the pane that did it (“Shaders rebuilt.”).
struct SettingsFeedback: Equatable {
    var pane: SettingsPane
    var text: String
    var failed = false
    /// Restore Defaults offers Undo.
    var undo = false
}
