import SwiftUI
import AppKit
import UniformTypeIdentifiers

// Configs › Check a Config. A config is read on this Mac and marked line by line; fixes change a copy
// held in memory. The file it came from, and the person's loadouts, are untouched until they ask.

@MainActor final class ConfigCheckerModel: ObservableObject {
    struct Source: Equatable {
        var name: String
        /// Where it came from, as the page says it (“dropped file”, “pasted text”, “your loadout”).
        var origin: String
    }
    let studio: ConfigStudioModel
    /// Save as Loadout…: the launcher's import review, given a name and the config text.
    var save: @MainActor (String, Data) -> Void = { _, _ in }
    var notify: @MainActor (String) -> Void = { _ in }
    @Published private(set) var source: Source?
    @Published private(set) var report: ConfigCheckReport?
    @Published private(set) var applied: Set<Int> = []
    @Published private(set) var checking = false
    @Published private(set) var failure: String?
    @Published var dialect: ConfigDialect { didSet { if dialect != oldValue { ruleset = nil; recheck() } } }
    /// The server rules to check against; nil checks the game's own limits only.
    @Published var ruleset: String? { didSet { if ruleset != oldValue { recheck() } } }
    @Published var markedOnly = false
    private var data: Data?
    private var task: Task<Void, Never>?

    init(studio: ConfigStudioModel) { self.studio = studio; dialect = studio.dialect }

    var rulesets: [ConfigRuleset] {
        let mode: ConfigCatalogMode = dialect == .cod2x ? .cod2x : .stock
        return studio.checkup.rules?.rulesets.filter { $0.mode == mode } ?? []
    }
    var rulesetTitle: String { ruleset.flatMap { id in rulesets.first { $0.id == id }?.title } ?? "No server rules" }
    static func title(_ dialect: ConfigDialect) -> String { dialect == .cod2x ? "CoD2x" : "Original 1.3" }

    // MARK: What the page shows

    var lines: [ConfigCheckShownLine] {
        guard let report else { return [] }
        let shown = report.shown(applied: applied)
        return markedOnly ? shown.filter { $0.mark != nil } : shown
    }
    /// The findings still open: not fixed yet.
    var open: [ConfigCheckFinding] { report?.findings.filter { !applied.contains($0.id) } ?? [] }
    var fixable: [ConfigCheckFinding] { report?.fixable.filter { !applied.contains($0.id) } ?? [] }
    func open(_ kind: ConfigCheckFinding.Kind) -> [ConfigCheckFinding] { open.filter { $0.kind == kind } }
    var cleaned: Data { report?.applying(applied) ?? Data() }
    var summary: (count: Int, title: String, detail: String) {
        guard let report else { return (0, "", "") }
        let left = open.count
        func lines(_ count: Int) -> String { count == 1 ? "1 line" : "\(count) lines" }
        if report.findings.isEmpty {
            return (0, "This config is clean", "Every line is a setting or key of this game, with a value the game takes.")
        }
        if !applied.isEmpty {
            let notes = open.allSatisfy { $0.kind == .note }
            return (left, left == 0 ? "\(applied.count) fixed, nothing left" : "\(applied.count) fixed, \(left) left for you",
                    (left == 0 ? "" : notes ? "What is left are notes, not faults. " : "") + "Nothing was changed on disk.")
        }
        let parts = [(open(.broken).count, "won’t work"), (open(.different).count, "won’t do what \(open(.different).count == 1 ? "it says" : "they say")"), (open(.note).count, "\(open(.note).count == 1 ? "is" : "are") worth knowing")]
            .filter { $0.0 > 0 }.map { "\($0.0) \($0.1)" }
        return (left, "\(lines(left)) \(left == 1 ? "needs" : "need") a look", parts.joined(separator: ", ") + ".")
    }
    var meta: String {
        guard let report, let source else { return "" }
        let count = report.lines.count == 1 ? "1 line" : "\(report.lines.count) lines"
        return "\(count) · \(source.origin) · nothing was changed on disk"
    }

    // MARK: Sources

