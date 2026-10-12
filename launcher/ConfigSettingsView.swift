import SwiftUI
import AppKit

/// A loadout's Settings tab: Guided and Expert share the eight purpose groups and the
/// same effective values. Every control saves one transaction when its gesture ends.
struct ConfigSettingsView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    var openSettings: () -> Void
    @Environment(\.palette) private var palette
    @FocusState private var searchFocused: Bool

    var body: some View {
        GeometryReader { proxy in
            // Below 480 points for the editor, a menu replaces the group rail.
            let compact = proxy.size.width - GroupRail.width - 22 < 480
            // Expert rows set their name above the controls when the 214-point name column, a slider
            // with its field and presets, and the row's three actions don't fit side by side.
            let stacked = proxy.size.width - (compact ? 0 : GroupRail.width + 22) < ExpertRow.sideBySideWidth
            VStack(alignment: .leading, spacing: StudioMetrics.sectionGap) {
                if studio.level == .expert { ExpertTools(studio: studio, searchFocused: $searchFocused) }
                HStack(alignment: .top, spacing: 22) {
                    if !compact {
                        GroupRail(studio: studio, expert: studio.level == .expert).frame(width: GroupRail.width)
                    }
                    Group {
                        if studio.level == .guided {
                            GuidedEditor(studio: studio, compact: compact)
                        } else if studio.textMode {
                            TextEditorPane(studio: studio)
                        } else {
                            ExpertEditor(studio: studio, compact: compact, stacked: stacked, openSettings: openSettings)
                        }
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                }
            }
        }
        .background {
            // Command-F: the page search lives in Expert.
            Button("") { studio.level = .expert; searchFocused = true }.keyboardShortcut("f", modifiers: .command).opacity(0).accessibilityHidden(true)
        }
    }
}

// MARK: - Group rail

private struct GroupRail: View {
    /// Wider than StudioMetrics' 160 points (and the boards' 186): SF Pro needs 200 points for
    /// “Frame & movement” and “HUD & crosshair” with Expert's counts, and names never truncate.
    static let width: CGFloat = 200
    @ObservedObject var studio: ConfigStudioModel
    var expert: Bool
    @FocusState private var focused: Bool
    var body: some View {
        // A system selection list: native highlight (active and inactive), focus ring, arrow keys
        // and selected state. While Expert searches, no row is selected.
        List(selection: selection) {
            Section {
                ForEach(ConfigPurposeGroup.allCases, id: \.self) { group in
                    StudioRailRow(title: studio.catalogTitle(group), symbol: group.symbol, count: expert ? studio.specs(in: group).count : nil)
                        .tag(group)
                }
            }
            if expert {
                Section("Filters") {
                    ForEach(ConfigExpertFilter.allCases) { filter in
                        if #available(macOS 14.0, *) {
                            filterToggle(filter).selectionDisabled()
                        } else {
                            filterToggle(filter)
                        }
                    }
                }
            }
        }
        .listStyle(.sidebar)
        .scrollContentBackground(.hidden)
        .focusable()
        .focused($focused)
        .simultaneousGesture(TapGesture().onEnded { focused = true })
        .onMoveCommand { direction in
            guard direction == .up || direction == .down else { return }
            let groups = ConfigPurposeGroup.allCases
            guard let index = groups.firstIndex(of: studio.group) else { return }
            let next = min(groups.count - 1, max(0, index + (direction == .down ? 1 : -1)))
            selection.wrappedValue = groups[next]
        }
        .accessibilityLabel("Setting groups")
    }

    private var selection: Binding<ConfigPurposeGroup?> {
        Binding(get: {
            guard expert else { return studio.group }
            return studio.search.isEmpty && studio.filter == nil ? studio.group : nil
        }, set: { group in
            guard let group else { return }
            studio.group = group; studio.filter = nil
            studio.search = ""; studio.highlighted = nil
        })
    }

    private func filterToggle(_ filter: ConfigExpertFilter) -> some View {
        Toggle(isOn: Binding(get: { studio.filter == filter }, set: { enabled in
            studio.filter = enabled ? filter : nil
            studio.search = ""; studio.highlighted = nil
        })) {
            StudioRailRow(title: filter.title, symbol: filter == .changed ? "pencil" : "lock",
                          count: filter == .changed ? studio.changedCount : studio.lockedCount)
        }
        .toggleStyle(.checkbox)
        .accessibilityLabel(filter.title)
    }
}

private struct GroupMenu: View {
    @ObservedObject var studio: ConfigStudioModel
    var body: some View {
        Picker("Setting group", selection: Binding(get: { studio.group }, set: { studio.group = $0; studio.filter = nil; studio.search = "" })) {
            ForEach(ConfigPurposeGroup.allCases, id: \.self) { Label(studio.catalogTitle($0), systemImage: $0.symbol).tag($0) }
        }
        .labelsHidden().fixedSize()
    }
}

extension ConfigStudioModel {
    func catalogTitle(_ group: ConfigPurposeGroup) -> String { catalog?.groups.first { $0.id == group }?.title ?? group.rawValue }
}

// MARK: - Guided

private struct GuidedEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    var compact: Bool
    @Environment(\.palette) private var palette

    var body: some View {
        let specs = studio.guidedSpecs(in: studio.group)
        ScrollView {
            VStack(alignment: .leading, spacing: StudioMetrics.sectionGap) {
                HStack(alignment: .bottom, spacing: 16) {
                    VStack(alignment: .leading, spacing: 4) {
                        if compact { GroupMenu(studio: studio).padding(.bottom, 4) }
                        Text(studio.catalogTitle(studio.group)).font(StudioTypography.groupTitle).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                        Text(studio.group.blurb).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer(minLength: 8)
                    Button { studio.level = .expert; studio.filter = nil; studio.search = "" } label: {
                        Text("All \(studio.specs(in: studio.group).count) settings ›").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.link)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Show all \(studio.specs(in: studio.group).count) \(studio.catalogTitle(studio.group)) settings in Expert")
                }
                .padding(.horizontal, 4)
                SettingsGroup {
                    ForEach(specs) { spec in
                        GuidedRow(studio: studio, checkup: studio.checkup, spec: spec)
                        if spec.name.lowercased() == "sensitivity" { DPIRow(studio: studio) }
                    }
                }
                .disabled(studio.isReadOnly)
                Text("Changes save to this loadout as you make them. Undo anytime from History.")
                    .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.horizontal, 6)
            }
            .padding(.trailing, 4).padding(.bottom, 8)
        }
        .scrollIndicators(.automatic)
    }
}

private struct GuidedRow: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var spec: ConfigSettingSpec
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let state = studio.state(spec)
        VStack(spacing: 0) {
            if spec.name.lowercased() == "com_maxfps" {
                FrameCapRow(studio: studio, spec: spec, state: state)
            } else {
                SettingsRow(spec.title, detail: spec.setting.summary) {
                    GuidedControl(studio: studio, spec: spec, state: state)
                }
                .accessibilityHint(spec.setting.summary)
            }
            if let tip = checkup.tip(spec) { RuleTip(studio: studio, checkup: checkup, spec: spec, text: tip.text, fix: tip.fix) }
        }
    }
}

