import SwiftUI
import AppKit

// Owner: share-launcher (0.4.1). Share Your Setup's screens (ShareSetup board; Sheets board kinds
// share, shareNoName, shareSending, shareOffline, shareLimit, shareAlready, shareSent and
// sharePreview): the sheet with its states, See What’s Sent…, the Sent sheet, and the Share Your
// Setup… button on Player Setups. The loadout's menus add their items in ConfigsView.swift.

struct ShareSetupSheet: View {
    @ObservedObject var model: LauncherModel
    @StateObject private var share: ConfigShareSheetModel
    private let start: ConfigShareStart
    @Environment(\.palette) private var palette

    init(model: LauncherModel, start: ConfigShareStart) {
        self.model = model; self.start = start
        let profile = model.studio.profiles.first { $0.id.caseInsensitiveCompare(start.loadout) == .orderedSame }
            ?? ConfigProfile(id: start.loadout, displayName: start.loadout, isActive: false)
        let dialect = model.studio.session?.configDialect ?? .cod2x
        _share = StateObject(wrappedValue: ConfigShareSheetModel(loadout: profile, dialect: dialect, center: model.shareCenter, app: model.version))
    }

    /// The board's `upload`, by the mapping the app already uses for its mirror image `download` in
    /// sheet icons and action buttons (Import's sheet, Add a Config: `square.and.arrow.down`). SF
    /// Symbols has no up arrow over a line (`arrow.up.to.line` puts the line above), and this is
    /// the system's Share symbol. The sheet and Player Setups' button.
    static let symbol = "square.and.arrow.up"

    var body: some View {
        let name = share.loadout.displayName
        SheetScaffold(icon: .symbol(Self.symbol), title: "Share “\(name)” with other players",
                      message: "It joins Player Setups, the public list in Configs that every CoD2 Unbound player can browse. A person looks at each setup before it is listed.",
                      actions: [.cancel { share.cancel(); model.dismissSheet() },
                                .primary(share.actionTitle, enabled: share.canSend) { share.send() }]) {
            VStack(alignment: .leading, spacing: 12) {
                // A sheet that asks for a name shows one field across its width, already focused.
                LauncherTextField("Name to show with the setup", text: $share.name, prompt: "The name players know you by", shape: .capsule,
                                  invalid: share.nameProblem != nil, autofocus: true)
                    .disabled(share.busy)
                if let problem = share.nameProblem {
                    Text(problem).font(.system(size: 12)).foregroundStyle(palette.negative).padding(.leading, 16)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityLabel("Problem: \(problem)")
                }
                // Nothing to share (ShareSetup board): only the neutral banner says why; the rows and
                // their counts would describe a setup that isn't sent.
                if share.phase != .nothingToShare {
                    FormGroup {
                        ForEach(Array(rows.enumerated()), id: \.offset) { _, row in ShareFactRow(symbol: row.symbol, title: row.title, detail: row.detail) }
                    }
                    .opacity(share.busy ? 0.45 : 1)
                }
                if let refusal = share.refusal { Banner(share.phase == .nothingToShare ? .neutral : .error, message: refusal) }
            }
        } note: {
            Button("See What’s Sent…") { share.previewShown = true }.quietAction().padding(.horizontal, -6)
                .disabled(share.busy || share.prepared == nil)
                .help("The exact text that is sent, and the lines that are left out")
        }
        .sheet(isPresented: $share.previewShown) { SharePreviewSheet(share: share, app: model.version).sheetShapeAudit() }
        .onAppear {
            let model = model
            share.sent = { record in model.present(.shareSent(record: record, loadout: name)) }
            let read = start.preset == nil ? try? model.studio.session?.readProfile(share.loadout.id) : nil
            share.start(read: read, catalog: model.studio.catalog, preset: start.preset)
        }
        .onDisappear { share.cancel() }
        // A refusal is said where the player is looking, and read out.
        .onChange(of: share.refusal) { _, refusal in if let refusal { model.studio.announce(refusal) } }
        .onChange(of: share.phase) { _, phase in if phase == .sending { model.studio.announce("Sending…") } }
    }

