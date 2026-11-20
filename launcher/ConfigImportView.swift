import SwiftUI
import AppKit
import UniformTypeIdentifiers

// The import sheet: choose a source (a link, a Windows profile), then review every line in
// plain words before one confirmed transaction. Risky lines start unticked; blocked and
// locked lines can't be ticked, even with Import Everything.

/// Hosts the import sheet and accepts dropped config files or text on the Configs page.
struct ConfigImportFlows: ViewModifier {
    @ObservedObject var discover: ConfigDiscoverModel
    @Environment(\.undoManager) private var undoManager
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var targeted = false

    func body(content: Content) -> some View {
        content
            .sheet(isPresented: Binding(get: { discover.step != nil }, set: { if !$0 { discover.closeImport() } })) {
                // The sheet is its own window: the review harness's shape audit covers the main window only.
                ConfigImportSheet(discover: discover, undo: undoManager).environment(\.palette, palette).environment(\.shapeAudit, nil)
            }
            .onDrop(of: [UTType.fileURL, UTType.plainText], isTargeted: $targeted) { accept($0) }
            .overlay {
                if targeted {
                    RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous)
                        .strokeBorder(palette.accent, style: StrokeStyle(lineWidth: 2, dash: [8, 6]))
                        .background(palette.selection, in: RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous))
                        .overlay {
                            Label("Drop to review this config", systemImage: "square.and.arrow.down")
                                .font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading)
                        }
                        .padding(12).allowsHitTesting(false)
                        .transition(.opacity)
                }
            }
            .animation(reduceMotion ? nil : .easeOut(duration: 0.18), value: targeted)
    }

    private func accept(_ providers: [NSItemProvider]) -> Bool {
        guard discover.step == nil, let provider = providers.first else { return false }
        let discover = self.discover
        if provider.hasItemConformingToTypeIdentifier(UTType.fileURL.identifier) {
            _ = provider.loadObject(ofClass: URL.self) { url, _ in
                guard let url, url.isFileURL else { return }
                Task { @MainActor in discover.importFile(url) }
            }
            return true
        }
        if provider.canLoadObject(ofClass: String.self) {
            _ = provider.loadObject(ofClass: String.self) { text, _ in
                guard let text else { return }
                Task { @MainActor in discover.importText(text, dropped: true) }
            }
            return true
        }
        return false
    }
}

extension View {
    func configImportFlows(_ discover: ConfigDiscoverModel) -> some View { modifier(ConfigImportFlows(discover: discover)) }
}

/// One sheet for the whole flow, so choosing a source moves straight on to its review.
struct ConfigImportSheet: View {
    @ObservedObject var discover: ConfigDiscoverModel
    var undo: UndoManager?
    var body: some View {
        switch discover.step {
        case .link(let start): LinkStep(discover: discover, text: start)
        case .windows: WindowsStep(discover: discover)
        case .review:
            if let review = discover.review { ImportReviewSheet(review: review, discover: discover, undo: undo) }
        case nil: EmptyView()
        }
    }
}

// MARK: - Link

private struct LinkStep: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @State var text: String
    @State private var problem: String?
    @Environment(\.palette) private var palette

    var body: some View {
        let empty = text.trimmingCharacters(in: .whitespaces).isEmpty
        SheetScaffold(icon: .symbol("link"), title: "Add a config from a link",
                      message: "CoD2 Unbound reads the link as plain text and shows you every line before anything changes.",
                      actions: [.cancel { discover.closeImport() }, .primary("Review", enabled: !empty, submit)]) {
            VStack(alignment: .leading, spacing: 12) {
                LauncherTextField("Config link", text: $text, prompt: "https://", shape: .capsule, invalid: problem != nil, autofocus: true)
                    .onChange(of: text) { _, _ in problem = nil }
                if let problem {
                    Label(problem, systemImage: "exclamationmark.circle").font(StudioTypography.help).foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true)
                }
                VStack(alignment: .leading, spacing: 6) {
                    hint("A file on the web, starting with https://")
                    hint("A GitHub file or gist, from github.com or gist.github.com")
                    hint("A server on this Mac, such as http://localhost:8000/config.cfg")
                }
            }
        } note: {
            if empty { Text("Paste or type a link to review it.") }
        }
    }
    private func hint(_ text: String) -> some View {
        Label(text, systemImage: "checkmark").font(StudioTypography.help).foregroundStyle(palette.secondary)
    }
    private func submit() {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        guard let url = URL(string: trimmed), ConfigImportLoader.permittedURL(url) else {
            problem = "Use an https:// link, a GitHub file or gist, or http://localhost for a server on this Mac. Links with passwords in them aren’t accepted."
            return
        }
        discover.importLink(url)
    }
}