/// A rule this setting breaks under the chosen rules, with A1's fix when it has one.
private struct RuleTip: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var spec: ConfigSettingSpec
    var text: String
    var fix: ConfigCheckupSuggestion?
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "lightbulb").foregroundStyle(palette.accent).accessibilityHidden(true)
            Text("\(checkup.selectionTitle): \(text)").font(.system(size: 13)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            if let fix, let change = fix.changes.first(where: { $0.setting.caseInsensitiveCompare(spec.name) == .orderedSame }) {
                Button("Use \(ConfigValues.display(spec, change.newValue))") { checkup.apply(fix.id, undo: undo) }
                    .glassAction().controlSize(.small).fixedSize().disabled(studio.isReadOnly)
                    .help(fix.title)
            }
        }
        .padding(.vertical, 10).padding(.leading, 14).padding(.trailing, 12)
        .background(palette.accent.opacity(0.07), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.accent.opacity(0.25), lineWidth: 1))
        .padding(.horizontal, SettingsMetrics.radius).padding(.bottom, 14)
        .accessibilityElement(children: .contain)
    }
}

/// Plain controls: switches, menus, sliders with a readout, or a field when the range is too wide to slide.
private struct GuidedControl: View {
    @ObservedObject var studio: ConfigStudioModel
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        switch spec.type {
        case .bool:
            if spec.labelsCoverRange {
                LabelChoice(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
            } else {
                Toggle(spec.title, isOn: Binding(get: { state.flag }, set: { studio.set(spec, to: $0 ? "1" : "0", undo: undo) }))
                    .toggleStyle(.switch).labelsHidden().tint(palette.control)
            }
        case .enumeration:
            ChoiceMenu(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
        case .string:
            if spec.name.lowercased() == "name" {
                HStack(spacing: 12) {
                    if let value = state.value, !value.isEmpty { QuakeName(name: value).font(.system(size: 13, weight: .semibold)).lineLimit(1) }
                    CommitField(value: state.value ?? "", label: spec.title, width: 200) { studio.set(spec, to: $0, undo: undo) }
                }
            } else {
                CommitField(value: state.value ?? "", label: spec.title, width: 200, secure: spec.isSecret) { studio.set(spec, to: $0, undo: undo) }
            }
        case .int, .float:
            if spec.name.lowercased() == "m_pitch" {
                // A signed scale: the direction flips and the magnitude is kept.
                Picker(spec.title, selection: Binding(get: { (state.number ?? 0.022) < 0 }, set: { inverted in
                    let magnitude = abs(state.number ?? 0.022)
                    studio.set(spec, to: ConfigValues.format(inverted ? -magnitude : magnitude), undo: undo)
                })) { Text("Normal").tag(false); Text("Inverted").tag(true) }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
            } else if spec.name.lowercased() == "r_anisotropy", let minimum = spec.minimum, let maximum = spec.maximum {
                let levels = stride(from: 0, through: 6, by: 1).map { pow(2.0, Double($0)) }.filter { $0 >= minimum && $0 <= maximum }
                Picker(spec.title, selection: Binding(get: { state.number ?? minimum }, set: { studio.set(spec, to: ConfigValues.format($0, integer: true), undo: undo) })) {
                    ForEach(levels + ((state.number).map { levels.contains($0) ? [] : [$0] } ?? []), id: \.self) { Text("\(Int($0))×").tag($0) }
                }
                .labelsHidden().fixedSize()
            } else if spec.labelsCoverRange {
                LabelChoice(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
            } else if let range = spec.slideRange {
                HStack(spacing: 14) {
                    if spec.practicalRange != nil {
                        // The slider covers the catalog's practical range; the field takes any value the game accepts.
                        CommitSlider(value: state.number ?? range.lowerBound, range: range, step: spec.step, label: spec.title, readout: nil) {
                            studio.set(spec, to: ConfigValues.text(number: $0, spec: spec), undo: undo)
                        }
                        .frame(width: 240)
                        NumberEntry(spec: spec, state: state, showUnit: true) { studio.set(spec, to: $0, undo: undo) }
                    } else {
                        CommitSlider(value: state.number ?? range.lowerBound, range: range, step: spec.step, label: spec.title,
                                     readout: { ConfigValues.display(spec, ConfigValues.format($0, integer: spec.type == .int)) }) {
                            studio.set(spec, to: ConfigValues.text(number: $0, spec: spec), undo: undo)
                        }
                        .frame(width: 330)
                    }
                    if !spec.valueLabels.isEmpty { LabelPresets(spec: spec) { studio.set(spec, to: $0, undo: undo) } }
                }
                .practicalRangeHint(spec)
            } else {
                HStack(spacing: 10) {
                    NumberEntry(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
                    if !spec.valueLabels.isEmpty { LabelPresets(spec: spec) { studio.set(spec, to: $0, undo: undo) } }
                }
            }
        case .vec2, .vec3, .vec4, .color:
            VectorEntry(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
        }
    }
}

private extension View {
    /// The practical range as a tooltip and a VoiceOver hint, when the catalog gives one.
    @ViewBuilder func practicalRangeHint(_ spec: ConfigSettingSpec) -> some View {
        if let text = PracticalRangeHint.text(spec) { help(text).accessibilityHint(text) } else { self }
    }
}

enum PracticalRangeHint {
    /// “Slider range 0.01–30. You can type any value from 0.01 to 100.” The range is catalog
    /// metadata, not a measurement of what players use, so it is named for what it does.
    static func text(_ spec: ConfigSettingSpec) -> String? {
        guard let range = spec.practicalRange else { return nil }
        var text = "Slider range \(ConfigValues.format(range.lowerBound))–\(ConfigValues.format(range.upperBound))."
        if let minimum = spec.minimum, let maximum = spec.maximum {
            text += " You can type any value from \(ConfigValues.format(minimum)) to \(ConfigValues.format(maximum))."
        }
        return text
    }
}

/// Mouse DPI is loadout metadata, not a game setting: it only turns mouse speed into distance.
private struct DPIRow: View {
    @ObservedObject var studio: ConfigStudioModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    var body: some View {
        let dpi = studio.metadata?.dpi
        let speed = studio.spec(named: "sensitivity").flatMap { studio.state($0).number }
        let yaw = studio.spec(named: "m_yaw").flatMap { studio.state($0).number } ?? 0.022
        let distance = dpi.flatMap { dpi in speed.flatMap { MouseMath.centimetresPer360(dpi: dpi, sensitivity: $0, yaw: yaw) } }
        SettingsRow("Your mouse’s DPI", detail: "From your mouse’s own settings. CoD2 Silicon uses it only to work out how far you turn; the game never reads it.") {
            HStack(spacing: 12) {
                CommitField(value: dpi.map { ConfigValues.format($0) } ?? "", label: "Mouse DPI", width: 80, prompt: "DPI", validate: { text in
                    let trimmed = text.trimmingCharacters(in: .whitespaces)
                    if trimmed.isEmpty { return "" }
                    guard let value = ConfigValues.number(trimmed), value > 0, value <= 100_000 else { return nil }
                    return ConfigValues.format(value)
                }) { studio.setDPI($0.isEmpty ? nil : ConfigValues.number($0), undo: undo) }
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text(MouseMath.label(distance)).font(.stencil(20)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f))
                    Text("cm per full turn").font(.system(size: 12)).foregroundStyle(palette.secondary)
                }
                .frame(minWidth: 150, alignment: .trailing)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel(distance == nil ? "Enter your mouse’s DPI to see how far you turn" : "\(MouseMath.label(distance)) centimetres per full turn")
                .accessibilityAddTraits(.isStaticText)
            }
        }
    }
}

/// Frame rate cap: three meaningful presets and the person's own number (catalog range).
private struct FrameCapRow: View {
    @ObservedObject var studio: ConfigStudioModel
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    @State private var custom = false

    var body: some View {
        let presets = FrameCap.presets.prefix(3)
        let value = Int((state.number ?? 0).rounded())
        let isCustom = custom || !presets.contains { $0.value == value }
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 3) {
                Text(spec.title).font(.system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text)
                Text(spec.setting.summary).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
            HStack(spacing: 10) {
                ForEach(Array(presets), id: \.value) { preset in
                    Button { custom = false; studio.set(spec, to: String(preset.value), undo: undo) } label: {
                        CrateLabel(numeral: "\(preset.value)", caption: preset.caption, selected: !isCustom && value == preset.value)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("\(preset.value) frames per second, \(preset.caption)")
                    .accessibilityAddTraits(!isCustom && value == preset.value ? .isSelected : [])
                }
                VStack(alignment: .leading, spacing: 6) {
                    if isCustom {
                        CommitField(value: String(value), label: "Custom frame rate cap", width: nil, plain: true,
                                    validate: { ConfigValues.number($0).map { ConfigValues.text(number: $0, spec: spec) } }) { studio.set(spec, to: $0, undo: undo) }
                            .font(.stencil(34)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f)).frame(height: 34)
                    } else {
                        Button { custom = true } label: { Text("—").font(.stencil(34)).foregroundStyle(palette.numeral).frame(maxWidth: .infinity, minHeight: 34, alignment: .leading) }
                            .buttonStyle(.plain).accessibilityLabel("Custom frame rate cap")
                    }
                    Text(isCustom ? FrameCap.customCaption(value) : "Your own number").font(.system(size: 11)).foregroundStyle(palette.secondary)
                }
                .crate(selected: isCustom, palette: palette)
            }
        }
        .padding(.horizontal, SettingsMetrics.radius).padding(.top, 16).padding(.bottom, 14)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .contain)
        .accessibilityLabel(spec.title)
    }
}

