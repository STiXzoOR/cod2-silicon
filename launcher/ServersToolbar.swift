import SwiftUI
import AppKit

// Owner: ship-servers. The Servers toolbar (Servers board): All · Favorites · Recent, the search
// field (⌘F), Filters with its popover and count, the status pill with Refresh/Stop, and Connect to
// Address…. Every item sits in the window's toolbar, so the system draws its glass.

extension View {
    func serversToolbar(_ model: LauncherModel, searchFocused: FocusState<Bool>.Binding, searchRequest: Int) -> some View {
        modifier(ServersToolbar(model: model, searchFocused: searchFocused, searchRequest: searchRequest))
    }
}

private struct ServersToolbar: ViewModifier {
    @ObservedObject var model: LauncherModel
    var searchFocused: FocusState<Bool>.Binding
    var searchRequest: Int
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            // Item sizes stay fixed: a toolbar item that changes width after the window lays it out
            // overlaps its neighbours. In a narrow window macOS moves Connect to Address into the
            // toolbar's overflow menu (it stays on ⌘L and File › Connect to Address…).
            content.toolbar {
                leading
                ToolbarSpacer(.flexible)
                ToolbarItem { ServersStatusPill(model: model) }
                ToolbarSpacer(.fixed)
                ToolbarItem { connect }
            }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar {
            leading
            ToolbarItem { Spacer() }
            ToolbarItem { ServersStatusPill(model: model) }
            ToolbarItem { connect }
        }
    }
    @ToolbarContentBuilder private var leading: some ToolbarContent {
        ToolbarItem(placement: .navigation) { scope }
        ToolbarItem(placement: .navigation) { search }
        ToolbarItem(placement: .navigation) { ServerFiltersButton(model: model) }
    }
    private var search: some View { ToolbarSearchField(prompt: "Search servers, maps or mods", text: $model.search, focus: searchFocused, request: searchRequest) }
    private var scope: some View {
        ScopeSwitch("Which servers", selection: $model.serverScope, options: ServerScope.allCases.map { ($0, $0.title) })
    }
    private var connect: some View {
        Button { model.connectToAddress() } label: {
            Label("Connect to Address…", systemImage: "plus").labelStyle(.titleAndIcon).font(.system(size: 13, weight: .semibold))
        }
        .keyboardShortcut("l", modifiers: .command)
        .accessibilityLabel("Connect to Address…")
        .help("Join a server by its address or a cod2x:// link (⌘L)")
    }
}

/// A toolbar's search field. ⌘F (Edit › Find, or the page's own key) focuses it; Escape clears it.
struct ToolbarSearchField: View {
    var prompt: String
    var width: CGFloat = 260
    @Binding var text: String
    var focus: FocusState<Bool>.Binding
    var request: Int
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass").font(.system(size: 13)).foregroundStyle(palette.tertiary).accessibilityHidden(true)
            TextField(prompt, text: $text, prompt: Text(prompt).foregroundStyle(palette.tertiary))
                .textFieldStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.text)
                .focused(focus)
                .onExitCommand { text = ""; focus.wrappedValue = false }
                .accessibilityLabel(prompt)
            if text.isEmpty {
                Text("⌘F").font(.system(size: 12)).tracking(0.5).foregroundStyle(palette.hint).accessibilityHidden(true)
            } else {
                Button { text = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(palette.tertiary) }
                    .buttonStyle(.plain).accessibilityLabel("Clear search").help("Clear search")
            }
        }
        .padding(.horizontal, 8)
        .frame(width: width)
        // A toolbar item is hosted apart from the page, where focus state doesn't reach; ⌘F hands
        // the field the keyboard through AppKit instead.
        .background(FieldFocusAnchor(request: request))
    }
}

/// Makes the text field beside it first responder each time `request` changes.
private struct FieldFocusAnchor: NSViewRepresentable {
    var request: Int
    final class Coordinator { var handled: Int?; init() {} }
    func makeCoordinator() -> Coordinator { Coordinator() }
    func makeNSView(context: Context) -> NSView { NSView() }
    func updateNSView(_ view: NSView, context: Context) {
        guard context.coordinator.handled != request else { return }
        let first = context.coordinator.handled == nil
        context.coordinator.handled = request
        guard !first else { return }
        Task { @MainActor in
            var ancestor = view.superview
            for _ in 0..<8 {
                guard let current = ancestor else { return }
                if let field = Self.field(in: current) {
                    field.window?.makeFirstResponder(field)
                    return
                }
                ancestor = current.superview
            }
        }
    }
    private static func field(in view: NSView) -> NSTextField? {
        if let field = view as? NSTextField, field.isEditable { return field }
        for child in view.subviews { if let field = field(in: child) { return field } }
        return nil
    }
}

