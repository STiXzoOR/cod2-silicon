import SwiftUI
import AppKit

// Owner: ship-servers. The details panel beside the list (Servers board): the map art, map and mode,
// the name, the address with Copy, four facts, the ready check, the roster, and at the bottom, always
// visible, Launch (“Launch Anyway” when the ready check says the loadout needs a change) with the star.

struct ServerDetailsPanel: View {
    @ObservedObject var model: LauncherModel
    var server: GameServer?
    var image: NSImage?
    var compact = false
    @StateObject private var check = ServerReadyCheckModel()
    @Environment(\.palette) private var palette
    private let radius: CGFloat = 26, inset: CGFloat = 14
    var body: some View {
        VStack(spacing: 12) {
            if let server {
                ScrollView {
                    content(server).padding(.bottom, 8)
                }
                .scrollIndicators(.automatic)
                .mask(VStack(spacing: 0) { Color.black; LinearGradient(colors: [.black, .clear], startPoint: .top, endPoint: .bottom).frame(height: 24) })
                .id(server.address)
                actions(server)
            } else {
                Spacer(minLength: 0)
                VStack(spacing: 12) {
                    Image(systemName: "dot.radiowaves.left.and.right").font(.system(size: 28, weight: .light)).foregroundStyle(palette.tertiary)
                        .accessibilityHidden(true)
                    Text("Select a server to see its map, rules and who’s playing.").font(.system(size: 13)).foregroundStyle(palette.secondary)
                        .multilineTextAlignment(.center).fixedSize(horizontal: false, vertical: true)
                }
                .padding(.horizontal, 24)
                .frame(maxWidth: .infinity)
                .accessibilityElement(children: .combine)
                .accessibilityAddTraits(.isStaticText)
                Spacer(minLength: 0)
            }
        }
        .padding(inset)
        .frame(maxHeight: .infinity, alignment: .top)
        .contentSurface(RoundedRectangle(cornerRadius: radius, style: .continuous))
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Server details")
    }