    func check(_ data: Data, name: String, origin: String) {
        self.data = data; source = Source(name: name, origin: origin)
        recheck()
    }
    private func recheck() {
        task?.cancel()
        guard let data else { return }
        if studio.catalog == nil { studio.loadCatalogNow() }
        if studio.checkup.rules == nil { studio.checkup.load() }
        guard let catalog = studio.catalog else { failure = "The settings catalog is missing or damaged. Reinstall CoD2 Unbound to restore it."; return }
        let dialect = dialect, ruleset = ruleset, rules = studio.checkup.rules
        checking = true; failure = nil; applied = []
        task = Task { [weak self] in
            do {
                let report = try await ConfigChecker.check(data, dialect: dialect, ruleset: ruleset, catalog: catalog, rules: rules)
                guard let self, !Task.isCancelled else { return }
                self.report = report; self.checking = false
                self.studio.announce(self.summary.title + ". " + self.summary.detail)
            } catch is CancellationError {
            } catch {
                guard let self, !Task.isCancelled else { return }
                self.report = nil; self.checking = false; self.failure = ConfigImportReviewModel.message(for: error)
            }
        }
    }
    /// Check Another: back to the empty page. Nothing was saved, so nothing is lost but the marks.
    func reset() {
        task?.cancel()
        data = nil; source = nil; report = nil; applied = []; failure = nil; checking = false; markedOnly = false
    }
    func checkFile(_ url: URL) {
        // The game reads at most 65,535 characters of a config; a megabyte is far past any real one.
        guard let handle = try? FileHandle(forReadingFrom: url), let data = try? handle.read(upToCount: 1_048_577) else {
            failure = "That file couldn’t be read."; return
        }
        try? handle.close()
        guard data.count <= 1_048_576 else { failure = "That file is too large to be a config."; return }
        check(data, name: url.lastPathComponent, origin: "from \(url.deletingLastPathComponent().lastPathComponent)")
    }
    func chooseFile(in window: NSWindow? = NSApp?.keyWindow) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true; panel.canChooseDirectories = false; panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [UTType(filenameExtension: "cfg"), UTType(filenameExtension: "bak"), UTType.plainText].compactMap { $0 }
        panel.message = "Choose a Call of Duty 2 config to check. It is read on this Mac and not changed."
        panel.prompt = "Check"
        if let window, window.isVisible, window.attachedSheet == nil {
            panel.beginSheetModal(for: window) { [weak self] response in
                guard response == .OK, let url = panel.url else { return }
                MainActor.assumeIsolated { self?.checkFile(url) }
            }
        } else if panel.runModal() == .OK, let url = panel.url {
            checkFile(url)
        }
    }
    func checkText(_ text: String, name: String, origin: String) {
        guard !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { failure = "There is no config text to check."; return }
        guard let data = try? ConfigDocument.encode(text) else { failure = "That text has characters Call of Duty 2 can’t store in a config."; return }
        check(data, name: name, origin: origin)
    }
    func paste() {
        guard let text = NSPasteboard.general.string(forType: .string), !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            failure = "The clipboard has no config text to check."; return
        }
        checkText(text, name: "Pasted config", origin: "pasted")
    }
    func checkLoadout(_ profile: ConfigProfile) {
        guard let read = try? studio.session?.readProfile(profile.id) else { failure = "That loadout couldn’t be read."; return }
        check(read.document.serialized(), name: "\(profile.displayName) loadout", origin: "your loadout, unchanged")
    }
    func drop(_ providers: [NSItemProvider]) -> Bool {
        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.fileURL.identifier) }) {
            _ = provider.loadObject(ofClass: URL.self) { [weak self] url, _ in
                guard let url else { return }
                Task { @MainActor [weak self] in self?.checkFile(url) }
            }
            return true
        }
        if let provider = providers.first(where: { $0.canLoadObject(ofClass: String.self) }) {
            _ = provider.loadObject(ofClass: String.self) { [weak self] text, _ in
                guard let text else { return }
                Task { @MainActor [weak self] in self?.checkText(text, name: "Dropped config", origin: "dropped text") }
            }
            return true
        }
        return false
    }

    // MARK: Fixes

    func fix(_ finding: ConfigCheckFinding) {
        guard finding.fix != nil else { return }
        applied.insert(finding.id)
        studio.announce("\(finding.fixLabel ?? "Fixed"): line \(finding.line).")
    }
    func fixAll() {
        let ids = fixable.map(\.id)
        guard !ids.isEmpty else { return }
        applied.formUnion(ids)
        studio.announce(summary.title + ". " + summary.detail)
    }
    func undo() { applied = []; studio.announce("Put every line back.") }
    func copyCleaned() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(ConfigDocument.text(cleaned), forType: .string)
        notify(applied.isEmpty ? "Copied the config." : "Copied the config with your fixes.")
    }
    func export(in window: NSWindow? = NSApp?.keyWindow) {
        let panel = NSSavePanel()
        let name = source?.name ?? "config.cfg"
        panel.nameFieldStringValue = name.hasSuffix(".cfg") ? name : "config.cfg"
        panel.allowedContentTypes = [UTType(filenameExtension: "cfg")].compactMap { $0 }
        panel.message = "Save the config with your fixes as a new file. The one you checked stays as it is."
        let data = cleaned
        let write: (URL) -> Void = { [weak self] url in
            do { try data.write(to: url, options: .atomic) } catch { self?.notify("Couldn’t save the file. \(error.localizedDescription)") }
        }
        if let window, window.isVisible, window.attachedSheet == nil {
            panel.beginSheetModal(for: window) { response in
                guard response == .OK, let url = panel.url else { return }
                MainActor.assumeIsolated { write(url) }
            }
        } else if panel.runModal() == .OK, let url = panel.url { write(url) }
    }
    func saveAsLoadout() {
        let name = (source?.name ?? "Checked config").replacingOccurrences(of: ".cfg", with: "")
        save(name, cleaned)
    }

    /// The review harness shows a finished check without waiting (preview mode only).
    func preview(_ report: ConfigCheckReport, source: Source, applied: Set<Int> = []) {
        self.report = report; self.source = source; self.applied = applied; data = report.bytes; checking = false; failure = nil
    }
}