// MARK: - Windows

/// Bounded discovery of Windows player profiles on mounted drives or a chosen folder. It reads
/// which profiles exist, never their contents, until the person picks one to review.
@MainActor final class WindowsProfileFinder: ObservableObject {
    @Published private(set) var discovery: WindowsProfileDiscovery?
    @Published private(set) var searched: [String] = []
    @Published private(set) var scanning = false
    private var folders: [URL] = []

    private var find: @Sendable ([URL]) async -> (WindowsProfileDiscovery, searched: [String]) = ConfigDiscoverModel.searchWindowsProfiles

    func scan(_ find: @escaping @Sendable ([URL]) async -> (WindowsProfileDiscovery, searched: [String])) {
        self.find = find
        scan()
    }
    func scan() {
        scanning = true
        let folders = self.folders, find = self.find
        Task { [weak self] in
            let result = await find(folders)
            guard let self else { return }
            self.discovery = result.0
            self.searched = result.searched
            self.scanning = false
        }
    }
    /// The folder chooser, attached to the import sheet it was opened from.
    func chooseFolder(in window: NSWindow? = NSApp?.keyWindow) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false; panel.canChooseDirectories = true; panel.allowsMultipleSelection = false
        panel.message = "Choose your Call of Duty 2 folder, its players folder, or one player profile."
        panel.prompt = "Look Here"
        let chosen: @MainActor (URL) -> Void = { [weak self] url in self?.folders.append(url); self?.scan() }
        if let window, window.isVisible, window.attachedSheet == nil {
            panel.beginSheetModal(for: window) { response in
                guard response == .OK, let url = panel.url else { return }
                MainActor.assumeIsolated { chosen(url) }
            }
        } else if panel.runModal() == .OK, let url = panel.url {
            chosen(url)
        }
    }
}

