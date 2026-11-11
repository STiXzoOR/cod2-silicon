import AppKit
import SwiftUI

// Review harness, owner ship-servers. Servers screens (one per state on the Servers boards and the
// server sheets) and the servers part of the page actions.

extension Snapshots {
    static let serversScreens = ["servers", "servers-filters", "servers-connect", "servers-connect-error", "servers-password", "servers-version",
                                 "servers-remove-favorite"]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let serversPageScreens = ["servers-refreshing", "servers-refreshing-empty", "servers-never", "servers-filtered", "servers-no-reply",
                                     "servers-favorites", "servers-favorites-empty", "servers-recent", "servers-inspector-empty"]
    static let serversAccessibilityScreens = ["servers", "servers-filters", "servers-connect", "servers-password", "servers-version", "servers-remove-favorite"]
    static let serversLabels: [String: [String]] = [
        "servers": ["Which servers", "Search servers, maps or mods", "Filters", "Connect to Address…", "Refresh server list", "Sorted by ping",
                    "Launch on Northern Lights | Public", "Copy address", "Remove from favorites", "Roster, 18 of 24 players"],
        "servers-filters": ["Filters, 2 on", "Game version", "Game mode", "Ping", "Hide empty servers", "Hide full servers",
                            "Hide servers that need a password", "Reset"],
        "servers-connect": ["Connect to an address", "Server address", "Server password", "Add to Favorites", "Recent addresses", "Launch", "Cancel"],
        "servers-connect-error": ["Use a host name or IP address"],
        "servers-password": ["“Silicon Sessions” needs a password", "Password for Silicon Sessions", "Remember for this server, on this Mac only", "Launch"],
        "servers-version": ["“Sunday Rifles · EU” runs Original 1.3", "Don’t ask again; always use the version the server runs", "Launch with Original 1.3"],
        "servers-remove-favorite": ["Remove “Desert Fox SD” from Favorites?", "Remove", "Cancel"],
        "servers-refreshing": ["Stop refreshing", "86 of 212 answered"],
        "servers-refreshing-empty": ["Stop refreshing", "FINDING SERVERS"],
        "servers-never": ["NO CONTACT YET", "Refresh Server List", "Not refreshed yet"],
        "servers-filtered": ["NO MATCH", "Clear Search and Filters", "Filters, 2 on"],
        "servers-no-reply": ["Problem: The master servers didn’t answer", "Try Again"],
        "servers-favorites": ["Launch on Northern Lights | Public"],
        "servers-favorites-empty": ["NO FAVORITES YET", "Show All Servers"],
        "servers-recent": ["Launch on Northern Lights | Public"],
        "servers-inspector-empty": ["Select a server to see its map, rules and who’s playing."],
    ]
}

