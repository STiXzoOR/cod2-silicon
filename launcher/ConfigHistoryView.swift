import SwiftUI

/// A loadout's snapshots, newest first, with “now” at the top. Selecting a snapshot only
/// previews what differs; Restore saves the current state as a snapshot first.
struct ConfigHistoryView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette
    /// The timeline shows focus the way a native list does, on the selected row, not as a ring.
    @FocusState private var focused: Bool

    var body: some View {
        GeometryReader { proxy in
            if proxy.size.width >= 760 {
                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 10) {
                        ScrollView { timeline.padding(.trailing, 4) }.scrollIndicators(.automatic)
                        footnote
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                    SnapshotInspector(studio: studio, session: session).frame(width: 372).frame(maxHeight: .infinity, alignment: .top)
                }
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 14) {
                        timeline
                        footnote
                        SnapshotInspector(studio: studio, session: session).frame(minHeight: 320)
                    }
                    .padding(.trailing, 4)
                }
            }
        }
    }

    private var timeline: some View {
        let entries = studio.visibleHistory
        return ZStack(alignment: .topLeading) {
            // The rail runs through the round nodes, which sit 10 points into each row.
            Rectangle().fill(palette.separator).frame(width: 2).padding(.leading, 24).padding(.vertical, 26).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 4) {
                SnapshotRow(kind: .now, title: "How \(session.current?.profile.displayName ?? "this loadout") is set up now",
                            detail: session.current?.profile.isActive == true ? "Everything the next game will use." : "Everything the game uses when you choose this loadout.",
                            source: "Now", time: session.history.first.map { HistoryDate.text($0.date) },
                            selected: studio.snapshotID == nil, focused: focused) { studio.snapshotID = nil }
                ForEach(entries) { entry in
                    SnapshotRow(kind: .source(entry.source), title: studio.title(of: entry), detail: ConfigDiff.sentence(studio.changes(in: entry)),
                                source: entry.source.title, time: HistoryDate.text(entry.date), selected: studio.snapshotID == entry.id, focused: focused) {
                        studio.snapshotID = entry.id
                    }
                }
                if entries.isEmpty {
                    Text(studio.historyFilter == .all ? "No snapshots yet. Every change you save here adds one."
                                                      : "No snapshots from \(studio.historyFilter.title.lowercased()) yet.")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).padding(.leading, 50).padding(.vertical, 12)
                }
            }
        }
        .focusable(interactions: .edit).focused($focused).focusEffectDisabled()
        .onMoveCommand { direction in
            let ids: [String?] = [nil] + entries.map(\.id)
            guard let index = ids.firstIndex(of: studio.snapshotID) else { return }
            let next = direction == .down ? min(ids.count - 1, index + 1) : direction == .up ? max(0, index - 1) : index
            studio.snapshotID = ids[next]
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Snapshots")
    }

    private var footnote: some View {
        Text("CoD2 Unbound keeps the last \(ConfigHistory.retention) snapshots of each loadout. This one has \(session.history.count) of \(ConfigHistory.retention). Older ones are removed on their own.")
            .font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary).padding(.horizontal, 4).fixedSize(horizontal: false, vertical: true)
    }
}

private enum SnapshotKind: Equatable {
    case now, source(ConfigHistorySource)
    var symbol: String {
        switch self {
        case .now: "checkmark"
        case .source(let source):
            switch source {
            case .you: "person"; case .import: "square.and.arrow.down"; case .checkup: "waveform.path.ecg"
            case .gameSession: "gamecontroller"; case .restore: "arrow.uturn.backward"; case .migration: "shippingbox"
            }
        }
    }
}

private struct SnapshotRow: View {
    var kind: SnapshotKind
    var title: String
    var detail: String
    var source: String
    var time: String?
    var selected: Bool
    var focused = false
    var action: () -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        Button(action: action) {
            HStack(alignment: .top, spacing: 16) {
                // “Now” is the selected node; the others are quiet panels with their source's symbol.
                Image(systemName: kind.symbol).font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(kind == .now ? palette.accent : palette.chipIcon)
                    .frame(width: 30, height: 30)
                    .background(Circle().fill(palette.panel))
                    .background(Circle().fill(kind == .now ? palette.selection : .clear))
                    .overlay(Circle().strokeBorder(kind == .now ? palette.selectionStroke : palette.wellStroke, lineWidth: 1))
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 4) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.tertiary).lineSpacing(2).fixedSize(horizontal: false, vertical: true)
                    HStack(spacing: 8) {
                        Badge(source)
                        if let time { Text(time).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary) }
                    }
                    .padding(.top, 2)
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 10).padding(.leading, 10).padding(.trailing, 20)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous).strokeBorder(selected ? (focused ? palette.accent : palette.selectionStroke) : .clear, lineWidth: selected && focused ? 2 : 1))
            .contentShape(RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: RowMetrics.radius))
        .accessibilityLabel("\(title), \(source)\(time.map { ", \($0)" } ?? "")")
        .accessibilityHint(detail)
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

