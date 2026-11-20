import SwiftUI
import AppKit

/// A loadout's Binds tab: List (actions with primary and second keys, and advanced
/// binds) and Visual (the keyboard and pointing device, with an inspector). Keys are
/// engine identities; the drawing uses the current macOS input source for labels.
struct ConfigBindsView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @ObservedObject var session: ConfigStudioSession
    init(studio: ConfigStudioModel, session: ConfigStudioSession) {
        self.studio = studio; self.session = session; binds = studio.binds; devices = studio.binds.devices
    }

    var body: some View {
        Group {
            switch binds.layout {
            case .list: BindsListLayout(studio: studio, binds: binds)
            case .visual: BindsVisualLayout(studio: studio, binds: binds, devices: devices)
            }
        }
        .background(WindowReader { binds.window = $0 })
        .onAppear { devices.startObserving(); devices.refresh() }
        .onDisappear { binds.cancelCapture(); devices.stopObserving() }
    }
}

/// Gives the store the window its capture monitor may listen to, as soon as the view
/// joins (or leaves) one.
private struct WindowReader: NSViewRepresentable {
    var found: @MainActor (NSWindow?) -> Void
    func makeNSView(context: Context) -> Reporter { Reporter(found: found) }
    func updateNSView(_ view: Reporter, context: Context) { view.found = found }

    final class Reporter: NSView {
        var found: @MainActor (NSWindow?) -> Void
        init(found: @escaping @MainActor (NSWindow?) -> Void) { self.found = found; super.init(frame: .zero) }
        required init?(coder: NSCoder) { nil }
        override func viewDidMoveToWindow() { super.viewDidMoveToWindow(); found(window) }
    }
}

// MARK: - List

private struct BindsListLayout: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.palette) private var palette
    @FocusState private var searchFocused: Bool

    var body: some View {
        GeometryReader { proxy in
            // The binds on the left; the advanced bind being edited in its own column on the right.
            if proxy.size.width >= 840 {
                HStack(alignment: .top, spacing: 24) {
                    VStack(alignment: .leading, spacing: 16) {
                        search
                        ScrollView { lists.padding(.trailing, 2).padding(.bottom, 8) }.scrollIndicators(.automatic)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                    if binds.draft != nil {
                        // The editor can outgrow the window; it scrolls on its own, beside the list.
                        ScrollView { AdvancedBindEditor(studio: studio, binds: binds).padding(.bottom, 8) }
                            .frame(width: 420).frame(maxHeight: .infinity, alignment: .top)
                    }
                }
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 16) {
                        search
                        AdvancedBindsList(studio: studio, binds: binds)
                        if binds.draft != nil { AdvancedBindEditor(studio: studio, binds: binds) }
                        actionGroups
                    }
                    .padding(.trailing, 4)
                }
            }
        }
    }

    private var search: some View {
        SearchPill("Find an action or key", text: $binds.query, focus: $searchFocused, width: nil)
            .frame(maxWidth: .infinity)
            .help("Find an action or a key (⌘F)")
            .onChange(of: studio.findRequest) { _, _ in searchFocused = true }
    }

    private var lists: some View {
        VStack(alignment: .leading, spacing: 16) {
            AdvancedBindsList(studio: studio, binds: binds)
            actionGroups
        }
    }

    @ViewBuilder private var actionGroups: some View {
        VStack(alignment: .leading, spacing: 16) {
            ForEach(ConfigBindingGroup.allCases, id: \.self) { group in
                let rows = binds.rows(in: group)
                if !rows.isEmpty {
                    VStack(alignment: .leading, spacing: 8) {
                        // The section title, with the two key columns' heads over their keys.
                        HStack(spacing: 8) {
                            Text(group.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .accessibilityAddTraits(.isHeader)
                            GroupLabel("Primary", header: false).frame(width: KeyChip.width).accessibilityHidden(true)
                            GroupLabel("Second", header: false).frame(width: KeyChip.width).accessibilityHidden(true)
                        }
                        .padding(.leading, 6).padding(.trailing, 20)
                        FormGroup {
                            ForEach(rows, id: \.action.name) { row in
                                BindRowView(studio: studio, binds: binds, row: row).hairline()
                            }
                        }
                    }
                }
            }
            let others = binds.otherEntries
            if !others.isEmpty {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Other binds").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6)
                        .accessibilityAddTraits(.isHeader)
                    FormGroup {
                        ForEach(others, id: \.key) { entry in
                            OtherBindRow(studio: studio, binds: binds, entry: entry).hairline()
                        }
                    }
                }
            }
            if binds.bindings != nil && ConfigBindingGroup.allCases.allSatisfy({ binds.rows(in: $0).isEmpty }) && others.isEmpty {
                StudioEmptyState(title: "NO MATCH", message: "No action or key matches. Try a key name like “Space”, or an action like “reload”.").frame(maxWidth: .infinity)
            }
        }
    }
}

private extension View {
    /// A form row's hairline across the full width; the group hides the first one.
    func hairline() -> some View { modifier(HairlineAbove()) }
}
private struct HairlineAbove: ViewModifier {
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        content.overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
    }
}

private struct BindRowView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    var row: ConfigBindingRow
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let review = binds.review.flatMap { $0.action?.name == row.action.name ? $0 : nil }
        let listeningHere: Bool = {
            if case .slot(let action, _)? = binds.target { return action == row.action.name }
            return false
        }()
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(row.action.title).font(.system(size: 14)).foregroundStyle(palette.text)
                    if !row.additional.isEmpty {
                        Text("Also " + row.additional.map(binds.keyName).joined(separator: ", ")).font(StudioTypography.help).foregroundStyle(palette.tertiary)
                    }
                }
                Spacer(minLength: 8)
                KeyChip(studio: studio, binds: binds, row: row, slot: .primary)
                KeyChip(studio: studio, binds: binds, row: row, slot: .secondary)
            }
            .frame(minHeight: 28)
            if listeningHere, let note = binds.captureNote {
                StudioStatusLabel(title: note, tone: .caution)
            }
            if let review {
                // A key that already does something: a tip with the choice, beside the row it is about.
                Banner(.tip, symbol: "exclamationmark.triangle", message: binds.reviewText(review)) {
                    HStack(spacing: 8) {
                        Button("Use \(review.key.map(binds.keyName) ?? "It") for \(row.action.title)") { binds.confirmReview(undo: undo) }
                            .glassAction().controlSize(.large).fixedSize()
                        Button("Cancel") { binds.cancelReview() }.glassAction().controlSize(.large).keyboardShortcut(.cancelAction)
                    }
                }
            }
        }
        .padding(.vertical, 8).padding(.horizontal, 20)
        .frame(minHeight: FormMetrics.rowHeight)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(row.action.title)
    }
}

/// A key button: press it, then press the key to use. Escape cancels. Drawn as the form row's field.
private struct KeyChip: View {
    static let width: CGFloat = 92
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    var row: ConfigBindingRow
    var slot: ConfigBindingSlot
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let key = slot == .primary ? row.primary : row.secondary
        let listening = binds.target == .slot(action: row.action.name, slot: slot)
        let label = listening ? "Press a key…" : key.map(binds.keyName) ?? "Add"
        let slotName = slot == .primary ? "Primary" : "Second"
        Button {
            if listening { binds.cancelCapture() } else { binds.beginCapture(.slot(action: row.action.name, slot: slot)) }
        } label: {
            Text(label).font(.system(size: 13, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.85)
                .foregroundStyle(listening ? palette.accent : key == nil ? palette.tertiary : palette.text)
                .frame(width: Self.width, height: 28)
                .background(listening ? palette.selection : palette.field, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous)
                    .strokeBorder(listening ? palette.accent.opacity(0.75) : palette.wellStroke, lineWidth: listening ? 2 : 1))
                .contentShape(RoundedRectangle(cornerRadius: 7, style: .continuous))
        }
        .buttonStyle(.plain)
        .disabled(studio.isReadOnly)
        .shapeAudit(.field, .rounded(7), "bind key \(row.action.name) \(slot == .primary ? "primary" : "second")")
        .contextMenu {
            if key != nil { Button("Clear \(slotName) Key") { binds.assign(nil, to: row.action.name, slot: slot, undo: undo) } }
            Button("Change…") { binds.beginCapture(.slot(action: row.action.name, slot: slot)) }
        }
        .accessibilityLabel("\(slotName) key for \(row.action.title): \(key.map(binds.keyName) ?? "none")")
        .accessibilityHint(listening ? "Press a key or click a mouse button. Escape cancels." : "Press to change")
    }
}