/// Page actions, servers part: scope, search, filters, sorting, keyboard selection and ⌘F in the real
/// window; the join flow's password and version questions on a review model. Nothing is sent to the network.
@MainActor enum ServersChecks {
    static func pageActions(_ model: LauncherModel, window: NSWindow) async throws -> Bool {
        let sample = LauncherModel(snapshot: true)
        model.servers = sample.servers; model.library = sample.library
        model.selectedServer = model.servers.first?.address
        model.search = "no server matches this"
        guard model.selected == nil else { print("FAIL: a server hidden by the search stayed selected"); return false }
        model.navigate(to: .servers)
        try await Task.sleep(for: .milliseconds(500))
        print("PAGE: read the filtered server list")
        guard let clear = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "Clear Search" }) else {
            print("FAIL: Clear Search is not accessible in the empty list"); return false
        }
        ConfigsPreview.click(window, element: clear)
        try await Task.sleep(for: .milliseconds(300))
        guard model.search.isEmpty, model.selected != nil else { print("FAIL: Clear Search did not restore the list and selection"); return false }

        // Sorting: the active column reverses.
        let ascending = model.visibleServers.map(\.address)
        model.sortServers(by: .ping)
        guard model.visibleServers.map(\.address) == ascending.reversed() else { print("FAIL: active sort did not reverse"); return false }
        model.sortServers(by: .ping)

        // Arrow keys move the selection once a row is chosen.
        try await Task.sleep(for: .milliseconds(300))
        print("PAGE: read the sorted server list")
        guard let serverRow = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix(QuakeColors.plain(model.visibleServers[0].name) + ",") }),
              AccessibilityTree.press(serverRow) else { print("FAIL: server row has no selection action"); return false }
        try await Task.sleep(for: .milliseconds(200))
        ConfigsPreview.pressArrow(window, keyCode: 125)
        try await Task.sleep(for: .milliseconds(200))
        guard model.selected?.address == model.visibleServers[1].address else { print("FAIL: the server Down arrow did not move selection"); return false }

        // ⌘F focuses the search field: typing then searches.
        ConfigsPreview.press(window, keyCode: 3, characters: "f", modifiers: .command)
        try await Task.sleep(for: .milliseconds(400))
        ConfigsPreview.press(window, keyCode: 6, characters: "z")
        try await Task.sleep(for: .milliseconds(300))
        guard model.search == "z" else { print("FAIL: ⌘F did not put typing into the server search (search is “\(model.search)”)"); return false }
        model.search = ""
        print("PASS: Clear Search restores rows; a hidden server is never selected; the active sort reverses; Down moves the selection; ⌘F focuses search")

        // Scope and filters.
        model.serverScope = .favorites
        guard Set(model.visibleServers.map(\.address)) == model.library.favorites else { print("FAIL: Favorites shows other servers"); return false }
        model.serverScope = .recent
        guard model.visibleServers.allSatisfy({ model.library.recent.contains($0.address) }) else { print("FAIL: Recent shows other servers"); return false }
        model.serverScope = .all
        model.serverFilters.hidePassworded = true; model.serverFilters.version = .codx
        guard model.serverFilters.count == 2, !model.visibleServers.isEmpty,
              model.visibleServers.allSatisfy({ !$0.password && ServerFacts($0).isCoD2x }) else { print("FAIL: filters did not hide locked and Original 1.3 servers"); return false }
        let saved = try? JSONDecoder().decode(ServerBrowserPrefs.self, from: Data(contentsOf: model.home.appendingPathComponent("launcher-servers.json")))
        guard saved?.filters == model.serverFilters else { print("FAIL: filters were not saved"); return false }
        model.serverFilters = ServerFilters()
        print("PASS: Favorites and Recent scopes; filters hide what they say, count themselves and are saved")

        // Connect to Address input and the join link.
        let inputs: [(String, String?)] = [("203.0.113.24:28960", "203.0.113.24:28960"), ("game.example", "game.example:28960"),
                                           ("cod2://203.0.113.24:28962", "203.0.113.24:28962"), ("cod2x://%2Bconnect+203.0.113.90:28962/", "203.0.113.90:28962"),
                                           ("cod2x://connect/198.51.100.7:28960", "198.51.100.7:28960"), ("bad address;quit", nil), ("", nil)]
        for (input, expected) in inputs where ServerAddressInput.parse(input)?.address != expected {
            print("FAIL: Connect to Address read “\(input)” as \(ServerAddressInput.parse(input)?.address ?? "nothing")"); return false
        }
        guard let link = ServerAddressInput.joinLink("203.0.113.24:28960"), link == "cod2x://%2Bconnect+203.0.113.24:28960/",
              (try? LaunchLink(link))?.address == "203.0.113.24:28960" else { print("FAIL: Copy Join Link does not round-trip"); return false }
        print("PASS: Connect to Address reads addresses, host names and cod2:// and cod2x:// links; join links open in the app")

        // Joining: a locked server asks for its password; a server on the other version asks to switch for one match.
        let review = LauncherModel(snapshot: true)
        let locked = review.servers.first { $0.password }!
        review.join(locked.address)
        guard case .password(let address)? = review.sheet, address == locked.address else { print("FAIL: a locked server did not ask for its password"); return false }
        review.dismissSheet()
        let original = review.servers.first { !ServerFacts($0).isCoD2x && !$0.password }!
        let originalLink = try LaunchLink.direct(original.address)
        guard !review.admit(originalLink), case .switchVersion? = review.sheet, review.matchEngine == nil else {
            print("FAIL: launching on an Original 1.3 server did not ask to switch versions"); return false
        }
        review.dismissSheet()
        review.alwaysUseServerVersion = true
        guard review.admit(originalLink), review.chosenEngine == .original, review.normalEngine == .cod2x, review.sheet == nil else {
            print("FAIL: with Don’t ask again on, the match did not use Original 1.3"); return false
        }
        let cod2x = review.servers.first { ServerFacts($0).isCoD2x && !$0.password }!
        guard review.admit(try LaunchLink.direct(cod2x.address)), review.chosenEngine == .cod2x, review.matchEngine == nil else {
            print("FAIL: the next server did not go back to CoD2x"); return false
        }
        print("PASS: a locked server asks for its password; another game version asks first, or switches for one match with Don’t ask again")
        return try await fittingLoadout()
    }

    /// The ready check offers another saved loadout that already suits the server, and using it lasts for this game only.
    static func fittingLoadout() async throws -> Bool {
        let review = LauncherModel(snapshot: true)
        review.preview("servers-ready")
        let home = try ConfigsPreview.seed(review, screen: "servers-fitting")
        defer { try? FileManager.default.removeItem(at: home) }
        try await ConfigsPreview.prepare(review, screen: "servers-ready", home: home)
        guard let server = review.selected else { print("FAIL: the ready-check server is not selected"); return false }
        guard review.fittingLoadout(for: server.address, except: "Main") == nil else { print("FAIL: a loadout that breaks the rules was offered"); return false }
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x)
        let scrim = try store.read(id: "Scrim")
        var document = scrim.document
        // Scrim already keeps the competitive frame rate cap (250 fps); the server sets the rest while you play.
        for (name, value) in [("m_filter", "0"), ("cl_mouseAccel", "0"), ("r_picmip_manual", "0")] { try document.setSetting(named: name, value: value) }
        try store.update(id: "Scrim", document: document, expected: scrim.version, summary: "Changed 3 settings")
        try review.studio.session?.refresh()
        guard review.fittingLoadout(for: server.address, except: "Main")?.id == "Scrim" else {
            let profile = try review.readySession?.readProfile("Scrim")
            let report = profile.flatMap { review.readyChecker?.evaluate(server: server, profile: $0, mode: .cod2x) }
            print("FAIL: Scrim suits the server but wasn't offered (\(report.map { "\($0.status) " } ?? "")\(report?.details.filter { $0.status != .ok }.map { "\($0.setting) \($0.status)\($0.serverOwned ? " (server)" : "")" } ?? []))"); return false
        }
        let main = try store.read(id: "Main").version, active = review.studio.activeProfile?.id
        let check = ServerReadyCheckModel()
        check.useLoadout("Scrim", model: review, address: server.address)
        guard review.serverConnection.profileID == "Scrim", ReadyCheckCard.launchTitle(review, server) == "Launch with Scrim",
              try store.read(id: "Main").version == main, review.studio.activeProfile?.id == active else {
            print("FAIL: Use “Scrim” did not choose it for this game only"); return false
        }
        print("PASS: the ready check offers only a loadout that suits the server; Use “Scrim” chooses it for this game only")
        return true
    }
}