private struct WindowsStep: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @StateObject private var finder = WindowsProfileFinder()
    @State private var choice: String?
    @Environment(\.palette) private var palette

    var body: some View {
        let found = !(finder.discovery?.profiles.isEmpty ?? true)
        SheetScaffold(icon: .symbol("pc"), title: "Bring in your Windows setup",
                      message: "CoD2 Unbound looks for Call of Duty 2 player profiles in the usual install folders on drives connected to this Mac. Each profile you import becomes a new loadout, and your Windows files are never changed.",
                      width: 560,
                      actions: [SheetAction(title: "Choose Folder…", role: .leading) { finder.chooseFolder() },
                                .cancel { discover.closeImport() }, .primary("Review", enabled: choice != nil) { review() }]) {
            VStack(alignment: .leading, spacing: 12) {
                Group {
                    if finder.scanning || finder.discovery == nil {
                        HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Looking on connected drives…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                            .frame(maxWidth: .infinity, minHeight: 200).accessibilityElement(children: .combine)
                    } else if let discovery = finder.discovery, found {
                        List(selection: $choice) {
                            let main = discovery.profiles.filter { !$0.isBackup }, backups = discovery.profiles.filter(\.isBackup)
                            if !main.isEmpty { Section("Profiles") { ForEach(main) { row($0) } } }
                            if !backups.isEmpty { Section("Backups (optional)") { ForEach(backups) { row($0) } } }
                        }
                        .listStyle(.inset(alternatesRowBackgrounds: false))
                        .scrollContentBackground(.hidden)
                        .frame(minHeight: 220)
                        .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
                        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    } else {
                        EmptyState("No profiles found", message: "Choose the folder that holds your Call of Duty 2 install, its players folder, or one profile.",
                                   actionTitle: "Choose Folder…") { finder.chooseFolder() }
                            .padding(.vertical, 30)
                    }
                }
                if let discovery = finder.discovery, !finder.scanning {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(finder.searched.isEmpty ? "No drives to look on." : "Looked on: \(finder.searched.joined(separator: ", ")).")
                        if discovery.unreadableDirectories > 0 {
                            Text("\(discovery.unreadableDirectories) folder\(discovery.unreadableDirectories == 1 ? "" : "s") couldn’t be read. If your profiles are there, choose the folder yourself.")
                        }
                        if discovery.truncated { Text("The search stopped early because a folder was very large. Choose the profile folder to look there.") }
                    }
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                }
            }
        } note: {
            if found && choice == nil { Text("Choose a profile to review.") }
        }
        .onAppear { finder.scan(discover.findWindowsProfiles) }
    }

    private func row(_ profile: WindowsProfileCandidate) -> some View {
        HStack(spacing: 10) {
            Image(systemName: profile.isBackup ? "clock.arrow.circlepath" : "person.crop.square").foregroundStyle(palette.chipIcon).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                Text(profile.profileName).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                HStack(spacing: 6) {
                    if profile.isActive { Text("In use on Windows").foregroundStyle(palette.positive) }
                    else if profile.isBackup { Text("Backup copy").foregroundStyle(palette.secondary) }
                    Text(profile.configURL.lastPathComponent).font(.typewriter(11)).foregroundStyle(palette.address)
                }
                .font(StudioTypography.help)
            }
        }
        .padding(.vertical, 3)
        .tag(profile.id)
        .accessibilityElement(children: .combine)
    }
    private func review() {
        guard let profile = finder.discovery?.profiles.first(where: { $0.id == choice }) else { return }
        discover.importWindows(profile)
    }
}

// MARK: - Review

private struct ImportReviewSheet: View {
    @ObservedObject var review: ConfigImportReviewModel
    @ObservedObject var discover: ConfigDiscoverModel
    var undo: UndoManager?
    @Environment(\.palette) private var palette
    @State private var confirming: ConfirmationRequest?

    var body: some View {
        // The sheet's anatomy: symbol tile, a title that names the thing, the review, then the actions.
        VStack(alignment: .leading, spacing: 16) {
            header
            switch review.phase {
            case .loading:
                VStack(spacing: 10) {
                    ProgressView().controlSize(.small)
                    Text("Reading and checking every line…").font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity).accessibilityElement(children: .combine)
            case .failed(let message):
                EmptyState("Couldn’t read it", message: message, actionTitle: "Close", prominent: false) { discover.closeImport() }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            case .ready:
                ScrollView { reviewBody.padding(.bottom, 4) }
            }
            footer
        }
        .padding(22)
        .environment(\.formContext, .inset)
        .frame(width: 1000).frame(minHeight: 460, idealHeight: 760, maxHeight: .infinity)
        .confirmationAlert($confirming)
    }