/// A key name in a list of binds: the shared key cap at a row's height.
private struct BindKeyLabel: View {
    var text: String
    var width: CGFloat? = KeyChip.width
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
            .padding(.horizontal, 8).frame(minWidth: width, minHeight: 28)
            .background(palette.field, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
    }
}

private struct OtherBindRow: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    var entry: ConfigBindingEntry
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 12) {
            BindKeyLabel(text: binds.keyName(entry.key))
            VStack(alignment: .leading, spacing: 2) {
                Text(binds.describe(entry)).font(.system(size: 14)).foregroundStyle(palette.text)
                Text(entry.text).font(.typewriter(12)).foregroundStyle(palette.tertiary).lineLimit(1).truncationMode(.middle).textSelection(.enabled)
            }
            Spacer(minLength: 8)
            if let chain = binds.ownedChain(on: entry.key) {
                Button("Edit…") { binds.selectChain(chain.id) }.glassAction().controlSize(.large)
            } else {
                Button("Clear") { binds.setKey(entry.key, action: nil, undo: undo) }.glassAction().controlSize(.large).disabled(studio.isReadOnly)
            }
        }
        .padding(.vertical, 8).padding(.horizontal, 20)
        .frame(minHeight: FormMetrics.detailRowHeight)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(binds.keyName(entry.key)), \(binds.describe(entry))")
    }
}

// MARK: - Advanced binds

/// The advanced binds, above the action groups: a key that does more than one thing.
private struct AdvancedBindsList: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                Text("Advanced binds").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).accessibilityAddTraits(.isHeader)
                Text("One key that does more than one thing.").font(StudioTypography.help).foregroundStyle(palette.tertiary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Button { binds.newChain() } label: { Label("New Bind", systemImage: "plus") }.quietAction().fixedSize()
                    .disabled(studio.isReadOnly || binds.chainsUnreadable != nil)
            }
            .padding(.leading, 6)
            if let problem = binds.chainsUnreadable {
                StudioBanner(title: "Advanced binds changed outside Config Studio", detail: problem, tone: .caution)
            }
            let newDraft = binds.draft.flatMap { $0.isNew ? $0 : nil }
            if binds.chains.isEmpty && newDraft == nil {
                Text("None yet. A toggle, a cycle of messages, or several actions on one press start here.")
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 6)
            } else {
                VStack(spacing: 4) {
                    ForEach(binds.chains) { chain in
                        ChainCard(binds: binds, draft: ConfigChainDraft(chain: chain), selected: binds.chainID == chain.id) { binds.selectChain(chain.id) }
                    }
                    if let newDraft { ChainCard(binds: binds, draft: newDraft, selected: true) {} }
                }
                .padding(4)
                .background(palette.panel, in: RoundedRectangle(cornerRadius: FormMetrics.radius(.panel), style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: FormMetrics.radius(.panel), style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
                .shapeAudit(.container, .rounded(FormMetrics.radius(.panel)), "advanced binds")
            }
        }
    }
}

/// The advanced bind being edited: its name, key and type, its steps, a preview and the lines it writes.
private struct AdvancedBindEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    @State private var confirming = false

    var body: some View {
        ChainEditor(studio: studio, binds: binds, confirming: $confirming)
            .alert(binds.chainEdit.map { !$0.conflictingKeys.isEmpty } == true ? "Replace the current binding?" : "Save this bind?", isPresented: $confirming) {
                Button("Save Bind") { binds.saveChain(confirmed: true, undo: undo) }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text(confirmationText)
            }
    }
    private var confirmationText: String {
        var lines: [String] = []
        if let edit = binds.chainEdit, !edit.conflictingKeys.isEmpty {
            lines.append("\(edit.conflictingKeys.compactMap { EngineKey(spelling: $0) }.map(binds.keyName).joined(separator: ", ")) already does something else. Saving replaces it.")
        }
        if !binds.chainRisks.isEmpty { lines.append("This bind " + binds.chainRisks.map { $0.summary.prefix(1).lowercased() + $0.summary.dropFirst() }.joined(separator: "; ") + ".") }
        return lines.joined(separator: " ")
    }
}

private struct ChainCard: View {
    @ObservedObject var binds: ConfigBindsModel
    var draft: ConfigChainDraft
    var selected: Bool
    var action: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 16, style: .continuous)
        Button(action: action) {
            HStack(spacing: 12) {
                KeyCap(draft.key.isEmpty ? "—" : EngineKey(spelling: draft.key).map(binds.keyName) ?? draft.key)
                    .frame(minWidth: 30).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                    Text(binds.detail(of: draft)).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineLimit(1)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Badge(draft.kind.short)
            }
            .padding(.vertical, 6).padding(.leading, 12).padding(.trailing, 16)
            .frame(minHeight: 48)
            .background(selected ? palette.selection : .clear, in: shape)
            .overlay(shape.strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(shape)
        }
        .buttonStyle(RowButtonStyle(radius: 16))
        .accessibilityLabel("\(title), \(binds.detail(of: draft)), \(draft.kind.short), key \(draft.key.isEmpty ? "not chosen" : draft.key)")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
    private var title: String { draft.isNew && draft.displayName == nil ? "New bind" : binds.title(of: draft) }
}

