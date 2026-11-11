import AppKit
import SwiftUI

// Review harness, owner ship-servers. Servers screens and the servers part of the page actions.

extension Snapshots {
    static let serversScreens = ["servers", "servers-empty"]
    /// Also rendered in the `--group pages` runs and read by `--accessibility`.
    static let serversPageScreens = ["servers-no-cache", "servers-inspector-empty", "servers-direct-error", "servers-refreshing", "servers-refreshing-empty"]
    static let serversAccessibilityScreens = ["servers", "servers-empty"]
    static let serversLabels: [String: [String]] = [
        "servers": ["Hide empty servers", "Hide full servers"],
        "servers-empty": ["Clear Filters", "Server address"],
        "servers-no-cache": ["Refresh", "Server address"],
        "servers-refreshing-empty": ["Stop Refreshing", "Server address"],
        "servers-inspector-empty": ["CHOOSE A SERVER", "Select First Server"],
        "servers-direct-error": ["Use a hostname or IP address"],
    ]
}

/// Page actions, servers part: filters, sorting, keyboard selection and direct-connect validation in the real window.
@MainActor enum ServersChecks {
    static func pageActions(_ model: LauncherModel, window: NSWindow) async throws -> Bool {
        let sample = LauncherModel(snapshot: true)
        model.servers = sample.servers; model.library = sample.library
        model.selectedServer = model.servers.first?.address
        model.search = "no server matches this"
        guard model.selected == nil else { print("FAIL: filtered-out server remained in the inspector"); return false }
        model.page = .servers
        try await Task.sleep(for: .milliseconds(400))
        print("PAGE: read the filtered server list")
        guard let clear = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label == "Clear Filters" }) else {
            print("FAIL: Clear Filters is not accessible in the empty list"); return false
        }
        ConfigsPreview.click(window, element: clear)
        try await Task.sleep(for: .milliseconds(300))
        guard model.search.isEmpty, model.selected != nil else { print("FAIL: Clear Filters did not restore the list and selection"); return false }
        let ascending = model.visibleServers.map(\.address)
        model.sortServers(by: .ping)
        guard model.visibleServers.map(\.address) == ascending.reversed() else { print("FAIL: active sort did not reverse"); return false }
        try await Task.sleep(for: .milliseconds(300))
        print("PAGE: read the sorted server list")
        guard let serverRow = AccessibilityTree.elements(of: window).first(where: { $0.role == "AXButton" && $0.label.hasPrefix(QuakeColors.plain(model.visibleServers[0].name) + ",") }),
              AccessibilityTree.press(serverRow) else { print("FAIL: server row has no selection action"); return false }
        try await Task.sleep(for: .milliseconds(200))
        ConfigsPreview.pressArrow(window, keyCode: 125)
        try await Task.sleep(for: .milliseconds(200))
        guard model.selected?.address == model.visibleServers[1].address else { print("FAIL: the server Down arrow did not move selection"); return false }
        model.directPassword = "direct-only"; model.serverPassword = "selected-only"
        model.directAddress = "bad address;quit"
        model.connect()
        guard model.directConnectError != nil, model.directPassword == "direct-only", model.serverPassword == "selected-only" else { return false }
        model.directAddress = "203.0.113.24:28960"
        guard model.directAddressError == nil else { return false }
        print("PASS: real Clear Filters click restores rows; hidden selection clears; active sort reverses; direct validation and passwords stay separate")
        return true
    }
}
