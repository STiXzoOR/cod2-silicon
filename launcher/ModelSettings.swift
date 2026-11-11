import AppKit
import SwiftUI

// Owner: ship-settings. Settings: saving settings, CoD2x files, and checking for and installing updates.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.
@MainActor extension LauncherModel {
    func scheduleSettingsSave() {
        settingsSaveTask?.cancel()
        settingsSaveTask = Task { [weak self] in
            do { try await Task.sleep(for: .milliseconds(250)) } catch { return }
            self?.saveSettings()
        }
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
                advancedError = "Line \(index + 1) can’t be used. Enter one setting and value, without quotes or command separators. Paths and private information are managed by the app."
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
    func saveSettings() {
        do {
            let text = try settings.launchConfig(migrated: configStudio?.migrationCompleted == true)
            guard !snapshot else { savedSettings = settings; settingsSaveError = nil; return }
            try PrivateFile.write(Data(text.utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try PrivateFile.write(JSONEncoder().encode(settings), to: home.appendingPathComponent("launcher-settings.json"))
            savedSettings = settings; settingsSaveError = nil
            studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        } catch { settingsSaveError = "Your changes couldn’t be saved. Check that your CoD2 Silicon folder is writable, then try again." }
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
        guard !snapshot, !cod2xFilesBusy else { return }
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.message = "Choose a Windows Call of Duty 2 folder that has CoD2x installed."
        guard let window = NSApp.keyWindow else { return }
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
    /// Explicit HTTPS request to GitHub Releases. Nothing is downloaded or installed until Install Update.
    func checkUpdates() {
        guard !snapshot, !checkingUpdates, !installingUpdate else { return }
        updateWasChecked = true; updateError = false
        checkingUpdates = true; updateMessage = "Checking GitHub Releases…"
        studio.announce(updateMessage)
        Task {
            defer { checkingUpdates = false; studio.announce(updateMessage) }
            do {
                var request = URLRequest(url: URL(string: "https://api.github.com/repos/STiXzoOR/cod2-silicon/releases?per_page=10")!)
                request.timeoutInterval = 15; request.setValue("CoD2-Silicon-Launcher", forHTTPHeaderField: "User-Agent")
                request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let response = response as? HTTPURLResponse, response.statusCode == 200 else { throw LauncherError(message: "Release service is unavailable. Try again later.") }
                let feed = try ReleaseNotes.parse(data)
                if !feed.dispatches.isEmpty { library.dispatches = feed.dispatches }
                library.dispatchesCheckedAt = Date(); saveLibrary()
                if let latest = feed.latestVersion, ReleaseNotes.isNewer(latest, than: version) {
                    updateMessage = "Version \(latest) is available. Download it when you're ready."
                    releaseURL = URL(string: "https://github.com/STiXzoOR/cod2-silicon/releases")
                    updateOffer = AppUpdate.offer(version: latest, assets: feed.latestAssets)
                } else { updateMessage = "You're up to date · \(version)"; releaseURL = nil; updateOffer = nil }
            } catch { updateMessage = "Updates couldn’t be checked. Check your internet connection and try again."; updateError = true }
        }
    }
    /// Install Update: only on the player's click; refused while a game runs or where the app can't replace
    /// itself (then the Releases page opens, as before). Progress and errors replace the update message.
    func installUpdate() {
        guard !snapshot, !installingUpdate, !checkingUpdates, let offer = updateOffer else { return }
        let app = Bundle.main.bundleURL, bundleID = Bundle.main.bundleIdentifier ?? ""
        if let refusal = AppUpdate.refusal(app: app, gameRunning: gameRunning) { updateFailed(refusal); return }
        installingUpdate = true; updateMessage = AppUpdate.Message.downloading(offer.version)
        Task {
            var folder: URL?
            do {
                let staged = try await AppUpdate.download(offer, beside: app)
                folder = staged.folder
                updateMessage = AppUpdate.Message.checking
                let candidate = try await Task.detached {
                    try AppUpdate.verify(staged.archive, size: offer.archiveSize, sha256: staged.sha256)
                    let unpacked = try AppUpdate.unpack(staged.archive, in: staged.folder)
                    try AppUpdate.check(unpacked, bundleID: bundleID, version: offer.version, team: AppUpdate.teamIdentifier(of: app))
                    return unpacked
                }.value
                if let refusal = AppUpdate.refusal(app: app, gameRunning: gameRunning) { throw refusal }
                updateMessage = AppUpdate.Message.installing(offer.version)
                try AppUpdate.swap(app, with: candidate)
                try? FileManager.default.removeItem(at: staged.folder)
                updateMessage = AppUpdate.Message.restarting
                relaunch(app)
            } catch {
                if let folder { try? FileManager.default.removeItem(at: folder) }
                installingUpdate = false
                updateFailed(error)
            }
        }
    }
    func updateFailed(_ error: Error) {
        updateError = true
        updateMessage = AppUpdate.message(error)
        if (error as? AppUpdate.Failure)?.opensReleases == true { NSWorkspace.shared.open(AppUpdate.releases) }
    }
    /// Starts the new app once this one has quit.
    func relaunch(_ app: URL) {
        let helper = Process()
        helper.executableURL = URL(fileURLWithPath: "/bin/sh")
        helper.arguments = ["-c", "while /bin/kill -0 \"$1\" 2>/dev/null; do /bin/sleep 0.2; done; /usr/bin/open \"$2\"",
                            "cod2-silicon-update", String(getpid()), app.path]
        do { try helper.run() } catch {
            installingUpdate = false; updateMessage = "Version \(updateOffer?.version ?? "") is installed. Quit and open CoD2 Silicon to use it."; return
        }
        NSApp.terminate(nil)
    }

    // MARK: State other units read
    /// Where updates stand: Home's pill, Settings › Updates and the app menu read this.
    var updateState: UpdateState {
        if installingUpdate { return .installing(message: updateMessage) }
        if checkingUpdates { return .checking }
        if updateError { return .failed(message: updateMessage) }
        if let offer = updateOffer { return .ready(version: offer.version, installable: true) }
        if releaseURL != nil, updateWasChecked { return .ready(version: "", installable: false) }
        return updateWasChecked ? .upToDate(version: version) : .notChecked
    }
    /// When the release list was last fetched (nil before the first check).
    var lastUpdateCheck: Date? { library.dispatchesCheckedAt }
    /// The fetched release notes, newest first (the bundled ones before any check).
    var releaseNotes: [Dispatch] { library.dispatches.isEmpty ? Dispatch.bundled : library.dispatches }

    // MARK: Sheets and confirmations this unit owns
    /// Player card and Settings › Game Data: Replace…
    func presentReplaceKey() { present(.replaceKey) }
    /// Saves a replacement CD key from the Replace sheet; the error to show inline, or nil when saved.
    func saveReplacementKey(_ key: String) -> String? {
        guard KeyStore.valid(key) else { return "That isn’t a valid Call of Duty 2 key. Check each character and try again." }
        do { if !snapshot { try KeyStore.save(key, at: preferences) } } catch { return error.localizedDescription }
        keyVerified = true
        return nil
    }
    /// Home's update pill and Settings › Updates.
    func showWhatsNew() { present(.whatsNew) }
    /// Settings › About.
    func showAcknowledgements() { present(.acknowledgements) }
    /// Settings › Game Data › Set Up Again…: setup runs in the main window, so Settings closes first.
    /// (ship-settings puts this behind its alert.)
    func setUpAgain() {
        settingsWindow?.orderOut(nil)
        showMainWindow()
        restartSetup()
    }
    /// Settings › Privacy and Library › Matches: Delete Summaries… (ship-settings adds deletion and its alert).
    func requestDeleteSummaries() {}
    /// Settings › Advanced: Reset Settings… (ship-settings adds the reset and its alert).
    func requestResetSettings() {}

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them. Screens named
    /// `settings…` and `about…` render the Settings window (the harness picks the window by name).
    func previewSettings(_ screen: String) -> Bool {
        switch screen {
        case "settings": settingsPane = .general
        case "settings-display": settingsPane = .display
        case "settings-game-data": settingsPane = .gameData
        case "settings-privacy": settingsPane = .privacy
        case "settings-updates": settingsPane = .updates
        case "settings-advanced": settingsPane = .advanced
        case "about": settingsPane = .about
        case "settings-legacy": settingsPane = .display
        case "settings-legacy-custom": settingsPane = .display; settings.fps = 144
        case "settings-invalid": settingsPane = .advanced; editAdvancedOptions("name invalid;quit")
        case "settings-save-error": settingsPane = .general; settingsSaveError = "Your changes couldn’t be saved. Check that your CoD2 Silicon folder is writable, then try again."
        case "settings-original": settingsPane = .general; settings.engine = .original; savedSettings = settings
        case "settings-original-missing": settingsPane = .general; installedEngines = [.cod2x]
        case "settings-cod2x-installed": settingsPane = .general; cod2xFilesInstalled = true; cod2xFilesMessage = CoD2xFiles.Message.installed
        case "settings-cod2x-downloading": settingsPane = .general; cod2xFilesBusy = true; cod2xFilesMessage = CoD2xFiles.Message.downloading
        case "settings-cod2x-error": settingsPane = .general; cod2xFilesMessage = CoD2xFiles.message(CoD2xFiles.Failure.noCoD2x)
        case "settings-reports": settingsPane = .privacy; settings.sendCrashReports = true; settings.sendCoD2xReports = true; savedSettings = settings
        case "settings-update", "about-update", "settings-update-installing", "settings-update-error":
            settingsPane = screen.hasPrefix("about") ? .about : .updates
            previewUpdateOffer()
            if screen == "settings-update-installing" { installingUpdate = true; updateMessage = AppUpdate.Message.downloading("0.4.0") }
            if screen == "settings-update-error" { updateError = true; updateMessage = AppUpdate.message(AppUpdate.Failure.mismatch) }
        default: return false
        }
        return true
    }
    /// A synthetic newer release for the harness (never fetched).
    func previewUpdateOffer() {
        releaseURL = AppUpdate.releases
        updateOffer = AppUpdate.Offer(version: "0.4.0", archive: AppUpdate.releases, archiveSize: 1, sums: AppUpdate.releases)
        updateMessage = "Version 0.4.0 is available. Download it when you're ready."
    }
}
