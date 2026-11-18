import SwiftUI

// Owner: ship-shell. Sheets from the Help menu and the game's lifecycle (AllSheets board): Keyboard
// Shortcuts and The game couldn't start. The Quit-while-running alert goes through `model.confirm(_:)`
// (ModelShell.swift).

struct KeyboardShortcutsSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    /// The board's sixteen, then the rest of the menu bar's shortcuts, in reading order (left column first).
    static let items: [(name: String, keys: [String])] = [
        ("Launch", ["⌘", "↩"]), ("Open game menu", ["⇧", "⌘", "↩"]),
        ("Home", ["⌘", "1"]), ("Servers", ["⌘", "2"]),
        ("Library", ["⌘", "3"]), ("Configs", ["⌘", "4"]),
        ("Settings", ["⌘", ","]), ("Jump to…", ["⌘", "K"]),
        ("Find", ["⌘", "F"]), ("Refresh servers", ["⌘", "R"]),
        ("Connect to address", ["⌘", "L"]), ("Add to Favorites", ["⌘", "D"]),
        ("Import config", ["⌘", "O"]), ("Paste config", ["⇧", "⌘", "V"]),
        ("Undo", ["⌘", "Z"]), ("Quick Look", ["Space"]),
        ("Copy address", ["⇧", "⌘", "C"]), ("Server filters", ["⌥", "⌘", "F"]),
        ("New loadout", ["⌘", "N"]), ("Duplicate loadout", ["⇧", "⌘", "D"]),
        ("Export loadout", ["⇧", "⌘", "E"]), ("Hide sidebar", ["⌃", "⌘", "S"]),
        ("Enter full screen", ["⌃", "⌘", "F"]), ("Help", ["⌘", "?"]),
    ]
    var body: some View {
        SheetScaffold(icon: .symbol("keyboard"), title: "Keyboard shortcuts", message: "Every shortcut is also in the menu bar.", width: 560,
                      actions: [.primary("Done") { model.dismissSheet() }]) {
            Grid(horizontalSpacing: 22, verticalSpacing: 0) {
                ForEach(0..<(Self.items.count + 1) / 2, id: \.self) { row in
                    GridRow {
                        cell(Self.items[row * 2])
                        if row * 2 + 1 < Self.items.count { cell(Self.items[row * 2 + 1]) } else { Color.clear }
                    }
                }
            }
        }
    }
    private func cell(_ item: (name: String, keys: [String])) -> some View {
        HStack(spacing: 10) {
            Text(item.name).font(.system(size: 13)).foregroundStyle(palette.text).frame(maxWidth: .infinity, alignment: .leading)
            ShortcutKeys(item.keys)
        }
        .frame(height: 32)
        .overlay(alignment: .bottom) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine)
    }
}

/// Asked once of people who set the app up before Setup had the question (SheetCrashAsk board). Escape is
/// “Don’t Send”; Return presses nothing, because the question has no default answer.
struct CrashReportsQuestionSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var detailsShown = false
    var body: some View {
        SheetScaffold(icon: .symbol("ladybug"), title: "Send crash reports to CoD2 Silicon?",
                      message: "New in this version. If the game crashes or stops responding, the launcher can send this project a report of what went wrong. The choice is yours.",
                      width: 560,
                      actions: [SheetAction(title: "See What’s Sent…", role: .leading) { detailsShown = true },
                                .cancel("Don’t Send") { model.answerCrashReports(send: false); model.dismissSheet() },
                                SheetAction(title: "Send Crash Reports", role: .primary, returnKey: false) { model.answerCrashReports(send: true); model.dismissSheet() }]) {
            VStack(alignment: .leading, spacing: 12) {
                VStack(alignment: .leading, spacing: 10) {
                    ForEach(Array(CrashReportsQuestion.rows.enumerated()), id: \.offset) { _, item in
                        HStack(alignment: .top, spacing: 12) {
                            Image(systemName: item.symbol).font(.system(size: 14, weight: .medium)).foregroundStyle(palette.accent).frame(width: 22).accessibilityHidden(true)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(item.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                                Text(item.detail).font(.system(size: 12)).foregroundStyle(palette.secondary).lineSpacing(1).fixedSize(horizontal: false, vertical: true)
                            }
                        }
                        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                    }
                }
                .padding(14).frame(maxWidth: .infinity, alignment: .leading)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                Text(CrashReportsQuestion.changeLater).font(.system(size: 12)).foregroundStyle(palette.secondary)
            }
            .popover(isPresented: $detailsShown, arrowEdge: .bottom) { CrashReportContents(model: model) }
        }
    }
}

/// Launch failed before the game started: says what, shows the path when it's about a folder, and offers the fix.
struct GameCouldNotStartSheet: View {
    @ObservedObject var model: LauncherModel
    var reason: String
    @Environment(\.palette) private var palette
    var body: some View {
        let problem = model.launchProblem ?? .other(reason)
        SheetScaffold(icon: .symbol("exclamationmark.triangle", negative: true), title: "The game couldn’t start", message: reason, width: 480,
                      actions: [.cancel { model.dismissSheet(); model.launchProblem = nil },
                                .primary(problem.fixTitle) { model.fixLaunchProblem() }]) {
            if let path = problem.path {
                Text(path).font(.typewriter(13, bold: true)).foregroundStyle(palette.address).textSelection(.enabled)
                    .padding(.leading, 58).fixedSize(horizontal: false, vertical: true)
            }
        } note: {
            Button("Show Log", action: model.showLogsInFinder).quietAction().help("Show the game’s logs in Finder")
        }
    }
}