private struct CrateLabel: View {
    var numeral: String
    var caption: String
    var selected: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(numeral).font(.stencil(34)).tracking(0.68).foregroundStyle(selected ? (palette.dark ? palette.accent : Palette.hex(0x4e5b2f)) : palette.numeral).frame(height: 34)
            Text(caption).font(.system(size: 11)).foregroundStyle(palette.secondary)
        }
        .crate(selected: selected, palette: palette)
    }
}

private extension View {
    func crate(selected: Bool, palette: Palette) -> some View {
        self.padding(.vertical, 14).padding(.horizontal, 16)
            .frame(maxWidth: .infinity, minHeight: 88, alignment: .topLeading)
            .background(selected ? palette.selection : palette.crate, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(selected ? palette.selectionStroke : (palette.dark ? Color.white.opacity(0.09) : palette.panelStroke), lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }
}

// MARK: - Expert

private struct ExpertTools: View {
    @ObservedObject var studio: ConfigStudioModel
    var searchFocused: FocusState<Bool>.Binding
    @Environment(\.palette) private var palette

    var body: some View {
        // One row when it fits; otherwise the tools wrap into a second row, and at the narrowest
        // the rules get a row of their own above search and Edit as Text, never squeezing labels.
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 10) { compare; Spacer(minLength: 8); tools }
            VStack(alignment: .leading, spacing: 10) { compare; tools }
            VStack(alignment: .leading, spacing: 10) {
                compare
                ExpertRulesStatus(checkup: studio.checkup)
                HStack(spacing: 10) { search; textToggle }
            }
        }
        .controlSize(.regular)
    }

    private var compare: some View {
        HStack(spacing: 10) {
            HStack(spacing: 8) {
                Text("Compare with").font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize()
                // A player from Discover appears here only while it's the comparison.
                Picker("Compare with", selection: Binding(get: {
                    studio.player.map { CompareChoice.player($0.entryID) } ?? studio.compareID.map(CompareChoice.loadout) ?? .nobody
                }, set: { choice in
                    switch choice {
                    case .nobody: studio.stopComparing()
                    case .loadout(let id): studio.compareID = id
                    case .player: break
                    }
                })) {
                    Text("Nobody").tag(CompareChoice.nobody)
                    if let player = studio.player { Text(player.name).tag(CompareChoice.player(player.entryID)) }
                    ForEach(studio.profiles.filter { $0.id != studio.current?.profile.id }) { Text($0.displayName).tag(CompareChoice.loadout($0.id)) }
                }
                .labelsHidden().fixedSize()
            }
            if let name = studio.comparisonName {
                VStack(alignment: .leading, spacing: 1) {
                    Label("\(name)’s value", systemImage: "diamond.fill").font(.system(size: 12)).foregroundStyle(palette.compare).fixedSize()
                    if let provenance = studio.comparisonProvenance {
                        Text(provenance).font(.system(size: 11)).foregroundStyle(palette.secondary).fixedSize()
                    }
                }
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Compare markers show \(name)’s values" + (studio.comparisonProvenance.map { ". \($0)" } ?? ""))
                .accessibilityAddTraits(.isStaticText)
            }
        }
    }

    private var tools: some View {
        HStack(spacing: 10) {
            ExpertRulesStatus(checkup: studio.checkup)
            search
            textToggle
        }
    }

    private var search: some View {
        HStack(spacing: 6) {
            Image(systemName: "magnifyingglass").foregroundStyle(palette.secondary).accessibilityHidden(true)
            TextField("Find a setting", text: $studio.search).textFieldStyle(.plain).focused(searchFocused).frame(width: 150)
                .onExitCommand { studio.search = ""; searchFocused.wrappedValue = false }
            if !studio.search.isEmpty {
                Button { studio.search = "" } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(palette.tertiary).accessibilityLabel("Clear search")
            }
        }
        .padding(.horizontal, 12).frame(height: ControlMetrics.height(.large))
        .background(palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.6), in: Capsule())
        .overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1))
        .help("Find a setting by name, description or console name (⌘F)")
    }

    private var textToggle: some View {
        Button { studio.toggleText() } label: {
            if studio.textMode { Label("Show Controls", systemImage: "slider.horizontal.3") }
            else { Label("Edit as Text", systemImage: "text.alignleft") }
        }
        .glassAction().fixedSize()
        .help(studio.textMode ? "Back to the controls" : "Edit this loadout’s config as text")
    }
}

private struct ExpertRulesStatus: View {
    @ObservedObject var checkup: ConfigCheckupModel
    var body: some View {
        HStack(spacing: 10) {
            RulesMenu(checkup: checkup)
            if !checkup.rulesetIDs.isEmpty {
                let count = checkup.violations.count
                StudioStatusLabel(title: count == 0 ? "Follows these rules" : count == 1 ? "1 setting breaks them" : "\(count) settings break them",
                                  tone: count == 0 ? .positive : .negative)
                    .fixedSize()
            }
        }
    }
}