    @ViewBuilder private func content(_ server: GameServer) -> some View {
        let facts = ServerFacts(server)
        let artRadius = Concentric.radius(in: radius, inset: inset)
        VStack(alignment: .leading, spacing: 12) {
            ZStack {
                MapArtFrame(map: server.map, image: image, size: .fill)
                LinearGradient(stops: [.init(color: .clear, location: 0.55), .init(color: .black.opacity(0.5), location: 1)], startPoint: .top, endPoint: .bottom)
            }
            .frame(height: compact ? 104 : 150)
            .clipShape(RoundedRectangle(cornerRadius: artRadius, style: .continuous))
            .shapeAudit(.inset, .rounded(artRadius), "details art")
            .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 6) {
                Eyebrow(text: "\(MapCatalog.displayName(server.map)) · \(GameModes.long(server.gametype))", size: 12, tracking: 0.28)
                QuakeName(name: server.name).font(.system(size: 19, weight: .bold)).lineLimit(2).accessibilityAddTraits(.isHeader)
                HStack(spacing: 4) {
                    // Not selectable: AppKit's selectable-text view loops forever answering VoiceOver
                    // for a relabelled text (a stack overflow); Copy and ⇧⌘C copy the address.
                    Text(server.address).font(.typewriter(13)).foregroundStyle(palette.address)
                        .accessibilityLabel("Address \(server.address)")
                    CopyAddressButton { model.copyServer(server) }
                }
            }
            .padding(.horizontal, 6)
            Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 8) {
                GridRow { fact("Version", ServerVersion.label(server)); fact("Mod", server.mod.isEmpty ? "None" : server.mod) }
                GridRow { fact("Frame cap", facts.frameCap); fact("Access", facts.access) }
            }
            .padding(.horizontal, 6)
            ReadyCheckCard(model: model, studio: model.studio, connection: model.serverConnection, check: check, server: server)
                // The card holds its own controls; they don't reach into the panel's corners.
                .shapeAudit(.container, .rounded(0), "ready-check group")
            Rectangle().fill(palette.separator).frame(height: 1).padding(.horizontal, 6)
            roster(server)
        }
    }

    private func roster(_ server: GameServer) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .firstTextBaseline) {
                Eyebrow(text: "Roster · \(server.playerCount) of \(server.maxPlayers)", size: 12, tracking: 0.24, color: palette.tertiary)
                Spacer()
                Text("Score · Ping").font(.system(size: 11)).foregroundStyle(palette.tertiary)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("Roster, \(server.playerCount) of \(server.maxPlayers) players")
            .accessibilityAddTraits(.isHeader)
            VStack(spacing: 2) {
                ForEach(server.players) { player in
                    HStack(spacing: 10) {
                        QuakeName(name: player.name).font(.system(size: 13)).lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
                        Text("\(player.score)").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).frame(width: 34, alignment: .trailing)
                        Text("\(player.ping) ms").font(.system(size: 13)).foregroundStyle(palette.tertiary).frame(width: 52, alignment: .trailing)
                    }
                    .monospacedDigit()
                    .frame(height: 26)
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("\(QuakeColors.plain(player.name)), score \(player.score), \(player.ping) milliseconds")
                    .accessibilityAddTraits(.isStaticText)
                }
                if server.players.isEmpty {
                    Text(server.playerCount == 0 ? "Nobody is playing right now." : "This server didn’t list its players.")
                        .font(.system(size: 13)).foregroundStyle(palette.tertiary).frame(maxWidth: .infinity, alignment: .leading).padding(.vertical, 4)
                }
            }
        }
        .padding(.horizontal, 6)
    }

    private func actions(_ server: GameServer) -> some View {
        let favorite = model.isFavorite(server.address)
        let presentation = check.presentation(model, loadout: "")
        let unfixed = check.address == server.address && presentation.kind == .needsChange && model.serverConnection.overrides.isEmpty
        return GlassContainer(spacing: 8) {
            HStack(spacing: 8) {
                Button { model.connect(server) } label: {
                    Label(unfixed ? "Launch Anyway" : ReadyCheckCard.launchTitle(model, server), systemImage: "play.fill").frame(maxWidth: .infinity)
                }
                .prominentAction().actionControlSize()
                .disabled(!model.canNavigate)
                .accessibilityLabel("\(unfixed ? "Launch anyway" : "Launch") on \(QuakeColors.plain(server.name))")
                .help(unfixed ? "Launch with your loadout as it is; the server may correct or refuse it" : "Launch on this server (Return in the list, or double-click a row)")
                Button { model.toggleFavorite(server) } label: {
                    Image(systemName: favorite ? "star.fill" : "star").font(.system(size: 15)).foregroundStyle(palette.dark ? palette.accent : palette.link).frame(width: 16, height: 16)
                }
                .glassAction(circle: true).actionControlSize()
                .accessibilityLabel(favorite ? "Remove from favorites" : "Add to favorites")
                .help(favorite ? "Remove from Favorites (⌘D)" : "Add to Favorites (⌘D)")
            }
        }
        // 26 from the panel's bottom edge, its corner radius, so the capsules don't sit in its corners.
        .padding(.horizontal, 6)
        .padding(.bottom, 12)
    }

    private func fact(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label).font(.system(size: 11)).foregroundStyle(palette.tertiary)
            Text(value).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).lineLimit(1)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }
}

/// The small copy button after the address; it shows a tick for a moment after copying.
private struct CopyAddressButton: View {
    var copy: () -> Void
    @State private var copied = false
    @Environment(\.palette) private var palette
    var body: some View {
        Button {
            copy(); copied = true
            Task { try? await Task.sleep(for: .seconds(1.5)); copied = false }
        } label: {
            Image(systemName: copied ? "checkmark" : "doc.on.doc").font(.system(size: 12, weight: .medium))
                .foregroundStyle(copied ? palette.positive : palette.secondary)
                .frame(width: 24, height: 24).contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(copied ? "Address copied" : "Copy address")
        .help("Copy address (⇧⌘C)")
    }
}
