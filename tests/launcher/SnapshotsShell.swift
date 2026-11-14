import AppKit
import SwiftUI

// Review harness, owner ship-shell. Setup, the sidebar, the player card, Jump to…, the shell's sheets
// and alert, and the shell checks.

extension Snapshots {
    /// One screen per Setup board state (Setup, SetupData…, SetupAgain), plus the honest in-between states.
    static let setupScreens = ["setup-welcome", "setup-data", "setup-data-found", "setup-data-error", "setup-key", "setup-key-ok", "setup-key-bad",
                               "setup-key-stored", "setup-shaders", "setup-shaders-starting", "setup-shaders-done", "setup-shaders-failed", "setup-yours",
                               "setup-ready", "setup-ready-approximate", "setup-ready-link", "setup-again", "setup-again-data"]
    /// Every ship-shell screen: setup, the player card (HomePlayer), Jump to… (HomeJump), the sidebar
    /// without favourites, the status toasts, the quit alert and The game couldn't start.
    static var shellScreens: [String] {
        setupScreens + ["home-notice", "home-game-running", "home-loadout-player-card", "home-loadout-player-card-no-key", "home-jump", "home-no-favorites",
                        "home-quit-running", "sheet-game-could-not-start", "sheet-game-could-not-start-other"]
    }
    /// Window-driving suites, run by `--suite NAME` (snapshots.py runs every suite `--list-suites` prints).
    static let shellSuites: [Suite] = [Suite(name: "shell-actions", run: ShellPreview.actions), Suite(name: "shell-accessibility", run: ShellPreview.accessibility)]
}

/// Shell and setup checks exercise the shipping views with synthetic data only.
@MainActor enum ShellPreview {
    struct Failure: Error { let message: String }
    static func require(_ condition: Bool, _ message: String) throws {
        if !condition { throw Failure(message: message) }
    }
    static func settle(_ milliseconds: Int = 650) async { try? await Task.sleep(for: .milliseconds(milliseconds)) }
    static func element(_ label: String, role: String = "AXButton", in window: NSWindow) -> AccessibilityTree.Element? {
        AccessibilityTree.elements(of: window).first { $0.role == role && $0.label == label }
    }
    static func click(_ title: String, in window: NSWindow) async throws {
        guard let button = element(title, in: window) else {
            throw Failure(message: "Missing button: \(title) (have \(AccessibilityTree.elements(of: window).filter { $0.role == "AXButton" }.map(\.label)))")
        }
        ConfigsPreview.click(window, element: button)
        await settle()
    }
    /// Typing into a field as a person does: click it, select what's there, and insert through the
    /// field editor, so SwiftUI's binding sees the edit.
    static func type(_ text: String, into element: AccessibilityTree.Element, in window: NSWindow) async throws {
        ConfigsPreview.click(window, element: element)
        await settle(300)
        guard let editor = window.firstResponder as? NSTextView else { throw Failure(message: "“\(element.label)” did not take focus") }
        editor.selectAll(nil)
        if text.isEmpty { editor.deleteBackward(nil) } else { editor.insertText(text, replacementRange: editor.selectedRange()) }
        await settle(300)
    }
    /// A key through the event queue, so local monitors (Jump to…) see it as they would a real one.
    static func post(_ window: NSWindow, keyCode: UInt16, characters: String, modifiers: NSEvent.ModifierFlags = []) async {
        for type in [NSEvent.EventType.keyDown, .keyUp] {
            if let event = NSEvent.keyEvent(with: type, location: .zero, modifierFlags: modifiers, timestamp: ProcessInfo.processInfo.systemUptime,
                                            windowNumber: window.windowNumber, context: nil, characters: characters,
                                            charactersIgnoringModifiers: characters, isARepeat: false, keyCode: keyCode) {
                NSApp.postEvent(event, atStart: false)
            }
        }
        await settle(400)
    }

