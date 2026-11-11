import AppKit
import SwiftUI

// Review harness, owner ship-shell. Setup screens, the toasts over Home, and the shell checks.

extension Snapshots {
    static let setupScreens = ["setup-data", "setup-data-missing", "setup-folder-error", "setup-key", "setup-key-invalid", "setup-key-stored", "setup-key-replace", "setup-shaders-ready",
                               "setup-shaders", "setup-ready", "setup-summary-ready", "setup-summary-approximate", "setup-summary-link", "setup-cancelable"]
    /// Every ship-shell screen: setup, and the status toasts over Home.
    static var shellScreens: [String] { setupScreens + ["home-notice", "home-game-running"] }
    /// Window-driving suites, run by `--suite NAME` (snapshots.py runs every suite `--list-suites` prints).
    static let shellSuites: [Suite] = [Suite(name: "shell-actions", run: ShellPreview.actions), Suite(name: "shell-accessibility", run: ShellPreview.accessibility)]
}

/// Shell and setup checks exercise the shipping views with synthetic data only.
@MainActor enum ShellPreview {
    struct Failure: Error { let message: String }
    static func require(_ condition: Bool, _ message: String) throws {
        if !condition { throw Failure(message: message) }
    }
    static func settle() async { try? await Task.sleep(for: .milliseconds(650)) }
    static func click(_ title: String, in window: NSWindow) async throws {
        guard let button = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == title }) else {
            throw Failure(message: "Missing button: \(title)")
        }
        ConfigsPreview.click(window, element: button)
        await settle()
    }
    static func actions(output: URL) async -> Bool {
        let model = LauncherModel(snapshot: true)
        model.preview("setup-key-stored")
        let window = await Snapshots.show(model: model, dark: true, glass: true)
        defer { window.orderOut(nil); window.close() }
        AccessibilityTree.wake(window)
        do {
            try require(model.keyReady, "A saved key must enable Continue")
            try await click("Continue", in: window)
            try require(model.step == 2, "Stored-key Continue reaches shaders")
            try await click("Back", in: window)
            try require(model.step == 1 && model.keyReady && model.keyInput.isEmpty, "Back preserves the verified stored key")
            try await click("Replace Key…", in: window)
            try require(!model.keyReady && model.replacingKey, "Replacing a key requires valid replacement input")
            try await click("Keep Stored Key", in: window)
            try require(model.keyReady && !model.replacingKey, "Keeping the stored key restores Continue")
            print("PASS: stored key → Continue → Back; Replace Key and Keep Stored Key use real controls")

            model.onboard = false; model.navigate(to: .library); model.restartSetup()
            let original = model.dataPath
            let folder = output.appendingPathComponent("synthetic-game")
            try FileManager.default.createDirectory(at: folder.appendingPathComponent("main"), withIntermediateDirectories: true)
            defer { try? FileManager.default.removeItem(at: folder) }
            for index in 0..<16 { try Data().write(to: folder.appendingPathComponent(String(format: "main/iw_%02d.iwd", index))) }
            try require(model.chooseData(folder), "Complete dropped/selected folders are accepted")
            await settle()
            ConfigsPreview.press(window, keyCode: 53, characters: "\u{1b}")
            await settle()
            try require(!model.onboard && model.page == .library && model.dataPath == original, "Escape cancels and restores the unconfirmed folder and originating page")
            model.restartSetup()
            try require(!model.chooseData(folder.appendingPathComponent("missing")) && !model.dataError.isEmpty && !model.setupCanContinue, "An incomplete folder blocks Continue and explains why inline")
            try require(model.notice.isEmpty, "Folder validation does not use the toast")
            print("PASS: folder validation and Escape restore the unconfirmed folder and Library page")

            var panel: NSOpenPanel?
            model.makeSetupPanel = { let value = NSOpenPanel(); panel = value; return value }
            await settle()
            try await click("Choose Folder…", in: window)
            try require(window.attachedSheet != nil && panel?.sheetParent === window, "Choose Folder must attach its native panel to the launcher")
            panel?.cancel(nil)
            await settle()
            try require(window.attachedSheet == nil, "Cancel closes only the folder sheet")
            print("PASS: Choose Folder opens an attached native sheet; Cancel returns to setup")

            model.cancelSetup(); model.preview("setup-summary-approximate")
            await settle()
            try require(AccessibilityTree.elements(of: window).contains { $0.label.contains("Clouds and much") || $0.value.contains("Clouds and much") }, "Approximate summary states the visual compromise")
            ConfigsPreview.press(window, keyCode: 36, characters: "\r")
            await settle()
            try require(!model.onboard && model.page == .home, "Return on summary opens the launcher without starting a game")
            model.preview("setup-summary-ready")
            await settle()
            try await click("Find a Server", in: window)
            try require(!model.onboard && model.page == .servers, "Find a Server completes setup into Servers")
            print("PASS: summary Return opens Home; Find a Server completes into Servers")

            model.preview("home-notice")
            await settle()
            let elements = AccessibilityTree.elements(of: window)
            let dismiss = elements.first { $0.label == "Dismiss message" && $0.role == "AXButton" }
            let launch = elements.first { $0.label.hasPrefix("Launch on ") && $0.role == "AXButton" }
            try require(dismiss != nil, "The status must have a Dismiss message button")
            try require(launch != nil, "Launch must remain in the accessibility tree: \(elements.filter { $0.role == "AXButton" }.map(\.label))")
            try require(!dismiss!.frame.intersects(launch!.frame), "Status must not cover Launch")
            let oldNotice = model.notice
            model.notice = "The game closed. You can launch again."
            await settle()
            let updatedStatus = AccessibilityTree.elements(of: window)
            try require(updatedStatus.contains { $0.label == model.notice || $0.value == model.notice }, "Updated status is available to VoiceOver")
            try require(!updatedStatus.contains { $0.label == oldNotice || $0.value == oldNotice }, "Status must not retain a stale accessible label")
            try await click("Dismiss message", in: window)
            try require(model.notice.isEmpty, "Dismiss clears the status")
            model.gameRunning = true
            try require(!model.canNavigate && !model.canImport, "Game-running command guards")
            model.preview("home-game-running")
            await settle()
            let runningButtons = AccessibilityTree.elements(of: window).filter { $0.role == "AXButton" }.map(\.label)
            try require(runningButtons.contains("Return to Game") && !runningButtons.contains("Dismiss message"), "The running-game explanation must keep its way back to the game")
            model.gameRunning = false; model.onboard = true
            try require(!model.canNavigate && !model.canImport, "Setup command guards")
            print("PASS: status leaves Launch accessible; setup and game-running command guards hold")
            return true
        } catch {
            print("FAIL: shell actions: \((error as? Failure)?.message ?? error.localizedDescription)")
            return false
        }
    }
    static func accessibility(output: URL) async -> Bool {
        var passed = true
        for screen in Snapshots.setupScreens {
            let model = LauncherModel(snapshot: true); model.preview(screen)
            let window = await Snapshots.show(model: model, dark: true, glass: true)
            AccessibilityTree.wake(window)
            await settle()
            let tree = AccessibilityTree.read(window)
            let unnamed = AccessibilityTree.unnamed(tree.elements)
            let roleless = AccessibilityTree.roleless(tree.elements)
            let primary = tree.elements.contains { $0.role == "AXButton" && $0.label == (model.step == 3 ? model.setupCompletionTitle : model.step == 2 ? (model.shaderBusy ? "Preparing Shaders…" : "Review Setup") : model.step == 1 && (!model.keyVerified || model.replacingKey) ? "Save Key and Continue" : "Continue") }
            let indicators = tree.elements.filter { $0.label.hasPrefix("Step ") || $0.value.hasPrefix("Step ") }
            passed = passed && unnamed.isEmpty && roleless.isEmpty && tree.problems.isEmpty && primary && indicators.count == 4
            print("A11Y \(screen): \(unnamed.count) unnamed controls, \(roleless.count) roleless elements, \(indicators.count) steps, primary \(primary)")
            for item in roleless { print("A11Y roleless: \(item.line)") }
            for (item, why) in unnamed { print("A11Y unnamed: \(item.line) \(why)") }
            try? tree.elements.map(\.line).joined(separator: "\n").write(to: output.appendingPathComponent(screen + ".txt"), atomically: true, encoding: .utf8)
            window.orderOut(nil); window.close()
        }
        print(passed ? "PASS: every setup control is named, all steps have roles, and each primary action is present" : "FAIL: setup accessibility")
        return passed
    }
}
