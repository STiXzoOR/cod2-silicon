import SwiftUI

/// A loadout's snapshots, newest first, with “now” at the top. Selecting a snapshot only
/// previews what differs; Restore saves the current state as a snapshot first.
struct ConfigHistoryView: View {
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var session: ConfigStudioSession
    @Environment(\.palette) private var palette

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
            Rectangle().fill(palette.separator.opacity(1.5)).frame(width: 2).padding(.leading, 27).padding(.vertical, 26).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                SnapshotRow(kind: .now, title: "How \(session.current?.profile.displayName ?? "this loadout") is set up now",
                            detail: session.current?.profile.isActive == true ? "Everything the next game will use." : "Everything the game uses when you choose this loadout.",
                            source: "Now", time: session.history.first.map { StencilDate.dayAndTime($0.date) },
                            selected: studio.snapshotID == nil) { studio.snapshotID = nil }
                ForEach(entries) { entry in
                    SnapshotRow(kind: .source(entry.source), title: studio.title(of: entry), detail: ConfigDiff.sentence(studio.changes(in: entry)),
                                source: entry.source.title, time: StencilDate.dayAndTime(entry.date), selected: studio.snapshotID == entry.id) {
                        studio.snapshotID = entry.id
                    }
                }
                if entries.isEmpty {
                    Text(studio.historyFilter == .all ? "No snapshots yet. Every change you save here adds one."
                                                      : "No snapshots from \(studio.historyFilter.title.lowercased()) yet.")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).padding(.leading, 56).padding(.vertical, 12)
                }
            }
        }
        .focusable()
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
        Text("CoD2 Silicon keeps the last \(ConfigHistory.retention) snapshots of each loadout. This one has \(session.history.count) of \(ConfigHistory.retention). Older ones are removed on their own.")
            .font(StudioTypography.help).foregroundStyle(palette.tertiary).padding(.horizontal, 6).fixedSize(horizontal: false, vertical: true)
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
    var action: () -> Void
    @Environment(\.palette) private var palette

    var body: some View {
        Button(action: action) {
            HStack(alignment: .top, spacing: 14) {
                Image(systemName: kind.symbol).font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(kind == .now ? palette.prominentText : palette.chipIcon)
                    .frame(width: 30, height: 30)
                    .background(Circle().fill(kind == .now ? palette.accent : (palette.dark ? Palette.hex(0x1d2119) : palette.crate)))
                    .overlay(Circle().strokeBorder(kind == .now ? palette.accent : (palette.dark ? Color.white.opacity(0.14) : palette.wellStroke), lineWidth: 1))
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 5) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.secondary).lineSpacing(2).fixedSize(horizontal: false, vertical: true)
                    HStack(spacing: 8) {
                        Text(source).font(.system(size: 11, weight: .semibold)).foregroundStyle(palette.chipText)
                            .padding(.horizontal, 8).frame(height: 20)
                            .overlay(Capsule().strokeBorder(palette.dark ? Color.white.opacity(0.14) : palette.wellStroke, lineWidth: 1))
                        if let time { Text(time).font(.typewriter(12)).foregroundStyle(palette.address) }
                    }
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 12).padding(.leading, 12).padding(.trailing, 14)
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(selected ? palette.selectionStroke : .clear, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
        .buttonStyle(RowButtonStyle(radius: 16))
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
                VStack(spacing: 10) {
                    Text("THIS IS NOW").font(.stencil(24)).tracking(2.9).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary).accessibilityAddTraits(.isHeader)
                    Text("Pick an earlier snapshot to see what has changed since then, or to go back to it.")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).lineSpacing(2)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .frame(maxWidth: .infinity).padding(.vertical, 60).padding(.horizontal, 10)
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Compare with now")
    }

    @ViewBuilder private func past(_ entry: ConfigHistoryEntry) -> some View {
        let changes = studio.changesSince(entry)
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text("COMPARE WITH NOW").font(.system(size: 11, weight: .semibold)).tracking(0.88).foregroundStyle(palette.tertiary)
                Text(studio.title(of: entry)).font(.system(size: 16, weight: .bold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isHeader)
                Text(StencilDate.dayAndTime(entry.date)).font(.typewriter(12)).foregroundStyle(palette.address)
            }
            Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 0) {
                GridRow {
                    Text("SETTING").frame(maxWidth: .infinity, alignment: .leading); Text("THEN"); Text(""); Text("NOW")
                }
                .font(.system(size: 11, weight: .semibold)).tracking(0.44).foregroundStyle(palette.tertiary).padding(.bottom, 4)
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
                Label("Restored. Your previous state is saved as a snapshot.", systemImage: "checkmark.circle")
                    .font(.system(size: 13)).foregroundStyle(palette.positive)
                    .padding(.vertical, 10).padding(.horizontal, 12).frame(maxWidth: .infinity, alignment: .leading)
                    .background(palette.positive.opacity(0.08), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(palette.positive.opacity(0.35), lineWidth: 1))
            }
            Text("Restoring saves how things are now as a new snapshot first, so you can always come back.")
                .font(StudioTypography.help).foregroundStyle(palette.secondary).lineSpacing(2).fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 8) {
                Button { studio.restore(entry, undo: undo) } label: { Text("Restore This Snapshot").frame(maxWidth: .infinity) }
                    .prominentAction().actionControlSize()
                    .disabled(studio.isReadOnly || changes.isEmpty)
                Menu {
                    Button("Restore the State Before This Change") { studio.restore(entry, revision: .before, undo: undo) }
                        .disabled(studio.isReadOnly || entry.beforeData == entry.data && entry.beforeMetadata == entry.metadata)
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
