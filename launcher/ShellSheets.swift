import SwiftUI

// Owner: ship-shell. Sheets from the Help menu and the game's lifecycle: Keyboard Shortcuts and
// The game couldn't start. Minimal bodies on the shared SheetScaffold; ship-shell finishes them
// against the AllSheets board (the Quit-while-running alert goes through `model.confirm(_:)`).

struct KeyboardShortcutsSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    /// The shortcuts the menus define today.
    static let groups: [(title: String, items: [(name: String, keys: [String])])] = [
        ("Play", [("Launch", ["⌘", "↩"]), ("Open Game Menu", ["⇧", "⌘", "↩"]), ("Refresh Server List", ["⌘", "R"])]),
        ("Go to", [("Home", ["⌘", "1"]), ("Servers", ["⌘", "2"]), ("Library", ["⌘", "3"]), ("Configs", ["⌘", "4"]),
                   ("Settings", ["⌘", ","]), ("Hide Sidebar", ["⌃", "⌘", "S"])]),
        ("Configs", [("Import Config", ["⌘", "O"]), ("Paste Config", ["⇧", "⌘", "V"])]),
    ]
    var body: some View {
        SheetScaffold(icon: .symbol("keyboard"), title: "Keyboard shortcuts", message: "Every menu command with a shortcut, in one place.",
                      actions: [.primary("Done") { model.dismissSheet() }]) {
            VStack(alignment: .leading, spacing: 16) {
                ForEach(Array(Self.groups.enumerated()), id: \.offset) { _, group in
                    VStack(alignment: .leading, spacing: 8) {
                        Text(group.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).accessibilityAddTraits(.isHeader)
                        ForEach(group.items, id: \.name) { item in
                            HStack(spacing: 10) {
                                Text(item.name).font(.system(size: 13)).foregroundStyle(palette.text).frame(maxWidth: .infinity, alignment: .leading)
                                ShortcutKeys(item.keys)
                            }
                            .accessibilityElement(children: .combine)
                        }
                    }
                }
            }
        }
    }
}

struct GameCouldNotStartSheet: View {
    @ObservedObject var model: LauncherModel
    var reason: String
    var body: some View {
        SheetScaffold(icon: .symbol("exclamationmark.triangle", negative: true), title: "The game couldn’t start", message: reason,
                      actions: [SheetAction(title: "Show Logs in Finder", role: .leading, action: model.showLogsInFinder),
                                .primary("Done") { model.dismissSheet() }]) {
            EmptyView()
        }
    }
}