// MARK: - Page

struct ConfigCheckerPage: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var checker: ConfigCheckerModel
    @Environment(\.palette) private var palette
    @State private var width: CGFloat = 0
    /// Two columns when there's room beside the Configs column; one above the other when there isn't.
    private var sideBySide: Bool { width >= 700 }

    var body: some View {
        VStack(alignment: .leading, spacing: StudioMetrics.sectionGap) {
            if let failure = checker.failure { Banner(.error, title: "Couldn’t check that", message: failure) }
            if checker.report != nil { result } else { empty }
        }
        .padding(.top, 84).padding(.horizontal, StudioMetrics.pageInset).padding(.bottom, 24)
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .onGeometryChange(for: CGFloat.self, of: { $0.size.width }) { width = $0 }
        .onAppear { checker.studio.loadCatalog(); checker.studio.checkup.load() }
    }

    // MARK: Empty

    private var empty: some View {
        Group {
            if sideBySide {
                HStack(alignment: .top, spacing: StudioMetrics.sectionGap) { CheckerDropZone(checker: checker); checks.frame(width: 340) }
            } else {
                ScrollView { VStack(spacing: StudioMetrics.sectionGap) { CheckerDropZone(checker: checker).frame(minHeight: 300); checks } }
            }
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }
    private var checks: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 14) {
                Text("What gets checked").font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                ForEach(Self.checkList, id: \.title) { check in
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: check.symbol).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.accent).frame(width: 22).accessibilityHidden(true)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(check.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                            Text(check.detail).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityElement(children: .combine)
                }
            }
        }
    }
    private static let checkList: [(symbol: String, title: String, detail: String)] = [
        ("xmark.circle", "Settings that don’t exist", "Typos and settings from other games, with the likely fix."),
        ("gauge.with.needle", "Values outside the limits", "Values the game refuses, and the nearest one it takes."),
        ("lock", "Protected settings", "Read-only, or only changing with cheats on."),
        ("shield", "What server rules change", "Settings that zPAM and other rule sets fix or limit."),
        ("doc.on.doc", "Repeats", "The same setting written twice, and which line wins."),
        ("key", "Passwords and logins", "Pointed out before you share the file, and never shown.")
    ]

    // MARK: Result

    private var result: some View {
        Group {
            if sideBySide {
                HStack(alignment: .top, spacing: StudioMetrics.sectionGap) { CheckerLines(checker: checker); CheckerFindings(checker: checker).frame(width: 340) }
            } else {
                // What needs a look comes first; the file's lines follow, each half scrolling on its own.
                VStack(spacing: StudioMetrics.sectionGap) { CheckerFindings(checker: checker); CheckerLines(checker: checker) }
            }
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }
}