    private var title: String {
        if review.provenance.signed { return "Review \(review.displayName)’s setup" }
        if review.provenance.windows { return "Review \(review.displayName) from Windows" }
        return "Review this config"
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(alignment: .top, spacing: 14) {
                Image(systemName: "square.and.arrow.down").font(.system(size: 20, weight: .medium)).foregroundStyle(palette.accent)
                    .frame(width: 44, height: 44)
                    .background(palette.selection, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 8) {
                    Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                        .fixedSize(horizontal: false, vertical: true)
                    source
                }
                Spacer(minLength: 12)
                destination
            }
            if let name = review.targetID.map({ _ in review.targetName }) {
                Text("This changes \(name) itself. History keeps the version from before, so you can undo it.")
                    .font(StudioTypography.help).foregroundStyle(palette.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if case .ready = review.phase {
                HStack(alignment: .center, spacing: 16) {
                    Picker("How to import", selection: Binding(get: { review.mode }, set: { review.setMode($0) })) {
                        Text("Review").tag(ConfigImportMode.review)
                        Text("Import Everything").tag(ConfigImportMode.everything)
                    }
                    .segmentedChoice()
                    .help("Review starts with risky lines left out. Import Everything ticks them too; blocked and locked lines stay out either way.")
                    Text(review.summary).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Button(review.showDetails ? "Hide Details" : "Show Details") { review.showDetails.toggle() }
                        .quietAction().fixedSize()
                        .accessibilityHint("Shows the exact lines")
                }
            }
        }
    }

    private var source: some View {
        HStack(spacing: 12) {
            Chip(review.provenance.source) {
                Image(systemName: review.provenance.symbol).accessibilityHidden(true)
                Text(review.provenance.source)
            }
            if review.provenance.signed { StatusLabel(.ready, "Signed catalog entry") }
            if review.provenance.windows {
                Chip("From Windows") {
                    Image(systemName: "pc").accessibilityHidden(true)
                    Text("From Windows")
                }
            }
            if let credit = review.provenance.credit {
                Text("Credit: \(credit)" + (review.provenance.publishedBy.map { " · As published by \($0)" } ?? ""))
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
            } else if let detail = review.provenance.detail {
                Text(detail).font(.typewriter(12)).foregroundStyle(palette.address).lineLimit(1).truncationMode(.middle)
                    .help(detail)
            }
        }
    }

    private var destination: some View {
        HStack(spacing: 8) {
            Text("Into").font(.system(size: 13)).foregroundStyle(palette.secondary)
            Picker("Into", selection: Binding(get: { review.targetID ?? "" }, set: { review.setTarget($0.isEmpty ? nil : $0) })) {
                Text("New loadout “\(review.displayName)”").tag("")
                Divider()
                ForEach(discover.studio.profiles) { Text($0.displayName).tag($0.id) }
            }
            .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
            .disabled(review.session == nil)
            .accessibilityLabel("Import into")
        }
        .fixedSize()
    }

    @ViewBuilder private var reviewBody: some View {
        let items = review.items
        VStack(alignment: .leading, spacing: 16) {
            if review.mode == .everything, review.riskyCount > 0 {
                Banner(.error, title: "This imports \(review.riskyCount == 1 ? "1 line" : "\(review.riskyCount) lines") that need your OK",
                       message: review.consequences.joined(separator: ". ") + ". Switch to Review to leave them out.")
            }
            if let error = review.applyError {
                Banner(.error, title: "Nothing was imported.", message: error)
            }
            if items.isEmpty {
                EmptyState("Nothing to import", message: "This config has no settings or key bindings to bring in.")
                    .padding(.vertical, 30)
            }
            // What needs your OK comes first, as tiles: each says what the line would do.
            let risky = items.filter { $0.section == .needsOK }
            if !risky.isEmpty {
                let ticked = risky.filter(\.selected).count
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 8) {
                        Image(systemName: "exclamationmark.triangle").font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(review.mode == .everything ? palette.negative : palette.caution).accessibilityHidden(true)
                        Text("Needs your OK").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                        Spacer(minLength: 8)
                        Text(ticked == 0 ? "Left out unless you tick them" : "\(ticked) of \(risky.count) ticked")
                            .font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary)
                    }
                    .padding(.horizontal, 6)
                    Grid(horizontalSpacing: 12, verticalSpacing: 12) {
                        ForEach(Array(stride(from: 0, to: risky.count, by: 3)), id: \.self) { start in
                            GridRow(alignment: .top) {
                                ForEach(start..<start + 3, id: \.self) { index in
                                    if index < risky.count {
                                        RiskTile(item: risky[index], everything: review.mode == .everything, toggle: { review.toggle($0) })
                                    } else {
                                        Color.clear.frame(height: 1).gridCellUnsizedAxes(.vertical)
                                    }
                                }
                            }
                        }
                    }
                }
                .accessibilityElement(children: .contain)
                .accessibilityLabel("Needs your OK")
            }
            // The changes by what they do, in two columns, with what stays protected beside them.
            let blocks = changeBlocks(items)
            if !blocks.isEmpty {
                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 12) {
                        ForEach(Array(blocks.enumerated()).filter { $0.offset % 2 == 0 }, id: \.offset) { _, block in group(block) }
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    VStack(alignment: .leading, spacing: 12) {
                        ForEach(Array(blocks.enumerated()).filter { $0.offset % 2 == 1 }, id: \.offset) { _, block in group(block) }
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                }
            }
            let machine = items.filter { $0.section == .machine }
            if !machine.isEmpty {
                ReviewGroup(title: "Choose on this Mac", trailing: "Set in Settings or Binds instead", items: machine,
                            details: review.showDetails, toggle: { review.toggle($0) })
            }
            let never = items.filter { $0.section == .never }
            if !never.isEmpty {
                ReviewGroup(title: "Never imported", trailing: "Passwords, file paths, protected settings, unknown commands",
                            items: never, details: true, toggle: { review.toggle($0) })
            }
        }
    }

    private struct Block {
        var title: String
        var trailing: String
        var items: [ConfigImportItem]
        var protected = false
    }
    private func changeBlocks(_ items: [ConfigImportItem]) -> [Block] {
        var blocks: [Block] = ConfigImportPresentation.groupOrder(catalog: discover.studio.catalog).compactMap { group in
            let rows = items.filter { $0.section == .change(group) }
            return rows.isEmpty ? nil : Block(title: group, trailing: "\(rows.filter(\.selected).count) of \(rows.count)", items: rows)
        }
        let protected = items.filter { $0.section == .protected }
        if !protected.isEmpty { blocks.append(Block(title: "Protected", trailing: "Locked by you · never imported", items: protected, protected: true)) }
        return blocks
    }
    private func group(_ block: Block) -> some View {
        ReviewGroup(title: block.title, trailing: block.trailing, items: block.items, details: review.showDetails, protected: block.protected,
                    toggle: { review.toggle($0) })
    }

    /// Why Apply is dimmed, said beside it; nil when it can apply.
    private var applyBlock: String? {
        if discover.studio.isReadOnly { return "Configs are read-only while Call of Duty 2 is running." }
        if review.receipt != nil { return "Already imported." }
        if review.selectedCount == 0 { return "Tick at least one change to import." }
        return nil
    }
    private var footer: some View {
        HStack(spacing: 8) {
            Group {
                if case .ready = review.phase, let reason = applyBlock { Text(reason) }
                else { Text("Nothing applies until you confirm. You can undo it from \(Text("History").foregroundColor(palette.link)).") }
            }
            .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            if case .ready = review.phase {
                Button("Cancel", role: .cancel) { discover.closeImport() }.glassAction().actionControlSize().keyboardShortcut(.cancelAction)
                Button(review.applyTitle, action: requestApply).prominentAction().actionControlSize().keyboardShortcut(.defaultAction)
                    .disabled(applyBlock != nil)
                    .help(applyBlock ?? "Apply the ticked changes as one History snapshot")
            } else {
                Button("Close", role: .cancel) { discover.closeImport() }.glassAction().actionControlSize().keyboardShortcut(.cancelAction)
            }
        }
        .padding(.top, 2)
    }

    private var confirmTitle: String {
        let count = review.selectedCount, risky = review.riskySelected.count
        return "Import \(count == 1 ? "1 change" : "\(count) changes")\(risky > 0 ? ", including \(risky) that need\(risky == 1 ? "s" : "") your OK" : "")?"
    }
    private var confirmMessage: String {
        let consequences = review.consequences
        let what = consequences.isEmpty ? "" : consequences.joined(separator: ". ") + ". "
        let into = review.targetID == nil ? "They go into a new loadout, \(review.displayName)." : "This changes \(review.targetName); History keeps the version from before."
        return what + into
    }
    private func requestApply() {
        guard review.mode == .everything || !review.riskySelected.isEmpty else { apply(); return }
        confirming = ConfirmationRequest(title: confirmTitle, message: confirmMessage, actionTitle: review.applyTitle, destructive: false) { apply() }
    }
    private func apply() {
        let existing = review.targetID != nil, name = review.targetName
        if let receipt = review.apply(undo: undo) { discover.finishImport(receipt, name: name, existing: existing) }
    }
}