private struct ExpertEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    var compact: Bool
    var stacked: Bool
    var openSettings: () -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        let sections = studio.expertSections()
        ScrollViewReader { reader in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 0, pinnedViews: []) {
                    if compact { GroupMenu(studio: studio).padding(.bottom, 10) }
                    ForEach(sections) { section in
                        HStack(alignment: .firstTextBaseline) {
                            Text(studio.catalogTitle(section.group)).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                            Spacer()
                            Text(section.specs.count == 1 ? "1 setting" : "\(section.specs.count) settings").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                        }
                        .padding(.horizontal, 4).padding(.top, 8).padding(.bottom, 8)
                        ForEach(Array(section.specs.enumerated()), id: \.element.id) { index, spec in
                            ExpertRow(studio: studio, checkup: studio.checkup, spec: spec, stacked: stacked, openSettings: openSettings)
                                .panelSegment(first: index == 0, last: index == section.specs.count - 1, highlighted: studio.highlighted == spec.name,
                                              bad: studio.checkup.evaluation(spec)?.status == .violation)
                                .id(spec.name)
                        }
                        if studio.filter == nil && studio.search.isEmpty, studio.protectedCount(in: section.group) > 0 {
                            Text("\(studio.protectedCount(in: section.group)) more \(studio.catalogTitle(section.group)) settings are protected or read-only in the game, so they aren’t changed here.")
                                .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.horizontal, 6).padding(.top, 8)
                        }
                    }
                    if sections.isEmpty {
                        StudioEmptyState(title: studio.search.isEmpty ? "ALL CLEAR" : "NO MATCH", message: emptyMessage).frame(maxWidth: .infinity)
                    }
                }
                .padding(.trailing, 4).padding(.bottom, 8)
                .disabled(studio.isReadOnly)
            }
            .scrollIndicators(.automatic)
            .onAppear { scroll(reader) }
            .onChange(of: studio.highlighted) { _ in scroll(reader) }
        }
    }
    private var emptyMessage: String {
        if !studio.search.isEmpty { return "No setting matches. Try a word like “jump”, “turn” or “sound”." }
        return studio.filter == .locked ? "Nothing is locked. Lock a setting to keep imports and Check-up away from it." : "Everything here matches the game’s defaults."
    }
    private func scroll(_ reader: ScrollViewProxy) {
        guard let name = studio.highlighted else { return }
        DispatchQueue.main.async { reader.scrollTo(name, anchor: .center) }
    }
}

private struct ExpertRow: View {
    /// The editor width below which a row's name sits above its controls: the 214-point name
    /// column, a slider with its field and presets, the three actions and the row's insets.
    static let sideBySideWidth: CGFloat = 640
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var spec: ConfigSettingSpec
    var stacked = false
    var openSettings: () -> Void
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    @State private var editingNote = false