/// Check a Config's toolbar, in the same places a loadout's toolbar uses: what the config is checked
/// for in the middle, and Check Another at the trailing edge once there is a result.
struct ConfigCheckerScope: View {
    @ObservedObject var checker: ConfigCheckerModel
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 8) {
            Text("Check for").font(.system(size: 12)).foregroundStyle(palette.secondary).padding(.leading, 10)
            Picker("Game version to check for", selection: $checker.dialect) {
                Text(ConfigCheckerModel.title(.cod2x)).tag(ConfigDialect.cod2x)
                Text(ConfigCheckerModel.title(.stock)).tag(ConfigDialect.stock)
            }
            .labelsHidden().fixedSize().help("The game version whose settings and limits the config is checked against")
            Picker("Server rules to check for", selection: $checker.ruleset) {
                Text("No server rules").tag(String?.none)
                ForEach(checker.rulesets) { Text($0.title).tag(String?.some($0.id)) }
            }
            .labelsHidden().fixedSize().help("Server rules to check against as well, such as zPAM’s")
        }
    }
}

struct ConfigCheckerAnother: View {
    @ObservedObject var checker: ConfigCheckerModel
    var body: some View {
        Button { checker.reset() } label: {
            Label("Check Another", systemImage: "xmark").labelStyle(.titleAndIcon).font(.system(size: 13, weight: .semibold))
        }
        .disabled(checker.report == nil && checker.failure == nil)
        .help("Close this check. Nothing was saved, so nothing is lost.")
    }
}

private struct CheckerDropZone: View {
    @ObservedObject var checker: ConfigCheckerModel
    @Environment(\.palette) private var palette
    @State private var targeted = false
    @State private var loadoutMenu = MenuAnchorBox()

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous)
        VStack(spacing: 14) {
            Image(systemName: "doc.text.magnifyingglass").font(.system(size: 34, weight: .light)).foregroundStyle(palette.accent).accessibilityHidden(true)
            Text("DROP A CONFIG HERE").font(.stencil(30)).tracking(1).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            Text("Or paste one. It is read on this Mac only: nothing is uploaded, and nothing is added to your loadouts unless you ask.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).frame(maxWidth: 420)
                .fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 10) {
                Button("Choose a File…") { checker.chooseFile() }.prominentAction()
                Button("Paste from Clipboard") { checker.paste() }
                // A real AppKit menu under the button, as the launcher's other menus of places are.
                Button {
                    guard let view = loadoutMenu.view else { return }
                    let checker = checker
                    NativeMenu.popUp((checker.studio.session?.profiles ?? []).map { profile in
                        .init(title: profile.displayName, symbol: "slider.horizontal.3") { checker.checkLoadout(profile) }
                    }, below: view)
                } label: { Label("Check a Loadout", systemImage: "chevron.down").labelStyle(.titleOnly) }
                .background(MenuAnchor(box: loadoutMenu))
                .fixedSize().disabled((checker.studio.session?.profiles ?? []).isEmpty)
                .accessibilityLabel("Check a Loadout").accessibilityHint("Shows your loadouts")
            }
            .controlSize(.large).padding(.top, 4)
        }
        .padding(40)
        .frame(maxWidth: .infinity, minHeight: 420)
        .background(targeted ? palette.selection : palette.panel, in: shape)
        .overlay(shape.strokeBorder(targeted ? palette.selectionStroke : palette.panelStroke, style: StrokeStyle(lineWidth: 1, dash: [6, 5])))
        .onDrop(of: [UTType.fileURL, UTType.plainText], isTargeted: $targeted) { checker.drop($0) }
        .accessibilityElement(children: .contain)
    }
}