    private var rows: [(symbol: String, title: String, detail: String)] {
        let sent: String, left: String
        if let prepared = share.prepared {
            sent = "\(count(prepared.settings, "setting", "settings")) and \(count(prepared.binds, "key bind", "key binds")) from this loadout, the name above and the app’s version."
            left = "Your name in the game, chat lines, passwords, server addresses and file paths: \(count(prepared.leftOut.count, "line", "lines")) here. A config never holds your CD key."
        } else {
            sent = "The settings and key binds from this loadout, the name above and the app’s version."
            left = "Your name in the game, chat lines, passwords, server addresses and file paths. A config never holds your CD key."
        }
        return [("doc.text", "What is sent", sent), ("lock", "What is left out", left),
                ("globe", "Where it goes", "To cod2unbound.com, which posts it as a public request on GitHub. Anyone can copy what is public, and you can take it back at any time.")]
    }
    private func count(_ value: Int, _ one: String, _ many: String) -> String { "\(value) \(value == 1 ? one : many)" }
}

/// One fact on a sheet: its symbol tile, a title and a line of detail (as Send crash reports? shows them).
private struct ShareFactRow: View {
    var symbol: String
    var title: String
    var detail: String
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .top, spacing: 14) {
            Image(systemName: symbol).font(.system(size: 14, weight: .medium)).foregroundStyle(palette.accent)
                .frame(width: 30, height: 30)
                .background(palette.selection, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                Text(detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineSpacing(1.5).fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal, 16).padding(.vertical, 11)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
    }
}

/// See What’s Sent…: the exact text, and the lines left out with the reason beside each. Opens over
/// the Share sheet and returns to it.
struct SharePreviewSheet: View {
    @ObservedObject var share: ConfigShareSheetModel
    var app: String
    enum Tab: Hashable { case sent, leftOut }
    @State private var tab = Tab.sent
    @State private var copied = false
    @Environment(\.palette) private var palette