private struct ChainEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Binding var confirming: Bool
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        if let draft = binds.draft {
            let listening = binds.target == .chainKey
            VStack(alignment: .leading, spacing: 12) {
                VStack(alignment: .leading, spacing: 3) {
                    Text(draft.isNew && draft.displayName == nil ? "New bind" : binds.title(of: draft))
                        .font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).lineLimit(2).accessibilityAddTraits(.isHeader)
                    Text(draft.key.isEmpty ? "Advanced bind, no key chosen yet" : "Advanced bind on \(EngineKey(spelling: draft.key).map(binds.keyName) ?? draft.key)")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
                FormGroup {
                    // Only Config Studio shows the name; the game never sees it.
                    FormRow("Name") {
                        CommitField(value: draft.name, label: "Name for this bind", width: 240, prompt: "Optional, like “Frame counter”") { name in
                            binds.updateDraft { $0.name = name }
                        }
                    }
                    FormRow("Key") {
                        Button { listening ? binds.cancelCapture() : binds.beginCapture(.chainKey) } label: {
                            Text(listening ? "Press a key…" : draft.key.isEmpty ? "Choose…" : EngineKey(spelling: draft.key).map(binds.keyName) ?? draft.key)
                                .font(.system(size: 13, weight: .semibold)).foregroundStyle(listening ? palette.accent : palette.text)
                                .frame(minWidth: KeyChip.width, minHeight: 28).padding(.horizontal, 6)
                                .background(listening ? palette.selection : palette.field, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                                .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous)
                                    .strokeBorder(listening ? palette.accent.opacity(0.75) : palette.wellStroke, lineWidth: listening ? 2 : 1))
                                .contentShape(RoundedRectangle(cornerRadius: 7, style: .continuous))
                        }
                        .buttonStyle(.plain)
                        .accessibilityLabel("Key for this bind: \(draft.key.isEmpty ? "not chosen" : draft.key). Press to change")
                    }
                    // A row in a narrow column: title and control on one line, the help under both.
                    VStack(alignment: .leading, spacing: 6) {
                        HStack(spacing: 14) {
                            Text("Type").font(.system(size: 14)).foregroundStyle(palette.text)
                            Spacer(minLength: 8)
                            Picker("What a press does", selection: Binding(get: { draft.kind }, set: { kind in binds.updateDraft { $0.kind = kind } })) {
                                ForEach(ConfigChainDraft.Kind.allCases) { Text($0.short).tag($0) }
                            }
                            .segmentedChoice()
                        }
                        Text(draft.kind.title).font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(.vertical, 9).padding(.horizontal, 20)
                    .frame(minHeight: FormMetrics.detailRowHeight)
                    .hairline()
                    .accessibilityElement(children: .contain)
                }
                if listening, let note = binds.captureNote { StudioStatusLabel(title: note, tone: .caution) }
                switch draft.kind {
                case .several: StepList(binds: binds, steps: draft.commands, actionsOnly: true,
                                        title: "Actions, in order") { steps in binds.updateDraft { $0.commands = steps } }
                case .cycle: StepList(binds: binds, steps: draft.steps, actionsOnly: false,
                                      title: "Steps, in order") { steps in binds.updateDraft { $0.steps = steps } }
                case .toggle: toggleRow(draft)
                }
                PressPreview(binds: binds)
                ForEach(Array(binds.chainNotes.enumerated()), id: \.offset) { _, note in
                    StudioStatusLabel(title: note.text, tone: note.tone)
                }
                if !binds.chainRisks.isEmpty {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(binds.chainBlocked ? "Config Studio won’t save this:" : "Needs your OK before saving:").font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text)
                        ForEach(binds.chainRisks, id: \.self) { risk in
                            StudioStatusLabel(title: risk.summary, tone: binds.chainBlocked ? .negative : .caution)
                        }
                    }
                }
                if let error = binds.chainError {
                    StudioStatusLabel(title: error, tone: .negative)
                }
                VStack(alignment: .leading, spacing: 8) {
                    Button { binds.showLines.toggle() } label: {
                        HStack(spacing: 6) {
                            Image(systemName: binds.showLines ? "chevron.down" : "chevron.right").font(.system(size: 11, weight: .semibold)).frame(width: 12)
                                .accessibilityHidden(true)
                            Text("Show the Lines")
                        }
                    }
                    .quietAction().fixedSize().padding(.horizontal, -6)
                    .accessibilityValue(binds.showLines ? "Shown" : "Hidden")
                    if binds.showLines {
                        VStack(alignment: .leading, spacing: 2) {
                            ForEach(Array(binds.chainLines.enumerated()), id: \.offset) { _, line in
                                Text(line).font(.typewriter(12)).foregroundStyle(palette.address).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                            }
                        }
                        .padding(.vertical, 10).padding(.horizontal, 12).frame(maxWidth: .infinity, alignment: .leading)
                        .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                        .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                        .accessibilityLabel("The lines this bind adds to the config")
                    }
                }
                HStack(spacing: 8) {
                    Button(draft.isNew ? "Discard" : "Delete Bind", role: .destructive) { binds.deleteChain(undo: undo) }
                        .destructiveAction().disabled(studio.isReadOnly && !draft.isNew)
                    Spacer()
                    Button("Save Bind") {
                        if binds.chainNeedsConfirmation { confirming = true } else { binds.saveChain(undo: undo) }
                    }
                    .prominentAction()
                    .disabled(binds.chainEdit == nil || binds.chainBlocked || studio.isReadOnly || !(binds.chainEdit?.hasChanges ?? false))
                }
                .actionControlSize()
                .padding(.top, 4)
            }
        }
    }

    @ViewBuilder private func toggleRow(_ draft: ConfigChainDraft) -> some View {
        FormSection(nil) {
            FormGroup {
                FormRow("Turns on or off") {
                    Picker("Setting", selection: Binding(get: { draft.setting }, set: { name in
                        binds.updateDraft { $0.setting = name; $0.values = [] }
                    })) {
                        ForEach(binds.toggleSettings) { Text($0.title).tag($0.name) }
                    }
                    .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
                }
            }
        } footer: {
            if let spec = studio.spec(named: draft.setting) {
                Text("Each press: " + draft.values.map { ConfigValues.display(spec, $0) }.joined(separator: " → ") + " → back to the start")
            }
        }
    }
}

/// Ordered steps with a kind, text, reordering and removal, as rows in a group.
private struct StepList: View {
    @ObservedObject var binds: ConfigBindsModel
    var steps: [ConfigChainDraft.Step]
    var actionsOnly: Bool
    var title: String
    var update: ([ConfigChainDraft.Step]) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).accessibilityAddTraits(.isHeader)
                Spacer()
                Button { add() } label: { Label(actionsOnly ? "Add Action" : "Add Step", systemImage: "plus") }.quietAction().fixedSize()
            }
            .padding(.leading, 6)
            FormGroup {
                ForEach(Array(steps.enumerated()), id: \.element.id) { index, step in
                    HStack(spacing: 8) {
                        Text("\(index + 1)").font(.system(size: 13, weight: .semibold)).monospacedDigit().foregroundStyle(palette.tertiary)
                            .frame(width: 14, alignment: .leading).accessibilityHidden(true)
                        HStack(spacing: 8) {
                            if actionsOnly {
                                actionPicker(step, index)
                                if step.kind != .action { field(step, index) }
                            } else {
                                kindPicker(step, index)
                                if step.kind == .action { actionPicker(step, index) } else { field(step, index) }
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        HStack(spacing: 0) {
                            IconStep(symbol: "chevron.up", label: "Move step \(index + 1) up") { move(index, -1) }.disabled(index == 0)
                            IconStep(symbol: "chevron.down", label: "Move step \(index + 1) down") { move(index, 1) }.disabled(index == steps.count - 1)
                            IconStep(symbol: "minus.circle", label: "Remove step \(index + 1)") { remove(index) }.disabled(steps.count == 1)
                        }
                    }
                    .padding(.vertical, 8).padding(.leading, 16).padding(.trailing, 12)
                    .frame(minHeight: FormMetrics.rowHeight)
                    .hairline()
                }
            }
        }
    }
    private func field(_ step: ConfigChainDraft.Step, _ index: Int) -> some View {
        StepField(text: step.text, label: "Step \(index + 1) \(step.kind.title.lowercased())") { text in set(index) { $0.text = text } }
    }
    private func kindPicker(_ step: ConfigChainDraft.Step, _ index: Int) -> some View {
        let actions = binds.bindings?.actions ?? []
        return Picker("Step \(index + 1) kind", selection: Binding(get: { step.kind }, set: { kind in
            set(index) { $0.kind = kind; $0.text = kind == .action ? actions.first?.name ?? "" : "" }
        })) {
            ForEach(ConfigChainDraft.StepKind.allCases) { Text($0.title).tag($0) }
        }
        .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
    }
    /// Game actions by name; in "several", the last choice switches the step to a console command.
    private func actionPicker(_ step: ConfigChainDraft.Step, _ index: Int) -> some View {
        Picker("Step \(index + 1) action", selection: Binding(get: { step.kind == .action ? step.text : "" }, set: { value in
            set(index) { if value.isEmpty { $0.kind = .command; $0.text = "" } else { $0.kind = .action; $0.text = value } }
        })) {
            ForEach(binds.bindings?.actions ?? []) { Text($0.title).tag($0.name) }
            if actionsOnly { Divider(); Text("Console command…").tag("") }
        }
        .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
    }
    private func set(_ index: Int, _ change: (inout ConfigChainDraft.Step) -> Void) {
        var list = steps
        guard list.indices.contains(index) else { return }
        change(&list[index])
        update(list)
    }
    private func move(_ index: Int, _ offset: Int) {
        var list = steps
        list.swapAt(index, index + offset)
        update(list)
    }
    private func remove(_ index: Int) {
        var list = steps
        list.remove(at: index)
        update(list)
    }
    private func add() {
        update(steps + [actionsOnly ? .init(kind: .action, text: "gocrouch") : .init(kind: .say, text: "")])
    }
}

