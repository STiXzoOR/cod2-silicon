import AppKit
import SwiftUI

// Review harness, shared (started by ship-foundation). The Foundations gallery: one of each shared
// building block from launcher/Components.swift and Theme.swift, in the main window's size, so the
// blocks are reviewed in both appearances, at the minimum size, by the shape audit and by VoiceOver
// (`foundations`, and `foundations-popover` with a popover open). A unit that adds a block adds a
// sample here in its own region.

struct FoundationsGallery: View {
    @ObservedObject var model: LauncherModel
    /// Opens the sample popover (the `foundations-popover` screen).
    var popover = false
    @Environment(\.colorScheme) private var scheme
    @Environment(\.colorSchemeContrast) private var contrast
    @State private var summaries = true
    @State private var vsync = false
    @State private var mode = "borderless"
    @State private var name = "Morrow"
    @State private var address = "203.0.113.24:28960"
    @State private var invalidAddress = "invalid address;quit"
    @State private var search = "toujane"
    @State private var scope = "All"
    @State private var hideEmpty = true
    @FocusState private var searchFocused: Bool
    var body: some View {
        let palette = Palette.of(scheme, contrast: contrast)
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                PageHeader(eyebrow: "Design system", title: "Foundations", subtitle: "Every shared building block, in this appearance.")
                    .padding(.leading, 6)
                HStack(alignment: .top, spacing: 20) {
                    VStack(alignment: .leading, spacing: 20) { forms; feedback }
                    VStack(alignment: .leading, spacing: 20) { status; mapArt }
                    VStack(alignment: .leading, spacing: 20) { controls; fields }
                }
            }
            .padding(.horizontal, 28).padding(.top, 36).padding(.bottom, 28)
            .frame(maxWidth: .infinity, alignment: .topLeading)
        }
        .background(palette.ground)
        .capsuleControls()
        .environment(\.palette, palette)
    }

    private func tile<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        ContentPanel {
            Eyebrow(text: title, size: 12).padding(.bottom, 14).accessibilityAddTraits(.isHeader)
            VStack(alignment: .leading, spacing: 14) { content() }
        }
    }

    private var forms: some View {
        tile("Forms") {
            // A switch row with detail first, so the field row is the group's last (never centred).
            FormSection("Game", content: {
                FormGroup {
                    FormRow("Local match summaries", detail: "Kept on this Mac for Check-up.", isOn: $summaries)
                    FormRow("Display mode", symbol: "display") {
                        PopUpButton("Display mode", selection: $mode, options: [("exclusive", "Exclusive"), ("borderless", "Borderless"), ("windowed", "Window")])
                    }
                    FormRow("Player name") { LauncherTextField("Player name", text: $name, width: 120) }
                }
            }, footer: { Text("Changes save automatically.") })
        }
    }

    private var feedback: some View {
        tile("Feedback") {
            Banner(message: "Servers you join appear in Recent.")
            Banner(.tip, symbol: "lightbulb", title: "Tip", message: "Press ⌘K to jump to any setting.")
            Banner(.error, message: "Your changes couldn’t be saved.") { Button("Try Again") {}.glassAction() }
            EmptyState("No recordings", message: "Recorded matches appear here.", actionTitle: "Show Folder in Finder", prominent: false) {}
            Toast(message: "The game closed. You can launch again.") { Button("Dismiss") {}.glassAction().actionControlSize() }
        }
    }

    private var status: some View {
        tile("Status") {
            VStack(alignment: .leading, spacing: 8) {
                StatusLabel(.ready); StatusLabel(.needsChange); StatusLabel(.wontWork); StatusLabel(.notMeasured)
            }
            HStack(spacing: 8) { Badge("Stock 1.3"); Badge("CoD2x 1.4", tone: .accent); Badge("Locked", tone: .negative) }
            HStack(spacing: 8) {
                Chip("18 of 24 players") { Text("18").font(.stencil(15)).foregroundStyle(Palette.of(scheme, contrast: contrast).accent); Text("of 24") }
                Chip("Toujane") { Image(systemName: "map"); Text("Toujane") }
            }
            Chip("Display 2560 × 1440", size: .large, glass: true, action: {}) { Image(systemName: "display"); Text("2560 × 1440 · Borderless") }
            HStack(spacing: 14) {
                ShortcutKeys(["⌘", "1"])
                ShortcutKeys(["⇧", "⌘", "↩"])
                HStack(spacing: 6) { ForEach(1...4, id: \.self) { PingBars(level: $0) } }
            }
            ProgressBar("Building shaders", value: 0.62)
            ProgressBar("Contacting master servers", value: nil)
        }
    }

    private var mapArt: some View {
        tile("Map art") {
            HStack(alignment: .top, spacing: 14) {
                MapArtFrame(map: "mp_toujane", image: nil, size: .row)
                MapArtFrame(map: "mp_carentan", image: nil, size: .card)
            }
            MapArtFrame(map: "mp_leningrad", image: nil, size: .fill).frame(height: 96)
                .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
    }

    private var controls: some View {
        tile("Controls") {
            HStack(spacing: 12) {
                Button("Launch") {}.prominentAction().actionControlSize()
                Button("Find a Server") {}.glassAction().actionControlSize()
            }
            Button("Delete Loadout…") {}.destructiveAction().actionControlSize()
            ToolbarPill {
                SymbolButton("folder", label: "Show in Finder") {}
                SymbolButton("arrow.clockwise", label: "Refresh") {}
                SymbolButton("info.circle", label: "About this server") {}
                    .popover(isPresented: .constant(popover), arrowEdge: .bottom) { popoverBody }
            }
            ScopeSwitch("Server scope", selection: $scope, options: [("All", "All"), ("Favorites", "Favorites"), ("Recent", "Recent")])
            Checkbox("Hide empty servers", isOn: $hideEmpty)
            HStack(spacing: 12) {
                Text("Vertical sync").font(.system(size: 13))
                SwitchToggle("Vertical sync", isOn: $vsync)
            }
            Button("Release Notes") {}.quietAction()
        }
    }

    private var fields: some View {
        tile("Fields") {
            SearchPill("Search servers", text: $search, focus: $searchFocused, width: 260)
            LauncherTextField("Server address", text: $address, shape: .capsule, monospaced: true, width: 260)
            LauncherTextField("Invalid server address", text: $invalidAddress, shape: .capsule, monospaced: true, invalid: true, width: 260)
            LauncherTextField("Password", text: .constant("secret"), shape: .capsule, secure: true, width: 260)
        }
    }

    /// A popover's content sits in wells (the inset form context), 16 points in.
    private var popoverBody: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Northern Lights").font(.system(size: 15, weight: .semibold))
            FormGroup {
                FormRow("Hide empty servers", isOn: $hideEmpty)
                FormRow("Hide full servers", isOn: $vsync)
            }
        }
        .padding(16)
        .frame(width: 300)
        .environment(\.formContext, .inset)
        .environment(\.palette, Palette.of(scheme, contrast: contrast))
    }
}
