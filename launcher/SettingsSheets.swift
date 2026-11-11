import SwiftUI
import AppKit

// Owner: ship-settings. Sheets opened from Settings, the player card and the Help menu: Replace CD
// Key, What's New, Report a Problem and Acknowledgements. Minimal bodies on the shared SheetScaffold;
// ship-settings finishes them against the AllSheets board.

struct ReplaceKeySheet: View {
    @ObservedObject var model: LauncherModel
    @State private var key = ""
    @State private var error: String?
    @Environment(\.palette) private var palette
    private var valid: Bool { KeyStore.valid(key) }
    var body: some View {
        SheetScaffold(icon: .symbol("key"), title: "Replace your CD key",
                      message: "Enter the key from your Call of Duty 2 case or store account. It replaces the one saved on this Mac.",
                      actions: [.cancel { model.dismissSheet() }, .primary("Replace Key", enabled: valid, replace)]) {
            VStack(alignment: .leading, spacing: 8) {
                LauncherTextField("CD key", text: $key, prompt: "XXXX XXXX XXXX XXXX XXXX", shape: .capsule, monospaced: true,
                                  invalid: !key.isEmpty && !valid)
                if let error {
                    Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
    }
    private func replace() {
        error = model.saveReplacementKey(key)
        if error == nil { model.dismissSheet() }
    }
}

struct WhatsNewSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SheetScaffold(icon: .mark, title: "What’s new in CoD2 Silicon", message: "Version \(model.version)",
                      actions: [SheetAction(title: "Release Notes", role: .leading, action: model.openReleaseNotes),
                                .primary("Done") { model.dismissSheet() }]) {
            VStack(alignment: .leading, spacing: 14) {
                ForEach(Array(model.releaseNotes.enumerated()), id: \.offset) { _, note in
                    VStack(alignment: .leading, spacing: 4) {
                        Text(note.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                        Text(note.summary).font(.system(size: 13)).foregroundStyle(palette.body).fixedSize(horizontal: false, vertical: true)
                    }
                    .accessibilityElement(children: .combine)
                }
            }
        }
    }
}

struct ReportProblemSheet: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        SheetScaffold(icon: .symbol("exclamationmark.bubble"), title: "Report a problem",
                      message: "Describe what happened in a new issue on GitHub. If the game crashed or froze, attach the newest report from Show Logs in Finder.",
                      actions: [SheetAction(title: "Show Logs in Finder", role: .leading, action: model.showLogsInFinder),
                                .cancel { model.dismissSheet() },
                                .primary("Open GitHub Issues") {
                                    NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon/issues")!)
                                    model.dismissSheet()
                                }]) {
            EmptyView()
        }
    }
}

/// The projects CoD2 Silicon builds on, shared by this sheet and Settings › About.
enum Credits {
    static let entries: [(name: String, detail: String)] = [
        ("opencod2", "The reconstructed engine this port builds on"),
        ("CoD2x", "Protocol and client behaviour reference, 1.4.6.8"),
        ("SDL3 · sdl2-compat", "Window, input and audio · zlib licence"),
        ("vkd3d-shader", "Builds your original shaders · LGPL 2.1 or later"),
        ("Big Shoulders · Courier Prime", "Typefaces · SIL Open Font License 1.1"),
    ]
    static let full = URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/CREDITS.md")!
    static let notice = URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/NOTICE.md")!
}

struct AcknowledgementsSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SheetScaffold(icon: .symbol("heart.text.square"), title: "Acknowledgements", message: "CoD2 Silicon is built on the work of these projects.",
                      actions: [SheetAction(title: "Full Credits", role: .leading) { NSWorkspace.shared.open(Credits.full) },
                                .primary("Done") { model.dismissSheet() }]) {
            VStack(alignment: .leading, spacing: 10) {
                ForEach(Credits.entries, id: \.name) { credit in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(credit.name).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                        Text(credit.detail).font(.system(size: 12)).foregroundStyle(palette.secondary)
                    }
                    .accessibilityElement(children: .combine)
                }
            }
        }
    }
}