private struct StepField: View {
    var text: String
    var label: String
    var commit: (String) -> Void
    var body: some View {
        CommitField(value: text, label: label, width: nil, validate: { $0 }, commit: commit)
            .frame(maxWidth: .infinity)
    }
}

private struct IconStep: View {
    var symbol: String
    var label: String
    var action: () -> Void
    @Environment(\.palette) private var palette
    @Environment(\.isEnabled) private var enabled
    var body: some View {
        Button(action: action) {
            Image(systemName: symbol).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.chipIcon)
                .frame(width: StudioMetrics.minimumHitTarget, height: StudioMetrics.minimumHitTarget).contentShape(Circle())
        }
        .buttonStyle(.plain).opacity(enabled ? 1 : 0.4)
        .help(label).accessibilityLabel(label)
    }
}

/// “Each press: A → B → back to the start”, from B2's machine, plus release effects.
private struct PressPreview: View {
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.palette) private var palette
    var body: some View {
        let presses = binds.pressPreview()
        if !presses.isEmpty, let draft = binds.draft {
            VStack(alignment: .leading, spacing: 6) {
                switch draft.kind {
                case .several:
                    line(lead: "One press:", parts: presses[0].press, joiner: "then")
                    if !presses[0].release.isEmpty { line(lead: "Letting go:", parts: presses[0].release, joiner: "then") }
                case .toggle:
                    let spec = binds.studio.spec(named: draft.setting)
                    line(lead: "Each press:", parts: loop(presses.map { press in
                        ConfigCommandWords(text: press.value.map { value in spec.map { ConfigValues.display($0, value) } ?? value } ?? "…", literal: false)
                    }), joiner: "→")
                case .cycle:
                    line(lead: "Each press:", parts: loop(presses.map { $0.press.first ?? ConfigCommandWords(text: "nothing", literal: false) }), joiner: "→")
                }
                if presses.contains(where: { !$0.complete }) {
                    StudioStatusLabel(title: "Part of this can’t be previewed, so the game may do more than shown.", tone: .caution)
                }
            }
            // A neutral banner: information, not a warning.
            .padding(.vertical, 12).padding(.horizontal, 14).frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            .accessibilityElement(children: .combine)
        }
    }
    /// The preview runs one press past the end; when that press repeats the first,
    /// it reads as the loop it is.
    private func loop(_ parts: [ConfigCommandWords]) -> [ConfigCommandWords] {
        guard parts.count > 2, parts.last == parts.first else { return parts }
        return parts.dropLast() + [ConfigCommandWords(text: "back to the start", literal: false)]
    }
    private func line(lead: String, parts: [ConfigCommandWords], joiner: String) -> some View {
        var text = Text(lead + " ").font(.system(size: 13, weight: .semibold)).foregroundColor(palette.secondary)
        for (index, part) in parts.enumerated() {
            if index > 0 { text = Text("\(text)\(Text(" \(joiner) ").foregroundColor(palette.tertiary))") }
            let word = (part.literal ? Text(part.text).font(.typewriter(12)) : Text(part.text).font(.system(size: 13, weight: .semibold))).foregroundColor(palette.text)
            text = Text("\(text)\(word)")
        }
        return text.fixedSize(horizontal: false, vertical: true)
    }
}

// MARK: - Visual

private struct BindsVisualLayout: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette

    var body: some View {
        GeometryReader { proxy in
            let wide = proxy.size.width >= 1040
            ScrollView {
                // The keyboard across the page; the pointing device and the chosen key side by side under it.
                VStack(alignment: .leading, spacing: 16) {
                    if binds.verification != nil || binds.learning != nil {
                        DeviceSetupPanel(binds: binds)
                    }
                    KeyboardPanel(studio: studio, binds: binds, devices: devices, available: proxy.size.width - 4, legendBeside: proxy.size.width >= 880)
                    if wide {
                        HStack(alignment: .top, spacing: 16) {
                            PointerPanel(studio: studio, binds: binds, devices: devices).frame(maxWidth: .infinity)
                            KeyInspector(studio: studio, binds: binds, devices: devices).frame(maxWidth: .infinity)
                        }
                        .fixedSize(horizontal: false, vertical: true)
                    } else {
                        // Narrow windows keep the inspector next to the keyboard it describes.
                        KeyInspector(studio: studio, binds: binds, devices: devices)
                        PointerPanel(studio: studio, binds: binds, devices: devices)
                    }
                }
                .padding(.trailing, 4).padding(.bottom, 8)
            }
        }
        .onAppear {
            if binds.keyboardID == nil { binds.keyboardID = devices.devices.first { $0.kind == .keyboard }?.id }
            if !devices.devices.contains(where: { $0.kind == .mouse }) && devices.devices.contains(where: { $0.kind == .trackpad }) { binds.pointer = .trackpad }
        }
    }
}

/// A key role's colour as a small square: the same tint and edge as the keys, hatched where the game can't use the key.
private struct KeySwatch: View {
    var role: StudioKeyRole
    var size: CGFloat = 14
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 4, style: .continuous)
        ZStack {
            palette.panel
            role.fill(palette)
            if role == .unavailable {
                Canvas { context, canvas in
                    var path = Path()
                    for offset in stride(from: -canvas.height, through: canvas.width, by: 5) {
                        path.move(to: CGPoint(x: offset, y: canvas.height)); path.addLine(to: CGPoint(x: offset + canvas.height, y: 0))
                    }
                    context.stroke(path, with: .color(palette.track), lineWidth: 1.5)
                }
            }
        }
        .frame(width: size, height: size)
        .clipShape(shape)
        .overlay(shape.strokeBorder(role.edge(palette), lineWidth: 1))
        .accessibilityHidden(true)
    }
}

@MainActor private func currentKeyboard(binds: ConfigBindsModel, devices: InputDevicesModel) -> InputDevice? {
    devices.devices.first { $0.id == binds.keyboardID } ?? devices.devices.first { $0.kind == .keyboard }
}