/// Filters, with how many are on; opens the Filters popover (Server › Filters…, ⌥⌘F).
private struct ServerFiltersButton: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let count = model.serverFilters.count
        Button { model.serverFiltersPresented.toggle() } label: {
            HStack(spacing: 8) {
                Image(systemName: "line.3.horizontal.decrease")
                Text("Filters")
                if count > 0 { Badge("\(count)", tone: .accent) }
            }
            .font(.system(size: 13, weight: .semibold))
        }
        .accessibilityLabel(count == 0 ? "Filters" : "Filters, \(count) on")
        .help("Filter by game version, mode, ping and who’s playing (⌥⌘F)")
        .popover(isPresented: $model.serverFiltersPresented, arrowEdge: .bottom) { ServerFiltersPopover(model: model) }
    }
}

/// The Filters popover (ServersFilters board). Filters are saved and apply in every scope.
struct ServerFiltersPopover: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let filters = model.serverFilters
        let shown = model.visibleServers.count, total = model.scopedServers.count
        VStack(alignment: .leading, spacing: 10) {
            FormGroup {
                FormRow("Game version") {
                    ScopeSwitch("Game version", selection: $model.serverFilters.version, options: [(.all, "Any"), (.codx, "CoD2x"), (.stock, "Original 1.3")])
                        .controlSize(.small)
                }
                FormRow("Game mode") {
                    PopUpButton("Game mode", selection: Binding(get: { filters.mode ?? "" }, set: { model.serverFilters.mode = $0.isEmpty ? nil : $0 }),
                                options: [("", "Any mode")] + model.serverModes.map { ($0, GameModes.long($0)) })
                }
                FormRow("Ping") {
                    PopUpButton("Ping", selection: Binding(get: { filters.maxPing ?? 0 }, set: { model.serverFilters.maxPing = $0 == 0 ? nil : $0 }),
                                options: [(0, "Any ping")] + ServerFilters.pingLimits.map { ($0, "Under \($0) ms") })
                }
                FormRow("Hide empty servers", isOn: $model.serverFilters.hideEmpty)
                FormRow("Hide full servers", isOn: $model.serverFilters.hideFull)
                FormRow("Hide servers that need a password", isOn: $model.serverFilters.hidePassworded)
            }
            HStack(spacing: 8) {
                Text("\(filters.count == 0 ? "No filters on" : "\(filters.count) \(filters.count == 1 ? "filter" : "filters") on") · \(shown) of \(total) \(total == 1 ? "server" : "servers") shown")
                    .font(.system(size: 12)).foregroundStyle(palette.tertiary)
                    .accessibilityAddTraits(.isStaticText)
                Spacer(minLength: 8)
                Button("Reset") { model.serverFilters = ServerFilters() }.glassAction().disabled(filters.count == 0)
                    .help("Show every server again")
            }
            .padding(.leading, 4)
        }
        .padding(14)
        .frame(width: 380)
        .environment(\.formContext, .inset)
    }
}

/// “8 servers · updated 2 min ago” with Refresh (⌘R), which becomes Stop while refreshing.
struct ServersStatusPill: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 6) {
            TimelineView(.periodic(from: .now, by: 30)) { context in
                Text(model.serverStatusLine(now: context.date)).font(.system(size: 12)).monospacedDigit().foregroundStyle(palette.secondary)
                    .lineLimit(1).fixedSize()
                    .accessibilityLabel(model.serverStatusLine(now: context.date))
                    .accessibilityAddTraits([.isStaticText, .updatesFrequently])
            }
            .padding(.leading, 10)
            Button { model.refreshing ? model.cancelRefresh() : model.refresh() } label: {
                Image(systemName: model.refreshing ? "stop.fill" : "arrow.clockwise").font(.system(size: 13, weight: .semibold)).frame(width: 18, height: 18)
            }
            .accessibilityLabel(model.refreshing ? "Stop refreshing" : "Refresh server list")
            .help(model.refreshing ? "Stop asking servers; what has answered stays" : "Ask the master servers and every server again (⌘R)")
        }
    }
}
