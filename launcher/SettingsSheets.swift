import SwiftUI
import AppKit

// Owner: ship-settings. Sheets opened from Settings, the player card, Home and the Help menu, built against the
// AllSheets board: Replace CD Key, What's New, Report a Problem and Acknowledgements.

struct ReplaceKeySheet: View {
    @ObservedObject var model: LauncherModel
    @State private var key = ""
    @State private var error: String?
    @FocusState private var focused: Bool
    @Environment(\.palette) private var palette
    private var raw: String { KeyFormat.groups(key).joined() }
    private var valid: Bool { KeyStore.valid(key) }
    var body: some View {
        let replacing = model.gameData.keyEnding != nil || model.keyVerified
        SheetScaffold(icon: .symbol("key"), title: replacing ? "Replace your CD key" : "Add your CD key",
                      message: replacing ? "The key you have now keeps working until you press Replace." : "The 20-character key that came with your game. Joining a server needs it.",
                      actions: [.cancel { model.dismissSheet() }, .primary(replacing ? "Replace Key" : "Save Key", enabled: valid, save)]) {
            VStack(alignment: .leading, spacing: 12) {
                TextField("CD key", text: Binding(get: { KeyFormat.display(key) }, set: { key = KeyFormat.display($0); error = nil }),
                          prompt: Text("XXXX XXXX XXXX XXXX XXXX").foregroundColor(palette.tertiary))
                    .textFieldStyle(.plain).font(.typewriter(20, bold: true)).tracking(2)
                    .foregroundStyle(palette.text).autocorrectionDisabled().focused($focused)
                    .padding(.horizontal, 22).frame(height: 54)
                    .background(palette.field, in: Capsule())
                    .overlay(Capsule().strokeBorder(stroke, lineWidth: focused || (raw.count == 20) ? 2 : 1))
                    .shapeAudit(.field, .capsule, "key field")
                    .onSubmit { if valid { save() } }
                    .accessibilityLabel("CD key, twenty characters")
                Label(error ?? hint, systemImage: symbol)
                    .font(.system(size: 13, weight: raw.count == 20 ? .semibold : .regular)).foregroundStyle(tone)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isStaticText)
            }
            .onAppear { focused = true }
        } note: {
            Text("Stored only on this Mac.")
        }
    }
    private var stroke: Color {
        if error != nil || (raw.count == 20 && !valid) { return palette.negative }
        if valid { return palette.positive }
        return focused ? palette.accent.opacity(0.75) : palette.wellStroke
    }
    private var hint: String {
        if raw.isEmpty { return "Spaces and dashes are fine." }
        if raw.count < 20 { return raw.count == 19 ? "1 character to go." : "\(20 - raw.count) characters to go." }
        return valid ? "Checksum verified." : "This key doesn’t match. Check each character against the key that came with your game."
    }
    private var symbol: String { error != nil || (raw.count == 20 && !valid) ? "exclamationmark.circle" : valid ? "checkmark.circle" : "info.circle" }
    private var tone: Color { error != nil || (raw.count == 20 && !valid) ? palette.negative : valid ? palette.positive : palette.secondary }
    private func save() {
        error = model.saveReplacementKey(key)
        if error == nil { model.dismissSheet() }
    }
}

struct WhatsNewSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        if let offer = model.updateOffer, !model.updateNotes.isEmpty {
            SheetScaffold(icon: .mark, title: "What’s new in \(offer.version)",
                          message: "You have \(model.version). This update is \(SettingsText.bytes(Int64(offer.archiveSize))).",
                          actions: [.cancel("Later") { model.dismissSheet() },
                                    .primary("Install and Relaunch", enabled: !model.gameRunning && !model.installingUpdate) {
                                        model.dismissSheet(); model.installUpdate()
                                    }]) {
                FormGroup {
                    ForEach(Array(model.updateNotes.enumerated()), id: \.offset) { _, item in NoteRow(item: item) }
                }
                if model.gameRunning {
                    Text("Quit the game to install. The update replaces the app, so it can’t run while you play.")
                        .font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
            } note: {
                Button("Full release notes") { NSWorkspace.shared.open(model.releaseURL ?? AppUpdate.releases) }.quietAction()
            }
        } else {
            SheetScaffold(icon: .mark, title: "What’s new in \(model.version)", message: "The changes in the version you have.",
                          actions: [SheetAction(title: "Release Notes", role: .leading, action: model.openReleaseNotes),
                                    .primary("Done") { model.dismissSheet() }]) {
                VStack(alignment: .leading, spacing: 14) {
                    ForEach(Array(model.releaseNotes.prefix(6).enumerated()), id: \.offset) { _, note in
                        VStack(alignment: .leading, spacing: 4) {
                            Text(note.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                            Text(note.summary).font(.system(size: 13)).foregroundStyle(palette.body).fixedSize(horizontal: false, vertical: true)
                        }
                        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                    }
                }
            }
        }
    }
}