private struct KeyboardPopover: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        let keyboards = devices.devices.filter { $0.kind == .keyboard }
        VStack(alignment: .leading, spacing: 10) {
            if let device = currentKeyboard(binds: binds, devices: devices) {
                let presentation = devices.presentation(for: device)
                VStack(alignment: .leading, spacing: 3) {
                    if keyboards.count > 1 {
                        Picker("Keyboard", selection: Binding(get: { device.id }, set: { binds.keyboardID = $0 })) {
                            ForEach(keyboards) { Text($0.name).tag($0.id) }
                        }
                        .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
                    } else {
                        Text(device.name).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading)
                    }
                    Text("Connected · detected automatically").font(StudioTypography.help).foregroundStyle(palette.secondary)
                }
                row("Key shape", presentation.shape.explanation) {
                    Picker("Key shape", selection: Binding(get: { presentation.shape.value }, set: { value in update(device) { $0.shape = value } })) {
                        ForEach(KeyboardShape.allCases, id: \.self) { Text($0.rawValue.uppercased()).tag($0) }
                    }
                    .segmentedChoice()
                }
                row("Size", presentation.size.explanation) {
                    HStack(spacing: 8) {
                        if presentation.size.basis == .guessed { StudioStatusLabel(title: "Estimated", tone: .unknown).fixedSize() }
                        if presentation.size.basis == .verified { StudioStatusLabel(title: "Confirmed", tone: .positive).fixedSize() }
                        Picker("Keyboard size", selection: Binding(get: { presentation.size.value }, set: { value in update(device) { $0.size = value } })) {
                            ForEach([KeyboardSize.full, .tkl, .percent75, .percent65, .percent60], id: \.self) { Text($0.title).tag($0) }
                            if presentation.size.value == .unknown { Text("Unknown").tag(KeyboardSize.unknown) }
                        }
                        .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
                    }
                }
                row("Modifier labels", presentation.modifiers.explanation) {
                    Picker("Modifier labels", selection: Binding(get: { presentation.modifiers.value }, set: { value in update(device) { $0.modifiers = value } })) {
                        Text("Mac").tag(KeyboardModifierStyle.mac); Text("PC").tag(KeyboardModifierStyle.pc)
                    }
                    .segmentedChoice()
                }
            } else {
                Text("No keyboard reported").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading)
            }
            HStack {
                Text("Layout").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                Text("\(devices.inputSource.name) · from macOS").font(.system(size: 13)).foregroundStyle(palette.secondary)
            }
            Text("Shape and layout come from macOS. Change the size if the drawing looks wrong. Labels follow your input source; saved binds stay on the same game keys.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            if currentKeyboard(binds: binds, devices: devices) != nil {
                Button("Press Keys to Verify…") { dismiss(); binds.beginVerification() }
                    .glassAction().controlSize(.large)
                    .disabled(studio.gameRunning)
                    .help("Answer a few questions by pressing keys, so the shape, size and modifier labels match your keyboard.")
            }
        }
        .padding(18).frame(width: 380)
    }
    /// A manual choice replaces a verified answer, so it no longer says Confirmed.
    private func update(_ device: InputDevice, _ change: (inout InputDeviceOverride) -> Void) {
        var value = devices.overrides[device.id] ?? .init()
        change(&value)
        value.verified = false
        devices.setOverride(value, for: device.id)
    }
    private func row<Control: View>(_ title: String, _ detail: String, @ViewBuilder control: () -> Control) -> some View {
        HStack(alignment: .center, spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 14)).foregroundStyle(palette.text)
                Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 8)
            control()
        }
        .frame(minHeight: 40)
    }
}

private struct PointerPopover: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Pointing devices").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading)
            HStack {
                Text("Show").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                Picker("Device shown", selection: $binds.pointer) { Text("Mouse").tag(InputDeviceKind.mouse); Text("Trackpad").tag(InputDeviceKind.trackpad) }
                    .segmentedChoice()
            }
            HStack {
                Text("Mouse DPI").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                CommitField(value: studio.metadata?.dpi.map { ConfigValues.format($0) } ?? "", label: "Mouse DPI", width: 72, alignment: .trailing, prompt: "DPI",
                            validate: { text in
                                let trimmed = text.trimmingCharacters(in: .whitespaces)
                                if trimmed.isEmpty { return "" }
                                guard let value = ConfigValues.number(trimmed), value > 0, value <= 100_000 else { return nil }
                                return ConfigValues.format(value)
                            }) { studio.setDPI($0.isEmpty ? nil : ConfigValues.number($0), undo: undo) }
                    .disabled(studio.isReadOnly)
                Text("you enter this").font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            HStack {
                Text("Polling rate").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                StudioStatusLabel(title: "Not measured", tone: .unknown)
            }
            Text("Your mouse’s DPI isn’t shared with macOS, so mouse speed uses the number you enter here. It’s saved with this loadout.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            if binds.pointer == .mouse {
                Button("Learn Button Positions…") { dismiss(); binds.beginLearning() }
                    .glassAction().controlSize(.large)
                    .disabled(studio.gameRunning)
                    .help("Click each of your mouse’s buttons in turn, so the side buttons are placed and extra buttons explained.")
            }
        }
        .padding(18).frame(width: 360)
    }
}

