import SwiftUI
import AppKit

/// The Servers page (Servers boards): the toolbar, the list with its banners and empty states, and the
/// details panel. The list is in ServersList.swift, the details in ServerDetailsPanel.swift and the
/// toolbar with its Filters popover in ServersToolbar.swift.
struct ServersView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @FocusState private var searchFocused: Bool
    /// Bumped by ⌘F (the page's key or Edit › Find); the toolbar's search field takes the keyboard.
    @State private var searchRequest = 0
    @FocusState private var listFocused: Bool
    @State private var origin = CGPoint.zero
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }

    var body: some View {
        let rows = model.visibleServers
        GeometryReader { geometry in
            let leading = max(22, 264 - origin.x)
            let listWidth = geometry.size.width - leading - 18 - 352 - 22
            HStack(alignment: .top, spacing: 22) {
                VStack(alignment: .leading, spacing: 0) {
                    PageHeader(title: "Servers", subtitle: "Original 1.3 and CoD2x servers from the public master lists. Nothing is contacted until you refresh.")
                        .padding(.leading, 16).padding(.top, 88 - 52).padding(.bottom, 18)
                    ServersBanners(model: model).padding(.leading, 10)
                    if rows.isEmpty {
                        ServersEmptyState(model: model).padding(.top, model.refreshing || model.mastersDidNotAnswer ? 60 : 110)
                        Spacer(minLength: 0)
                    } else {
                        ServerList(model: model, rows: rows, compact: listWidth < 720, focus: $listFocused)
                    }
                }
                .frame(maxWidth: 790, maxHeight: .infinity, alignment: .top)
                ServerDetailsPanel(model: model, server: model.selected, image: model.selected.flatMap { artwork.image(for: $0.map) },
                                   compact: geometry.size.height < 830)
                    .frame(width: 352)
                    .padding(.top, 84 - 52).padding(.bottom, 22)
            }
            .padding(.leading, leading).padding(.trailing, 18).padding(.top, 52)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        }
        .background { ContourBackdrop().ignoresSafeArea() }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .serversToolbar(model, searchFocused: $searchFocused, searchRequest: searchRequest)
        // The page's own keys (the menu bar's items, when it has them, reach the same model methods).
        .background { pageKeys }
        // The list takes the keyboard first (arrows, Return), so no column head starts out focused.
        .defaultFocus($listFocused, true)
        .onAppear {
            model.serversAppeared()
            Task { @MainActor in listFocused = true }
        }
        .onChange(of: model.visibleServers.count) { _, _ in model.loadArtwork() }
        .onChange(of: model.serverSearchRequested) { _, _ in searchRequest += 1 }
    }

    private var pageKeys: some View {
        ZStack {
            Button("Find") { searchRequest += 1 }.keyboardShortcut("f", modifiers: .command)
            Button("Add to Favorites") { model.toggleFavoriteSelected() }.keyboardShortcut("d", modifiers: .command)
            Button("Copy Address") { model.copySelectedAddress() }.keyboardShortcut("c", modifiers: [.command, .shift])
            Button("Filters") { model.serverFiltersPresented = true }.keyboardShortcut("f", modifiers: [.command, .option])
        }
        .opacity(0).frame(width: 0, height: 0).accessibilityHidden(true)
    }
}