/// A line that needs your OK, as a tile: the shared checkbox, what it would do, and the exact line.
private struct RiskTile: View {
    var item: ConfigImportItem
    var everything: Bool
    var toggle: @MainActor (String) -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 14, style: .continuous)
        CheckboxRow(isOn: Binding(get: { item.selected }, set: { _ in toggle(item.id) })) {
            VStack(alignment: .leading, spacing: 3) {
                Text(item.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading)
                if let detail = item.detail, !detail.isEmpty {
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary)
                }
                Text(item.exact).font(.typewriter(12)).foregroundStyle(palette.text).textSelection(.enabled)
                    .accessibilityLabel("Exact line: \(item.exact)")
            }
            .fixedSize(horizontal: false, vertical: true)
        }
        .disabled(!item.selectable)
        .padding(.vertical, 12).padding(.horizontal, 14)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(palette.well, in: shape)
        .overlay(shape.strokeBorder(everything ? palette.negative.opacity(0.55) : palette.wellStroke, lineWidth: 1))
    }
}

/// One group of the review: its label and count over rows on a well, as a form group in a sheet.
private struct ReviewGroup: View {
    var title: String
    var trailing: String
    var items: [ConfigImportItem]
    var details: Bool
    var protected = false
    var toggle: @MainActor (String) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 10) {
                GroupLabel(title)
                Spacer(minLength: 8)
                Text(trailing).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary)
                    .multilineTextAlignment(.trailing)
            }
            .padding(.horizontal, 6)
            FormGroup {
                ForEach(items) { item in
                    ReviewRow(item: item, details: details, protected: protected, toggle: toggle)
                }
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel(title)
    }
}