/// Press keys to verify / Learn button positions, inline in the window that listens. A popover
/// or sheet is a window of its own, which the capture monitor (scoped to this window) can't hear.
private struct DeviceSetupPanel: View {
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.palette) private var palette

    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            HStack(alignment: .top, spacing: 16) {
                Image(systemName: binds.learning != nil ? "computermouse" : "keyboard").font(.system(size: 22)).foregroundStyle(palette.accent)
                    .frame(width: 28).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 8) {
                    if let flow = binds.verification { verification(flow) }
                    if let flow = binds.learning { learning(flow) }
                }
                Spacer(minLength: 0)
            }
        }
        .accessibilityElement(children: .contain)
    }

    @ViewBuilder private func verification(_ flow: KeyboardVerification) -> some View {
        Text("Verify your keyboard").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
        Text(flow.step.instruction).font(.system(size: 14)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
        if let note = flow.note { StudioStatusLabel(title: note, tone: .caution) }
        if flow.sizeChoices.isEmpty {
            Text("Only this window listens, until you finish. The keys you press here do nothing else. Escape stops.")
                .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
        } else {
            // The keys can't tell these sizes apart, so the person's choice is what's verified.
            Text("Keys work normally again: choose with a click, or with Tab and Space. Escape stops without changing anything.")
                .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
        }
        HStack(spacing: 8) {
            ForEach(flow.sizeChoices, id: \.self) { size in
                Button(size.title) { binds.chooseVerifiedSize(size) }.glassAction()
                    .accessibilityLabel("My keyboard is \(size.title)")
            }
            if flow.canSkip { Button("I Don’t Have This Key") { binds.skipVerificationStep() }.glassAction() }
            Button("Cancel") { binds.stopSetup() }.glassAction()
        }
        .actionControlSize()
    }

    @ViewBuilder private func learning(_ flow: PointerButtonLearning) -> some View {
        Text("Learn your mouse buttons").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
        Group {
            switch flow.step {
            case .position(let position): Text(position.instruction)
            case .extras: Text("Click any other buttons your mouse has, one at a time. Press Return when you’re done.")
            case .done: Text("Done.")
            }
        }
        .font(.system(size: 14)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
        if !flow.learned.isEmpty {
            let learned = PointerButtonPosition.allCases.compactMap { position in flow.learned[position].map { "\(position.title): \(Self.name($0))" } }
            Text(learned.joined(separator: " · ")).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
        }
        if let note = flow.note { StudioStatusLabel(title: note, tone: .caution) }
        // Every other click is an answer; the window's listener lets clicks on these buttons through.
        Text("Clicks are recorded, not pressed, until you finish, except on the buttons below. Press Return if your mouse doesn’t have this button; Escape stops.")
            .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
        HStack(spacing: 8) {
            Button(flow.step == .extras ? "Done" : "I Don’t Have This Button") { binds.skipLearningStep() }.glassAction()
            Button("Cancel") { binds.stopSetup() }.glassAction()
        }
        .actionControlSize()
        .onGeometryChange(for: CGRect.self, of: { $0.frame(in: .global) }) { binds.setupControlFrames = [$0] }
        .onDisappear { binds.setupControlFrames = [] }
    }
    private static func name(_ button: Int) -> String {
        PointerButtonLearning.usable(button) ? "Mouse \(button + 1)" : "button \(button + 1), unused by the game"
    }
}

private struct KeyboardPanel: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    /// The width this panel may take; keys grow to fill it, within reason.
    var available: CGFloat
    /// The key colours sit beside the drawing when there is room, and under it otherwise.
    var legendBeside = true
    @Environment(\.palette) private var palette
    /// Native key controls share one roving Tab stop.
    @StateObject private var keyFocus = KeyboardKeyFocus()
    @State private var keyboardOpen = false

    var body: some View {
        let device = currentKeyboard(binds: binds, devices: devices)
        let presentation = device.map { devices.presentation(for: $0) }
        let shape = presentation?.shape.value ?? devices.inputSource.shape
        let size = presentation.map { $0.size.value == .unknown ? .percent75 : $0.size.value } ?? .percent75
        let modifiers = presentation?.modifiers.value ?? .mac
        let keys = ConfigKeyboardLayout.keys(shape: shape, size: size)
        let columns = (keys.map { $0.x + $0.width }.max() ?? 15)
        let rows = (keys.map { $0.y + $0.height }.max() ?? 6)
        let inset: CGFloat = available < 640 ? 16 : 20
        let legendWidth: CGFloat = legendBeside ? 184 + 24 : 0
        // Keys keep a 4-point gap and never shrink below the hit target.
        let unit = min(48, max(StudioMetrics.minimumHitTarget + 4, ((available - inset * 2 - legendWidth) / columns).rounded(.down)))
        let finding = binds.target == .find
        let chipTitle = "\(device?.name ?? "Keyboard") · \(shape.rawValue.uppercased()) · \(devices.inputSource.name)"
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: inset, leading: inset, bottom: inset, trailing: inset)) {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 12) {
                    Chip(chipTitle, action: { keyboardOpen.toggle() }) {
                        Image(systemName: "keyboard").accessibilityHidden(true)
                        Text(chipTitle)
                        Image(systemName: "chevron.down").font(.system(size: 9, weight: .semibold)).accessibilityHidden(true)
                    }
                    .popover(isPresented: $keyboardOpen, arrowEdge: .bottom) { KeyboardPopover(studio: studio, binds: binds, devices: devices).studioPopover() }
                    Spacer(minLength: 8)
                    // The hint steps aside rather than truncating in narrow windows.
                    ViewThatFits(in: .horizontal) {
                        Text("Click any key or button to see or change what it does.").font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize()
                        Color.clear.frame(width: 0, height: 0)
                    }
                    Button { finding ? binds.cancelCapture() : binds.beginCapture(.find) } label: {
                        Label(finding ? "Press a Key…" : "Find by Pressing…", systemImage: "hand.point.up.left")
                    }
                    .glassAction().controlSize(.large).fixedSize()
                    .help("Press a key or click a mouse button to select it. Escape cancels.")
                }
                // In a narrow window the keys take the panel's tighter inset; the bar above them keeps 20.
                .padding([.top, .horizontal], 20 - inset)
                // No scroll view around the drawing: one would take the arrow keys for scrolling,
                // and the keys shrink to fit the launcher's narrowest window instead. The drawing
                // is one Tab stop (the chosen key, else the first); the arrow keys move focus and
                // selection together along it, and Tab leaves it.
                let entry = ConfigKeyboardLayout.entry(in: keys, selectedVirtualKey: binds.selectedVirtualKey, selectedKey: binds.selectedKey, source: devices.inputSource)
                let drawing = ZStack(alignment: .topLeading) {
                    ForEach(keys) { key in
                        KeyButton(binds: binds, devices: devices, key: key, unit: unit, modifiers: modifiers,
                                  tabStop: key.virtualKey == entry, focus: keyFocus,
                                  move: { direction in move(direction, from: key.virtualKey, keys: keys) })
                    }
                }
                .frame(width: columns * unit, height: rows * unit, alignment: .topLeading)
                .accessibilityElement(children: .contain)
                .accessibilityLabel("\(device?.name ?? "Keyboard"), \(shape.rawValue.uppercased()) layout. Each key says what it does.")
                if legendBeside {
                    HStack(alignment: .top, spacing: 24) { drawing; Legend(column: true) }
                } else {
                    drawing
                    Legend(column: false)
                }
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }
    /// Arrow keys follow the drawing: the nearest key in that direction. The next key becomes
    /// the Tab stop first, then takes focus.
    private func move(_ direction: MoveCommandDirection, from virtualKey: UInt16, keys: [ConfigKeyboardKey]) {
        let mapped: ConfigKeyboardDirection? = direction == .up ? .up : direction == .down ? .down : direction == .left ? .left : direction == .right ? .right : nil
        guard let mapped, let next = ConfigKeyboardLayout.neighbor(of: virtualKey, mapped, in: keys) else { return }
        keyFocus.focus(next)
    }
}

/// Keeps only weak references to this drawing's actual AppKit buttons.
@MainActor private final class KeyboardKeyFocus: ObservableObject {
    let buttons = NSMapTable<NSNumber, KeyboardKeyControl.KeyButton>.strongToWeakObjects()
    func entered(_ button: KeyboardKeyControl.KeyButton) {
        for candidate in buttons.objectEnumerator()?.allObjects as? [KeyboardKeyControl.KeyButton] ?? [] {
            candidate.entry = candidate === button
        }
        button.window?.recalculateKeyViewLoop()
    }
    func focus(_ key: UInt16) {
        guard let button = buttons.object(forKey: NSNumber(value: key)), let window = button.window else { return }
        _ = window.makeFirstResponder(button)
    }
}

/// The cap stays SwiftUI; its single accessible, focusable control uses AppKit's responder chain.
private struct KeyboardKeyControl: NSViewRepresentable {
    var virtualKey: UInt16
    var entry: Bool
    var selected: Bool
    var label: String
    var focus: KeyboardKeyFocus
    var move: (MoveCommandDirection) -> Void
    var select: () -> Void

    func makeNSView(context: Context) -> KeyButton {
        let button = KeyButton(frame: .zero)
        button.virtualKey = virtualKey
        button.focus = focus
        focus.buttons.setObject(button, forKey: NSNumber(value: virtualKey))
        return button
    }
    func updateNSView(_ button: KeyButton, context: Context) {
        button.entry = entry
        button.move = move
        button.select = select
        button.setAccessibilityLabel(label)
        button.setAccessibilitySelected(selected)
    }
    static func dismantleNSView(_ button: KeyButton, coordinator: ()) {
        let key = NSNumber(value: button.virtualKey)
        if button.focus?.buttons.object(forKey: key) === button { button.focus?.buttons.removeObject(forKey: key) }
    }