/// One change in What's New: its kind, a line, and a line of detail.
private struct NoteRow: View {
    var item: ReleaseNoteItem
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 14) {
            Badge(item.tag, tone: item.tag == "New" ? .accent : .neutral).frame(width: 64, alignment: .leading)
            VStack(alignment: .leading, spacing: 3) {
                Text(item.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                if !item.detail.isEmpty {
                    Text(item.detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal, FormMetrics.rowInset(.inset)).padding(.vertical, 11)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }
}

/// Opens GitHub's bug form in the browser, filled in with these lines. The app itself sends nothing.
struct ReportProblemSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var includeLog = false
    @State private var hasLog = false
    @State private var copied = false
    @State private var logMessage: String?
    var body: some View {
        let details = model.reportDetails
        SheetScaffold(icon: .symbol("ladybug"), title: "Report a problem",
                      message: "Opens a new issue on GitHub in your browser, filled in with the details below. You can edit everything before you post it.",
                      actions: [SheetAction(title: copied ? "Copied" : "Copy Details", symbol: copied ? "checkmark" : nil, role: .leading) {
                                    model.copyVersionInfo(); copied = true
                                },
                                .cancel { model.dismissSheet() },
                                .primary("Open GitHub", open)]) {
            VStack(alignment: .leading, spacing: 12) {
                Text(details.text).font(.typewriter(12)).foregroundStyle(palette.secondary).lineSpacing(3)
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 16).padding(.vertical, 14)
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    .shapeAudit(.inset, .rounded(16), "report details")
                    .accessibilityLabel("Details that go in the report")
                    .accessibilityValue(details.text)
                FormGroup {
                    FormRow("Include the last game log", detail: hasLog ? "Copied to the clipboard for you to paste. It can contain server names."
                            : "There’s no game log yet. The game writes one when it crashes or freezes.") {
                        SwitchToggle("Include the last game log", isOn: $includeLog).disabled(!hasLog)
                    }
                }
                if let logMessage {
                    Label(logMessage, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true).accessibilityAddTraits(.isStaticText)
                }
                Text("Never includes your CD key or your name.").font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.horizontal, 6)
            }
            .onAppear { hasLog = model.snapshot || model.lastGameLog() != nil }
            .task(id: copied) { if copied { try? await Task.sleep(for: .seconds(2)); copied = false } }
        }
    }
    private func open() {
        if includeLog {
            let result = model.copyLastGameLog()
            guard result.copied else { logMessage = result.text; return }
        }
        model.openIssue()
        model.dismissSheet()
    }
}

/// Everything the app ships that others made, each with its licence in full (SettingsAcknowledgements.swift).
struct AcknowledgementsSheet: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        if let selected = model.selectedAcknowledgement, let item = Acknowledgements.all.first(where: { $0.id == selected }) {
            SheetScaffold(icon: .symbol("person.2"), title: item.name, message: "\(item.author) · \(item.licence)", width: 600,
                          actions: [SheetAction(title: "All Acknowledgements", symbol: "chevron.left", role: .leading) { model.selectedAcknowledgement = nil },
                                    .primary("Done") { model.selectedAcknowledgement = nil; model.dismissSheet() }]) {
                ScrollView {
                    Text(item.text).font(.typewriter(11)).foregroundStyle(palette.secondary).lineSpacing(2)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(16)
                }
                .frame(height: 320)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                .shapeAudit(.inset, .rounded(16), "licence text")
                .accessibilityLabel("\(item.name) licence")
            }
        } else {
            SheetScaffold(icon: .symbol("person.2"), title: "Acknowledgements",
                          message: "CoD2 Silicon stands on other people’s work. Select one to read its licence.", width: 600,
                          actions: [SheetAction(title: "Copy All", role: .standard) {
                                        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(Acknowledgements.plainText, forType: .string)
                                    },
                                    .primary("Done") { model.dismissSheet() }]) {
                FormGroup {
                    ForEach(Acknowledgements.all) { item in
                        Button { model.selectedAcknowledgement = item.id } label: { AcknowledgementRow(item: item) }
                            .buttonStyle(.plain)
                            .accessibilityLabel("\(item.name), \(item.author), \(item.licence)")
                            .accessibilityHint("Shows the licence")
                    }
                }
            } note: {
                Text("Every component the app ships, with its licence in full.")
            }
        }
    }
}

private struct AcknowledgementRow: View {
    var item: Acknowledgement
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 12) {
            Text(item.name).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
            Spacer(minLength: 8)
            Text(item.author).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineLimit(1).truncationMode(.middle)
            Badge(item.licence)
            Image(systemName: "chevron.right").font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.tertiary)
        }
        .padding(.horizontal, FormMetrics.rowInset(.inset))
        .frame(minHeight: 40)
        .contentShape(Rectangle())
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
    }
}
