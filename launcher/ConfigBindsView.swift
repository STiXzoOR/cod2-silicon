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

    var body: some View {
        GeometryReader { proxy in
            if proxy.size.width >= 840 {
                HStack(alignment: .top, spacing: 16) {
                    actions
                    AdvancedBindsPanel(studio: studio, binds: binds).frame(width: 384).frame(maxHeight: .infinity, alignment: .top)
                }
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 16) {
                        actionRows
                        AdvancedBindsPanel(studio: studio, binds: binds)
                    }
                    .padding(.trailing, 4)
                }
            }
        }
    }

    private var search: some View {
        HStack(spacing: 12) {
            HStack(spacing: 6) {
                Image(systemName: "magnifyingglass").foregroundStyle(palette.secondary).accessibilityHidden(true)
                TextField("Find an action or key", text: $binds.query).textFieldStyle(.plain)
                if !binds.query.isEmpty {
                    Button { binds.query = "" } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(palette.tertiary).accessibilityLabel("Clear search")
                }
            }
            .padding(.horizontal, 12).frame(height: 32)
            .background(palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.6), in: Capsule())
            .overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1))
            HStack(spacing: 10) {
                Text("PRIMARY").frame(width: 72)
                Text("SECOND").frame(width: 72)
            }
            .font(.system(size: 11, weight: .semibold)).tracking(0.6).foregroundStyle(palette.tertiary).accessibilityHidden(true)
            .padding(.trailing, 14)
        }
    }

    private var actions: some View {
        VStack(alignment: .leading, spacing: 10) {
            search
            ScrollView { actionGroups.padding(.trailing, 2).padding(.bottom, 8) }.scrollIndicators(.automatic)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
    private var actionRows: some View {
        VStack(alignment: .leading, spacing: 10) { search; actionGroups }
    }

    @ViewBuilder private var actionGroups: some View {
        VStack(alignment: .leading, spacing: 14) {
            ForEach(ConfigBindingGroup.allCases, id: \.self) { group in
                let rows = binds.rows(in: group)
                if !rows.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(group.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6)
                            .accessibilityAddTraits(.isHeader)
                        SettingsPanel {
                            ForEach(Array(rows.enumerated()), id: \.element.action.name) { index, row in
                                BindRowView(studio: studio, binds: binds, row: row).hairline(index > 0)
                            }
                        }
                    }
                }
            }
            let others = binds.otherEntries
            if !others.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Other binds").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6)
                        .accessibilityAddTraits(.isHeader)
                    SettingsPanel {
                        ForEach(Array(others.enumerated()), id: \.element.key) { index, entry in
                            OtherBindRow(studio: studio, binds: binds, entry: entry).hairline(index > 0)
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

/// An opaque panel whose rows draw their own hairlines.
private struct SettingsPanel<Content: View>: View {
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 0) { content }
            .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
    }
}

private extension View {
    func hairline(_ visible: Bool) -> some View { modifier(HairlineAbove(visible: visible)) }
}
private struct HairlineAbove: ViewModifier {
    var visible: Bool
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        content.overlay(alignment: .top) { if visible { Rectangle().fill(palette.separator).frame(height: 1).padding(.leading, 18) } }
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
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(row.action.title).font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text)
                    if !row.additional.isEmpty {
                        Text("Also " + row.additional.map(binds.keyName).joined(separator: ", ")).font(.system(size: 11)).foregroundStyle(palette.tertiary)
                    }
                }
                Spacer(minLength: 8)
                KeyChip(studio: studio, binds: binds, row: row, slot: .primary)
                KeyChip(studio: studio, binds: binds, row: row, slot: .secondary)
            }
            .frame(minHeight: 32)
            if listeningHere, let note = binds.captureNote {
                StudioStatusLabel(title: note, tone: .caution)
            }
            if let review {
                HStack(spacing: 10) {
                    Image(systemName: "exclamationmark.triangle").foregroundStyle(palette.caution).accessibilityHidden(true)
                    Text(binds.reviewText(review)).font(.system(size: 12)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 4)
                    Button("Keep \(review.key.map(binds.keyName) ?? "it") for \(row.action.title) Only") { binds.confirmReview(undo: undo) }
                        .glassAction().controlSize(.small).fixedSize()
                    Button("Cancel") { binds.cancelReview() }.glassAction().controlSize(.small).keyboardShortcut(.cancelAction)
                }
                .padding(.vertical, 8).padding(.horizontal, 10)
                .background(palette.selection, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(palette.selectionStroke, lineWidth: 1))
                .accessibilityElement(children: .contain)
            }
        }
        .padding(.vertical, 7).padding(.leading, 18).padding(.trailing, 14)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(row.action.title)
    }
}