    var body: some View {
        let state = studio.state(spec)
        let theirs = studio.theirs(spec)
        VStack(alignment: .leading, spacing: 6) {
            if stacked {
                VStack(alignment: .leading, spacing: 8) {
                    name(state)
                    HStack(alignment: .center, spacing: 14) {
                        control(state, theirs).frame(maxWidth: .infinity, alignment: .leading)
                        actions(state)
                    }
                    .frame(minHeight: 40)
                }
            } else {
                HStack(alignment: .center, spacing: 14) {
                    name(state).frame(width: 214, alignment: .leading)
                    control(state, theirs).frame(maxWidth: .infinity, alignment: .leading)
                    actions(state)
                }
                .frame(minHeight: 40)
            }
            let evaluation = checkup.evaluation(spec)
            let issue = evaluation.flatMap { $0.status == .violation || $0.status == .conflict ? $0 : nil }
            if theirs != nil || state.note != nil || spec.launcherOwned || issue != nil {
                HStack(spacing: 10) {
                    if let issue { StudioStatusLabel(title: issue.reason, tone: issue.status.tone) }
                    if let theirs, let source = studio.comparisonName {
                        StudioCompareValue(value: theirs.display, source: source).fixedSize()
                        // The same rule guards the action: locks, the game running, the launcher and server-owned settings.
                        let blocked = studio.takeTheirsBlock(spec)
                        Button("Take Theirs") { studio.takeTheirs(spec, undo: undo) }.glassAction().controlSize(.small).fixedSize()
                            .disabled(blocked != nil)
                            .accessibilityLabel("Take \(source)’s \(spec.title), \(theirs.display)")
                            .accessibilityHint(blocked ?? "")
                        if let blocked {
                            Text(blocked).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    // With a comparison, Take Theirs already gives this reason.
                    if spec.launcherOwned && (theirs == nil || studio.comparisonName == nil) {
                        Text("CoD2 Silicon sets this from Settings each time the game starts.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                    }
                    if let note = state.note {
                        Text("Note: \(note)").font(StudioTypography.help).italic().foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                }
                .padding(.leading, stacked ? 0 : 228)
                .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(.vertical, 10).padding(.leading, 18).padding(.trailing, 12)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(spec.title)
        .accessibilityHint("Console name \(spec.name). \(spec.setting.summary)")
    }

    private func name(_ state: ConfigSettingState) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 6) {
                Text(spec.title).font(.system(size: 13, weight: .medium)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text)
                    .lineLimit(2).fixedSize(horizontal: false, vertical: true)
                if state.locked { Image(systemName: "lock.fill").font(.system(size: 10)).foregroundStyle(palette.accent).accessibilityLabel("Locked") }
                if state.changed && !spec.launcherOwned { Circle().fill(palette.accent).frame(width: 6, height: 6).accessibilityLabel("Changed from default").accessibilityAddTraits(.isStaticText) }
            }
            Text(spec.name).font(StudioTypography.consoleName).foregroundStyle(palette.address).lineLimit(1).textSelection(.enabled)
            if let badge = checkup.badge(spec) {
                Label(badge.text, systemImage: badge.tone.symbol).font(.system(size: 10.5, weight: .semibold))
                    .foregroundStyle(badge.tone.color(in: palette)).labelStyle(.titleOnly)
                    .padding(.horizontal, 7).frame(height: 18)
                    .overlay(Capsule().strokeBorder(badge.tone.color(in: palette).opacity(0.55), lineWidth: 1))
                    .accessibilityLabel("Rules: \(badge.text)")
            }
            if spec.setting.variant(for: studio.mode)?.serverControlled == true {
                Text("Servers can set this").font(.system(size: 10.5, weight: .semibold)).foregroundStyle(palette.chipText)
                    .padding(.horizontal, 7).frame(height: 18).overlay(Capsule().strokeBorder(palette.dark ? Color.white.opacity(0.16) : palette.wellStroke, lineWidth: 1))
            }
        }
    }

    @ViewBuilder private func control(_ state: ConfigSettingState, _ theirs: ConfigSettingState?) -> some View {
        if spec.launcherOwned {
            HStack(spacing: 10) {
                Text(state.display).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                Button("Change in Settings", action: openSettings).buttonStyle(.plain).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.link)
            }
        } else {
            ExpertControl(studio: studio, spec: spec, state: state, theirs: theirs)
        }
    }

    private func actions(_ state: ConfigSettingState) -> some View {
        HStack(spacing: 0) {
            IconAction(symbol: "arrow.uturn.backward", label: "Reset \(spec.title) to default, \(ConfigValues.display(spec, spec.defaultText))") {
                studio.reset(spec, undo: undo)
            }
            .disabled(!state.changed || spec.launcherOwned || spec.defaultText == nil)
            IconAction(symbol: state.locked ? "lock.fill" : "lock.open", label: state.locked ? "Unlock \(spec.title)" : "Lock \(spec.title)", on: state.locked) {
                studio.setLock(spec, !state.locked, undo: undo)
            }
            .disabled(spec.launcherOwned)
            IconAction(symbol: state.note == nil ? "square.and.pencil" : "note.text", label: state.note == nil ? "Add a note to \(spec.title)" : "Edit the note on \(spec.title)", on: state.note != nil) {
                editingNote = true
            }
            .popover(isPresented: $editingNote, arrowEdge: .bottom) { NoteEditor(studio: studio, spec: spec, note: state.note ?? "").studioPopover() }
        }
    }
}

/// Typed Expert controls: switch + word, segmented frame cap, slider + field + unit, menus and fields.
private struct ExpertControl: View {
    @ObservedObject var studio: ConfigStudioModel
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var theirs: ConfigSettingState?
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        switch spec.type {
        case .bool:
            if spec.labelsCoverRange {
                LabelChoice(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
            } else {
                HStack(spacing: 10) {
                    Toggle(spec.title, isOn: Binding(get: { state.flag }, set: { studio.set(spec, to: $0 ? "1" : "0", undo: undo) }))
                        .toggleStyle(.switch).labelsHidden().tint(palette.control)
                    Text(state.flag ? "On" : "Off").font(.system(size: 12)).foregroundStyle(palette.secondary).accessibilityHidden(true)
                }
            }
        case .enumeration:
            ChoiceMenu(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
        case .string:
            CommitField(value: state.value ?? "", label: spec.title, width: 220, secure: spec.isSecret) { studio.set(spec, to: $0, undo: undo) }
        case .vec2, .vec3, .vec4, .color:
            VectorEntry(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
        case .int, .float:
            if spec.name.lowercased() == "com_maxfps" {
                FrameCapSegments(studio: studio, spec: spec, state: state, theirs: theirs)
            } else if spec.labelsCoverRange {
                LabelChoice(spec: spec, state: state) { studio.set(spec, to: $0, undo: undo) }
            } else if let range = spec.slideRange {
                HStack(spacing: 10) {
                    CommitSlider(value: state.number ?? range.lowerBound, range: range, step: spec.step, label: spec.title, readout: nil,
                                 marker: theirs?.number) { studio.set(spec, to: ConfigValues.text(number: $0, spec: spec), undo: undo) }
                    NumberEntry(spec: spec, state: state, showUnit: true) { studio.set(spec, to: $0, undo: undo) }
                    if !spec.valueLabels.isEmpty { LabelPresets(spec: spec) { studio.set(spec, to: $0, undo: undo) } }
                }
                .practicalRangeHint(spec)
            } else {
                HStack(spacing: 10) {
                    NumberEntry(spec: spec, state: state, showUnit: true) { studio.set(spec, to: $0, undo: undo) }
                    if !spec.valueLabels.isEmpty { LabelPresets(spec: spec) { studio.set(spec, to: $0, undo: undo) } }
                }
            }
        }
    }
}

private struct FrameCapSegments: View {
    @ObservedObject var studio: ConfigStudioModel
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var theirs: ConfigSettingState?
    @Environment(\.undoManager) private var undo
    @State private var custom = false
    var body: some View {
        let value = Int((state.number ?? 0).rounded())
        let presets = FrameCap.presets.prefix(3).map(\.value)
        let isCustom = custom || !presets.contains(value)
        HStack(spacing: 10) {
            Picker(spec.title, selection: Binding(get: { isCustom ? -1 : value }, set: { choice in
                if choice < 0 { custom = true } else { custom = false; studio.set(spec, to: String(choice), undo: undo) }
            })) {
                ForEach(presets, id: \.self) { Text("\($0)").tag($0) }
                Text("Custom").tag(-1)
            }
            .pickerStyle(.segmented).labelsHidden().fixedSize()
            if isCustom { NumberEntry(spec: spec, state: state, showUnit: true) { studio.set(spec, to: $0, undo: undo) } }
        }
    }
}

private struct IconAction: View {
    var symbol: String
    var label: String
    var on = false
    var action: () -> Void
    @Environment(\.palette) private var palette
    @Environment(\.isEnabled) private var enabled
    var body: some View {
        Button(action: action) {
            Image(systemName: symbol).font(.system(size: 12, weight: .medium))
                .foregroundStyle(on ? palette.accent : palette.chipIcon)
                .frame(width: StudioMetrics.minimumHitTarget, height: StudioMetrics.minimumHitTarget)
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .opacity(enabled ? 1 : 0.3)
        .help(label)
        .accessibilityLabel(label)
    }
}

private struct NoteEditor: View {
    @ObservedObject var studio: ConfigStudioModel
    var spec: ConfigSettingSpec
    @State var note: String
    @Environment(\.dismiss) private var dismiss
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Note on \(spec.title)").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.heading)
            TextField("Why it’s set this way", text: $note, axis: .vertical).lineLimit(2...5).textFieldStyle(.roundedBorder).frame(width: 280)
            HStack {
                if studio.state(spec).note != nil {
                    Button("Remove Note") { studio.setNote(spec, "", undo: undo); dismiss() }.glassAction().controlSize(.small)
                }
                Spacer()
                Button("Save Note") { studio.setNote(spec, note, undo: undo); dismiss() }.prominentAction().controlSize(.small).keyboardShortcut(.defaultAction)
            }
        }
        .padding(16)
        .disabled(studio.isReadOnly)
    }
}

// MARK: - Shared controls

private enum CompareChoice: Hashable { case nobody, loadout(String), player(String) }

/// A setting whose every value has a name in the game's own menus: choose by name.
private struct LabelChoice: View {
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var commit: (String) -> Void
    var body: some View {
        let current = state.value.flatMap { value in spec.valueLabels.first { ConfigValues.same(spec, $0.text, value) }?.text }
        let picker = Picker(spec.title, selection: Binding(get: { current ?? "" }, set: { if !$0.isEmpty { commit($0) } })) {
            ForEach(spec.valueLabels, id: \.text) { Text($0.title).tag($0.text) }
            // A value outside the named ones (set by hand) still shows as itself.
            if current == nil { Text(state.value ?? "Not set").tag("") }
        }
        .labelsHidden().fixedSize()
        if spec.valueLabels.count <= 4 && spec.valueLabels.allSatisfy({ $0.title.count <= 14 }) {
            picker.pickerStyle(.segmented)
        } else {
            picker.pickerStyle(.menu)
        }
    }
}

/// Named values from the game's menus, as quick choices beside a number.
private struct LabelPresets: View {
    var spec: ConfigSettingSpec
    var commit: (String) -> Void
    private var presetsIcon: NSImage {
        let name = "Presets for \(spec.title)"
        let image = NSImage(systemSymbolName: "slider.horizontal.3", accessibilityDescription: name) ?? NSImage()
        image.accessibilityDescription = name
        return image
    }
    var body: some View {
        Menu {
            ForEach(spec.valueLabels, id: \.text) { label in
                Button(ConfigValues.display(spec, label.text)) { commit(label.text) }
            }
        } label: {
            Label {
                Text("Presets").accessibilityLabel("Presets for \(spec.title)")
            } icon: {
                Image(nsImage: presetsIcon)
            }
        }
        .menuStyle(.borderlessButton).fixedSize()
        .accessibilityLabel("Presets for \(spec.title)")
        .help("Values the game’s own menus name")
    }
}

private struct ChoiceMenu: View {
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var commit: (String) -> Void
    var body: some View {
        let index = state.value.flatMap { ConfigValues.enumIndex($0, choices: spec.choices) }
        Picker(spec.title, selection: Binding(get: { index ?? -1 }, set: { if spec.choices.indices.contains($0) { commit(spec.choices[$0]) } })) {
            ForEach(Array(spec.choices.enumerated()), id: \.offset) { Text($0.element).tag($0.offset) }
            if index == nil { Text(state.value ?? "Not set").tag(-1) }
        }
        .labelsHidden().fixedSize()
    }
}

private struct NumberEntry: View {
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var showUnit = false
    var commit: (String) -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 6) {
            CommitField(value: state.value ?? "", label: spec.title, width: 84, alignment: .trailing,
                        validate: { ConfigValues.number($0).map { ConfigValues.text(number: $0, spec: spec) } }, commit: commit)
            if showUnit, let unit = spec.unit {
                Text(unit).font(.system(size: 11)).foregroundStyle(palette.tertiary).lineLimit(1).fixedSize().accessibilityHidden(true)
            }
        }
    }
}

private struct VectorEntry: View {
    var spec: ConfigSettingSpec
    var state: ConfigSettingState
    var commit: (String) -> Void
    var body: some View {
        CommitField(value: state.value ?? "", label: spec.title, width: 160, validate: { text in
            let values = text.split(separator: " ").map { ConfigValues.number(String($0)) }
            guard values.count == spec.vectorCount, values.allSatisfy({ $0 != nil }) else { return nil }
            return values.map { ConfigValues.format($0!) }.joined(separator: " ")
        }, commit: commit)
        .help("\(spec.vectorCount ?? 0) numbers separated by spaces")
    }
}

/// A field that saves on Return or when focus leaves; Escape restores the saved value.
/// `validate` returns the normalized text, or nil to refuse it.
struct CommitField: View {
    var value: String
    var label: String
    var width: CGFloat?
    var secure = false
    var plain = false
    var alignment: TextAlignment = .leading
    var prompt: String? = nil
    var validate: (String) -> String? = { $0 }
    var commit: (String) -> Void
    @State private var draft = ""
    @State private var invalid = false
    @FocusState private var focused: Bool
    @Environment(\.palette) private var palette

    var body: some View {
        field
            .multilineTextAlignment(alignment)
            .focused($focused)
            .onSubmit(apply)
            .onExitCommand { draft = value; invalid = false; focused = false }
            .onChange(of: focused) { if !$0 { apply() } }
            .onChange(of: value) { if !focused { draft = $0; invalid = false } }
            .onAppear { draft = value }
            .accessibilityLabel(label)
            .accessibilityValue(secure ? "" : draft)
    }
    @ViewBuilder private var field: some View {
        if plain {
            TextField(label, text: $draft, prompt: prompt.map { Text($0) }).textFieldStyle(.plain).labelsHidden()
        } else {
            Group {
                if secure { SecureField(label, text: $draft, prompt: prompt.map { Text($0) }) }
                else { TextField(label, text: $draft, prompt: prompt.map { Text($0) }) }
            }
            .textFieldStyle(.plain).labelsHidden()
            .font(.system(size: 13)).monospacedDigit()
            .padding(.horizontal, 8).frame(width: width, height: ControlMetrics.height(.large))
            .background(palette.well, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 7, style: .continuous).strokeBorder(invalid ? palette.negative : palette.wellStroke, lineWidth: 1))
        }
    }
    private func apply() {
        guard draft != value else { invalid = false; return }
        guard let normalized = validate(draft) else { invalid = true; NSSound.beep(); return }
        invalid = false
        draft = normalized
        if normalized != value { commit(normalized) }
    }
}

/// Previews while dragging and saves when the drag ends. Keyboard steps save after a short pause.
struct CommitSlider: View {
    var value: Double
    var range: ClosedRange<Double>
    var step: Double
    var label: String
    var readout: ((Double) -> String)?
    var marker: Double? = nil
    var commit: (Double) -> Void
    @State private var live: Double?
    @State private var dragging = false
    @State private var pending: Task<Void, Never>?
    @Environment(\.palette) private var palette

