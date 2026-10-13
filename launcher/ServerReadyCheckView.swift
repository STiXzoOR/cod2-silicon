import SwiftUI
import AppKit

/// The ready check above Deploy in the server inspector: does the loadout this game would use
/// suit this server, which loadout to use for this connection, and a fix that lasts for this
/// connection only. Unknown rules get general advice, never an all-clear.
struct ReadyCheckCard: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var connection: ConfigServerConnection
    var server: GameServer
    @StateObject private var check = ServerReadyCheckModel()
    @State private var showDetails = false
    @State private var confirmingFix = false
    @Environment(\.palette) private var palette
    private let radius: CGFloat = 16

    private struct Key: Equatable {
        var address: String
        var chosen: String?
        var overrides: [String: String]
        var active: String?
        var observed: Date?
        var catalog: Bool
    }

    var body: some View {
        let address = server.address
        let mine = Self.owns(connection, address)
        let chosen = mine ? connection.profileID : nil
        let overrides = mine ? connection.overrides : [:]
        let loadout = name(chosen ?? studio.activeProfile?.id)
        let presentation = check.presentation(model, loadout: loadout)
        VStack(alignment: .leading, spacing: 10) {
            if !overrides.isEmpty {
                fixedBanner(overrides, loadout: loadout, address: address)
            } else {
                if let chosen { usingBanner(name(chosen), address: address) }
                if check.evaluated { evaluation(presentation, address: address) }
            }
            loadoutPicker(chosen: chosen, address: address)
        }
        .task(id: Key(address: address, chosen: chosen, overrides: overrides, active: studio.activeProfile?.id,
                      observed: server.observedAt, catalog: studio.catalog != nil)) {
            check.evaluate(model, address: address)
        }
        .onAppear { studio.loadCatalog() }
        .confirmationDialog("Fix for this server only?", isPresented: $confirmingFix) {
            Button("Fix for This Server Only") { check.fix(model, address: address) }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("While you play on \(QuakeColors.plain(server.name)), the game uses \(presentation.fixes.joined(separator: ", ")). Your “\(loadout)” loadout doesn’t change. Anything you change in the game during this connection is discarded when you leave.")
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Ready check")
    }

    /// “Deploy with Scrim” when this connection uses another loadout.
    @MainActor static func deployTitle(_ model: LauncherModel, _ server: GameServer) -> String {
        guard owns(model.serverConnection, server.address), let id = model.serverConnection.profileID,
              let profile = model.studio.profiles.first(where: { $0.id == id }) else { return "Deploy" }
        return "Deploy with \(profile.displayName)"
    }
    @MainActor private static func owns(_ connection: ConfigServerConnection, _ address: String) -> Bool {
        guard let mine = connection.address else { return false }
        return (try? LaunchLink.canonicalAddress(address)) == mine
    }
    private func name(_ id: String?) -> String {
        guard let id else { return "current" }
        return studio.profiles.first { $0.id == id }?.displayName ?? id
    }
    private var locked: Bool { model.gameRunning || studio.isReadOnly }

    // MARK: Evaluation

    @ViewBuilder private func evaluation(_ p: ReadyCheckPresentation, address: String) -> some View {
        if p.kind == .matches {
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    StudioStatusLabel(title: p.title, tone: .positive)
                    Spacer(minLength: 4)
                    detailsButton
                }
                if showDetails { detailsList(p) }
            }
            .padding(.horizontal, 8)
        } else {
            VStack(alignment: .leading, spacing: 8) {
                Label { Text(p.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.heading) }
                    icon: { Image(systemName: p.tone.symbol).foregroundStyle(p.tone.color(in: palette)) }
                    .accessibilityAddTraits(.isHeader)
                if let message = p.message {
                    Text(message).font(StudioTypography.help).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
                }
                ForEach(p.lines) { line in
                    HStack(alignment: .top, spacing: 8) {
                        Image(systemName: line.tone.symbol).font(.system(size: 11)).foregroundStyle(line.tone.color(in: palette)).padding(.top, 1)
                            .accessibilityHidden(true)
                        Text(line.text).font(StudioTypography.help)
                            .foregroundStyle(line.tone == .caution ? palette.text : (palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary))
                    }
                    .accessibilityElement(children: .combine)
                }
                if showDetails { detailsList(p) }
                HStack(spacing: 6) {
                    if p.canFix {
                        Button("Fix for This Server Only") { confirmingFix = true }.glassAction().controlSize(.large).disabled(locked)
                            .help(locked ? "Quit the game first" : "Use these values while you play here only. Your loadout doesn’t change.")
                    }
                    if p.kind == .unknown, !model.snapshot {
                        Button(check.checking ? "Checking…" : "Check Again") { Task { await check.checkAgain(model, address: address) } }
                            .glassAction().controlSize(.large).disabled(check.checking)
                            .help("Ask this server for its current rules")
                    }
                    if !p.exact.isEmpty { detailsButton }
                }
                .padding(.top, 2)
            }
            .fixedSize(horizontal: false, vertical: true)
            .padding(.vertical, 12).padding(.horizontal, 14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(fill(p.kind), in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).stroke(edge(p.kind), lineWidth: 1))
        }
    }

    private var detailsButton: some View {
        Button(showDetails ? "Hide Details" : "Details") { showDetails.toggle() }
            .buttonStyle(.plain).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.link)
            .accessibilityHint("Shows the rules that were checked and what isn’t checked")
    }

    private func detailsList(_ p: ReadyCheckPresentation) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            if p.kind == .matches, let message = p.message {
                Text(message).font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            ForEach(Array(p.exact.enumerated()), id: \.offset) { _, line in
                Text(line).font(.typewriter(11)).foregroundStyle(palette.address).textSelection(.enabled)
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }

    private func fill(_ kind: ReadyCheckPresentation.Kind) -> Color {
        switch kind {
        case .needsChange: palette.caution.opacity(0.07)
        default: palette.dark ? Palette.hex(0x11140f) : palette.crate
        }
    }
    private func edge(_ kind: ReadyCheckPresentation.Kind) -> Color {
        switch kind {
        case .needsChange: palette.caution.opacity(0.42)
        default: palette.dark ? Color.white.opacity(0.09) : palette.wellStroke
        }
    }

    // MARK: Choices for this connection

    private func resolved<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        content()
            .padding(.vertical, 12).padding(.horizontal, 14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.positive.opacity(0.07), in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).stroke(palette.positive.opacity(0.38), lineWidth: 1))
    }

    private func fixedBanner(_ overrides: [String: String], loadout: String, address: String) -> some View {
        let values = overrides.keys.sorted().map { setting -> String in
            let spec = studio.spec(named: setting)
            return "\(spec?.title ?? "A setting") \(spec.map { ConfigValues.display($0, overrides[setting]) } ?? overrides[setting] ?? "")"
        }
        return resolved {
            HStack(alignment: .top, spacing: 10) {
                Image(systemName: "checkmark.circle").foregroundStyle(palette.positive).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 4) {
                    Text("Fixed for this server only").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.heading)
                    Text("\(values.joined(separator: ", ")) while you play here. Your “\(loadout)” loadout is unchanged.")
                        .font(StudioTypography.help).foregroundStyle(palette.text)
                    Text("Anything you change in the game during this connection is discarded when you leave.")
                        .font(StudioTypography.help).foregroundStyle(palette.secondary)
                }
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityElement(children: .combine)
                Spacer(minLength: 4)
                Button("Undo") { check.undo(model, address: address) }.glassAction().controlSize(.large)
                    .accessibilityLabel("Undo the fix for this server")
            }
        }
    }

    private func usingBanner(_ chosen: String, address: String) -> some View {
        resolved {
            HStack(alignment: .top, spacing: 10) {
                Image(systemName: "checkmark.circle").foregroundStyle(palette.positive).accessibilityHidden(true)
                Text("Using your “\(chosen)” loadout for this game. \(studio.activeProfile?.displayName ?? "Your active loadout") stays as it is.")
                    .font(StudioTypography.help).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 4)
                Button("Undo") { check.useLoadout(nil, model: model, address: address) }.glassAction().controlSize(.large)
                    .accessibilityLabel("Go back to your active loadout for this game")
            }
        }
    }

    @ViewBuilder private func loadoutPicker(chosen: String?, address: String) -> some View {
        if studio.profiles.count > 1, let active = studio.activeProfile?.id {
            HStack(spacing: 8) {
                Text("Loadout for this game").font(StudioTypography.help).foregroundStyle(palette.secondary)
                Spacer(minLength: 4)
                Picker("Loadout for this game", selection: Binding(get: { chosen ?? active }, set: { id in
                    check.useLoadout(id == active ? nil : id, model: model, address: address)
                })) {
                    ForEach(studio.profiles) { profile in
                        Text(profile.isActive ? "\(profile.displayName) (in use)" : profile.displayName).tag(profile.id)
                    }
                }
                .labelsHidden().fixedSize().disabled(locked)
                .help(locked ? "Quit the game first" : "Use another of your loadouts for this game only. It doesn’t change which loadout is in use.")
            }
            .padding(.horizontal, 8)
        }
    }
}
