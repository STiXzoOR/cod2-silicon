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

private struct SheetTitle: View {
    var title: String
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Eyebrow(text: "Import", size: 12, tracking: 0.3)
            Text(title).font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}

// MARK: - Link

private struct LinkStep: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @State var text: String
    @State private var problem: String?
    @FocusState private var focused: Bool
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SheetTitle(title: "Add a config from a link")
            Text("CoD2 Silicon reads the link as plain text and shows you every line before anything changes.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            TextField("Link", text: $text, prompt: Text("https://"))
                .textFieldStyle(.roundedBorder).font(.system(size: 13)).focused($focused)
                .onSubmit(submit)
                .accessibilityLabel("Config link")
            VStack(alignment: .leading, spacing: 6) {
                hint("A file on the web, starting with https://")
                hint("A GitHub file or gist, from github.com or gist.github.com")
                hint("A server on this Mac, such as http://localhost:8000/config.cfg")
            }
            if let problem {
                Label(problem, systemImage: "exclamationmark.circle").font(StudioTypography.help).foregroundStyle(palette.negative)
                    .fixedSize(horizontal: false, vertical: true)
            }
            HStack(spacing: 10) {
                Spacer()
                Button("Cancel", role: .cancel) { discover.closeImport() }.glassAction().keyboardShortcut(.cancelAction)
                Button("Review", action: submit).prominentAction().keyboardShortcut(.defaultAction)
                    .disabled(text.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .padding(26).frame(width: 520)
        .onAppear { focused = true }
    }
    private func hint(_ text: String) -> some View {
        Label(text, systemImage: "checkmark").font(StudioTypography.help).foregroundStyle(palette.secondary)
    }
    private func submit() {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
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
    func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false; panel.canChooseDirectories = true; panel.allowsMultipleSelection = false
        panel.message = "Choose your Call of Duty 2 folder, its players folder, or one player profile."
        panel.prompt = "Look Here"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        folders.append(url)
        scan()
    }
}

private struct WindowsStep: View {
    @ObservedObject var discover: ConfigDiscoverModel
    @StateObject private var finder = WindowsProfileFinder()
    @State private var choice: String?
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SheetTitle(title: "Bring in your Windows setup")
            Text("CoD2 Silicon looks for Call of Duty 2 player profiles in the usual install folders on drives connected to this Mac. Each profile you import becomes a new loadout, and your Windows files are never changed.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            Group {
                if finder.scanning || finder.discovery == nil {
                    HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Looking on connected drives…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                        .frame(maxWidth: .infinity, minHeight: 200).accessibilityElement(children: .combine)
                } else if let discovery = finder.discovery, !discovery.profiles.isEmpty {
                    List(selection: $choice) {
                        let main = discovery.profiles.filter { !$0.isBackup }, backups = discovery.profiles.filter(\.isBackup)
                        if !main.isEmpty { Section("Profiles") { ForEach(main) { row($0) } } }
                        if !backups.isEmpty { Section("Backups (optional)") { ForEach(backups) { row($0) } } }
                    }
                    .listStyle(.inset(alternatesRowBackgrounds: false))
                    .frame(minHeight: 220)
                } else {
                    StudioEmptyState(title: "NO PROFILES FOUND",
                                     message: "Choose the folder that holds your Call of Duty 2 install, its players folder, or one profile.")
                        .frame(maxWidth: .infinity)
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
            HStack(spacing: 10) {
                Button("Choose Folder…") { finder.chooseFolder() }.glassAction()
                Spacer()
                Button("Cancel", role: .cancel) { discover.closeImport() }.glassAction().keyboardShortcut(.cancelAction)
                Button("Review") { review() }.prominentAction().keyboardShortcut(.defaultAction).disabled(choice == nil)
            }
        }
        .padding(26).frame(width: 560)
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
    @State private var confirming = false

    var body: some View {
        VStack(spacing: 0) {
            header
            Rectangle().fill(palette.separator).frame(height: 1)
            switch review.phase {
            case .loading:
                VStack(spacing: 10) {
                    ProgressView().controlSize(.small)
                    Text("Reading and checking every line…").font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity).accessibilityElement(children: .combine)
            case .failed(let message):
                StudioEmptyState(title: "COULDN’T READ IT", message: message)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            case .ready:
                ScrollView { reviewBody.padding(.vertical, 16).padding(.horizontal, 26) }
            }
            Rectangle().fill(palette.separator).frame(height: 1)
            footer
        }
        .frame(width: 780).frame(minHeight: 460, idealHeight: 720, maxHeight: .infinity)
        .confirmationDialog(confirmTitle, isPresented: $confirming) {
            Button(review.applyTitle) { apply() }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text(confirmMessage)
        }
    }

    private var title: String {
        if review.provenance.signed { return "Review \(review.displayName)’s setup" }
        if review.provenance.windows { return "Review \(review.displayName) from Windows" }
        return "Review this config"
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .top, spacing: 16) {
                SheetTitle(title: title)
                Spacer(minLength: 12)
                if case .ready = review.phase {
                    Picker("How to import", selection: Binding(get: { review.mode }, set: { review.setMode($0) })) {
                        Text("Review").tag(ConfigImportMode.review)
                        Text("Import Everything").tag(ConfigImportMode.everything)
                    }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                    .help("Review starts with risky lines left out. Import Everything ticks them too; blocked and locked lines stay out either way.")
                }
            }
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 8) { source; Spacer(minLength: 12); destination }
                VStack(alignment: .leading, spacing: 10) { source; destination }
            }
            if let name = review.targetID.map({ _ in review.targetName }) {
                Text("This changes \(name) itself. History keeps the version from before, so you can undo it.")
                    .font(StudioTypography.help).foregroundStyle(palette.dark ? Palette.hex(0xe2dbc9) : palette.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(.top, 22).padding(.horizontal, 26).padding(.bottom, 16)
    }

    private var source: some View {
        HStack(spacing: 8) {
            Chip(text: review.provenance.source, symbol: review.provenance.symbol)
            if review.provenance.signed { Chip(text: "Signed catalog entry", symbol: "checkmark.seal", tone: .positive) }
            if review.provenance.windows { Chip(text: "From Windows", symbol: "pc") }
            if let credit = review.provenance.credit {
                Text("Credit: \(credit)" + (review.provenance.permission ? " · Shared with permission" : ""))
                    .font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
            } else if let detail = review.provenance.detail {
                Text(detail).font(.typewriter(12)).foregroundStyle(palette.address).lineLimit(1).truncationMode(.middle)
                    .help(detail)
            }
        }
    }

    private var destination: some View {
        HStack(spacing: 8) {
            Text("Into").font(StudioTypography.help).foregroundStyle(palette.secondary)
            Picker("Into", selection: Binding(get: { review.targetID ?? "" }, set: { review.setTarget($0.isEmpty ? nil : $0) })) {
                Text("New loadout “\(review.displayName)”").tag("")
                Divider()
                ForEach(discover.studio.profiles) { Text($0.displayName).tag($0.id) }
            }
            .labelsHidden().fixedSize()
            .disabled(review.session == nil)
            .accessibilityLabel("Import into")
        }
    }

    @ViewBuilder private var reviewBody: some View {
        let items = review.items
        VStack(alignment: .leading, spacing: 12) {
            if review.mode == .everything, review.riskyCount > 0 {
                StudioBanner(title: "This imports \(review.riskyCount == 1 ? "1 line" : "\(review.riskyCount) lines") that need your OK",
                             detail: review.consequences.joined(separator: ". ") + ". Switch to Review to leave them out.", tone: .negative)
            }
            if let error = review.applyError {
                StudioBanner(title: "Nothing was imported.", detail: error, tone: .negative)
            }
            HStack(alignment: .firstTextBaseline) {
                Text(review.summary).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 12)
                Button(review.showDetails ? "Hide Details" : "Show Details") { review.showDetails.toggle() }
                    .buttonStyle(.plain).foregroundStyle(palette.link).font(.system(size: 13, weight: .semibold))
                    .accessibilityHint("Shows the exact lines")
            }
            if items.isEmpty {
                StudioEmptyState(title: "NOTHING TO IMPORT", message: "This config has no settings or key bindings to bring in.")
                    .frame(maxWidth: .infinity)
            }
            ForEach(ConfigImportPresentation.groupOrder(catalog: discover.studio.catalog), id: \.self) { group in
                let rows = items.filter { $0.section == .change(group) }
                if !rows.isEmpty {
                    ReviewGroup(title: group, trailing: "\(rows.filter(\.selected).count) of \(rows.count)", items: rows,
                                details: review.showDetails, toggle: { review.toggle($0) })
                }
            }
            let protected = items.filter { $0.section == .protected }
            if !protected.isEmpty {
                ReviewGroup(title: "Protected", symbol: "lock", trailing: "Locked by you · never imported", tone: .quiet, items: protected,
                            details: review.showDetails, protected: true, toggle: { review.toggle($0) })
            }
            let risky = items.filter { $0.section == .needsOK }
            if !risky.isEmpty {
                let ticked = risky.filter(\.selected).count
                ReviewGroup(title: "Needs your OK", symbol: "exclamationmark.triangle",
                            trailing: ticked == 0 ? "Left out unless you tick them" : "\(ticked) of \(risky.count) ticked",
                            tone: review.mode == .everything ? .riskyAll : .risky, items: risky, details: true, toggle: { review.toggle($0) })
            }
            let machine = items.filter { $0.section == .machine }
            if !machine.isEmpty {
                ReviewGroup(title: "Choose on this Mac", symbol: "desktopcomputer", trailing: "Set in Settings or Binds instead", tone: .quiet,
                            items: machine, details: review.showDetails, toggle: { review.toggle($0) })
            }
            let never = items.filter { $0.section == .never }
            if !never.isEmpty {
                ReviewGroup(title: "Never imported", symbol: "nosign", trailing: "Passwords, file paths, protected settings, unknown commands",
                            tone: .quiet, items: never, details: true, toggle: { review.toggle($0) })
            }
        }
    }

    private var footer: some View {
        HStack(spacing: 12) {
            Text("Nothing changes until you confirm. Each import is one History snapshot you can undo.")
                .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            if case .ready = review.phase {
                Button("Cancel", role: .cancel) { discover.closeImport() }.glassAction().actionControlSize().keyboardShortcut(.cancelAction)
                Button(review.applyTitle, action: requestApply).prominentAction().actionControlSize().keyboardShortcut(.defaultAction)
                    .disabled(review.selectedCount == 0 || review.receipt != nil || discover.studio.isReadOnly)
                    .help(discover.studio.isReadOnly ? "Quit Call of Duty 2 to import" : "Apply the ticked changes as one History snapshot")
            } else {
                Button("Close", role: .cancel) { discover.closeImport() }.glassAction().actionControlSize().keyboardShortcut(.cancelAction)
            }
        }
        .padding(.top, 14).padding(.bottom, 16).padding(.leading, 26).padding(.trailing, 22)
        .background(palette.dark ? Color.black.opacity(0.14) : Color.black.opacity(0.03))
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
        if review.mode == .everything || !review.riskySelected.isEmpty { confirming = true } else { apply() }
    }
    private func apply() {
        let existing = review.targetID != nil, name = review.targetName
        if let receipt = review.apply(undo: undo) { discover.finishImport(receipt, name: name, existing: existing) }
    }
}