/// A key button: press it, then press the key to use. Escape cancels.
private struct KeyChip: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    var row: ConfigBindingRow
    var slot: ConfigBindingSlot
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let key = slot == .primary ? row.primary : row.secondary
        let listening = binds.target == .slot(action: row.action.name, slot: slot)
        let label = listening ? "Press a key…" : key.map(binds.keyName) ?? "Add key…"
        let slotName = slot == .primary ? "Primary" : "Second"
        Button {
            if listening { binds.cancelCapture() } else { binds.beginCapture(.slot(action: row.action.name, slot: slot)) }
        } label: {
            Text(label).font(.system(size: 12, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.85)
                .foregroundStyle(listening ? palette.accent : key == nil ? palette.secondary : palette.text)
                .frame(width: 72, height: 28)
                .background(key == nil && !listening ? Color.clear : listening ? palette.selection : (palette.dark ? Palette.hex(0x22251e) : palette.crate),
                            in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous)
                    .strokeBorder(listening ? palette.accent : palette.wellStroke, style: StrokeStyle(lineWidth: 1, dash: key == nil && !listening ? [3, 3] : [])))
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

private struct OtherBindRow: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    var entry: ConfigBindingEntry
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 10) {
            Text(binds.keyName(entry.key)).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text)
                .frame(width: 72, height: 28)
                .background(palette.dark ? Palette.hex(0x22251e) : palette.crate, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            VStack(alignment: .leading, spacing: 2) {
                Text(binds.describe(entry)).font(.system(size: 13)).foregroundStyle(palette.text)
                Text(entry.text).font(.typewriter(11)).foregroundStyle(palette.address).lineLimit(1).truncationMode(.middle).textSelection(.enabled)
            }
            Spacer(minLength: 8)
            if let chain = binds.ownedChain(on: entry.key) {
                Button("Edit…") { binds.selectChain(chain.id) }.glassAction().controlSize(.small)
            } else {
                Button("Clear") { binds.setKey(entry.key, action: nil, undo: undo) }.glassAction().controlSize(.small).disabled(studio.isReadOnly)
            }
        }
        .padding(.vertical, 7).padding(.leading, 18).padding(.trailing, 14)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(binds.keyName(entry.key)), \(binds.describe(entry))")
    }
}

// MARK: - Advanced binds

