import SwiftUI
import AppKit

/// A loadout's Check-up: what the latest recorded match measured (or that nothing was),
/// how the loadout reads against the chosen rules, and A1's suggestions with their exact
/// changes, reasons, observable effect, evidence and sources.
struct ConfigCheckupView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette
    init(studio: ConfigStudioModel, session: ConfigStudioSession) {
        self.studio = studio; self.session = session; checkup = studio.checkup
    }

    var body: some View {
        Group {
            if let error = checkup.loadError {
                Banner(.error, title: "Check-up isn’t available", message: error)
            } else if let report = checkup.report {
                content(report)
            } else {
                HStack(spacing: 10) { ProgressView().controlSize(.small); Text("Reviewing this loadout…").font(.system(size: 13)).foregroundStyle(palette.secondary) }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .onAppear { checkup.load() }
    }

    private func content(_ report: ConfigCheckupReport) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                RulesBar(studio: studio, checkup: checkup, report: report)
                MatchPanel(checkup: checkup)
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 190), spacing: 12)], spacing: 12) {
                    ForEach(checkup.gauges(report)) { DialCard(gauge: $0) }
                }
                if let reason = checkup.holdBack {
                    Banner(.tip, symbol: "exclamationmark.triangle", title: "Check-up can’t suggest changes for this loadout",
                           message: reason + " Settings that clearly break the chosen rules are still shown.")
                } else {
                    Banner(.neutral, title: "Advice from your settings and the rules you choose",
                           message: checkup.shownMatch == nil
                               ? "No match has been measured yet, so nothing here claims how smooth your game ran or how steady your connection was."
                               : "The cards above are measured from one match. Suggestions come from your settings and the rules, not from that match.")
                }
                ForEach(checkup.appliedHere()) { item in AppliedCard(studio: studio, checkup: checkup, item: item) }
                let suggestions = report.suggestions
                ForEach(Array(suggestions.enumerated()), id: \.element.id) { index, suggestion in
                    SuggestionCard(studio: studio, checkup: checkup, suggestion: suggestion, top: index == 0)
                }
                if suggestions.isEmpty && checkup.appliedHere().isEmpty && checkup.holdBack == nil {
                    let empty = checkup.standing(report).empty
                    StudioEmptyState(title: empty.title, message: empty.message)
                        .frame(maxWidth: .infinity)
                }
                let hidden = checkup.dismissedHere.count
                if hidden > 0 {
                    HStack(spacing: 10) {
                        Text(hidden == 1 ? "1 suggestion set aside for now." : "\(hidden) suggestions set aside for now.")
                            .font(.system(size: 13)).foregroundStyle(palette.secondary)
                        Button("Show Again", action: checkup.showAgain).glassAction().controlSize(.large)
                    }
                    .padding(.horizontal, 6)
                }
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(Array(report.limitations.enumerated()), id: \.offset) { _, line in
                        Text(line).font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    }
                }
                .padding(.horizontal, 6)
                // The footnotes wrap onto lines of their own rather than run past the window's edge.
                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 20) { footnotes }
                    VStack(alignment: .leading, spacing: 6) { footnotes }
                }
                .font(StudioTypography.help).foregroundStyle(palette.tertiary)
                .padding(.horizontal, 6)
            }
            .padding(.trailing, 4).padding(.bottom, 24)
        }
    }

    @ViewBuilder private var footnotes: some View {
        Label { Text("Locked settings are never suggested.") } icon: { Image(systemName: "lock").accessibilityHidden(true) }
            .fixedSize()
        Button { studio.tab = .history } label: {
            Label { Text("Every change can be undone in \(Text("History").foregroundColor(palette.link)).") } icon: {
                Image(systemName: "arrow.uturn.backward").accessibilityHidden(true)
            }
        }
        .buttonStyle(.plain).fixedSize()
        .accessibilityLabel("Every change can be undone in History")
        Button { studio.tab = .settings; studio.level = .expert } label: {
            Text("Want the exact settings? \(Text("Expert").foregroundColor(palette.link)) shows them behind each suggestion.")
        }
        .buttonStyle(.plain).fixedSize()
        .accessibilityLabel("Want the exact settings? Expert shows them behind each suggestion.")
    }
}