    var body: some View {
        let prepared = share.prepared
        SheetScaffold(icon: .symbol("doc.text"), title: "What is sent",
                      message: "This text, the name you type and the app’s version, \(ConfigShare.appVersion(app)). Nothing else leaves your Mac.",
                      width: 560,
                      actions: [SheetAction(title: copied ? "Copied" : "Copy", role: .leading) { copy() },
                                .primary("Done") { share.previewShown = false }]) {
            VStack(alignment: .leading, spacing: 12) {
                ScopeSwitch("Lines to show", selection: $tab,
                            options: [(.sent, "Sent · \(prepared?.lines ?? 0)"), (.leftOut, "Left Out · \(prepared?.leftOut.count ?? 0)")])
                    .fixedSize()
                FormGroup {
                    ScrollView {
                        Group {
                            if tab == .sent { sentText(prepared) } else { leftOutLines(prepared) }
                        }
                        .padding(.horizontal, 14).padding(.vertical, 12)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .frame(height: 248)
                }
            }
        }
        .onChange(of: tab) { _, _ in copied = false }
    }

    private func sentText(_ prepared: ConfigShare.Prepared?) -> some View {
        Text(prepared?.text.trimmingCharacters(in: .newlines) ?? "")
            .font(.typewriter(12)).foregroundStyle(palette.secondary).lineSpacing(3)
            .textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
            // Selectable text keeps its own words as its name, so the well is one element that says
            // what it is, then reads the text.
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("The text that is sent")
            .accessibilityValue(prepared?.text.trimmingCharacters(in: .newlines) ?? "")
            .accessibilityAddTraits(.isStaticText)
    }
    @ViewBuilder private func leftOutLines(_ prepared: ConfigShare.Prepared?) -> some View {
        let lines = prepared?.leftOut ?? []
        if lines.isEmpty {
            Text("Nothing was left out of this loadout.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
        } else {
            LazyVStack(alignment: .leading, spacing: 8) {
                ForEach(lines) { line in
                    HStack(alignment: .firstTextBaseline, spacing: 12) {
                        Text(line.shown).font(.typewriter(12)).foregroundStyle(palette.secondary).lineLimit(1).truncationMode(.middle)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        Text(line.because).font(.system(size: 11)).foregroundStyle(palette.tertiary).multilineTextAlignment(.trailing)
                            .frame(width: 190, alignment: .trailing)
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(line.shown). Left out: \(line.because)")
                }
            }
        }
    }
    /// Copies what the tab shows: the text that is sent, or the left-out lines with their reasons.
    private func copy() {
        guard let prepared = share.prepared else { return }
        let text = tab == .sent ? prepared.text : prepared.leftOut.map { "\($0.shown)\t\($0.because)" }.joined(separator: "\n") + "\n"
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        copied = true
        NSAccessibility.post(element: NSApp as Any, notification: .announcementRequested, userInfo: [.announcement: "Copied."])
    }
}

/// Sent: where the request is, and how to take it back.
struct ShareSentSheet: View {
    @ObservedObject var model: LauncherModel
    var record: ConfigShareRecord
    var loadout: String
    @Environment(\.palette) private var palette
    var body: some View {
        SheetScaffold(icon: .symbol("checkmark.circle", positive: true), title: "Your setup was sent",
                      message: "It shows up in Player Setups under the name \(record.name) once a person has looked at it, usually within a few days. There is nothing more for you to do.",
                      width: 480, actions: [SheetAction(title: "Done", role: .primary, initialFocus: true) { model.dismissSheet() }]) {
            VStack(alignment: .leading, spacing: 12) {
                // The address well, as the app's other wells of config text (12-point corners).
                Text(Self.shortAddress(record.url)).font(.typewriter(12)).foregroundStyle(palette.address)
                    .textSelection(.enabled).padding(.horizontal, 14).padding(.vertical, 12)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("Request address: \(Self.shortAddress(record.url))")
                    .accessibilityAddTraits(.isStaticText)
                Text("To take it back, choose Stop Sharing from this loadout’s menu. No reason is asked for.")
                    .font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.leading, 6).fixedSize(horizontal: false, vertical: true)
            }
        } note: {
            Button("View Request") { NSWorkspace.shared.open(record.url) }.quietAction().padding(.horizontal, -6)
                .help("Open the public request in your browser")
        }
        .onAppear { model.studio.announce("Your setup was sent.") }
    }
    static func shortAddress(_ url: URL) -> String {
        var text = url.absoluteString
        for prefix in ["https://", "http://"] where text.hasPrefix(prefix) { text.removeFirst(prefix.count) }
        return text
    }
}

/// Player Setups › Share Your Setup…: a plain button beside the title. With one loadout it opens the
/// sheet; with more it asks which, in a menu, and says why any can't be shared.
struct ShareYourSetupButton: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var center: ConfigShareCenter
    @State private var anchor = MenuAnchorBox()
    init(model: LauncherModel) { self.model = model; studio = model.studio; center = model.shareCenter }

    var body: some View {
        Button { open() } label: {
            Label("Share Your Setup…", systemImage: ShareSetupSheet.symbol)
        }
        .glassAction().actionControlSize().fixedSize()
        .background(MenuAnchor(box: anchor))
        .disabled(studio.session == nil || studio.profiles.isEmpty || !model.canNavigate)
        .accessibilityLabel("Share Your Setup")
        .accessibilityHint(studio.profiles.count > 1 ? "Shows your loadouts to choose one" : "Opens the sheet that sends this loadout to Player Setups")
        .help(model.canNavigate ? "Send one of your loadouts to Player Setups" : "Sharing is off while Call of Duty 2 is running")
        .onAppear { center.refresh(studio) }
    }

    private func open() {
        center.refresh(studio)
        let profiles = studio.profiles
        if profiles.count == 1, let only = profiles.first, model.canShareLoadout(only) { model.shareLoadout(only.id); return }
        guard let view = anchor.view else { return }
        let refusals = profiles.compactMap { center.refusal($0, studio: studio) }
        NativeMenu.popUp(profiles.map { profile in
            NativeMenu.Item(title: profile.displayName, enabled: model.canShareLoadout(profile)) { model.shareLoadout(profile.id) }
        }, below: view, header: "Share a loadout", footnote: refusals.isEmpty ? nil : refusals.joined(separator: "\n"))
    }
}

/// The loadout's share items, after Export for Windows… in its More menu and its row's context
/// menu: Share with Other Players…, or once shared, Stop Sharing… and View Request.
struct ShareMenuItems: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var center: ConfigShareCenter
    var profile: ConfigProfile
    init(model: LauncherModel, profile: ConfigProfile) { self.model = model; center = model.shareCenter; self.profile = profile }
    var body: some View {
        if center.record(profile.id) != nil {
            Button("Stop Sharing…") { model.requestStopSharing(profile.id) }.disabled(center.withdrawing.contains(profile.id.lowercased()))
            Button("View Request") { model.viewShareRequest(profile.id) }
        } else {
            Button("Share with Other Players…") { model.shareLoadout(profile.id) }.disabled(!model.canShareLoadout(profile))
        }
    }
}
