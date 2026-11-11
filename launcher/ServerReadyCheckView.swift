import SwiftUI
import AppKit

/// The ready check in the details panel (Servers and ServersReady boards): a quiet line when the
/// loadout suits the server; a card that says what must change, with Fix for This Server Only and
/// another loadout that already fits; general advice when the server's rules are unknown (never an
/// all-clear). A fix or another loadout lasts for this connection only.
struct ReadyCheckCard: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @ObservedObject var connection: ConfigServerConnection
    @ObservedObject var check: ServerReadyCheckModel
    var server: GameServer
    @State private var showDetails = false
    @State private var confirmingFix = false
    @State private var fitting: ConfigProfile?
    @Environment(\.palette) private var palette
    private let radius: CGFloat = 16

    private struct Key: Equatable {
        var address: String
        var chosen: String?
        var overrides: [String: String]
        var active: String?
        var observed: Date?
        var catalog: Bool
        var profiles: [String]
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
                if check.evaluated && check.address == address { evaluation(presentation, loadout: loadout, chosen: chosen, address: address) }
            }
        }
        .task(id: Key(address: address, chosen: chosen, overrides: overrides, active: studio.activeProfile?.id,
                      observed: server.observedAt, catalog: studio.catalog != nil, profiles: studio.profiles.map(\.id))) {
            check.evaluate(model, address: address)
            let current = chosen ?? studio.activeProfile?.id
            fitting = check.presentation(model, loadout: loadout).kind == .needsChange ? model.fittingLoadout(for: address, except: current) : nil
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

    /// “Launch with Scrim” when this connection uses another loadout.
    @MainActor static func launchTitle(_ model: LauncherModel, _ server: GameServer) -> String {
        guard owns(model.serverConnection, server.address), let id = model.serverConnection.profileID,
              let profile = model.studio.profiles.first(where: { $0.id == id }) else { return "Launch" }
        return "Launch with \(profile.displayName)"
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

    @ViewBuilder private func evaluation(_ p: ReadyCheckPresentation, loadout: String, chosen: String?, address: String) -> some View {
        switch p.kind {
        case .matches, .serverSets:
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    StudioStatusLabel(title: "Your “\(loadout)” loadout is ready for this server", tone: .positive)
                    Spacer(minLength: 4)
                    detailsButton
                }
                if p.kind == .serverSets {
                    Text("This server sets some of your settings while you play here. Your loadout doesn’t change.")
                        .font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
                if showDetails { detailsList(p) }
            }
            .padding(.horizontal, 6)
        case .needsChange:
            VStack(alignment: .leading, spacing: 8) {
                Label { Text(p.title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.caution) }
                    icon: { Image(systemName: "exclamationmark.triangle").foregroundStyle(palette.caution) }
                    .accessibilityAddTraits(.isHeader)
                ForEach(p.lines.filter { $0.tone == .caution }) { line in
                    Text(line.text).font(.system(size: 12)).foregroundStyle(palette.text).lineSpacing(1.5)
                        .fixedSize(horizontal: false, vertical: true)
                }
                if showDetails { detailsList(p) }
                HStack(spacing: 6) {
                    if p.canFix {
                        Button("Fix for This Server Only") { confirmingFix = true }.glassAction().actionControlSize().disabled(locked)
                            .help(locked ? "Quit the game first" : "Use the server’s values while you play here only. Your loadout doesn’t change.")
                    }
                    if let fitting {
                        Button("Use “\(fitting.displayName)”") { check.useLoadout(fitting.id, model: model, address: address) }.quietAction().disabled(locked)
                            .help("Your “\(fitting.displayName)” loadout already suits this server. Use it for this game only.")
                    }
                    Spacer(minLength: 0)
                    if !p.exact.isEmpty { detailsButton }
                }
            }
            .padding(.vertical, 12).padding(.horizontal, 14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.selection, in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).strokeBorder(palette.selectionStroke, lineWidth: 1))
        case .unknown, .unavailable:
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    StudioStatusLabel(title: p.title, tone: .unknown)
                    Spacer(minLength: 4)
                    if p.kind == .unknown, !model.snapshot {
                        Button(check.checking ? "Checking…" : "Check Again") { Task { await check.checkAgain(model, address: address) } }
                            .quietAction().disabled(check.checking)
                            .help("Ask this server for its current rules")
                    }
                }
                if let message = p.message {
                    Text(message).font(StudioTypography.help).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(.horizontal, 6)
        }
    }

    private var detailsButton: some View {
        Button(showDetails ? "Hide Details" : "Details") { showDetails.toggle() }
            .quietAction().font(.system(size: 12, weight: .semibold))
            .accessibilityHint("Shows the rules that were checked and what isn’t checked")
    }

    private func detailsList(_ p: ReadyCheckPresentation) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            if let message = p.message {
                Text(message).font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            ForEach(p.lines.filter { $0.tone != .caution }) { line in
                Text(line.text).font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
            ForEach(Array(p.exact.enumerated()), id: \.offset) { _, line in
                Text(line).font(.typewriter(11)).foregroundStyle(palette.address).textSelection(.enabled)
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }

    // MARK: Choices for this connection

    private func resolved<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        content()
            .padding(.vertical, 12).padding(.horizontal, 14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(palette.positive.opacity(0.07), in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).strokeBorder(palette.positive.opacity(0.38), lineWidth: 1))
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
                Button("Undo") { check.undo(model, address: address) }.glassAction().actionControlSize()
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
                Button("Undo") { check.useLoadout(nil, model: model, address: address) }.glassAction().actionControlSize()
                    .accessibilityLabel("Go back to your active loadout for this game")
            }
        }
    }
}