/// The rule sets to check against (several may apply), and how this loadout reads.
struct RulesBar: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var report: ConfigCheckupReport
    @Environment(\.palette) private var palette
    @State private var showingRules = false

    var body: some View {
        let standing = checkup.standing(report)
        VStack(alignment: .leading, spacing: 10) {
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 10) { RulesMenu(checkup: checkup); summary(standing); Spacer(minLength: 0) }
                VStack(alignment: .leading, spacing: 8) { RulesMenu(checkup: checkup); HStack(spacing: 10) { summary(standing) } }
            }
            if showingRules {
                ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 16, leading: 20, bottom: 16, trailing: 20)) {
                    VStack(alignment: .leading, spacing: 10) {
                        ForEach(report.policy, id: \.setting) { evaluation in
                            PolicyRow(studio: studio, checkup: checkup, evaluation: evaluation)
                        }
                        let related = checkup.rules?.relevantConflicts(rulesetIDs: report.rulesetIDs).filter { !Set($0.rulesetIDs).isSubset(of: report.rulesetIDs) } ?? []
                        ForEach(related) { conflict in
                            StudioStatusLabel(title: "Elsewhere: \(conflict.reason)", tone: .information)
                        }
                    }
                }
            }
        }
    }
}

extension RulesBar {
    @ViewBuilder func summary(_ standing: ConfigCheckupStanding) -> some View {
        if let status = standing.status {
            StudioStatusLabel(title: status.title, tone: status.tone).fixedSize(horizontal: false, vertical: true)
            Button(showingRules ? "Hide Rules" : "Show Rules") { showingRules.toggle() }.glassAction().controlSize(.large).fixedSize()
        }
    }
}

struct RulesMenu: View {
    @ObservedObject var checkup: ConfigCheckupModel
    @Environment(\.palette) private var palette
    /// VoiceOver reads a menu button by its picture's description; a name put on the menu alone is
    /// not read, and the bordered menu style reads no name at all. So the name rides on a picture
    /// with nothing in it, and the capsule is drawn here.
    private var name: NSImage {
        let image = NSImage(size: NSSize(width: 1, height: 1))
        image.accessibilityDescription = "Rules to check against: \(checkup.selectionTitle)"
        return image
    }
    var body: some View {
        HStack(spacing: 8) {
            Text("Rules").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary)
            // Several rule sets can apply at once, so a menu of ticks drawn as the capsule pop-up button.
            Menu {
                ForEach(checkup.available) { ruleset in
                    Button {
                        checkup.toggle(ruleset.id)
                    } label: {
                        if checkup.rulesetIDs.contains(ruleset.id) { Label(ruleset.title, systemImage: "checkmark") } else { Text(ruleset.title) }
                    }
                }
                if !checkup.rulesetIDs.isEmpty { Divider(); Button("No Rules") { checkup.rulesetIDs = [] } }
            } label: {
                Label {
                    Text(checkup.selectionTitle).lineLimit(1)
                        .accessibilityLabel("Rules to check against: \(checkup.selectionTitle)")
                } icon: {
                    Image(nsImage: name)
                }
            }
            .menuStyle(.borderlessButton).fixedSize()
            .padding(.leading, 8).padding(.trailing, 10).frame(height: 28)
            .background(palette.button, in: Capsule()).overlay(Capsule().strokeBorder(palette.buttonStroke, lineWidth: 1))
            .shapeAudit(.control, .capsule, "rules menu")
            .accessibilityLabel("Rules to check against: \(checkup.selectionTitle)")
        }
    }
}