private struct AdvancedBindsPanel: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    @State private var confirming = false

    var body: some View {
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 12) {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 3) {
                        Text("Advanced binds").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                        Text("One key that does more than one thing.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                    }
                    Spacer()
                    Button { binds.newChain() } label: { Label("New", systemImage: "plus") }.glassAction().controlSize(.small)
                        .disabled(studio.isReadOnly || binds.chainsUnreadable != nil)
                }
                if let problem = binds.chainsUnreadable {
                    StudioBanner(title: "Advanced binds changed outside Config Studio", detail: problem, tone: .caution)
                }
                VStack(spacing: 6) {
                    ForEach(binds.chains) { chain in
                        ChainCard(binds: binds, draft: ConfigChainDraft(chain: chain), selected: binds.chainID == chain.id) { binds.selectChain(chain.id) }
                    }
                    if let draft = binds.draft, draft.isNew {
                        ChainCard(binds: binds, draft: draft, selected: true) {}
                    }
                    if binds.chains.isEmpty && binds.draft == nil {
                        Text("None yet. A toggle, a cycle of messages, or several actions on one press start here.")
                            .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                }
                if binds.draft != nil {
                    Rectangle().fill(palette.separator).frame(height: 1)
                    ChainEditor(studio: studio, binds: binds, confirming: $confirming)
                }
            }
        }
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
        Button(action: action) {
            HStack(spacing: 12) {
                Text(draft.key.isEmpty ? "—" : EngineKey(spelling: draft.key).map(binds.keyName) ?? draft.key)
                    .font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text)
                    .frame(minWidth: 34, minHeight: 28).padding(.horizontal, 6)
                    .background(palette.dark ? Palette.hex(0x22251e) : palette.crate, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                VStack(alignment: .leading, spacing: 3) {
                    Text(draft.isNew ? "New bind" : summary).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
                    Text(detail).font(.system(size: 11)).foregroundStyle(palette.secondary).lineLimit(1)
                }
                Spacer(minLength: 4)
                Text(draft.kind.short).font(.system(size: 10.5, weight: .semibold)).foregroundStyle(palette.chipText)
                    .padding(.horizontal, 7).frame(height: 18)
                    .overlay(Capsule().strokeBorder(palette.dark ? Color.white.opacity(0.14) : palette.wellStroke, lineWidth: 1))
            }
            .padding(.vertical, 10).padding(.horizontal, 12)
            .background(selected ? palette.selection : palette.crate, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(selected ? palette.selectionStroke : palette.wellStroke, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("\(summary), \(draft.kind.title), key \(draft.key.isEmpty ? "not chosen" : draft.key)")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
    private var summary: String {
        switch draft.kind {
        case .several: return draft.commands.map { binds.words([Data($0.command.utf8)])?.text ?? $0.command }.joined(separator: ", then ")
        case .toggle: return binds.studio.spec(named: draft.setting)?.title ?? draft.setting
        case .cycle: return "Cycle of \(draft.steps.count)"
        }
    }
    private var detail: String {
        switch draft.kind {
        case .several: return "On one press"
        case .toggle: return "Each press: " + draft.values.joined(separator: " → ")
        case .cycle: return "Each press: " + draft.steps.map { $0.text.isEmpty ? "…" : $0.text }.joined(separator: " → ")
        }
    }
}

private struct ChainEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @Binding var confirming: Bool
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        if let draft = binds.draft {
            VStack(alignment: .leading, spacing: 12) {
                HStack(spacing: 10) {
                    Text("Key").font(StudioTypography.help).foregroundStyle(palette.secondary)
                    let listening = binds.target == .chainKey
                    Button { listening ? binds.cancelCapture() : binds.beginCapture(.chainKey) } label: {
                        Text(listening ? "Press a key…" : draft.key.isEmpty ? "Choose…" : EngineKey(spelling: draft.key).map(binds.keyName) ?? draft.key)
                            .font(.system(size: 12, weight: .semibold)).foregroundStyle(listening ? palette.accent : palette.text)
                            .frame(minWidth: 72, minHeight: 28).padding(.horizontal, 6)
                            .background(listening ? palette.selection : palette.crate, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
                            .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous).strokeBorder(listening ? palette.accent : palette.wellStroke, lineWidth: 1))
                            .contentShape(RoundedRectangle(cornerRadius: 7, style: .continuous))
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Key for this bind: \(draft.key.isEmpty ? "not chosen" : draft.key). Press to change")
                    if listening, let note = binds.captureNote { StudioStatusLabel(title: note, tone: .caution) }
                    Spacer()
                }
                VStack(alignment: .leading, spacing: 6) {
                    Text("Each press").font(StudioTypography.help).foregroundStyle(palette.secondary)
                    Picker("What a press does", selection: Binding(get: { draft.kind }, set: { kind in binds.updateDraft { $0.kind = kind } })) {
                        ForEach(ConfigChainDraft.Kind.allCases) { Text($0.short).tag($0) }
                    }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                    Text(draft.kind.title).font(.system(size: 11)).foregroundStyle(palette.tertiary)
                }
                switch draft.kind {
                case .several: StepList(binds: binds, steps: draft.commands, numbered: true, actionsOnly: true,
                                        caption: "These run together, top to bottom, on one press.") { steps in binds.updateDraft { $0.commands = steps } }
                case .cycle: StepList(binds: binds, steps: draft.steps, numbered: true, actionsOnly: false,
                                      caption: "Steps, in order. Each press moves to the next one.") { steps in binds.updateDraft { $0.steps = steps } }
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
                DisclosureGroup(isExpanded: $binds.showLines) {
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(Array(binds.chainLines.enumerated()), id: \.offset) { _, line in
                            Text(line).font(.typewriter(12)).foregroundStyle(palette.address).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    .padding(10).frame(maxWidth: .infinity, alignment: .leading)
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                } label: {
                    Text("Show the lines").font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.link)
                }
                HStack(spacing: 8) {
                    Button(draft.isNew ? "Discard" : "Delete Bind", role: .destructive) { binds.deleteChain(undo: undo) }
                        .glassAction().controlSize(.small).disabled(studio.isReadOnly && !draft.isNew)
                    Spacer()
                    Button("Save Bind") {
                        if binds.chainNeedsConfirmation { confirming = true } else { binds.saveChain(undo: undo) }
                    }
                    .prominentAction().controlSize(.small)
                    .disabled(binds.chainEdit == nil || binds.chainBlocked || studio.isReadOnly || !(binds.chainEdit?.hasChanges ?? false))
                }
            }
        }
    }

    @ViewBuilder private func toggleRow(_ draft: ConfigChainDraft) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 10) {
                Text("Turns").font(StudioTypography.help).foregroundStyle(palette.secondary)
                Picker("Setting", selection: Binding(get: { draft.setting }, set: { name in
                    binds.updateDraft { $0.setting = name; $0.values = [] }
                })) {
                    ForEach(binds.toggleSettings) { Text($0.title).tag($0.name) }
                }
                .labelsHidden()
            }
            if let spec = studio.spec(named: draft.setting) {
                Text("Each press: " + draft.values.map { ConfigValues.display(spec, $0) }.joined(separator: " → ") + " → back to the start")
                    .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

/// Ordered steps with a kind, text, reordering and removal.
private struct StepList: View {
    @ObservedObject var binds: ConfigBindsModel
    var steps: [ConfigChainDraft.Step]
    var numbered: Bool
    var actionsOnly: Bool
    var caption: String
    var update: ([ConfigChainDraft.Step]) -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(caption).font(StudioTypography.help).foregroundStyle(palette.secondary)
            ForEach(Array(steps.enumerated()), id: \.element.id) { index, step in
                VStack(alignment: .leading, spacing: 6) {
                    HStack(spacing: 6) {
                        Text("\(index + 1)").font(.stencil(12)).foregroundStyle(palette.prominentText)
                            .frame(width: 22, height: 22).background(Circle().fill(palette.accent)).accessibilityHidden(true)
                        if actionsOnly { actionPicker(step, index) } else { kindPicker(step, index) }
                        IconStep(symbol: "chevron.up", label: "Move step \(index + 1) up") { move(index, -1) }.disabled(index == 0)
                        IconStep(symbol: "chevron.down", label: "Move step \(index + 1) down") { move(index, 1) }.disabled(index == steps.count - 1)
                        IconStep(symbol: "xmark", label: "Remove step \(index + 1)") { remove(index) }.disabled(steps.count == 1)
                    }
                    if step.kind == .action && !actionsOnly {
                        actionPicker(step, index).padding(.leading, 28)
                    } else if step.kind != .action {
                        StepField(text: step.text, label: "Step \(index + 1) \(step.kind.title.lowercased())") { text in set(index) { $0.text = text } }
                            .padding(.leading, 28)
                    }
                }
                .padding(.vertical, 6).padding(.leading, 8).padding(.trailing, 6)
                .background(palette.dark ? Color.white.opacity(0.035) : Color.white.opacity(0.5), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            }
            Button { add() } label: { Label(actionsOnly ? "Add Action" : "Add Step", systemImage: "plus") }.glassAction().controlSize(.small)
        }
    }
    private func kindPicker(_ step: ConfigChainDraft.Step, _ index: Int) -> some View {
        let actions = binds.bindings?.actions ?? []
        return Picker("Step \(index + 1) kind", selection: Binding(get: { step.kind }, set: { kind in
            set(index) { $0.kind = kind; $0.text = kind == .action ? actions.first?.name ?? "" : "" }
        })) {
            ForEach(ConfigChainDraft.StepKind.allCases) { Text($0.title).tag($0) }
        }
        .labelsHidden().frame(maxWidth: .infinity)
    }
    /// Game actions by name; in "several", the last choice switches the step to a console command.
    private func actionPicker(_ step: ConfigChainDraft.Step, _ index: Int) -> some View {
        Picker("Step \(index + 1) action", selection: Binding(get: { step.kind == .action ? step.text : "" }, set: { value in
            set(index) { if value.isEmpty { $0.kind = .command; $0.text = "" } else { $0.kind = .action; $0.text = value } }
        })) {
            ForEach(binds.bindings?.actions ?? []) { Text($0.title).tag($0.name) }
            if actionsOnly { Divider(); Text("Console command…").tag("") }
        }
        .labelsHidden().frame(maxWidth: .infinity)
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
            Image(systemName: symbol).font(.system(size: 10, weight: .semibold)).foregroundStyle(palette.chipIcon)
                .frame(width: 24, height: 24).contentShape(Circle())
        }
        .buttonStyle(.plain).opacity(enabled ? 1 : 0.3)
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
            .padding(.vertical, 10).padding(.horizontal, 12).frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
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
        var text = Text(lead + " ").font(.system(size: 12, weight: .semibold)).foregroundColor(palette.secondary)
        for (index, part) in parts.enumerated() {
            if index > 0 { text = text + Text(" \(joiner) ").foregroundColor(palette.accent) }
            text = text + (part.literal ? Text(part.text).font(.typewriter(12)) : Text(part.text).font(.system(size: 13, weight: .semibold))).foregroundColor(palette.text)
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
                VStack(alignment: .leading, spacing: 14) {
                    DeviceChips(studio: studio, binds: binds, devices: devices)
                    if wide {
                        HStack(alignment: .top, spacing: 16) {
                            KeyboardPanel(binds: binds, devices: devices, available: proxy.size.width - 16 - 320)
                            PointerPanel(studio: studio, binds: binds, devices: devices).frame(maxWidth: .infinity)
                        }
                        KeyInspector(studio: studio, binds: binds, devices: devices)
                    } else {
                        // Narrow windows keep the inspector next to the keyboard it describes.
                        KeyboardPanel(binds: binds, devices: devices, available: proxy.size.width - 4)
                        KeyInspector(studio: studio, binds: binds, devices: devices, compact: proxy.size.width < 720)
                        PointerPanel(studio: studio, binds: binds, devices: devices)
                    }
                }
                .padding(.trailing, 4).padding(.bottom, 8)
            }
        }
    }
}