private struct SnapshotInspector: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.undoManager) private var undo
    @Environment(\.palette) private var palette

    var body: some View {
        let entry = studio.snapshotID.flatMap { id in session.history.first { $0.id == id } }
        ContentPanel(radius: StudioMetrics.panelRadius, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
            if let entry {
                past(entry)
            } else {
                // An empty-state heading: the shared stencil heading and one line.
                EmptyState("This is now", message: "Pick an earlier snapshot to see what has changed since then, or to go back to it.")
                    .frame(maxWidth: .infinity, maxHeight: .infinity).padding(.vertical, 20).padding(.horizontal, 8)
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Compare with now")
    }

    @ViewBuilder private func past(_ entry: ConfigHistoryEntry) -> some View {
        let changes = studio.changesSince(entry)
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                GroupLabel("Compare with now", header: false)
                Text(studio.title(of: entry)).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isHeader)
                Text(HistoryDate.text(entry.date)).font(StudioTypography.help).monospacedDigit().foregroundStyle(palette.tertiary)
            }
            Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 0) {
                GridRow {
                    Text("Setting").frame(maxWidth: .infinity, alignment: .leading); Text("Then"); Text(""); Text("Now")
                }
                .font(.groupLabel).foregroundStyle(palette.tertiary).padding(.bottom, 8)
                .accessibilityHidden(true)
                if changes.isEmpty {
                    Divider().gridCellUnsizedAxes(.horizontal)
                    GridRow { Text("No differences").foregroundStyle(palette.text); Text("—"); Text(""); Text("—") }
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).padding(.vertical, 9)
                }
                ForEach(changes) { change in
                    Rectangle().fill(palette.separator).frame(height: 1).gridCellUnsizedAxes(.horizontal)
                    // Grid rows pass modifiers to every cell, so the first cell reads the whole row.
                    GridRow(alignment: .firstTextBaseline) {
                        Text(change.label).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .accessibilityLabel(change.then.isEmpty ? "\(change.label), \(change.now)" : "\(change.label), then \(change.then), now \(change.now)")
                        Text(change.then.isEmpty ? "—" : change.then).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                            .accessibilityHidden(true)
                        Text("→").foregroundStyle(palette.tertiary).accessibilityHidden(true)
                        Text(change.now).fontWeight(.semibold).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                            .accessibilityHidden(true)
                    }
                    .font(.system(size: 13)).monospacedDigit().padding(.vertical, 9)
                }
            }
            Spacer(minLength: 0)
            if studio.restoredSnapshotID == entry.id {
                Label("Restored. Your previous state is saved as a snapshot.", systemImage: "checkmark")
                    .font(.system(size: 12, weight: .medium)).foregroundStyle(palette.positive).fixedSize(horizontal: false, vertical: true)
                    .padding(.vertical, 12).padding(.horizontal, 16).frame(maxWidth: .infinity, alignment: .leading)
                    .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            }
            Text("Restoring saves how things are now as a new snapshot first, so you can always come back.")
                .font(StudioTypography.help).foregroundStyle(palette.tertiary).lineSpacing(2).fixedSize(horizontal: false, vertical: true)
            // Restore on the right, More on the left as on the board; Restore stays first in the focus order.
            // Both compare the whole revision, advanced-bind companion included, with now.
            Button("Restore This Snapshot") { studio.restore(entry, undo: undo) }
                .prominentAction().actionControlSize().fixedSize()
                .disabled(!studio.canRestore(entry, revision: .after))
                .frame(maxWidth: .infinity, alignment: .trailing)
                .overlay(alignment: .leading) {
                    Menu {
                        Button("Restore the State Before This Change") { studio.restore(entry, revision: .before, undo: undo) }
                            .disabled(!studio.canRestore(entry, revision: .before))
                    } label: { Image(systemName: "ellipsis") }
                        .menuStyle(.borderlessButton).menuIndicator(.hidden).fixedSize()
                        .frame(width: ControlMetrics.action, height: ControlMetrics.action)
                        .glassSurface(Circle(), interactive: true)
                        .accessibilityLabel("More actions for this snapshot")
                }
        }
        .frame(maxHeight: .infinity, alignment: .top)
    }
}

/// A snapshot's time in a sentence-case line (“6 Oct at 18:42”), as the History board writes it.
private enum HistoryDate {
    static func text(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_GB")
        formatter.dateFormat = "d MMM 'at' HH:mm"
        return formatter.string(from: date)
    }
}