/// Refreshing (the count, a bar and Stop) and the master servers not answering (the list's time and Try Again).
struct ServersBanners: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        if model.refreshing {
            let asking = model.refreshAsked == 0
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 6) {
                    Text(asking ? "Asking the master servers for the list…" : "Asking \(model.refreshAsked) servers. \(model.refreshAnswered) \(model.refreshAnswered == 1 ? "has" : "have") answered.")
                        .font(.system(size: 13)).foregroundStyle(palette.text)
                    ProgressBar("Refreshing the server list", value: asking ? nil : Double(model.refreshFinished) / Double(max(1, model.refreshAsked)))
                }
                .accessibilityElement(children: .combine)
                .accessibilityAddTraits(.updatesFrequently)
                Button("Stop", action: model.cancelRefresh).glassAction().controlSize(.large)
                    .accessibilityLabel("Stop refreshing")
            }
            .padding(.vertical, 10).padding(.leading, 14).padding(.trailing, 11)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            .padding(.bottom, 12)
        } else if model.mastersDidNotAnswer {
            Banner(.error, message: model.library.cachedAt.map { "The master servers didn’t answer. You’re seeing the list from \(LauncherModel.clock($0))." }
                   ?? "The master servers didn’t answer. Check your internet connection, then try again.") {
                Button("Try Again", action: model.refresh).glassAction().controlSize(.large)
            }
            .padding(.bottom, 12)
        }
    }
}

/// Never refreshed, nothing matches, no favourites yet, nothing played yet, and the first answers on their way.
struct ServersEmptyState: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        if model.refreshing {
            EmptyState("Finding servers", message: "Servers appear here as they answer.")
        } else if model.serverScope == .favorites && model.library.favorites.isEmpty {
            EmptyState("No favorites yet", message: "Star a server to keep it here and in the sidebar.", actionTitle: "Show All Servers", prominent: false) {
                model.serverScope = .all
            }
        } else if model.serverScope == .recent && model.library.recent.isEmpty {
            EmptyState("Nothing played yet", message: "Servers you launch on appear here, ready to rejoin.", actionTitle: "Show All Servers", prominent: false) {
                model.serverScope = .all
            }
        } else if model.scopedServers.isEmpty {
            EmptyState("No contact yet", message: "Ask the public master servers who’s online. CoD2 Unbound contacts them only when you refresh.",
                       actionTitle: "Refresh Server List", action: model.refresh)
        } else {
            let query = model.search.trimmingCharacters(in: .whitespacesAndNewlines)
            let filters = model.serverFilters.count > 0
            EmptyState("No match", message: query.isEmpty ? "No server matches these filters." : filters ? "No server matches “\(query)” with these filters." : "No server matches “\(query)”.",
                       actionTitle: query.isEmpty ? "Clear Filters" : filters ? "Clear Search and Filters" : "Clear Search", prominent: false, action: model.clearServerFilters)
        }
    }
}

/// Topographic contour lines behind the server list: original, quiet and content-layer.
struct ContourBackdrop: View {
    @Environment(\.palette) private var palette
    var body: some View {
        Canvas { context, size in
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .radialGradient(
                Gradient(colors: [(palette.dark ? Palette.hex(0x3a3220) : Palette.hex(0xe2d6b8)).opacity(palette.dark ? 0.45 : 0.6), .clear]),
                center: CGPoint(x: size.width * 0.7, y: 0), startRadius: 0, endRadius: size.width * 0.9))
            let lines = ["M260 120 C 420 60 620 90 760 150 S 1100 260 1440 180", "M260 170 C 430 110 610 140 770 200 S 1110 310 1440 230",
                         "M260 225 C 440 165 600 195 780 255 S 1120 365 1440 285", "M260 285 C 450 225 590 255 790 315 S 1130 425 1440 345",
                         "M260 350 C 460 290 580 320 800 380 S 1140 490 1440 410", "M980 520 C 1060 470 1180 470 1240 520 S 1300 640 1210 680 S 1010 640 980 520Z",
                         "M1020 530 C 1080 495 1160 495 1205 530 S 1250 620 1185 650 S 1040 620 1020 530Z"]
            let scale = CGAffineTransform(scaleX: size.width / 1440, y: size.height / 900)
            for line in lines {
                context.stroke(SVGPath.parse(line, transform: scale), with: .color((palette.dark ? Palette.hex(0xc9b27a) : Palette.hex(0x6f5a2a)).opacity(palette.dark ? 0.035 : 0.06)), lineWidth: 1.2)
            }
        }
        .accessibilityHidden(true)
    }
}