private struct Chip: View {
    var text: String
    var symbol: String
    var tone: StudioStatusTone? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        Label(text, systemImage: symbol)
            .font(.system(size: 12, weight: .semibold)).lineLimit(1)
            .foregroundStyle(tone.map { $0.color(in: palette) } ?? palette.chipText)
            .padding(.horizontal, 10).frame(height: 26)
            .background((tone.map { $0.color(in: palette).opacity(0.06) } ?? (palette.dark ? Color.white.opacity(0.06) : Color.white.opacity(0.55))), in: Capsule())
            .overlay(Capsule().stroke(tone.map { $0.color(in: palette).opacity(0.45) } ?? (palette.dark ? Color.white.opacity(0.12) : palette.wellStroke), lineWidth: 1))
            .fixedSize()
    }
}

private struct ReviewGroup: View {
    enum Tone { case plain, quiet, risky, riskyAll }
    var title: String
    var symbol: String? = nil
    var trailing: String
    var tone: Tone = .plain
    var items: [ConfigImportItem]
    var details: Bool
    var protected = false
    var toggle: @MainActor (String) -> Void
    @Environment(\.palette) private var palette
    private let radius: CGFloat = 16

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 10) {
                HStack(spacing: 8) {
                    if let symbol { Image(systemName: symbol).foregroundStyle(accent).accessibilityHidden(true) }
                    Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.heading)
                }
                .accessibilityElement(children: .combine).accessibilityAddTraits(.isHeader)
                Spacer(minLength: 8)
                Text(trailing).font(StudioTypography.help).foregroundStyle(tone == .risky || tone == .riskyAll ? palette.text : palette.secondary)
                    .multilineTextAlignment(.trailing)
            }
            .padding(.vertical, 11).padding(.horizontal, 16)
            .overlay(alignment: .bottom) { Rectangle().fill(edge.opacity(0.6)).frame(height: 1) }
            ForEach(Array(items.enumerated()), id: \.element.id) { index, item in
                ReviewRow(item: item, details: details, protected: protected, toggle: toggle)
                    .overlay(alignment: .top) { if index > 0 { Rectangle().fill(palette.separator).frame(height: 1) } }
            }
        }
        .background(fill, in: RoundedRectangle(cornerRadius: radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).stroke(edge, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: radius, style: .continuous))
        .accessibilityElement(children: .contain)
        .accessibilityLabel(title)
    }
    private var accent: Color {
        switch tone { case .risky: palette.caution; case .riskyAll: palette.negative; default: palette.chipIcon }
    }
    private var fill: Color {
        switch tone {
        case .plain: palette.panel
        case .quiet: .clear
        case .risky: palette.caution.opacity(0.06)
        case .riskyAll: palette.negative.opacity(0.06)
        }
    }
    private var edge: Color {
        switch tone {
        case .plain, .quiet: palette.panelStroke
        case .risky: palette.caution.opacity(0.5)
        case .riskyAll: palette.negative.opacity(0.55)
        }
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
                Toggle(isOn: Binding(get: { item.selected }, set: { _ in toggle(item.id) })) { label }
                    .toggleStyle(.checkbox)
            } else {
                HStack(alignment: .top, spacing: 8) {
                    Image(systemName: symbol).font(.system(size: 12)).foregroundStyle(palette.chipIcon).frame(width: 16).padding(.top, 1)
                        .accessibilityHidden(true)
                    label
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.vertical, 11).padding(.horizontal, 16)
    }

    private var symbol: String {
        switch item.section { case .protected: "lock"; case .machine: "desktopcomputer"; default: "nosign" }
    }

    private var label: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                Text(item.title).font(.system(size: 14, weight: item.risky ? .semibold : .regular))
                    .foregroundStyle(item.selectable ? palette.text : (palette.dark ? Palette.hex(0xcfc6b1) : palette.text))
                ForEach(item.labels, id: \.self) { label in
                    Text(label).font(.system(size: 10.5, weight: .semibold)).foregroundStyle(palette.secondary)
                        .padding(.horizontal, 7).frame(height: 18)
                        .overlay(Capsule().stroke(palette.track, lineWidth: 1))
                }
            }
            if let detail = item.detail, !detail.isEmpty {
                Text(detail).font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            if details { exact }
        }
        .fixedSize(horizontal: false, vertical: true)
    }

    @ViewBuilder private var exact: some View {
        Group {
            if protected, let previous = item.previous {
                Text(previous).foregroundColor(palette.address) + Text("  (theirs: \(item.exact))").foregroundColor(palette.secondary)
            } else if let previous = item.previous, !item.risky {
                Text(previous).strikethrough().foregroundColor(palette.secondary) + Text("  ") + Text(item.exact).foregroundColor(palette.accent)
            } else {
                Text(item.exact).foregroundColor(palette.address)
            }
        }
        .font(.typewriter(12)).lineSpacing(3).textSelection(.enabled)
        .accessibilityLabel("Exact line: \(item.exact)")
    }
}