/// A device chip's label; the icon box keeps every chip the same height.
private struct ChipLabel: View {
    var title: String
    var symbol: String
    var body: some View {
        Label { Text(title).lineLimit(1) } icon: { Image(systemName: symbol).frame(width: 16, height: 16) }
    }
}

private struct DeviceChips: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette
    @State private var keyboardOpen = false
    @State private var pointerOpen = false

    var body: some View {
        let keyboard = currentKeyboard(binds: binds, devices: devices)
        let shape = keyboard.map { devices.presentation(for: $0).shape.value } ?? devices.inputSource.shape
        HStack(spacing: 10) {
            Button { keyboardOpen.toggle() } label: {
                ChipLabel(title: "\(keyboard?.name ?? "Keyboard") · \(shape.rawValue.uppercased()) · \(devices.inputSource.name)", symbol: "keyboard")
            }
            .glassAction().controlSize(.small).fixedSize()
            .popover(isPresented: $keyboardOpen, arrowEdge: .bottom) { KeyboardPopover(binds: binds, devices: devices) }
            Button { pointerOpen.toggle() } label: {
                ChipLabel(title: binds.pointer == .trackpad ? "Trackpad" : "Mouse" + (studio.metadata?.dpi.map { " · \(ConfigValues.format($0)) DPI" } ?? ""),
                          symbol: binds.pointer == .trackpad ? "rectangle.and.hand.point.up.left" : "computermouse")
            }
            .glassAction().controlSize(.small).fixedSize()
            .popover(isPresented: $pointerOpen, arrowEdge: .bottom) { PointerPopover(studio: studio, binds: binds, devices: devices) }
            // The hint steps aside rather than truncating in narrow windows.
            ViewThatFits(in: .horizontal) {
                Text("Click any key or button to see or change what it does.").font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize()
                Color.clear.frame(width: 0, height: 0)
            }
            Spacer(minLength: 8)
            let finding = binds.target == .find
            Button { finding ? binds.cancelCapture() : binds.beginCapture(.find) } label: {
                ChipLabel(title: finding ? "Press a Key…" : "Find by Pressing…", symbol: "hand.point.up.left")
            }
            .glassAction().controlSize(.small).fixedSize()
            .help("Press a key or click a mouse button to select it. Escape cancels.")
        }
        .onAppear {
            if binds.keyboardID == nil { binds.keyboardID = devices.devices.first { $0.kind == .keyboard }?.id }
            if !devices.devices.contains(where: { $0.kind == .mouse }) && devices.devices.contains(where: { $0.kind == .trackpad }) { binds.pointer = .trackpad }
        }
    }
}