    static func actions(output: URL) async -> Bool {
        let model = LauncherModel(snapshot: true)
        var opened: [LauncherWindowID] = []
        model.openWindowHandler = { opened.append($0) }
        model.preview("setup-welcome")
        let window = await Snapshots.show(model: model, dark: true, glass: true)
        defer { window.orderOut(nil); window.close() }
        AccessibilityTree.wake(window)
        do {
            // Welcome → Game data → CD key (stored) → Shaders → Make it yours → Ready, with real clicks.
            try await click("Get Started", in: window)
            try require(model.setupStep == .data && model.setupCanContinue, "Get Started opens Game data with the found folder")
            try await click("Continue", in: window)
            try require(model.setupStep == .key && model.keyReady, "A stored key needs no typing")
            try require(element("Replace…", in: window) != nil, "A stored key offers Replace…")
            try await click("Continue", in: window)
            try require(model.setupStep == .shaders && !model.shaderFailed, "Stored-key Continue reaches ready shaders")
            try await click("Back", in: window)
            try require(model.setupStep == .key && model.keyReady && model.keyInput.isEmpty, "Back keeps the stored key and asks for nothing")
            try await click("Continue", in: window)
            try await click("Continue", in: window)
            try require(model.setupStep == .yours, "Shaders › Continue reaches Make it yours")
            guard let name = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "In-game name" }) else {
                throw Failure(message: "Make it yours has no name field")
            }
            try await type("^1Red ^7Fox", into: name, in: window)
            guard let windowed = AccessibilityTree.elements(of: window).first(where: { $0.label.hasPrefix("Window. ") }) else { throw Failure(message: "No Window display choice") }
            ConfigsPreview.click(window, element: windowed)
            await settle()
            try require(model.setupName == "^1Red ^7Fox" && model.setupDisplayMode == "windowed", "Name and display choice are held until Continue (\(model.setupName), \(model.setupDisplayMode))")
            try require(model.settings.fullscreen != "windowed", "Make it yours changes nothing before Continue")
            try await click("Continue", in: window)
            try require(model.setupStep == .ready && model.playerName == "^1Red ^7Fox" && model.settings.fullscreen == "windowed", "Continue applies the name and the display mode")
            let summary = AccessibilityTree.elements(of: window).filter { ($0.label + " " + $0.value).contains("Fox") }
            try require(summary.contains { ($0.label + " " + $0.value).contains("Red Fox · Window at") }, "Ready summarises the real name and display: \(summary.map(\.line))")
            ConfigsPreview.press(window, keyCode: 36, characters: "\r")
            await settle()
            try require(!model.onboard && model.page == .home, "Return on Ready opens the launcher without starting a game")
            print("PASS: six setup steps with real clicks; a stored key is kept across Back; Make it yours applies only on Continue")