/// The config, line by line, with a mark and a few words beside each line that has a finding.
private struct CheckerLines: View {
    @ObservedObject var checker: ConfigCheckerModel
    @Environment(\.palette) private var palette

    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 16, leading: 0, bottom: 8, trailing: 0)) {
            VStack(alignment: .leading, spacing: 10) {
                HStack(alignment: .center, spacing: 12) {
                    VStack(alignment: .leading, spacing: 3) {
                        Text(checker.source?.name ?? "").font(.system(size: 16, weight: .bold)).foregroundStyle(palette.heading).lineLimit(1).truncationMode(.middle)
                            .accessibilityAddTraits(.isHeader)
                        Text(checker.meta).font(StudioTypography.help).foregroundStyle(palette.secondary).lineLimit(1)
                    }
                    Spacer(minLength: 12)
                    Picker("Which lines to show", selection: $checker.markedOnly) {
                        Text("All Lines").tag(false)
                        Text("Only Marked").tag(true)
                    }
                    .segmentedChoice()
                }
                .padding(.horizontal, 18)
                Rectangle().fill(palette.separator).frame(height: 1)
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 0) {
                        ForEach(checker.lines) { line in CheckerLineRow(line: line) }
                    }
                    .padding(.vertical, 4)
                }
                .accessibilityLabel("Config lines")
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct CheckerLineRow: View {
    var line: ConfigCheckShownLine
    @Environment(\.palette) private var palette

    var body: some View {
        let removed = line.mark == .removed
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Text("\(line.id)").font(.typewriter(12)).monospacedDigit().foregroundStyle(palette.tertiary).frame(width: 40, alignment: .trailing)
            Group {
                if let symbol { Image(systemName: symbol).font(.system(size: 11, weight: .bold)).foregroundStyle(color) } else { Color.clear }
            }
            .frame(width: 14, height: 12).accessibilityHidden(true)
            Text(line.text.isEmpty ? " " : line.text).font(.typewriter(13)).foregroundStyle(removed ? palette.tertiary : palette.text)
                .strikethrough(removed).lineLimit(1).truncationMode(.tail).textSelection(.enabled)
            Spacer(minLength: 8)
            if !line.note.isEmpty { Text(line.note).font(StudioTypography.help).foregroundStyle(color).lineLimit(1).layoutPriority(1) }
        }
        .padding(.vertical, 3).padding(.trailing, 18)
        .background(tinted ? color.opacity(0.09) : .clear)
        .accessibilityElement(children: .ignore).accessibilityAddTraits(.isStaticText)
        .accessibilityLabel("Line \(line.id): \(line.text)" + (line.note.isEmpty ? "" : ". \(line.note)"))
    }
    private var tinted: Bool { if case .finding = line.mark { return true }; return false }
    private var symbol: String? {
        switch line.mark {
        case .finding(.broken): "xmark"
        case .finding(.different): "exclamationmark.triangle.fill"
        case .finding(.note): "info.circle"
        case .removed: "minus"
        case .changed: "checkmark"
        case nil: nil
        }
    }
    private var color: Color {
        switch line.mark {
        case .finding(.broken): palette.negative
        case .finding(.different): palette.caution
        case .finding(.note): palette.compare
        case .removed, .changed: palette.positive
        case nil: palette.secondary
        }
    }
}

/// The summary, the actions on the whole config, and each finding with its own fix.
private struct CheckerFindings: View {
    @ObservedObject var checker: ConfigCheckerModel
    @Environment(\.palette) private var palette