/// The match the dials measure from (the latest, or one opened from Library or Home), said as a
/// match (it may have used another loadout), with Refresh. Reading happens only here and after a
/// game, never on a timer.
private struct MatchPanel: View {
    @ObservedObject var checkup: ConfigCheckupModel
    @Environment(\.palette) private var palette
    var body: some View {
        if let match = checkup.matchPresentation {
            ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
                ViewThatFits(in: .horizontal) {
                    HStack(alignment: .top, spacing: 16) { words(match); Spacer(minLength: 8); buttons }
                    VStack(alignment: .leading, spacing: 10) { words(match); buttons }
                }
            }
        } else {
            HStack(spacing: 10) {
                Text("Match summaries haven’t been read yet.").font(StudioTypography.help).foregroundStyle(palette.secondary)
                refresh
            }
            .padding(.horizontal, 6)
        }
    }
    private func words(_ match: ConfigMatchPresentation) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            if match.tone == .information {
                Text(match.title).font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
            } else {
                StudioStatusLabel(title: match.title, tone: match.tone)
            }
            Text(match.detail).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            // MARK: - ship-play
            if let summary = checkup.shownMatch?.freezeSummary {
                Text(summary).font(StudioTypography.help).foregroundStyle(palette.caution).fixedSize(horizontal: false, vertical: true)
                if let times = checkup.shownMatch?.freezeTimes {
                    Text(times).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
            }
            ForEach(match.lines, id: \.self) { line in
                Text(line).font(.typewriter(12)).foregroundStyle(palette.address).fixedSize(horizontal: false, vertical: true)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits([.isHeader, .isStaticText])
    }
    private var buttons: some View {
        HStack(spacing: 8) {
            if checkup.focusedMatchID != nil, checkup.shownMatch?.id != checkup.latestMatch?.id {
                // Both labels are one height, so the two capsules line up whatever their symbols.
                Button { checkup.focusedMatchID = nil } label: { Label("Show Last Match", systemImage: "clock.arrow.circlepath").frame(height: 16) }
                    .glassAction().actionControlSize().fixedSize()
                    .help("Measure from your most recent match instead")
            }
            refresh
        }
    }
    private var refresh: some View {
        Button { checkup.refreshMatches() } label: { Label("Refresh", systemImage: "arrow.clockwise").frame(height: 16) }
            .glassAction().actionControlSize().fixedSize()
            .help("Read the match summaries the game saved on this Mac again")
            .accessibilityLabel("Refresh match summaries")
    }
}

private struct PolicyRow: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var evaluation: ConfigRuleEvaluation
    @Environment(\.palette) private var palette
    var body: some View {
        let spec = studio.spec(named: evaluation.setting)
        let title = spec?.title ?? studio.catalog?.setting(named: evaluation.setting)?.title ?? evaluation.setting
        let value = spec.map { ConfigSettingState(spec: $0, raw: evaluation.value, locked: false, note: nil).display } ?? evaluation.value ?? "Not set"
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 10) {
                Text(title).font(.system(size: 13, weight: .medium)).foregroundStyle(palette.text)
                Text(value).font(.system(size: 13)).foregroundStyle(palette.secondary)
                Spacer(minLength: 8)
                StudioStatusLabel(title: evaluation.serverOwned && evaluation.status == .unknown ? "Set by the server" : evaluation.status.title, tone: evaluation.status.tone)
            }
            Text(evaluation.reason).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            if let source = evaluation.sources.first { SourceLine(checkup: checkup, source: source) }
        }
        .accessibilityElement(children: .combine)
    }
}

private struct SourceLine: View {
    @ObservedObject var checkup: ConfigCheckupModel
    var source: ConfigRuleSource
    @Environment(\.palette) private var palette
    var body: some View {
        let reference = checkup.sourceText(source)
        if let url = reference.url {
            Link(destination: url) { Text("Source: \(reference.text)").font(StudioTypography.help) }.foregroundStyle(palette.link)
        } else {
            Text("Source: \(reference.text)").font(.typewriter(12)).foregroundStyle(palette.address).lineLimit(1).truncationMode(.middle)
        }
    }
}

/// One of the four cards: what it measures, the outcome in a word as a value, its status, what the
/// player would notice, and the raw figures behind Show Details.
private struct DialCard: View {
    var gauge: ConfigCheckupGauge
    @Environment(\.palette) private var palette
    @State private var details = false
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            // The label, the value and the status each take a line, so a long status never meets the title.
            VStack(alignment: .leading, spacing: 8) {
                Text(gauge.area.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).lineLimit(1)
                Text(gauge.value).font(.value).foregroundStyle(palette.heading).lineLimit(1).minimumScaleFactor(0.8)
                StudioStatusLabel(title: gauge.status, tone: gauge.tone)
                Text(gauge.detail).font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("\(gauge.area.title): \(gauge.value == "—" ? "no value" : gauge.value), \(gauge.status). \(gauge.detail)")
            .accessibilityAddTraits(.isStaticText)
            if !gauge.figures.isEmpty {
                if details {
                    Text(gauge.figures).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                        .textSelection(.enabled)
                }
                Button(details ? "Hide Details" : "Show Details") { details.toggle() }.quietAction()
                    .padding(.horizontal, -6)
                    .accessibilityLabel(details ? "Hide the \(gauge.area.title.lowercased()) figures" : "Show the \(gauge.area.title.lowercased()) figures")
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, 16).padding(.horizontal, 20)
        .frame(maxWidth: .infinity, minHeight: 132, alignment: .topLeading)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .accessibilityElement(children: .contain)
    }
}

