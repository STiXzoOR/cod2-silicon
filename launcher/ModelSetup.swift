import AppKit
import SwiftUI

// Owner: ship-shell. Setup: the first-run and Set Up Again flow, shader preparation, and returning to a running game.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.
@MainActor extension LauncherModel {
    var canCancelSetup: Bool { setupReturnPage != nil }
    var hasPendingLaunch: Bool { pendingLink != nil || fastPlay }
    var setupCompletionTitle: String { pendingLink != nil ? "Join Server" : fastPlay ? "Open Game Menu" : "Open CoD2 Silicon" }
    var setupServerAddress: String? { pendingLink?.address }
    var setupCanContinue: Bool {
        guard !shaderBusy else { return false }
        switch step {
        case 0: return !dataPath.isEmpty && dataError.isEmpty
        case 1: return keyReady
        default: return true
        }
    }
    func pickData() {
        guard !shaderBusy, let window = launcherWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = makeSetupPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Choose Folder"
        panel.message = "Choose your Call of Duty 2 folder. Its main folder must contain all 16 game archives, iw_00.iwd through iw_15.iwd."
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
        dataError = "This folder is incomplete. Choose the Call of Duty 2 folder with all 16 game archives in its main folder, from iw_00.iwd through iw_15.iwd."
        return false
    }
    func rememberData() throws {
        guard !dataPath.isEmpty, GameSettings.safeValue(dataPath), GameSettings.safeValue(home.path) else { throw LauncherError(message: "Choose a path without quotes, plus signs, semicolons or control characters.") }
        try PrivateFile.write(Data((dataPath + "\n").utf8), to: home.appendingPathComponent("data-path.txt"))
    }
    var keyReady: Bool { (keyVerified && !replacingKey) || KeyStore.valid(keyInput) }
    func replaceKey() { replacingKey = true; keyInput = ""; setupError = "" }
    func keepStoredKey() { replacingKey = false; keyInput = ""; setupError = "" }
    func nextSetup() {
        guard setupCanContinue else { return }
        setupError = ""
        do {
            switch step {
            case 0:
                guard snapshot || LauncherFindData(dataPath) != nil else {
                    _ = chooseData(URL(fileURLWithPath: dataPath)); return
                }
                if !snapshot { try rememberData() }
                dataChanged = dataChanged || dataPath != confirmedDataPath
                confirmedDataPath = dataPath
                step = 1
            case 1:
                if replacingKey || !keyVerified {
                    if !snapshot { try KeyStore.save(keyInput, at: preferences) }
                    keyInput = ""; keyVerified = true; replacingKey = false
                }
                step = 2
                if approximate || dataChanged { prepareShaders() }
            case 2: step = 3
            default: finishSetup()
            }
        } catch { setupError = error.localizedDescription }
    }
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
    func backSetup() { if step > 0 && !shaderBusy { setupError = ""; step -= 1 } }
    func prepareShaders(selected: String? = nil) {
        guard !shaderBusy, !snapshot, shaderRefresh == nil else { return }
        step = 2; setupError = ""
        shaderBusy = true; shaderCount = 0; shaderMessage = "Reading your game files…"
        let data = dataPath, parent = home
        let progress = Task { [weak self] in
            while !Task.isCancelled {
                let count = await Task.detached(priority: .utility) { Self.stagedShaderCount(in: parent) }.value
                if let self, self.shaderBusy, let count {
                    self.shaderCount = min(Self.shaderTotal, max(self.shaderCount, count))
                    self.shaderMessage = "Building shaders from your game files…"
                }
                try? await Task.sleep(for: .milliseconds(120))
            }
        }
        setupTask = Task {
            let verified = await Task.detached(priority: .utility) { LauncherPrepareShaders(data, selected) }.value
            progress.cancel()
            shaderCount = verified ? Self.shaderTotal : 0
            shaderBusy = false; approximate = !verified
            dataChanged = false
            shaderMessage = verified ? "Original shaders ready" : "The original shaders couldn’t be built."
            if onboard && step == 2 { step = 3 }
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
    func finishSetup(findServer: Bool = false) {
        guard !shaderBusy, keyVerified, !dataPath.isEmpty else { return }
        do {
            if !snapshot { try PrivateFile.write(Data("prepared\n".utf8), to: home.appendingPathComponent(".launcher-setup-complete")) }
            onboard = false; setupReturnPage = nil
            navigate(to: findServer ? .servers : .home)
            loadArtwork()
            if !findServer && hasPendingLaunch { play() }
        } catch { setupError = "Couldn’t save setup. Try again. " + error.localizedDescription }
    }
    func restartSetup() { restartSetup(at: 0) }
    func restartSetup(at requestedStep: Int) {
        guard !gameRunning, !installingUpdate else { return }
        if !onboard, keyVerified, !dataPath.isEmpty, snapshot || LauncherFindData(dataPath) != nil {
            setupReturnPage = page; confirmedDataPath = dataPath
        }
        keyInput = ""; replacingKey = false; setupError = ""; dataError = ""; attemptedDataPath = ""
        step = min(2, max(0, requestedStep)); onboard = true
        if step == 2 && approximate { prepareShaders() }
    }
    func cancelSetup() {
        guard let destination = setupReturnPage else { return }
        dataPath = confirmedDataPath; keyInput = ""; replacingKey = false
        setupError = ""; dataError = ""; attemptedDataPath = ""
        setupReturnPage = nil; onboard = false
        pendingLink = nil; fastPlay = false
        navigate(to: destination)
    }
    func returnToGame() {
        guard let pid = activeGamePID, let game = NSRunningApplication(processIdentifier: pid) else { return }
        launcherWindow?.orderOut(nil)
        NSApp.setActivationPolicy(.accessory)
        game.activate(options: .activateAllWindows)
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewShell(_ screen: String) -> Bool {
        switch screen {
        case "setup-data": onboard = true; step = 0
        case "setup-data-missing": onboard = true; step = 0; dataPath = ""
        case "setup-key": onboard = true; step = 1; keyVerified = false; keyInput = "7Q4M 2KX9 LP3R"
        case "setup-shaders": onboard = true; step = 2; shaderBusy = true; shaderCount = 518; shaderMessage = "Building shaders from your game files…"
        case "setup-ready", "setup-summary-ready": onboard = true; step = 3; shaderCount = Self.shaderTotal; approximate = false
        case "setup-key-replace": onboard = true; step = 1; keyVerified = true; replacingKey = true
        case "setup-shaders-ready": onboard = true; step = 2; shaderCount = Self.shaderTotal; approximate = false
        case "setup-key-stored": onboard = true; step = 1; keyVerified = true
        case "setup-key-invalid": onboard = true; step = 1; keyVerified = false; keyInput = "1111 1111 1111 1111 1111"
        case "setup-summary-approximate": onboard = true; step = 3; approximate = true; shaderCount = 0
        case "setup-summary-link": onboard = true; step = 3; pendingLink = try? LaunchLink.direct("192.0.2.10:28960")
        case "setup-cancelable": navigate(to: .library); restartSetup()
        case "setup-folder-error": onboard = true; step = 0; dataPath = ""; attemptedDataPath = "/Users/Player/Downloads/Call of Duty 2"; dataError = "This folder is incomplete. Choose the Call of Duty 2 folder with all 16 game archives in its main folder, from iw_00.iwd through iw_15.iwd."
        case "home-notice": page = .home; notice = "Couldn’t open the game. Your files are safe. Choose your Call of Duty 2 folder again in Settings, then try Launch. If the folder is on an external drive, connect the drive first. You can select and copy this message, or dismiss it when you’re ready."
        case "home-game-running": page = .home; gameRunning = true; notice = "Call of Duty 2 is still running. Quit the game first, then quit CoD2 Silicon."
        default: return false
        }
        return true
    }
}