@MainActor private func currentKeyboard(binds: ConfigBindsModel, devices: InputDevicesModel) -> InputDevice? {
    devices.devices.first { $0.id == binds.keyboardID } ?? devices.devices.first { $0.kind == .keyboard }
}

private struct KeyboardPopover: View {
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette

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
                        .labelsHidden()
                    } else {
                        Text(device.name).font(.system(size: 14, weight: .bold)).foregroundStyle(palette.heading)
                    }
                    Text("Connected · detected automatically").font(StudioTypography.help).foregroundStyle(palette.secondary)
                }
                row("Key shape", presentation.shape.explanation) {
                    Picker("Key shape", selection: Binding(get: { presentation.shape.value }, set: { value in update(device) { $0.shape = value } })) {
                        ForEach(KeyboardShape.allCases, id: \.self) { Text($0.rawValue.uppercased()).tag($0) }
                    }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                }
                row("Size", presentation.size.explanation) {
                    Picker("Keyboard size", selection: Binding(get: { presentation.size.value }, set: { value in update(device) { $0.size = value } })) {
                        ForEach([KeyboardSize.full, .tkl, .percent75, .percent65, .percent60], id: \.self) { Text(sizeTitle($0)).tag($0) }
                        if presentation.size.value == .unknown { Text("Unknown").tag(KeyboardSize.unknown) }
                    }
                    .pickerStyle(.menu).labelsHidden().fixedSize()
                }
                row("Modifier labels", presentation.modifiers.explanation) {
                    Picker("Modifier labels", selection: Binding(get: { presentation.modifiers.value }, set: { value in update(device) { $0.modifiers = value } })) {
                        Text("Mac").tag(KeyboardModifierStyle.mac); Text("PC").tag(KeyboardModifierStyle.pc)
                    }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                }
            } else {
                Text("No keyboard reported").font(.system(size: 14, weight: .bold)).foregroundStyle(palette.heading)
            }
            HStack {
                Text("Layout").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                Text("\(devices.inputSource.name) · from macOS").font(.system(size: 13)).foregroundStyle(palette.secondary)
            }
            Text("Shape and layout come from macOS. Change the size if the drawing looks wrong. Labels follow your input source; saved binds stay on the same game keys.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(18).frame(width: 380)
    }
    private func sizeTitle(_ size: KeyboardSize) -> String {
        switch size { case .full: "Full"; case .tkl: "Tenkeyless"; case .percent75: "75%"; case .percent65: "65%"; case .percent60: "60%"; case .unknown: "Unknown" }
    }
    private func update(_ device: InputDevice, _ change: (inout InputDeviceOverride) -> Void) {
        var value = devices.overrides[device.id] ?? .init()
        change(&value)
        devices.setOverride(value, for: device.id)
    }
    private func row<Control: View>(_ title: String, _ detail: String, @ViewBuilder control: () -> Control) -> some View {
        HStack(alignment: .center, spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 13)).foregroundStyle(palette.text)
                Text(detail).font(.system(size: 11)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
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
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Pointing devices").font(.system(size: 14, weight: .bold)).foregroundStyle(palette.heading)
            HStack {
                Text("Show").font(.system(size: 13)).foregroundStyle(palette.text)
                Spacer()
                Picker("Device shown", selection: $binds.pointer) { Text("Mouse").tag(InputDeviceKind.mouse); Text("Trackpad").tag(InputDeviceKind.trackpad) }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
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
        }
        .padding(18).frame(width: 360)
    }
}