/// A suggestion: the outcome as its title, why, what you'll notice, the evidence and its source.
/// One main button per card: Apply, beside Not Now and Show Me as quiet buttons. Only the top card's
/// Apply is gold.
private struct SuggestionCard: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var suggestion: ConfigCheckupSuggestion
    var top: Bool
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let showing = checkup.showing.contains(suggestion.id)
        VStack(alignment: .leading, spacing: 8) {
            ViewThatFits(in: .horizontal) {
                HStack(alignment: .center, spacing: 8) { heading; Spacer(minLength: 12); buttons(showing) }
                VStack(alignment: .leading, spacing: 10) { heading; buttons(showing) }
            }
            Text(suggestion.why).font(.system(size: 13)).foregroundStyle(palette.secondary).lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true)
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Image(systemName: "eye").font(.system(size: 13)).foregroundStyle(palette.accent).accessibilityHidden(true)
                Text("\(Text("You’ll notice: ").fontWeight(.semibold))\(Text(suggestion.notice))").font(.system(size: 13)).foregroundStyle(palette.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if showing {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(suggestion.changes, id: \.setting) { change in
                        HStack(spacing: 8) {
                            Text("•").foregroundStyle(palette.tertiary).accessibilityHidden(true)
                            Text(checkup.changeText(change)).font(.system(size: 13)).monospacedDigit().foregroundStyle(palette.text)
                            Spacer(minLength: 4)
                            if let spec = studio.spec(named: change.setting) {
                                Button("Open in Expert") { studio.jump(to: spec) }.quietAction().fixedSize()
                            }
                        }
                    }
                }
                .padding(.vertical, 12).padding(.horizontal, 16)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                .padding(.vertical, 4)
            }
            Text(checkup.evidenceText(suggestion)).font(StudioTypography.help).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            ForEach(Array(suggestion.sources.prefix(2).enumerated()), id: \.offset) { _, source in SourceLine(checkup: checkup, source: source) }
        }
        .padding(.vertical, 16).padding(.horizontal, 20)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous).strokeBorder(top ? palette.selectionStroke : palette.panelStroke, lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityLabel(suggestion.title)
    }

    private var heading: some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(suggestion.title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                .accessibilityAddTraits(.isHeader)
            if top { Badge("Top suggestion", tone: .accent) }
        }
    }

    private func buttons(_ showing: Bool) -> some View {
        HStack(spacing: 8) {
            Button("Not Now") { checkup.notNow(suggestion.id) }.quietAction().fixedSize()
            Button(showing ? "Hide" : "Show Me") {
                if showing { checkup.showing.remove(suggestion.id) } else { checkup.showing.insert(suggestion.id) }
            }
            .quietAction().fixedSize()
            Group {
                if top {
                    Button("Apply") { checkup.apply(suggestion.id, undo: undo) }.prominentAction()
                } else {
                    Button("Apply") { checkup.apply(suggestion.id, undo: undo) }.glassAction()
                }
            }
            .actionControlSize().fixedSize().disabled(studio.isReadOnly)
            .shapeAudit(.field, .capsule, "checkup apply \(suggestion.id)")
        }
    }
}

private struct AppliedCard: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var checkup: ConfigCheckupModel
    var item: ConfigAppliedSuggestion
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "checkmark").font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.positive).accessibilityHidden(true)
            Text("\(Text("Applied: ").fontWeight(.semibold))\(item.suggestion.title). Saved to History.").font(.system(size: 14)).foregroundStyle(palette.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            Button { checkup.undoApplied(item.id) } label: { Label("Undo", systemImage: "arrow.uturn.backward") }
                .glassAction().actionControlSize().fixedSize().disabled(studio.isReadOnly)
                .shapeAudit(.field, .capsule, "checkup undo \(item.id)")
        }
        .padding(.vertical, 16).padding(.horizontal, 20)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.panelRadius, style: .continuous).strokeBorder(palette.panelStroke, lineWidth: 1))
        .accessibilityElement(children: .contain)
    }
}