    var body: some View {
        let shown = live ?? value
        HStack(spacing: 14) {
            slider
                .overlay(alignment: .topLeading) {
                    if let marker, range.contains(marker) {
                        GeometryReader { proxy in
                            Image(systemName: "diamond.fill").font(.system(size: 8)).foregroundStyle(palette.compare)
                                .position(x: 8 + (proxy.size.width - 16) * (marker - range.lowerBound) / (range.upperBound - range.lowerBound), y: -3)
                        }
                        .allowsHitTesting(false).accessibilityHidden(true)
                    }
                }
            if let readout {
                Text(readout(shown)).font(.system(size: 13, weight: .semibold)).monospacedDigit().foregroundStyle(palette.text)
                    .frame(minWidth: 110, alignment: .trailing).accessibilityHidden(true)
            }
        }
    }
    private var slider: some View {
        let binding = Binding(get: { live ?? value }, set: { newValue in
            live = newValue
            guard !dragging else { return }
            pending?.cancel()
            pending = Task { @MainActor in
                try? await Task.sleep(for: .milliseconds(500))
                guard !Task.isCancelled, !dragging, let current = live else { return }
                live = nil
                commit(current)
            }
        })
        return Group {
            if step > 0 {
                Slider(value: binding, in: range, step: step, onEditingChanged: editing)
            } else {
                Slider(value: binding, in: range, onEditingChanged: editing)
            }
        }
        .tint(palette.control)
        .accessibilityLabel(label)
        .accessibilityValue(readout.map { $0(live ?? value) } ?? ConfigValues.format(live ?? value))
    }
    private func editing(_ active: Bool) {
        dragging = active
        if !active, let current = live {
            pending?.cancel()
            live = nil
            commit(current)
        }
    }
}

// MARK: - Lazy grouped panel

private struct PanelSegment: InsettableShape {
    var radius: CGFloat
    var top: Bool
    var bottom: Bool
    var inset: CGFloat = 0
    func inset(by amount: CGFloat) -> PanelSegment { var copy = self; copy.inset += amount; return copy }
    func path(in rect: CGRect) -> Path {
        let r = rect.insetBy(dx: inset, dy: inset)
        let tr = top ? max(0, radius - inset) : 0, br = bottom ? max(0, radius - inset) : 0
        var path = Path()
        path.move(to: CGPoint(x: r.minX, y: r.minY + tr))
        if tr > 0 { path.addArc(center: CGPoint(x: r.minX + tr, y: r.minY + tr), radius: tr, startAngle: .degrees(180), endAngle: .degrees(270), clockwise: false) }
        path.addLine(to: CGPoint(x: r.maxX - tr, y: r.minY))
        if tr > 0 { path.addArc(center: CGPoint(x: r.maxX - tr, y: r.minY + tr), radius: tr, startAngle: .degrees(270), endAngle: .degrees(0), clockwise: false) }
        path.addLine(to: CGPoint(x: r.maxX, y: r.maxY - br))
        if br > 0 { path.addArc(center: CGPoint(x: r.maxX - br, y: r.maxY - br), radius: br, startAngle: .degrees(0), endAngle: .degrees(90), clockwise: false) }
        path.addLine(to: CGPoint(x: r.minX + br, y: r.maxY))
        if br > 0 { path.addArc(center: CGPoint(x: r.minX + br, y: r.maxY - br), radius: br, startAngle: .degrees(90), endAngle: .degrees(180), clockwise: false) }
        path.closeSubpath()
        return path
    }
}

/// The open outline of a segment: sides always, top and bottom only on the first and last rows.
private struct PanelSegmentEdge: Shape {
    var radius: CGFloat
    var top: Bool
    var bottom: Bool
    func path(in rect: CGRect) -> Path {
        let r = rect.insetBy(dx: 0.5, dy: 0.5)
        let tr = top ? radius - 0.5 : 0, br = bottom ? radius - 0.5 : 0
        var path = Path()
        path.move(to: CGPoint(x: r.minX, y: top ? r.minY + tr : rect.minY))
        path.addLine(to: CGPoint(x: r.minX, y: bottom ? r.maxY - br : rect.maxY))
        if bottom {
            path.addArc(center: CGPoint(x: r.minX + br, y: r.maxY - br), radius: br, startAngle: .degrees(180), endAngle: .degrees(90), clockwise: true)
            path.addLine(to: CGPoint(x: r.maxX - br, y: r.maxY))
            path.addArc(center: CGPoint(x: r.maxX - br, y: r.maxY - br), radius: br, startAngle: .degrees(90), endAngle: .degrees(0), clockwise: true)
        } else { path.move(to: CGPoint(x: r.maxX, y: rect.maxY)) }
        path.addLine(to: CGPoint(x: r.maxX, y: top ? r.minY + tr : rect.minY))
        if top {
            path.addArc(center: CGPoint(x: r.maxX - tr, y: r.minY + tr), radius: tr, startAngle: .degrees(0), endAngle: .degrees(-90), clockwise: true)
            path.addLine(to: CGPoint(x: r.minX + tr, y: r.minY))
            path.addArc(center: CGPoint(x: r.minX + tr, y: r.minY + tr), radius: tr, startAngle: .degrees(-90), endAngle: .degrees(-180), clockwise: true)
        }
        return path
    }
}

private extension View {
    /// One row of an opaque settings panel inside a lazy stack, so long groups stay fast.
    func panelSegment(first: Bool, last: Bool, highlighted: Bool, bad: Bool = false) -> some View {
        modifier(PanelSegmentStyle(first: first, last: last, highlighted: highlighted, bad: bad))
    }
}

private struct PanelSegmentStyle: ViewModifier {
    var first: Bool
    var last: Bool
    var highlighted: Bool
    var bad = false
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        let shape = PanelSegment(radius: SettingsMetrics.radius, top: first, bottom: last)
        content
            .background(highlighted ? palette.selection : bad ? palette.negative.opacity(0.05) : .clear, in: shape)
            .background(palette.panel, in: shape)
            .overlay(alignment: .top) { if !first { Rectangle().fill(palette.separator).frame(height: 1).padding(.leading, 18) } }
            .overlay { PanelSegmentEdge(radius: SettingsMetrics.radius, top: first, bottom: last).stroke(highlighted ? palette.selectionStroke : palette.panelStroke, lineWidth: 1) }
    }
}