private struct KeyboardPanel: View {
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    /// The width this panel may take; keys grow to fill it, within reason.
    var available: CGFloat
    @Environment(\.palette) private var palette
    @FocusState private var focused: Bool

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
        // Keys keep a 4-point gap and never shrink below the hit target.
        let unit = min(48, max(StudioMetrics.minimumHitTarget + 4, ((available - inset * 2) / columns).rounded(.down)))
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: inset, leading: inset, bottom: 16, trailing: inset)) {
            VStack(alignment: .leading, spacing: 14) {
                ScrollView(.horizontal, showsIndicators: false) {
                    ZStack(alignment: .topLeading) {
                        ForEach(keys) { key in
                            KeyButton(binds: binds, devices: devices, key: key, unit: unit, modifiers: modifiers)
                        }
                    }
                    .frame(width: columns * unit, height: rows * unit, alignment: .topLeading)
                }
                .frame(maxWidth: columns * unit, alignment: .leading)
                .focusable()
                .focused($focused)
                .onMoveCommand { direction in move(direction, keys: keys) }
                .accessibilityElement(children: .contain)
                .accessibilityLabel("\(device?.name ?? "Keyboard"), \(shape.rawValue.uppercased()) layout. Each key says what it does.")
                Legend()
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }
    /// Arrow keys follow the drawing: the nearest key in that direction.
    private func move(_ direction: MoveCommandDirection, keys: [ConfigKeyboardKey]) {
        let source = devices.inputSource
        guard let selected = binds.selectedKey,
              let from = keys.first(where: { source.mapping(virtualKey: $0.virtualKey).engineKey == selected }) ?? keys.first else { return }
        let cx = from.x + from.width / 2, cy = from.y + from.height / 2
        let candidates = keys.filter { $0.virtualKey != from.virtualKey }.compactMap { key -> (ConfigKeyboardKey, Double)? in
            let dx = key.x + key.width / 2 - cx, dy = key.y + key.height / 2 - cy
            switch direction {
            case .left where dx < -0.3: return (key, abs(dx) + abs(dy) * 3)
            case .right where dx > 0.3: return (key, abs(dx) + abs(dy) * 3)
            case .up where dy < -0.3: return (key, abs(dy) + abs(dx) * 3)
            case .down where dy > 0.3: return (key, abs(dy) + abs(dx) * 3)
            default: return nil
            }
        }
        guard let next = candidates.min(by: { $0.1 < $1.1 })?.0 else { return }
        binds.selectedKey = source.mapping(virtualKey: next.virtualKey).engineKey ?? binds.selectedKey
        binds.selectedVirtualKey = next.virtualKey
    }
}