    final class KeyButton: NSButton {
        var virtualKey: UInt16 = 0
        var entry = false
        weak var focus: KeyboardKeyFocus?
        var move: ((MoveCommandDirection) -> Void)?
        var select: (() -> Void)?
        override init(frame: NSRect) {
            super.init(frame: frame)
            title = ""
            isTransparent = true
            setButtonType(.momentaryPushIn)
            focusRingType = .exterior
            target = self
            action = #selector(choose)
        }
        required init?(coder: NSCoder) { nil }
        // Expose the actual control, rather than NSButton's unlabeled decorative cell.
        override func isAccessibilityElement() -> Bool { true }
        override func accessibilityRole() -> NSAccessibility.Role? { .button }
        override func accessibilityChildren() -> [Any]? { [] }
        override var accessibilityFocusedUIElement: Any? { self }
        override func isAccessibilityFocused() -> Bool { window?.isKeyWindow == true && window?.firstResponder === self }
        override func accessibilityHitTest(_ point: NSPoint) -> Any? { self }
        override var acceptsFirstResponder: Bool { isEnabled }
        override var canBecomeKeyView: Bool { entry && isEnabled && !isHiddenOrHasHiddenAncestor && window != nil }
        override func becomeFirstResponder() -> Bool {
            guard super.becomeFirstResponder() else { return false }
            focus?.entered(self)
            select?()
            return true
        }
        @objc private func choose() {
            guard let window, window.makeFirstResponder(self) else { return }
            select?()
        }
        override func mouseDown(with event: NSEvent) {
            guard let window, window.makeFirstResponder(self) else { return }
            super.mouseDown(with: event)
        }
        override func accessibilityPerformPress() -> Bool {
            guard let window, window.makeFirstResponder(self) else { return false }
            performClick(nil)
            return true
        }
        override func keyDown(with event: NSEvent) {
            if event.modifierFlags.intersection([.command, .control, .option]).isEmpty {
                let direction: MoveCommandDirection? = [123: .left, 124: .right, 125: .down, 126: .up][event.keyCode]
                if let direction { move?(direction); return }
                if event.keyCode == 48 {
                    if event.modifierFlags.contains(.shift) { window?.selectPreviousKeyView(self) }
                    else { window?.selectNextKeyView(self) }
                    return
                }
            }
            super.keyDown(with: event)
        }
        override var focusRingMaskBounds: NSRect { bounds }
        override func drawFocusRingMask() { NSBezierPath(roundedRect: bounds, xRadius: 7, yRadius: 7).fill() }
    }
}

private struct KeyButton: View {
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    var key: ConfigKeyboardKey
    var unit: CGFloat
    var modifiers: KeyboardModifierStyle
    /// The drawing's one Tab stop; the other keys are reached with the arrow keys.
    var tabStop = false
    var focus: KeyboardKeyFocus
    var move: (MoveCommandDirection) -> Void

    var body: some View {
        let mapping = devices.inputSource.mapping(virtualKey: key.virtualKey)
        let engine = mapping.engineKey
        let entry = engine.flatMap { binds.bindings?.entry(for: $0) }
        let role: StudioKeyRole = { if case .available = mapping { return binds.role(of: entry) }; return .unavailable }()
        let label = Self.label(key.virtualKey, source: devices.inputSource, modifiers: modifiers)
        let caption = entry.map { binds.describe($0) }
        let width = key.width * unit - 4, height = key.height * unit - 4
        let selected = binds.selectedVirtualKey == key.virtualKey || (binds.selectedVirtualKey == nil && engine != nil && engine == binds.selectedKey)
        ZStack {
            StudioKeyCap(key: label, action: width >= 76 || key.virtualKey == 49 ? caption : nil, role: role, selected: selected, width: width, height: height)
                .allowsHitTesting(false).accessibilityHidden(true)
            KeyboardKeyControl(virtualKey: key.virtualKey, entry: tabStop, selected: selected,
                               label: "\(Self.spoken(label, key.virtualKey)), \(role == .unavailable ? mapping.explanation ?? StudioKeyRole.unavailable.rawValue : caption ?? "not bound")",
                               focus: focus, move: move) {
                binds.selectedVirtualKey = key.virtualKey
                binds.selectedKey = engine
            }
        }
        .frame(width: width, height: height)
        .offset(x: key.x * unit, y: key.y * unit)
    }
    static func label(_ virtualKey: UInt16, source: InputSourceSnapshot, modifiers: KeyboardModifierStyle) -> String {
        let mac = modifiers == .mac
        switch virtualKey {
        case 55, 54: return mac ? "⌘" : "Win"
        case 58, 61: return mac ? "⌥" : "Alt"
        case 59, 62: return mac ? "⌃" : "Ctrl"
        case 56, 60: return "shift"
        case 57: return "caps"
        case 63: return "fn"
        case 48: return "tab"
        case 36: return "return"
        case 51: return "delete"
        case 53: return "esc"
        case 49: return ""
        case 76: return "enter"
        case 117: return "del"
        case 115: return "home"; case 119: return "end"; case 116: return "pg up"; case 121: return "pg dn"; case 114: return "help"
        case 71: return "clear"
        default:
            let text = source.label(virtualKey: virtualKey)
            return text.count == 1 ? text.uppercased() : text
        }
    }
    static func spoken(_ label: String, _ virtualKey: UInt16) -> String {
        switch virtualKey {
        case 49: return "Space"; case 55: return "Command"; case 54: return "Right Command"; case 58: return "Option"; case 61: return "Right Option"
        case 59: return "Control"; case 62: return "Right Control"; case 56: return "Shift"; case 60: return "Right Shift"
        default: return label
        }
    }
}

private struct Legend: View {
    /// A column beside the drawing, or a grid under it.
    var column = true
    @Environment(\.palette) private var palette
    var body: some View {
        Group {
            if column {
                VStack(alignment: .leading, spacing: 12) {
                    GroupLabel("Key colours", header: false)
                    items
                }
                .frame(maxWidth: .infinity, alignment: .leading).padding(.top, 2)
            } else {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: 14, alignment: .leading)], alignment: .leading, spacing: 8) { items }
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Key colours: Movement, Communication, Combat, Game and UI, not bound, and keys the game can’t use")
    }
    private var items: some View {
        ForEach(StudioKeyRole.allCases, id: \.self) { role in
            // Labels wrap inside a narrow grid cell rather than running past the card edge.
            HStack(alignment: .top, spacing: 8) {
                KeySwatch(role: role)
                Text(role.rawValue).font(.system(size: 13)).foregroundStyle(palette.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

private struct PointerPanel: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette
    @State private var pointerOpen = false

    var body: some View {
        let mouse: [(String, String?, Int)] = [("Mouse 1", "Left button", 200), ("Mouse 2", "Right button", 201), ("Mouse 3", "Wheel click", 202),
                                               ("Mouse 4", "Side button", 203), ("Mouse 5", "Side button", 204), ("Wheel up", nil, 206), ("Wheel down", nil, 205)]
        let trackpad: [(String, String?, Int)] = [("Click", "Mouse 1", 200), ("Two-finger click", "Mouse 2", 201),
                                                  ("Scroll up", "Wheel up", 206), ("Scroll down", "Wheel down", 205)]
        let chipTitle = binds.pointer == .trackpad ? "Trackpad" : "Mouse" + (studio.metadata?.dpi.map { " · \(ConfigValues.format($0)) DPI" } ?? "")
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 12) {
                    Chip(chipTitle, action: { pointerOpen.toggle() }) {
                        Image(systemName: binds.pointer == .trackpad ? "rectangle.and.hand.point.up.left" : "computermouse").accessibilityHidden(true)
                        Text(chipTitle)
                        Image(systemName: "chevron.down").font(.system(size: 9, weight: .semibold)).accessibilityHidden(true)
                    }
                    .popover(isPresented: $pointerOpen, arrowEdge: .bottom) { PointerPopover(studio: studio, binds: binds, devices: devices).studioPopover() }
                    Spacer(minLength: 8)
                    ScopeSwitch("Device shown", selection: $binds.pointer, options: [(InputDeviceKind.mouse, "Mouse"), (InputDeviceKind.trackpad, "Trackpad")])
                }
                VStack(spacing: 4) {
                    ForEach(binds.pointer == .trackpad ? trackpad : mouse, id: \.2) { item in
                        let key = EngineKey(number: item.2)!
                        let entry = binds.bindings?.entry(for: key)
                        let selected = binds.selectedKey == key && binds.selectedVirtualKey == nil
                        let shape = RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous)
                        Button { binds.selectedKey = key; binds.selectedVirtualKey = nil } label: {
                            HStack(spacing: 12) {
                                KeySwatch(role: binds.role(of: entry))
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.0).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                                    let detail = binds.pointer == .mouse ? binds.pointerButtonDetail(key) ?? item.1 : item.1
                                    if let detail { Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary) }
                                }
                                Spacer(minLength: 8)
                                Text(binds.describe(entry)).font(.system(size: 13)).foregroundStyle(entry == nil ? palette.tertiary : palette.secondary).lineLimit(1)
                            }
                            .padding(.vertical, 6).padding(.leading, 12).padding(.trailing, 16)
                            .background(selected ? palette.selection : .clear, in: shape)
                            .overlay(shape.strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
                            .contentShape(shape)
                        }
                        .buttonStyle(RowButtonStyle(radius: RowMetrics.radius))
                        .accessibilityLabel("\(item.0)\((binds.pointer == .mouse ? binds.pointerButtonDetail(key) : nil).map { ", \($0)" } ?? ""), \(binds.describe(entry))")
                    }
                }
                if binds.pointer == .mouse {
                    ForEach(binds.pointerExtras, id: \.self) { button in
                        HStack(alignment: .firstTextBaseline, spacing: 12) {
                            KeySwatch(role: .unavailable)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Button \(button + 1)").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                                Text(PointerButtonLearning.explanation(button)).font(StudioTypography.help).foregroundStyle(palette.tertiary)
                                    .fixedSize(horizontal: false, vertical: true)
                            }
                        }
                        .padding(.vertical, 4).padding(.horizontal, 12)
                        .accessibilityElement(children: .combine)
                    }
                    if let count = binds.mouse?.buttonCount {
                        Text(count == 1 ? "This mouse reports 1 button." : "This mouse reports \(count) buttons.")
                            .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.horizontal, 4)
                    }
                    Banner(message: "Buttons 6 and up can’t be used by the original game. Map them to a key in your mouse’s software.")
                } else {
                    Text("Trackpad clicks and scrolling reach the game as these mouse buttons and wheel moves, as set in System Settings › Trackpad.")
                        .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true).padding(.horizontal, 4)
                }
                Spacer(minLength: 0)
                HStack(spacing: 12) {
                    Text((studio.metadata?.dpi.map { "\(ConfigValues.format($0)) DPI · you entered this" } ?? "DPI not entered") + " · Polling rate not measured")
                        .font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 8)
                    if binds.pointer == .mouse {
                        Button("Learn Button Positions…") { binds.beginLearning() }
                            .glassAction().controlSize(.large).fixedSize()
                            .disabled(studio.gameRunning)
                            .help("Click each of your mouse’s buttons in turn, so the side buttons are placed and extra buttons explained.")
                    }
                }
            }
            .frame(maxHeight: .infinity, alignment: .topLeading)
        }
    }
}