// MARK: - Edit as text

private struct TextEditorPane: View {
    @ObservedObject var studio: ConfigStudioModel
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if studio.textStale {
                StudioBanner(title: "This loadout changed on disk while you were editing.",
                             detail: studio.textEdited ? "Revert to see the new version. Your text edits are discarded when you revert." : nil, tone: .caution)
            }
            ConfigCodeEditor(text: studio.text, issues: studio.textIssues, selection: studio.textSelection, editable: !studio.isReadOnly,
                             catalog: studio.catalog, dialect: studio.dialect) { studio.editText($0) }
                .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
                .accessibilityLabel("Loadout as text")
            if !studio.textIssues.isEmpty {
                ScrollView {
                    VStack(alignment: .leading, spacing: 4) {
                        ForEach(studio.textIssues) { issue in
                            Button { studio.textSelection = issue.range } label: {
                                HStack(alignment: .firstTextBaseline, spacing: 8) {
                                    Image(systemName: issue.blocking ? "exclamationmark.circle" : "exclamationmark.triangle")
                                        .foregroundStyle(issue.blocking ? palette.negative : palette.caution).accessibilityHidden(true)
                                    Text("Line \(issue.line), column \(issue.column)").font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text)
                                    Text(issue.message).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                                    if !issue.excerpt.isEmpty { Text(issue.excerpt).font(.typewriter(12)).foregroundStyle(palette.address).lineLimit(1) }
                                    Spacer(minLength: 0)
                                }
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                            .accessibilityLabel("\(issue.blocking ? "Problem" : "Note") on line \(issue.line), column \(issue.column): \(issue.message)")
                            .accessibilityHint("Shows this line in the editor")
                        }
                    }
                }
                .frame(height: min(96, CGFloat(studio.textIssues.count) * 24))
            }
            HStack(spacing: 10) {
                Image(systemName: "info.circle").foregroundStyle(palette.secondary).accessibilityHidden(true)
                Text(studio.textBlocked ? "Fix the underlined problems to apply your edits." : "Edits apply to the loadout when you choose Apply. Lines the game won’t accept are underlined.")
                    .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 8)
                Text("\(studio.text.split(omittingEmptySubsequences: false, whereSeparator: \.isNewline).count) lines").font(.typewriter(12)).foregroundStyle(palette.address)
                Button("Revert", action: studio.revertText).glassAction().disabled(!studio.textEdited)
                Button("Apply Text Edits") { studio.applyText(undo: undo) }.prominentAction()
                    .disabled(!studio.textEdited || studio.textBlocked || studio.isReadOnly || studio.textStale)
                    .keyboardShortcut("s", modifiers: .command)
            }
        }
    }
}

/// An AppKit text view: native editing, undo and find, with config syntax colouring.
private struct ConfigCodeEditor: NSViewRepresentable {
    var text: String
    var issues: [ConfigTextIssue]
    var selection: NSRange?
    var editable: Bool
    var catalog: ConfigCatalog?
    var dialect: ConfigDialect
    var onChange: (String) -> Void
    @Environment(\.palette) private var palette

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> NSScrollView {
        let scroll = NSTextView.scrollableTextView()
        scroll.drawsBackground = false
        scroll.hasVerticalScroller = true
        guard let view = scroll.documentView as? NSTextView else { return scroll }
        view.isRichText = false
        view.allowsUndo = true
        view.usesFindBar = true
        view.isIncrementalSearchingEnabled = true
        view.drawsBackground = false
        view.isAutomaticQuoteSubstitutionEnabled = false
        view.isAutomaticDashSubstitutionEnabled = false
        view.isAutomaticTextReplacementEnabled = false
        view.isAutomaticSpellingCorrectionEnabled = false
        view.isContinuousSpellCheckingEnabled = false
        view.isGrammarCheckingEnabled = false
        view.smartInsertDeleteEnabled = false
        view.textContainerInset = NSSize(width: 12, height: 14)
        view.delegate = context.coordinator
        view.setAccessibilityLabel("Loadout as text")
        return scroll
    }

