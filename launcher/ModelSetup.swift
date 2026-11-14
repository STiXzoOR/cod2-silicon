import AppKit
import SwiftUI

/// The six steps of setup, in order (Setup boards): the bottom bar shows one dot per step.
enum SetupStep: Int, CaseIterable, Identifiable {
    case welcome, data, key, shaders, yours, ready
    var id: Int { rawValue }
    var title: String {
        switch self {
        case .welcome: "Welcome"; case .data: "Game data"; case .key: "CD key"
        case .shaders: "Shaders"; case .yours: "Make it yours"; case .ready: "Ready"
        }
    }
    var number: Int { rawValue + 1 }
    static let count = allCases.count
}

/// How the game fills the screen, as Setup › Make it yours offers it (settings.fullscreen values).
enum SetupDisplayMode: String, CaseIterable, Identifiable {
    case borderless, spaces, windowed
    var id: String { rawValue }
    var title: String { switch self { case .borderless: "Borderless"; case .spaces: "Native Space"; case .windowed: "Window" } }
    var detail: String {
        switch self {
        case .borderless: "Fills the screen. Switching apps is instant."
        case .spaces: "A macOS full-screen Space of its own."
        case .windowed: "A resizable window on your desktop."
        }
    }
    /// The words for any saved mode, including the exclusive full screen older launchers wrote.
    static func title(_ value: String) -> String {
        SetupDisplayMode(rawValue: value)?.title ?? (value == "exclusive" ? "Full screen" : value.capitalized)
    }
}

/// The game's sixteen archives, main/iw_00.iwd through main/iw_15.iwd.
enum GameArchives {
    static let names = (0..<16).map { String(format: "iw_%02d.iwd", $0) }
    /// For a folder that isn't valid game data: how many archives its likeliest `main` holds and which
    /// are missing. The folder itself, its `main`, or an app's Resources are looked at, as setup accepts them.
    static func survey(_ folder: String) -> (found: Int, missing: [String]) {
        let root = URL(fileURLWithPath: folder)
        var candidates = ["main", "Contents/Resources/main", "Contents/main", "Call of Duty 2.app/Contents/Resources/main"].map { root.appendingPathComponent($0) }
        // Someone who picks “main” itself gets the count of what's in it.
        if root.lastPathComponent.lowercased() == "main" { candidates.insert(root, at: 0) }
        var best: (found: Int, missing: [String]) = (0, names.map { "main/" + $0 })
        for main in candidates {
            let present = Set(((try? FileManager.default.contentsOfDirectory(atPath: main.path)) ?? []).map { $0.lowercased() })
            let missing = names.filter { !present.contains($0) }
            if names.count - missing.count > best.found { best = (names.count - missing.count, missing.map { "main/" + $0 }) }
        }
        return best
    }
}

// Owner: ship-shell. Setup: the first-run and Set Up Again flow, shader preparation, and returning to a running game.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.
@MainActor extension LauncherModel {
    var setupStep: SetupStep {
        get { SetupStep(rawValue: step) ?? .welcome }
        set { step = newValue.rawValue }
    }
    /// Set Up Again (from Settings): Cancel shows at every step and leaves with nothing changed.
    var canCancelSetup: Bool { setupReturnPage != nil }
    var hasPendingLaunch: Bool { pendingLink != nil || fastPlay }
    var setupCompletionTitle: String { pendingLink != nil ? "Join Server" : fastPlay ? "Open Game Menu" : "Open CoD2 Silicon" }
    var setupServerAddress: String? { pendingLink?.address }
    var setupCanContinue: Bool {
        switch setupStep {
        case .welcome, .ready: return true
        case .data: return !dataPath.isEmpty && dataError.isEmpty
        case .key: return keyReady
        case .shaders: return !shaderBusy
        case .yours: return setupNameError == nil
        }
    }
    /// The bottom bar's default action.
    var setupPrimaryTitle: String {
        switch setupStep {
        case .welcome: "Get Started"
        case .shaders: shaderFailed ? "Continue with Approximate Shaders" : "Continue"
        case .ready: setupCompletionTitle
        default: "Continue"
        }
    }
    /// The bottom bar's second action, or nil.
    var setupAlternateTitle: String? {
        switch setupStep {
        case .key where !(keyVerified && !replacingKey) && keyCheck != .verified: "Skip for Now"
        case .yours: "Skip"
        case .ready where !hasPendingLaunch: "Find a Server"
        default: nil
        }
    }