    var body: some View {
        let summary = checker.summary
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 18, leading: 18, bottom: 18, trailing: 18)) {
            VStack(alignment: .leading, spacing: 14) {
                HStack(alignment: .center, spacing: 14) {
                    Text("\(summary.count)").font(.stencil(54)).monospacedDigit()
                        .foregroundStyle(summary.count == 0 ? palette.positive : checker.applied.isEmpty ? palette.accent : palette.heading).accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 3) {
                        Text(summary.title).font(.system(size: 16, weight: .bold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                        Text(summary.detail).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                }
                .accessibilityElement(children: .combine).accessibilityAddTraits(.isHeader)
                actions
                Rectangle().fill(palette.separator).frame(height: 1)
                if checker.open.isEmpty { passed } else { list }
            }
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }

    @ViewBuilder private var actions: some View {
        let count = checker.fixable.count
        FlowChipsLayout(spacing: 8) {
            if count > 0 { Button(count == 1 ? "Fix 1 Line" : "Fix All \(count)") { checker.fixAll() }.prominentAction().help("Applies every fix under Won’t work and Won’t do what it says. Notes are left for you.") }
            Button("Save as Loadout…") { checker.saveAsLoadout() }.disabled(checker.studio.session == nil || checker.studio.isReadOnly)
                .help("Reviews this config, with your fixes, as a new loadout or into one you have")
            Button(checker.applied.isEmpty ? "Copy Config" : "Copy Cleaned Config") { checker.copyCleaned() }
            if !checker.applied.isEmpty {
                Button("Export…") { checker.export() }.help("Saves the config with your fixes as a new file")
                Button { checker.undo() } label: { Label("Undo", systemImage: "arrow.uturn.backward") }.help("Puts every line back as it was")
            }
        }
    }

    private var list: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                ForEach(ConfigCheckFinding.Kind.allCases, id: \.rawValue) { kind in
                    let findings = checker.open(kind)
                    if !findings.isEmpty {
                        VStack(alignment: .leading, spacing: 6) {
                            HStack(spacing: 8) {
                                Image(systemName: Self.symbol(kind)).font(.system(size: 12, weight: .bold)).foregroundStyle(color(kind)).accessibilityHidden(true)
                                Text(Self.title(kind)).font(.system(size: 13, weight: .bold)).foregroundStyle(palette.heading)
                                Spacer(minLength: 6)
                                Text(findings.count == 1 ? "1 line" : "\(findings.count) lines").font(StudioTypography.help).foregroundStyle(palette.secondary)
                            }
                            .accessibilityElement(children: .combine).accessibilityAddTraits(.isHeader)
                            ForEach(findings) { finding in row(finding) }
                        }
                    }
                }
            }
        }
    }
    private func row(_ finding: ConfigCheckFinding) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Text("\(finding.line)").font(.typewriter(12)).monospacedDigit().foregroundStyle(palette.tertiary).frame(width: 30, alignment: .trailing).padding(.top, 2)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                Text(finding.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                Text(finding.detail).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
            .accessibilityElement(children: .combine).accessibilityLabel("Line \(finding.line). \(finding.title). \(finding.detail)")
            Spacer(minLength: 6)
            if let label = finding.fixLabel {
                Button(label) { checker.fix(finding) }.controlSize(.small).fixedSize()
                    .accessibilityLabel("\(label): line \(finding.line), \(finding.title)")
            }
        }
        .padding(.vertical, 6).padding(.horizontal, 8)
        .background(palette.well.opacity(0.5), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }
    private var passed: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(checker.applied.isEmpty ? "Nothing to fix" : "Nothing left to fix").font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            ForEach(passes, id: \.self) { text in
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    Image(systemName: "checkmark").font(.system(size: 11, weight: .bold)).foregroundStyle(palette.positive).accessibilityHidden(true)
                    Text(text).font(.system(size: 13)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                }
            }
            Text("A server can still apply rules of its own. The ready check on Servers shows those before you join.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true).padding(.top, 4)
        }
    }
    private var passes: [String] {
        let game = ConfigCheckerModel.title(checker.dialect)
        let count = checker.report?.commands ?? 0
        return ["\(count) settings and keys, all known to \(game)", "Every value inside the game’s limits", "Nothing that needs cheats", "No repeats"]
            + (checker.ruleset == nil ? [] : ["Nothing \(checker.rulesetTitle) would change"]) + ["No passwords or logins"]
    }
    static func title(_ kind: ConfigCheckFinding.Kind) -> String {
        switch kind { case .broken: "Won’t work"; case .different: "Won’t do what it says"; case .note: "Worth knowing" }
    }
    static func symbol(_ kind: ConfigCheckFinding.Kind) -> String {
        switch kind { case .broken: "xmark"; case .different: "exclamationmark.triangle.fill"; case .note: "info.circle" }
    }
    private func color(_ kind: ConfigCheckFinding.Kind) -> Color {
        switch kind { case .broken: palette.negative; case .different: palette.caution; case .note: palette.compare }
    }
}

/// Buttons that wrap onto a second row when the panel is narrow.
struct FlowChipsLayout: Layout {
    var spacing: CGFloat = 8
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let rows = rows(proposal.width ?? .infinity, subviews)
        return CGSize(width: rows.map(\.width).max() ?? 0, height: rows.map(\.height).reduce(0, +) + spacing * CGFloat(max(0, rows.count - 1)))
    }
    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for row in rows(bounds.width, subviews) {
            var x = bounds.minX
            for index in row.items {
                let size = subviews[index].sizeThatFits(.unspecified)
                subviews[index].place(at: CGPoint(x: x, y: y + (row.height - size.height) / 2), proposal: .unspecified)
                x += size.width + spacing
            }
            y += row.height + spacing
        }
    }
    private func rows(_ width: CGFloat, _ subviews: Subviews) -> [(items: [Int], width: CGFloat, height: CGFloat)] {
        var rows: [(items: [Int], width: CGFloat, height: CGFloat)] = []
        for index in subviews.indices {
            let size = subviews[index].sizeThatFits(.unspecified)
            if var last = rows.last, last.width + spacing + size.width <= width {
                last.items.append(index); last.width += spacing + size.width; last.height = max(last.height, size.height)
                rows[rows.count - 1] = last
            } else {
                rows.append(([index], size.width, size.height))
            }
        }
        return rows
    }
}