            // CD key entry: checksum as you type, Skip for Now.
            model.preview("setup-key")
            await settle()
            guard let key = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "CD key, twenty characters" }) else {
                throw Failure(message: "The CD key step has no key field")
            }
            try await type("0000-0000-0000-0000-86D0", into: key, in: window)
            try require(model.keyCheck == .mismatch && !model.setupCanContinue, "A full key that fails the checksum blocks Continue")
            try require(AccessibilityTree.elements(of: window).contains { $0.label.contains("checksum doesn’t match") || $0.value.contains("checksum doesn’t match") }, "The mismatch is explained beside the field")
            try await type(LauncherModel.previewKey, into: key, in: window)
            try require(model.keyCheck == .verified && model.setupCanContinue && element("Skip for Now", in: window) == nil, "A verified checksum enables Continue in place of Skip")
            try await type("", into: key, in: window)
            try await click("Skip for Now", in: window)
            try require(model.setupStep == .shaders && !model.keyVerified && model.keyInput.isEmpty, "Skip for Now moves on without storing anything")
            print("PASS: the CD key checksum verifies as you type; Skip for Now stores nothing")

            // Set Up Again: Cancel at every step restores the folder and the page it came from.
            model.onboard = false; model.navigate(to: .library); model.restartSetup()
            await settle()
            try require(model.canCancelSetup && element("Cancel", in: window) != nil, "Set Up Again shows Cancel")
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
            try FileManager.default.removeItem(at: folder.appendingPathComponent("main/iw_04.iwd"))
            try require(!model.chooseData(folder) && model.dataError.hasPrefix("This folder is missing main/iw_04.iwd. ") && !model.setupCanContinue,
                        "A folder missing one archive names it and blocks Continue: \(model.dataError)")
            try require(model.dataArchivesFound == 15, "The refused folder's archives are counted")
            try require(model.notice.isEmpty, "Folder validation does not use the toast")
            var panel: NSOpenPanel?
            model.makeSetupPanel = { let value = NSOpenPanel(); panel = value; return value }
            await settle()
            try await click("Choose Folder…", in: window)
            try require(window.attachedSheet != nil && panel?.sheetParent === window, "Choose Folder must attach its native panel to the launcher")
            panel?.cancel(nil)
            for _ in 0..<20 where window.attachedSheet != nil { await settle(100) }
            await settle()
            try require(window.attachedSheet == nil, "Cancel closes only the folder sheet")
            // A click on a window that isn't key only activates it, so give the window key back first.
            window.makeKeyAndOrderFront(nil)
            await settle(300)
            try require(window.isKeyWindow, "The launcher window is key again after the folder sheet")
            // The setup bar's Cancel: the lowest one on screen (a closing panel can still list its own).
            let cancels = AccessibilityTree.elements(of: window).filter({ $0.role == "AXButton" && $0.label == "Cancel" })
            guard let cancel = cancels.max(by: { $0.frame.minY < $1.frame.minY }) else {
                throw Failure(message: "Set Up Again has no Cancel")
            }
            ConfigsPreview.click(window, element: cancel)
            await settle()
            // AppKit can drop the first posted click after the system's folder panel closes, although the
            // window is key and only this Cancel is listed (about half the runs; a click 0.7 s later
            // always lands). The check is that Cancel leaves setup, so a dropped click is posted again.
            if model.onboard {
                await settle(700)
                ConfigsPreview.click(window, element: cancel)
                await settle()
            }
            try require(!model.onboard && model.page == .library, "Cancel leaves Set Up Again for the page it came from (onboard \(model.onboard), page \(model.page), sheet \(window.attachedSheet != nil))")
            print("PASS: Set Up Again cancels from any step with nothing changed; a wrong folder names what's missing; Choose Folder attaches its panel")

            // Ready › Find a Server; a join link during setup waits for setup to finish.
            model.preview("setup-ready")
            model.receiveLink("cod2x://203.0.113.24:28960")
            try require(model.sheet == nil && model.pendingJoinAddress == "203.0.113.24:28960", "A link during setup waits")
            await settle()
            try await click("Find a Server", in: window)
            try require(!model.onboard && model.page == .servers, "Find a Server completes setup into Servers")
            if case .connect(let address) = model.sheet { try require(address == "203.0.113.24:28960", "The waiting link fills Connect to Address") }
            else { throw Failure(message: "Setup's end opens Connect to Address for the waiting link") }
            model.dismissSheet()
            await settle()
            print("PASS: Find a Server completes into Servers; a link that arrived during setup opens Connect to Address afterwards")

            // Links never start the game: each opens Connect to Address with the address filled in.
            for (link, address) in [("cod2x://203.0.113.24:28960", "203.0.113.24:28960"), ("cod2://198.51.100.7:28960/", "198.51.100.7:28960"),
                                    ("cod2x://connect/127.0.0.1:28961", "127.0.0.1:28961"), ("cod2x://connect%20localhost%3A28963%20password%20%22p%22", "localhost:28963")] {
                try require(LauncherModel.joinAddress(link) == address, "\(link) reads as \(address), not \(LauncherModel.joinAddress(link) ?? "nil")")
            }
            try require(LauncherModel.joinAddress("cod2x://evil;quit") == nil && LauncherModel.joinAddress("https://example.com") == nil, "Other links are refused")
            model.receiveLink("cod2x://203.0.113.24:28960")
            await settle()
            if case .connect(let address) = model.sheet { try require(address == "203.0.113.24:28960" && !model.gameRunning, "A link opens Connect to Address and starts nothing") }
            else { throw Failure(message: "A join link must open Connect to Address") }
            model.dismissSheet()
            await settle()
            let passwordLink = "cod2x://connect%20localhost%3A28960%20password%20%22fixture%22"
            model.receiveLink(passwordLink)
            if case .connect(let input) = model.sheet {
                try require(input.flatMap(ServerAddressInput.parse)?.password == "fixture", "A join link keeps its password for Connect to Address")
            } else { throw Failure(message: "A passworded link must open Connect to Address") }
            model.dismissSheet()
            print("PASS: cod2x and cod2 links open Connect to Address with the address filled in and never start the game")

            // Sidebar: labelled pages, starred servers with player counts, the Settings row, the plate.
            model.preview("home")
            await settle()
            let sidebar = AccessibilityTree.elements(of: window)
            for label in ["Home, Command 1", "Servers, Command 2", "Library, Command 3", "Configs, Command 4", "Settings, Command comma",
                          "Favorite server Northern Lights, 18 of 24 players"] {
                try require(sidebar.contains { $0.label == label || $0.value == label }, "The sidebar names “\(label)”")
            }
            opened.removeAll()
            try await click("Settings, Command comma", in: window)
            try require(opened.contains(.settings), "The Settings row opens the Settings window")
            guard let star = sidebar.first(where: { $0.label.hasPrefix("Favorite server Desert Fox") }) else { throw Failure(message: "Desert Fox is starred in the preview") }
            ConfigsPreview.click(window, element: star)
            await settle()
            try require(model.page == .servers && model.selectedServer == "203.0.113.90:28962", "A starred server opens Servers with it selected")
            print("PASS: sidebar rows are named with their shortcuts; Settings opens its window; a starred server opens in Servers")

            // Player card: opened from the plate, saved as you type into the synthetic Main loadout.
            let loadouts = try ConfigsPreview.seed(model, screen: "home-loadout-player-card")
            defer { try? FileManager.default.removeItem(at: loadouts) }
            try await ConfigsPreview.prepare(model, screen: "home-loadout-player-card", home: loadouts)
            model.navigate(to: .home)
            await settle()
            guard let plate = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasSuffix("Open your player card") }) else {
                throw Failure(message: "The player plate is not a button")
            }
            ConfigsPreview.click(window, element: plate)
            await settle(900)
            try require(model.playerCardPresented, "The plate opens the player card")
            guard let card = NSApp.windows.first(where: { $0 !== window && $0.isVisible && String(describing: Swift.type(of: $0)).contains("Popover") }) else {
                throw Failure(message: "The player card is not a visible popover")
            }
            AccessibilityTree.wake(card)
            guard let field = AccessibilityTree.elements(of: card).first(where: { $0.role == "AXTextField" && $0.label == "In-game name" }) else {
                throw Failure(message: "The player card has no name field")
            }
            try await type("^2Moss", into: field, in: card)
            await settle(900)
            try require(model.playerName == "^2Moss" && model.playerCardStatus == "Saved to your Main loadout.", "The name saves after a pause: \(model.playerName) / \(model.playerCardStatus ?? "nil")")
            let active = ((try? String(contentsOf: loadouts.appendingPathComponent("main/players/active.txt"), encoding: .isoLatin1)) ?? "Main")
                .trimmingCharacters(in: .whitespacesAndNewlines)
            let saved = (try? String(contentsOf: loadouts.appendingPathComponent("main/players/\(active)/config_mp.cfg"), encoding: .isoLatin1)) ?? ""
            let names = saved.split(separator: "\n").filter { $0.lowercased().contains(" name ") }
            try require(names.contains { $0.contains("\"^2Moss\"") }, "The name is written to the \(active) loadout's config_mp.cfg: \(names)")
            try await type("bad\"name", into: field, in: card)
            try require(model.playerCardError != nil && model.playerName == "^2Moss", "A name with a quote is refused and nothing is saved")
            try await type("^2Moss", into: field, in: card)
            if let done = AccessibilityTree.elements(of: card).first(where: { $0.role == "AXButton" && $0.label == "Done" }) { AccessibilityTree.press(done) }
            await settle()
            try require(!model.playerCardPresented, "Done closes the player card")
            print("PASS: the player card saves the name as you type, refuses quotes, and Done closes it")

            // Jump to…: finds servers in memory, moves with ↓, Return opens, Escape closes.
            model.showJump()
            await settle(900)
            try require(model.jumpPresented, "⌘K's action opens Jump to…")
            guard let jump = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "Jump to" }) else {
                throw Failure(message: "Jump to… has no search field")
            }
            try await type("north", into: jump, in: window)
            let found = JumpIndex.items(model, query: "north")
            try require(found.first?.title == "Northern Lights | Public" && found.first?.section == .servers, "“north” finds Northern Lights first: \(found.map(\.title))")
            let settingsRows = JumpIndex.items(model, query: "vertical")
            try require(settingsRows.contains { $0.title == "Vertical sync" && $0.detail == "Settings › Display" }, "Settings rows are found by their visible names")
            await post(window, keyCode: 53, characters: "\u{1b}")
            try require(!model.jumpPresented, "Escape closes Jump to…")
            model.showJump()
            await settle(900)
            guard let again = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXTextField" && $0.label == "Jump to" }) else {
                throw Failure(message: "Jump to… has no search field the second time")
            }
            try await type("sessions", into: again, in: window)
            await post(window, keyCode: 36, characters: "\r")
            try require(!model.jumpPresented && model.page == .servers && model.selectedServer == "192.0.2.10:28960", "Return on a server shows it in Servers")
            print("PASS: Jump to… finds servers, settings and actions; Return opens; Escape closes")

            // File › Launch targets the Servers selection, otherwise the last server.
            model.page = .servers; model.selectedServer = "198.51.100.7:28960"
            try require(model.selected?.address == "198.51.100.7:28960", "Servers shows a selection")
            let selectedLaunch = JumpIndex.items(model, query: "Launch").first { $0.keys == "⌘↩" }
            try require(selectedLaunch?.title == model.selected.map { "Launch on \(QuakeColors.plain($0.name))" }, "Jump’s Launch action names the visible Servers selection")
            model.page = .home
            let recentLaunch = JumpIndex.items(model, query: "Launch").first { $0.keys == "⌘↩" }
            try require(recentLaunch?.title == model.lastServer.map { "Launch on \(QuakeColors.plain($0.name))" }, "Away from Servers, Jump’s Launch action names the last server")

            // Sheets and the quit alert.
            model.preview("sheet-game-could-not-start")
            await settle()
            let sheetWindow = window.attachedSheet ?? window
            AccessibilityTree.wake(sheetWindow)
            let failed = AccessibilityTree.elements(of: sheetWindow)
            for label in ["Show Log", "Cancel", "Choose Folder…"] { try require(failed.contains { $0.role == "AXButton" && $0.label == label }, "The game couldn’t start offers \(label)") }
            try require(failed.contains { $0.label.contains("/Volumes/Games/CoD2") || $0.value.contains("/Volumes/Games/CoD2") }, "The sheet shows the missing folder")
            if let cancel = failed.first(where: { $0.role == "AXButton" && $0.label == "Cancel" }) { AccessibilityTree.press(cancel) }
            await settle()
            try require(model.sheet == nil && model.launchProblem == nil, "Cancel closes The game couldn’t start")
            model.gameRunning = true
            model.confirmQuitWithGame()
            await settle()
            try require(model.confirmation?.actionTitle == "Quit Launcher Only" && model.confirmation?.others.map(\.title) == ["Quit Both"] && model.confirmation?.destructive == false,
                        "Quitting with the game open asks Quit Launcher Only, Quit Both or Cancel")
            try require(window.attachedSheet != nil, "The quit question is an alert attached to the window")
            model.confirmation = nil
            await settle()
            print("PASS: The game couldn’t start names the folder and offers Choose Folder…, Show Log and Cancel; quitting with the game open asks first")

            // Dock menu: recent servers, Launch on Last Server, Open Game Menu, Connect to Address…; dimmed while the game runs.
            let target = DockMenuTarget()
            let running = model.dockMenu(target: target)
            try require(running.items.first { $0.title == "Launch on Last Server" }?.isEnabled == false, "Launch is dimmed while the game runs")
            model.gameRunning = false
            let menu = model.dockMenu(target: target)
            let titles = menu.items.filter { !$0.isSeparatorItem }.map(\.title)
            try require(titles == ["Recent servers", "Northern Lights | Public", "Sunday Rifles · EU", "Eastern Front", "Launch on Last Server", "Open Game Menu", "Connect to Address…"],
                        "The Dock menu matches the Surfaces board: \(titles)")
            try require(menu.items.first { $0.title == "Connect to Address…" }?.isEnabled == true, "Connect to Address… is available from the Dock")
            print("PASS: the Dock menu lists recent servers and the three actions, dimmed while the game runs")

            // Toasts.
            model.preview("home-notice")
            await settle()
            let elements = AccessibilityTree.elements(of: window)
            let dismiss = elements.first { $0.label == "Dismiss message" && $0.role == "AXButton" }
            try require(dismiss != nil, "The status must have a Dismiss message button")
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
            print("PASS: the status toast updates for VoiceOver and dismisses; setup and game-running command guards hold")
            return true
        } catch {
            print("FAIL: shell actions: \((error as? Failure)?.message ?? error.localizedDescription)")
            return false
        }
    }

    /// Every setup and shell screen: no unnamed control, no roleless text, the step's default action
    /// present by its title, and setup's progress read as one “Step N of 6, …” element.
    static func accessibility(output: URL) async -> Bool {
        var passed = true
        for screen in Snapshots.shellScreens {
            let model = LauncherModel(snapshot: true); model.preview(screen)
            // The player card's screens use the harness's synthetic loadouts, as their captures do.
            var home: URL?
            if screen.hasPrefix("home-loadout") {
                do { home = try ConfigsPreview.seed(model, screen: screen); try await ConfigsPreview.prepare(model, screen: screen, home: home!) }
                catch { print("FAIL: loadouts for \(screen): \(error)"); passed = false; continue }
            }
            defer { if let home { try? FileManager.default.removeItem(at: home) } }
            let window = await Snapshots.show(model: model, dark: true, glass: true, screen: screen)
            await settle()
            let popover = NSApp.windows.first { $0 !== window && $0.isVisible && String(describing: Swift.type(of: $0)).contains("Popover") }
            let target = window.attachedSheet ?? popover ?? window
            AccessibilityTree.wake(target)
            let tree = AccessibilityTree.read(target)
            let unnamed = AccessibilityTree.unnamed(tree.elements)
            let roleless = AccessibilityTree.roleless(tree.elements)
            var ok = unnamed.isEmpty && roleless.isEmpty && tree.problems.isEmpty
            var detail = ""
            if model.onboard {
                let primary = tree.elements.contains { $0.role == "AXButton" && $0.label == model.setupPrimaryTitle }
                let step = model.setupStep
                let indicators = tree.elements.filter { $0.label == "Step \(step.number) of 6, \(step.title)" || $0.value == "Step \(step.number) of 6, \(step.title)" }
                ok = ok && primary && indicators.count == 1
                detail = ", primary “\(model.setupPrimaryTitle)” \(primary), step indicator \(indicators.count)"
            }
            let expected = labels[screen] ?? []
            // Rows read as one element ("path, 4 of 16 game archives found"), so a label may be part of one.
            let missing = expected.filter { label in !tree.elements.contains { $0.label.contains(label) || $0.value.contains(label) } }
            ok = ok && missing.isEmpty
            passed = passed && ok
            print("A11Y \(screen): \(unnamed.count) unnamed controls, \(roleless.count) roleless elements\(detail)\(missing.isEmpty ? "" : ", missing \(missing)")")
            for item in roleless { print("A11Y roleless: \(item.line)") }
            for (item, why) in unnamed { print("A11Y unnamed: \(item.line) \(why)") }
            try? tree.elements.map(\.line).joined(separator: "\n").write(to: output.appendingPathComponent(screen + ".txt"), atomically: true, encoding: .utf8)
            popover?.orderOut(nil)
            window.attachedSheet.map { window.endSheet($0) }
            window.orderOut(nil); window.close()
        }
        print(passed ? "PASS: every setup and shell control is named, nothing is roleless, each step's action and progress are read" : "FAIL: shell accessibility")
        return passed
    }
    /// Labels a screen must expose, so an empty or unbuilt tree can't pass.
    static let labels: [String: [String]] = [
        "setup-welcome": ["Welcome to CoD2 Silicon", "Get Started"],
        "setup-data": ["Choose Folder…", "Don’t have the files yet?"],
        "setup-data-found": ["Choose a Different Folder…", "Game data found"],
        "setup-data-error": ["Choose Folder…", "4 of 16 game archives found"],
        "setup-key": ["CD key, twenty characters", "Paste", "Skip for Now"],
        "setup-key-bad": ["CD key, twenty characters", "Skip for Now"],
        "setup-key-stored": ["Replace…", "Verified"],
        "setup-shaders": ["Building shaders"],
        "setup-shaders-failed": ["Try Again", "Choose Mac Copy…"],
        "setup-yours": ["In-game name", "Skip"],
        "setup-ready": ["Find a Server", "Open CoD2 Silicon"],
        "setup-again": ["Cancel", "Back", "Replace…"],
        "home-loadout-player-card": ["In-game name", "Replace…", "Loadout for your next game", "Done", "Add colours…"],
        "home-loadout-player-card-no-key": ["Enter Key…", "Not entered"],
        "home-jump": ["Jump to"],
        "home-quit-running": ["Quit Launcher Only", "Quit Both", "Cancel"],
        "sheet-game-could-not-start": ["Choose Folder…", "Show Log", "Cancel"],
        "sheet-game-could-not-start-other": ["Try Again", "Show Log", "Cancel"],
    ]
}