    // MARK: Game data
    /// What the data step says about a folder that is in use: every archive is there, so it's 1.3.
    var dataFoundSummary: String { "All 16 game archives present · Version 1.3" }
    func pickData() {
        guard !shaderBusy, let window = launcherWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = makeSetupPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Choose Folder"
        panel.message = "Choose your Call of Duty 2 folder: the one that contains “main” with iw_00.iwd through iw_15.iwd."
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.chooseData(url)
        }
    }
    @discardableResult func chooseData(_ url: URL) -> Bool {
        guard url.isFileURL, !shaderBusy else { return false }
        attemptedDataPath = url.path
        if let valid = LauncherFindData(url.path) {
            dataPath = valid; dataError = ""; setupError = ""
            return true
        }
        let survey = GameArchives.survey(url.path)
        dataArchivesFound = survey.found
        let first = survey.missing.first ?? "main/iw_00.iwd"
        let more = survey.missing.count - 1
        dataError = more > 0
            ? "This folder is missing \(first) and \(more) more game archive\(more == 1 ? "" : "s"). Choose the Call of Duty 2 folder itself, the one that contains “main” with iw_00.iwd through iw_15.iwd."
            : "This folder is missing \(first). Choose the Call of Duty 2 folder itself, the one that contains “main” with iw_00.iwd through iw_15.iwd."
        return false
    }
    func rememberData() throws {
        guard !dataPath.isEmpty, GameSettings.safeValue(dataPath), GameSettings.safeValue(home.path) else { throw LauncherError(message: "Choose a path without quotes, plus signs, semicolons or control characters.") }
        try PrivateFile.write(Data((dataPath + "\n").utf8), to: home.appendingPathComponent("data-path.txt"))
    }

    // MARK: CD key
    var keyReady: Bool { (keyVerified && !replacingKey) || KeyStore.valid(keyInput) }
    /// The stored key's last four characters, the only part setup and the player card ever show.
    var storedKeyTail: String? {
        guard keyVerified else { return nil }
        if snapshot { return "7K2D" }
        guard let text = try? String(contentsOf: preferences, encoding: .utf8) else { return nil }
        let key = text.components(separatedBy: "\n").last { $0.hasPrefix("codkey=") && KeyStore.valid(String($0.dropFirst(7))) }
        return key.map { String(KeyStore.normalize(String($0.dropFirst(7))).suffix(4)) }
    }
    /// The key field's state as the person types: nothing yet, still short, or a full key that does or doesn't verify.
    enum KeyCheck: Equatable { case blank, typing(left: Int), verified, mismatch }
    var keyCheck: KeyCheck {
        let raw = KeyFormat.groups(keyInput).joined()
        if raw.isEmpty { return .blank }
        if raw.count < 20 { return .typing(left: 20 - raw.count) }
        return KeyStore.valid(keyInput) ? .verified : .mismatch
    }
    func pasteKey() {
        guard let text = NSPasteboard.general.string(forType: .string) else { return }
        keyInput = KeyFormat.display(text); setupError = ""
    }
    /// Setup › CD key › Replace…: the same sheet as the player card and Settings › Game Data.
    func replaceKey() { presentReplaceKey() }
    func keepStoredKey() { replacingKey = false; keyInput = ""; setupError = "" }

    // MARK: Steps
    func nextSetup() {
        guard setupCanContinue else { return }
        setupError = ""
        do {
            switch setupStep {
            case .welcome: setupStep = .data
            case .data:
                guard snapshot || LauncherFindData(dataPath) != nil else {
                    _ = chooseData(URL(fileURLWithPath: dataPath)); return
                }
                if !snapshot { try rememberData() }
                dataChanged = dataChanged || dataPath != confirmedDataPath
                confirmedDataPath = dataPath
                setupStep = .key
            case .key:
                if replacingKey || !keyVerified {
                    if !snapshot { try KeyStore.save(keyInput, at: preferences) }
                    keyInput = ""; keyVerified = true; replacingKey = false
                }
                enterShaders()
            case .shaders: enterYours()
            case .yours:
                try applyYours()
                setupStep = .ready
            case .ready: finishSetup()
            }
        } catch { setupError = error.localizedDescription }
    }
    /// The bottom bar's second action: Skip for Now (CD key), Skip (Make it yours), Find a Server (Ready).
    func alternateSetup() {
        setupError = ""
        switch setupStep {
        case .key: keyInput = ""; replacingKey = false; enterShaders()
        case .yours: setupNameError = nil; setupStep = .ready
        case .ready: finishSetup(findServer: true)
        default: break
        }
    }
    func backSetup() {
        guard setupStep != .welcome, !shaderBusy else { return }
        setupError = ""
        setupStep = SetupStep(rawValue: step - 1) ?? .welcome
    }
    private func enterShaders() {
        setupStep = .shaders
        if approximate || dataChanged { prepareShaders() } else { shaderFailed = false }
    }
    private func enterYours() {
        setupName = playerName == PlayerProfile.unnamed ? "" : playerName
        setupDisplayMode = SetupDisplayMode(rawValue: settings.fullscreen) != nil ? settings.fullscreen : ""
        setupNameError = nil
        setupStep = .yours
    }
    /// Make it yours › Continue: the name goes into the loadout the next game uses; the display mode into Settings.
    func applyYours() throws {
        let name = setupName.trimmingCharacters(in: .whitespaces)
        if !name.isEmpty, name != playerName {
            if let problem = PlayerNameRules.problem(name) { setupNameError = problem; throw LauncherError(message: problem) }
            if !snapshot, let error = savePlayerNameNow(name) { throw LauncherError(message: error) }
            if snapshot { playerName = name }
        }
        if !setupDisplayMode.isEmpty, setupDisplayMode != settings.fullscreen { settings.fullscreen = setupDisplayMode }
    }
    /// Ready › Player and display: the name and how the game fills the screen, as they are now.
    var setupPlayerSummary: String {
        let name = playerName == PlayerProfile.unnamed ? "No name yet" : QuakeColors.plain(playerName)
        let size = settings.resolution.replacingOccurrences(of: "x", with: " × ")
        return "\(name) · \(SetupDisplayMode.title(settings.fullscreen)) at \(size)"
    }

    // MARK: Shaders
    /// After an update, builds the shaders again if this version's generator is newer than the
    /// ones on disk. It takes a few seconds once; `play` waits for it.
    func refreshShaders() {
        guard !snapshot, !onboard, !dataPath.isEmpty, shaderRefresh == nil else { return }
        let data = dataPath
        shaderRefresh = Task {
            let verified = await Task.detached(priority: .userInitiated) { LauncherPrepareShaders(data, nil) }.value
            approximate = !verified
            shaderRefresh = nil
        }
    }
    func prepareShaders(selected: String? = nil) {
        guard !shaderBusy, !snapshot, shaderRefresh == nil else { return }
        if onboard { setupStep = .shaders }
        setupError = ""; shaderFailed = false; shaderSeconds = nil
        shaderBusy = true; shaderCount = 0; shaderMessage = "Reading your game files…"
        let data = dataPath, parent = home, started = Date()
        let progress = Task { [weak self] in
            while !Task.isCancelled {
                let count = await Task.detached(priority: .utility) { Self.stagedShaderCount(in: parent) }.value
                if let self, self.shaderBusy, let count, count > 0 {
                    self.shaderCount = min(Self.shaderTotal, max(self.shaderCount, count))
                    self.shaderMessage = "Building original shaders"
                }
                try? await Task.sleep(for: .milliseconds(120))
            }
        }
        setupTask = Task {
            let verified = await Task.detached(priority: .utility) { LauncherPrepareShaders(data, selected) }.value
            progress.cancel()
            shaderCount = verified ? Self.shaderTotal : 0
            shaderBusy = false; approximate = !verified; shaderFailed = !verified
            shaderSeconds = verified ? max(1, Int(Date().timeIntervalSince(started).rounded())) : nil
            dataChanged = false
            shaderMessage = verified ? "Original shaders ready" : "The original shaders couldn’t be built"
            // Setup may have been left in the background; only then say so outside the window.
            if verified, onboard, !NSApp.isActive { notifyShadersReady() }
        }
    }
    /// Payloads written so far into the extractor's private staging folder in the app home.
    nonisolated static func stagedShaderCount(in parent: URL) -> Int? {
        guard let entries = try? FileManager.default.contentsOfDirectory(at: parent, includingPropertiesForKeys: nil) else { return nil }
        let staging = entries.filter { $0.lastPathComponent.hasPrefix(".shaders-") && !$0.lastPathComponent.hasSuffix("-old") }
        guard let folder = staging.first else { return nil }
        let names = (try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []
        return names.filter { !$0.hasPrefix(".") && $0 != "manifest.json" }.count
    }
    func pickShaders() {
        guard !shaderBusy, let window = launcherWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = makeSetupPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Use Mac Copy"
        panel.message = "Choose your Mac Call of Duty 2 folder or its multiplayer app to use its original lighting, sky and water."
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.prepareShaders(selected: url.path)
        }
    }

    // MARK: Finishing, Set Up Again and Cancel
    func finishSetup(findServer: Bool = false) {
        guard !shaderBusy, !dataPath.isEmpty else { return }
        do {
            if !snapshot { try PrivateFile.write(Data("prepared\n".utf8), to: home.appendingPathComponent(".launcher-setup-complete")) }
            onboard = false; setupReturnPage = nil
            navigate(to: findServer ? .servers : .home)
            loadArtwork()
            if !findServer && hasPendingLaunch { play() }
            if let address = pendingJoinAddress { pendingJoinAddress = nil; connectToAddress(address) }
        } catch { setupError = "Setup couldn’t be saved. Try again. " + error.localizedDescription }
    }
    func restartSetup() { restartSetup(at: .data) }
    /// Older callers pass the step of the four-step setup: 0 game data, 1 CD key, 2 shaders.
    func restartSetup(at requestedStep: Int) {
        restartSetup(at: [SetupStep.data, .key, .shaders][min(2, max(0, requestedStep))])
    }
    /// Set Up Again: setup in the main window with Cancel at every step. Nothing changes until a step is confirmed.
    func restartSetup(at requested: SetupStep) {
        guard !gameRunning, !installingUpdate else { return }
        if !onboard, !dataPath.isEmpty, snapshot || LauncherFindData(dataPath) != nil {
            setupReturnPage = page; confirmedDataPath = dataPath
        }
        keyInput = ""; replacingKey = false; setupError = ""; dataError = ""; attemptedDataPath = ""
        setupNameError = nil; shaderFailed = approximate && !shaderBusy
        jumpPresented = false; playerCardPresented = false
        setupStep = requested; onboard = true
        if requested == .shaders && approximate { prepareShaders() }
    }
    func cancelSetup() {
        guard let destination = setupReturnPage else { return }
        dataPath = confirmedDataPath; keyInput = ""; replacingKey = false
        setupError = ""; dataError = ""; attemptedDataPath = ""; setupNameError = nil
        setupReturnPage = nil; onboard = false
        pendingLink = nil; fastPlay = false
        navigate(to: destination)
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewShell(_ screen: String) -> Bool {
        if previewSetup(screen) { return true }
        switch screen {
        case "home-notice": page = .home; notice = "Couldn’t open the game. Your files are safe. Choose your Call of Duty 2 folder again in Settings, then try Launch. If the folder is on an external drive, connect the drive first. You can select and copy this message, or dismiss it when you’re ready."
        case "home-game-running": page = .home; gameRunning = true; notice = "Call of Duty 2 is running. Settings you change apply next time."
        case "home-loadout-player-card": page = .home; playerCardPresented = true; playerCardStatus = "Saved to your Main loadout."
        case "home-loadout-player-card-no-key": page = .home; keyVerified = false; playerCardPresented = true
        case "home-jump": page = .home; jumpPresented = true; jumpQuery = "north"
        case "home-no-favorites": page = .home; library.favorites = []
        case "home-quit-running": page = .home; gameRunning = true; confirmQuitWithGame()
        case "sheet-game-could-not-start": page = .home; launchProblem = .dataMissing("/Volumes/Games/CoD2"); sheet = .gameCouldNotStart(reason: launchProblem!.message)
        case "sheet-game-could-not-start-other": page = .home; launchProblem = .other("The game couldn’t be opened: the operation couldn’t be completed."); sheet = .gameCouldNotStart(reason: launchProblem!.message)
        default: return false
        }
        return true
    }
    private func previewSetup(_ screen: String) -> Bool {
        guard screen.hasPrefix("setup-") else { return false }
        onboard = true
        switch screen {
        case "setup-welcome": setupStep = .welcome
        case "setup-data": setupStep = .data; dataPath = ""
        case "setup-data-found": setupStep = .data
        case "setup-data-error":
            setupStep = .data; dataPath = ""; attemptedDataPath = "/Users/Player/Downloads/cod2-backup"; dataArchivesFound = 4
            dataError = "This folder is missing main/iw_04.iwd and 11 more game archives. Choose the Call of Duty 2 folder itself, the one that contains “main” with iw_00.iwd through iw_15.iwd."
        case "setup-key": setupStep = .key; keyVerified = false
        case "setup-key-ok": setupStep = .key; keyVerified = false; keyInput = KeyFormat.display(Self.previewKey)
        case "setup-key-bad": setupStep = .key; keyVerified = false; keyInput = KeyFormat.display("000000000000000086D0")
        case "setup-key-stored": setupStep = .key
        case "setup-shaders": setupStep = .shaders; shaderBusy = true; shaderCount = 518; shaderMessage = "Building original shaders"
        case "setup-shaders-starting": setupStep = .shaders; shaderBusy = true; shaderCount = 0; shaderMessage = "Reading your game files…"
        case "setup-shaders-done": setupStep = .shaders; shaderCount = Self.shaderTotal; approximate = false; shaderSeconds = 48
        case "setup-shaders-failed": setupStep = .shaders; approximate = true; shaderFailed = true; shaderCount = 0
        case "setup-yours": setupStep = .yours; setupName = playerName; setupDisplayMode = "borderless"; settings.resolution = "2560x1440"
        case "setup-ready": setupStep = .ready; settings.resolution = "2560x1440"; settings.fullscreen = "borderless"; approximate = false
        case "setup-ready-approximate": setupStep = .ready; approximate = true; keyVerified = false
        case "setup-ready-link": setupStep = .ready; pendingLink = try? LaunchLink.direct("192.0.2.10:28960")
        case "setup-again": onboard = false; navigate(to: .library); restartSetup(at: .key)
        case "setup-again-data": onboard = false; navigate(to: .library); restartSetup()
        default: onboard = false; return false
        }
        return true
    }
    /// A key that passes the game's checksum, for the harness's verified state (never stored).
    static let previewKey = "000000000000000086D3"
}