    func updateNSView(_ scroll: NSScrollView, context: Context) {
        guard let view = scroll.documentView as? NSTextView else { return }
        context.coordinator.onChange = onChange
        view.isEditable = editable
        let font = NSFont(name: LauncherFonts.typewriter, size: 13) ?? .monospacedSystemFont(ofSize: 13, weight: .regular)
        view.font = font
        view.insertionPointColor = NSColor(palette.accent)
        if view.string != text {
            let selected = view.selectedRanges
            view.string = text
            view.selectedRanges = selected.filter { NSMaxRange($0.rangeValue) <= (text as NSString).length }
        }
        highlight(view, font: font)
        if let selection, selection != context.coordinator.shownSelection, NSMaxRange(selection) <= (view.string as NSString).length {
            context.coordinator.shownSelection = selection
            view.setSelectedRange(selection)
            view.scrollRangeToVisible(selection)
            view.showFindIndicator(for: selection)
            view.window?.makeFirstResponder(view)
        }
    }

    private func highlight(_ view: NSTextView, font: NSFont) {
        guard let storage = view.textStorage else { return }
        let length = storage.length
        storage.beginEditing()
        storage.setAttributes([.font: font, .foregroundColor: NSColor(palette.text)], range: NSRange(location: 0, length: length))
        // Byte offsets are UTF-16 offsets for single-byte text; other text is left plain.
        if view.string.unicodeScalars.allSatisfy({ $0.value <= 255 }) {
            let document = ConfigDocument(data: Data(view.string.unicodeScalars.map { UInt8($0.value) }), dialect: dialect)
            func color(_ range: Range<Int>, _ value: Color) {
                guard range.upperBound <= length, !range.isEmpty else { return }
                storage.addAttribute(.foregroundColor, value: NSColor(value), range: NSRange(location: range.lowerBound, length: range.count))
            }
            for command in document.commands {
                for (index, token) in command.tokens.enumerated() {
                    switch (command.kind, index) {
                    case (.setting, 0), (.bind, 0), (.unbind, 0), (.unbindall, 0), (.exec, 0), (.vstr, 0): color(token.range, palette.secondary)
                    case (.setting, 1), (.bind, 1), (.unbind, 1): color(token.range, palette.heading)
                    default: color(token.range, palette.accent)
                    }
                }
                for comment in command.comments { color(comment, palette.tertiary) }
            }
        }
        for issue in issues where NSMaxRange(issue.range) <= length {
            storage.addAttributes([.underlineStyle: NSUnderlineStyle.thick.rawValue | (issue.blocking ? 0 : NSUnderlineStyle.patternDot.rawValue),
                                   .underlineColor: NSColor(issue.blocking ? palette.negative : palette.caution)], range: issue.range)
        }
        storage.endEditing()
    }

    @MainActor final class Coordinator: NSObject, NSTextViewDelegate {
        var onChange: (String) -> Void = { _ in }
        var shownSelection: NSRange?
        func textDidChange(_ notification: Notification) {
            guard let view = notification.object as? NSTextView else { return }
            onChange(view.string)
        }
    }
}

// MARK: - Jump to any setting

struct JumpPalette: View {
    @ObservedObject var studio: ConfigStudioModel
    @State private var query = ""
    @State private var selection: String?
    @FocusState private var searching: Bool
    @Environment(\.dismiss) private var dismiss
    @Environment(\.palette) private var palette

    var body: some View {
        let results = studio.jumpResults(query)
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass").foregroundStyle(palette.secondary).accessibilityHidden(true)
                TextField("Jump to any setting", text: $query).textFieldStyle(.plain).font(.system(size: 16)).focused($searching)
                    .accessibilityLabel("Setting name or console name")
                    .onSubmit { go(results.first { $0.name == selection } ?? results.first) }
                    .arrowKeys { move($0, in: results) }
            }
            .padding(.horizontal, 14).frame(height: 46)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(palette.selectionStroke, lineWidth: 1))
            if results.isEmpty {
                Text("No setting matches. Try a word like “jump”, “turn” or “sound”.").font(.system(size: 13)).foregroundStyle(palette.secondary).padding(14)
                Spacer(minLength: 0)
            } else {
                ScrollViewReader { reader in
                    List(results, selection: $selection) { spec in
                        HStack(spacing: 12) {
                            Image(systemName: spec.group.symbol).foregroundStyle(palette.chipIcon).frame(width: 18).accessibilityHidden(true)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(spec.title).font(.system(size: 14)).foregroundStyle(palette.text)
                                Text(spec.name).font(StudioTypography.consoleName).foregroundStyle(palette.address)
                            }
                            Spacer(minLength: 8)
                            Text(studio.catalogTitle(spec.group)).font(.system(size: 12)).foregroundStyle(palette.secondary)
                            Text(studio.state(spec).display).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1).frame(width: 96, alignment: .trailing)
                        }
                        .padding(.vertical, 4)
                        .contentShape(Rectangle())
                        .onTapGesture(count: 2) { go(spec) }
                        .tag(spec.name)
                        .id(spec.name)
                        .accessibilityElement(children: .combine)
                        .accessibilityHint("Opens it in Expert")
                    }
                    .listStyle(.plain).scrollContentBackground(.hidden)
                    .onChange(of: selection) { name in if let name { reader.scrollTo(name) } }
                }
            }
            HStack(spacing: 14) {
                Text("↑ ↓ to move · Return to jump").font(.system(size: 11)).foregroundStyle(palette.secondary)
                Spacer()
                Button("Close") { dismiss() }.glassAction().controlSize(.small).keyboardShortcut(.cancelAction)
            }
        }
        .padding(12)
        .frame(width: 600, height: 440)
        .onAppear { searching = true; selection = results.first?.name }
        .onChange(of: query) { _ in selection = studio.jumpResults(query).first?.name }
    }
    private func move(_ direction: MoveCommandDirection, in results: [ConfigSettingSpec]) {
        guard !results.isEmpty else { return }
        let index = results.firstIndex { $0.name == selection } ?? -1
        let next = direction == .down ? min(results.count - 1, index + 1) : max(0, index - 1)
        selection = results[next].name
    }
    private func go(_ spec: ConfigSettingSpec?) {
        guard let spec else { return }
        studio.jump(to: spec)
        dismiss()
    }
}

private extension View {
    /// Up and down move the palette selection while typing (macOS 14+); on 13, Tab reaches the list.
    @ViewBuilder func arrowKeys(_ move: @escaping (MoveCommandDirection) -> Void) -> some View {
        if #available(macOS 14.0, *) {
            self.onKeyPress(.downArrow) { move(.down); return .handled }.onKeyPress(.upArrow) { move(.up); return .handled }
        } else { self }
    }
}