private struct ReviewRow: View {
    var item: ConfigImportItem
    var details: Bool
    var protected: Bool
    var toggle: @MainActor (String) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        Group {
            if item.selectable {
                CheckboxRow(isOn: Binding(get: { item.selected }, set: { _ in toggle(item.id) })) { label }
            } else {
                HStack(alignment: .top, spacing: 8) {
                    Image(systemName: symbol).font(.system(size: 14)).foregroundStyle(palette.tertiary).frame(width: 16).padding(.top, 1)
                        .accessibilityHidden(true)
                    label
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.vertical, 9).padding(.horizontal, 16)
        .frame(minHeight: FormMetrics.detailRowHeight)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
    }

    private var symbol: String {
        switch item.section { case .protected: "lock"; case .machine: "desktopcomputer"; default: "nosign" }
    }

    private var label: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 8) {
                Text(item.title).font(.system(size: 14, weight: item.risky ? .semibold : .regular)).foregroundStyle(palette.text)
                ForEach(item.labels, id: \.self) { Badge($0) }
            }
            if let detail = item.detail, !detail.isEmpty {
                Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary)
            }
            if details { exact }
        }
        .fixedSize(horizontal: false, vertical: true)
    }

    @ViewBuilder private var exact: some View {
        Group {
            if protected, let previous = item.previous {
                Text("\(Text(previous).foregroundColor(palette.text))\(Text("  (theirs: \(item.exact))").foregroundColor(palette.secondary))")
            } else if let previous = item.previous, !item.risky {
                Text("\(Text(previous).strikethrough().foregroundColor(palette.tertiary)) → \(Text(item.exact).foregroundColor(palette.text))")
            } else {
                Text(item.exact).foregroundColor(palette.secondary)
            }
        }
        .font(.typewriter(12)).lineSpacing(3).textSelection(.enabled)
        .accessibilityLabel("Exact line: \(item.exact)")
    }
}