private struct KeyButton: View {
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    var key: ConfigKeyboardKey
    var unit: CGFloat
    var modifiers: KeyboardModifierStyle

    var body: some View {
        let mapping = devices.inputSource.mapping(virtualKey: key.virtualKey)
        let engine = mapping.engineKey
        let entry = engine.flatMap { binds.bindings?.entry(for: $0) }
        let role: StudioKeyRole = { if case .available = mapping { return binds.role(of: entry) }; return .unavailable }()
        let label = Self.label(key.virtualKey, source: devices.inputSource, modifiers: modifiers)
        let caption = entry.map { binds.describe($0) }
        let width = key.width * unit - 4, height = key.height * unit - 4
        let selected = binds.selectedVirtualKey == key.virtualKey || (binds.selectedVirtualKey == nil && engine != nil && engine == binds.selectedKey)
        Button {
            binds.selectedVirtualKey = key.virtualKey
            binds.selectedKey = engine
        } label: {
            StudioKeyCap(key: label, action: width >= 76 || key.virtualKey == 49 ? caption : nil, role: role, selected: selected, width: width, height: height)
        }
        .buttonStyle(.plain)
        .offset(x: key.x * unit, y: key.y * unit)
        .accessibilityLabel("\(Self.spoken(label, key.virtualKey)), \(role == .unavailable ? mapping.explanation ?? StudioKeyRole.unavailable.rawValue : caption ?? "not bound")")
        .accessibilityAddTraits(selected ? .isSelected : [])
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
    @Environment(\.palette) private var palette
    var body: some View {
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 14) { items }
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: 14, alignment: .leading)], alignment: .leading, spacing: 8) { items }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Key colours: Movement, Communication, Combat, Game and UI, not bound, and keys the game can’t use")
    }
    private var items: some View {
        ForEach(StudioKeyRole.allCases, id: \.self) { role in
            HStack(spacing: 6) {
                RoundedRectangle(cornerRadius: 4).fill(role.fill).frame(width: 14, height: 14)
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(Color.white.opacity(0.18), lineWidth: 1))
                Text(role.rawValue).font(.system(size: 12)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary).fixedSize()
            }
        }
    }
}