private struct KeyInspector: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let source = devices.inputSource
        let mapping = binds.selectedVirtualKey.map { source.mapping(virtualKey: $0) } ?? binds.selectedKey.map { InputKeyMapping.available($0) }
        let key = mapping?.engineKey
        let entry = key.flatMap { binds.bindings?.entry(for: $0) }
        let available: Bool = { if case .available = mapping { return true }; return false }()
        let name = binds.selectedVirtualKey.map { KeyButton.spoken(KeyButton.label($0, source: source, modifiers: .mac), $0) } ?? key.map(binds.keyName) ?? "No key"
        let role: StudioKeyRole = available ? binds.role(of: entry) : .unavailable
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 16) {
                    StudioKeyCap(key: name.count <= 3 ? name : String(name.prefix(1)), role: role, width: 64, height: 64, large: true).accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 6) {
                        Text(title(name: name, entry: entry, available: available, mapping: mapping)).font(StudioTypography.groupTitle).foregroundStyle(palette.heading)
                            .fixedSize(horizontal: false, vertical: true)
                            .accessibilityAddTraits(.isHeader)
                        HStack(spacing: 8) {
                            KeySwatch(role: role)
                            Text(available ? (entry == nil ? "Not bound" : role.rawValue) : "Unavailable").font(.system(size: 13)).foregroundStyle(palette.secondary)
                        }
                    }
                }
                Text(description(entry: entry, mapping: mapping, name: name)).font(.system(size: 13)).foregroundStyle(palette.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                if let entry, binds.action(named: entry.actionName) == nil, let document = studio.current?.document {
                    let inspection = ConfigChains.inspect(key: entry.key.spelling, document: document)
                    let press = inspection.press.commands.compactMap(binds.words), release = inspection.release.commands.compactMap(binds.words)
                    if !press.isEmpty {
                        Text("Press: " + press.map(\.text).joined(separator: ", then ") + (release.isEmpty ? "" : ". Letting go: " + release.map(\.text).joined(separator: ", then ")))
                            .font(.system(size: 13)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                    }
                }
                if let entry, let key {
                    HStack(spacing: 12) {
                        GroupLabel("In the config file", header: false).frame(width: 132, alignment: .leading)
                        Text("bind \(key.spelling) \"\(entry.text)\"").font(.typewriter(12)).foregroundStyle(palette.address).textSelection(.enabled).lineLimit(2)
                            .padding(.vertical, 8).padding(.horizontal, 12).frame(maxWidth: .infinity, alignment: .leading)
                            .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                    }
                }
                Spacer(minLength: 0)
                if binds.target == .find {
                    Text("Press a key or click a mouse button… Escape cancels.").font(StudioTypography.help).foregroundStyle(palette.accent)
                }
                if available, let key { actions(key: key, entry: entry) }
            }
            .frame(maxHeight: .infinity, alignment: .topLeading)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Selected key")
    }
    private func actions(key: EngineKey, entry: ConfigBindingEntry?) -> some View {
        HStack(spacing: 8) {
            Menu {
                ForEach(ConfigBindingGroup.allCases, id: \.self) { group in
                    Section(group.title) {
                        ForEach((binds.bindings?.actions ?? []).filter { $0.group == group }) { action in
                            Button(action.title) { binds.setKey(key, action: action.name, undo: undo) }
                        }
                    }
                }
            } label: { Text(entry == nil ? "Assign Action" : "Change Action") }
                .menuStyle(.borderlessButton).fixedSize()
                .padding(.horizontal, 14).frame(height: ControlMetrics.action)
                .glassSurface(Capsule(), interactive: true)
                .disabled(studio.isReadOnly)
            if entry != nil {
                Button("Clear") { binds.setKey(key, action: nil, undo: undo) }.glassAction().disabled(studio.isReadOnly)
            }
            if let chain = binds.ownedChain(on: key) {
                Button { binds.layout = .list; binds.selectChain(chain.id) } label: { Label("Edit Steps…", systemImage: "pencil") }.glassAction()
            }
        }
        .actionControlSize()
    }
    private func title(name: String, entry: ConfigBindingEntry?, available: Bool, mapping: InputKeyMapping?) -> String {
        if !available { return mapping?.isReserved == true ? "\(name) opens the console" : "\(name) can’t be bound" }
        guard let entry else { return "\(name) is free" }
        return binds.describe(entry)
    }
    private func description(entry: ConfigBindingEntry?, mapping: InputKeyMapping?, name: String) -> String {
        if let explanation = mapping?.explanation { return explanation }
        guard let entry else { return "Not bound. Choose an action for it, or leave it free." }
        if let action = binds.action(named: entry.actionName) {
            let summary = binds.studio.catalog?.command(named: action.name)?.summary ?? action.summary
            return "Pressing \(name): \(summary.prefix(1).lowercased() + summary.dropFirst())"
        }
        if binds.ownedChain(on: entry.key) != nil { return "An advanced bind made here. Edit its steps in List." }
        return "A bind written outside Config Studio. It stays exactly as it is unless you change or clear it."
    }
}