private struct PointerPanel: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    @Environment(\.palette) private var palette

    var body: some View {
        let mouse: [(String, String?, Int)] = [("Mouse 1", "Left button", 200), ("Mouse 2", "Right button", 201), ("Mouse 3", "Wheel click", 202),
                                               ("Mouse 4", "Side button", 203), ("Mouse 5", "Side button", 204), ("Wheel up", nil, 206), ("Wheel down", nil, 205)]
        let trackpad: [(String, String?, Int)] = [("Click", "Mouse 1", 200), ("Two-finger click", "Mouse 2", 201),
                                                  ("Scroll up", "Wheel up", 206), ("Scroll down", "Wheel down", 205)]
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text(binds.pointer == .trackpad ? "Trackpad" : "Mouse").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading)
                        .accessibilityAddTraits(.isHeader)
                    Spacer()
                    Picker("Device shown", selection: $binds.pointer) { Text("Mouse").tag(InputDeviceKind.mouse); Text("Trackpad").tag(InputDeviceKind.trackpad) }
                        .pickerStyle(.segmented).labelsHidden().fixedSize().controlSize(.small)
                }
                VStack(spacing: 2) {
                    ForEach(binds.pointer == .trackpad ? trackpad : mouse, id: \.2) { item in
                        let key = EngineKey(number: item.2)!
                        let entry = binds.bindings?.entry(for: key)
                        Button { binds.selectedKey = key; binds.selectedVirtualKey = nil } label: {
                            HStack(spacing: 10) {
                                RoundedRectangle(cornerRadius: 4).fill(binds.role(of: entry).fill).frame(width: 12, height: 12).accessibilityHidden(true)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.0).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                                    if let detail = item.1 { Text(detail).font(.system(size: 11)).foregroundStyle(palette.secondary) }
                                }
                                Spacer(minLength: 8)
                                Text(binds.describe(entry)).font(.system(size: 12)).foregroundStyle(entry == nil ? palette.tertiary : palette.text).lineLimit(1)
                            }
                            .padding(.vertical, 6).padding(.horizontal, 10)
                            .background(binds.selectedKey == key && binds.selectedVirtualKey == nil ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(binds.selectedKey == key && binds.selectedVirtualKey == nil ? palette.selectionStroke : .clear, lineWidth: 1))
                            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                        }
                        .buttonStyle(RowButtonStyle(radius: 12))
                        .accessibilityLabel("\(item.0), \(binds.describe(entry))")
                    }
                }
                Text(binds.pointer == .trackpad
                     ? "Trackpad gestures arrive as these mouse buttons and wheel moves. Tap to click follows your System Settings."
                     : "Buttons 6 and up can’t be used by the original game. Map them to a key in your mouse’s software.")
                    .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                HStack(spacing: 6) {
                    Text(studio.metadata?.dpi.map { "\(ConfigValues.format($0)) DPI · you entered this" } ?? "DPI not entered")
                    Text("·")
                    Text("Polling rate not measured")
                }
                .font(StudioTypography.help).foregroundStyle(palette.tertiary)
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }
}

private struct KeyInspector: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var binds: ConfigBindsModel
    @ObservedObject var devices: InputDevicesModel
    /// Narrow windows put the actions under the description.
    var compact = false
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
            HStack(alignment: .top, spacing: 20) {
                StudioKeyCap(key: name.count <= 3 ? name : String(name.prefix(1)), role: role, width: 64, height: 64).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 6) {
                    HStack(spacing: 10) {
                        Text(title(name: name, entry: entry, available: available, mapping: mapping)).font(.system(size: 20, weight: .bold)).foregroundStyle(palette.heading)
                            .accessibilityAddTraits(.isHeader)
                        Text(available ? (entry == nil ? "Not bound" : role.rawValue) : "Unavailable").font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.chipText)
                            .padding(.horizontal, 8).frame(height: 20)
                            .overlay(Capsule().strokeBorder(palette.dark ? Color.white.opacity(0.16) : palette.wellStroke, lineWidth: 1))
                    }
                    Text(description(entry: entry, mapping: mapping, name: name)).font(.system(size: 13)).foregroundStyle(palette.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                    if let entry, binds.action(named: entry.actionName) == nil, let document = studio.current?.document {
                        let inspection = ConfigChains.inspect(key: entry.key.spelling, document: document)
                        let press = inspection.press.commands.compactMap(binds.words), release = inspection.release.commands.compactMap(binds.words)
                        if !press.isEmpty {
                            Text("Press: " + press.map(\.text).joined(separator: ", then ") + (release.isEmpty ? "" : ". Letting go: " + release.map(\.text).joined(separator: ", then ")))
                                .font(StudioTypography.help).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    if let entry, let key {
                        Text("bind \(key.spelling) \"\(entry.text)\"").font(.typewriter(12)).foregroundStyle(palette.address).textSelection(.enabled).lineLimit(2)
                    }
                    if binds.target == .find {
                        Text("Press a key or click a mouse button… Escape cancels.").font(.system(size: 13)).foregroundStyle(palette.accent)
                    }
                    if compact, available, let key { actions(key: key, entry: entry).padding(.top, 6) }
                }
                Spacer(minLength: 8)
                if !compact, available, let key { actions(key: key, entry: entry) }
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Selected key")
    }
    private func actions(key: EngineKey, entry: ConfigBindingEntry?) -> some View {
                    HStack(spacing: 8) {
                        if let chain = binds.ownedChain(on: key) {
                            Button("Edit Steps…") { binds.layout = .list; binds.selectChain(chain.id) }.glassAction()
                        }
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
