import SwiftUI

// Owner: ship-shell. The player card (HomePlayer board): opened from the sidebar's plate. The name
// other players see, edited in place with its colours shown as they will look, the CD key's state
// with Replace…, and the loadout for the next game. The name saves itself as you type.

struct PlayerCardPopover: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var studio: ConfigStudioModel
    @Environment(\.palette) private var palette
    @State private var name = ""
    @FocusState private var nameFocused: Bool
    @State private var showColours = false
    init(model: LauncherModel) { self.model = model; studio = model.studio }
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .top, spacing: 16) {
                PlayerDogTag(name: name, engine: model.chosenEngine)
                VStack(alignment: .leading, spacing: 8) {
                    Eyebrow(text: "Player", size: 12, tracking: 0.28)
                    VStack(alignment: .leading, spacing: 6) {
                        Text("Name other players see").font(.system(size: 12)).foregroundStyle(palette.secondary)
                        TextField("In-game name", text: $name, prompt: Text("Unknown Soldier").foregroundStyle(palette.tertiary))
                            .textFieldStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.text)
                            .focused($nameFocused)
                            .padding(.horizontal, 12).frame(height: 30)
                            .background(palette.field, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                            .overlay(RoundedRectangle(cornerRadius: 8, style: .continuous)
                                .strokeBorder(model.playerCardError != nil ? palette.negative : nameFocused ? palette.accent.opacity(0.75) : palette.wellStroke,
                                              lineWidth: nameFocused || model.playerCardError != nil ? 2 : 1))
                            .shapeAudit(.field, .rounded(8), "name field")
                            .disabled(!editable)
                            .accessibilityLabel("In-game name")
                            .onSubmit { model.flushPlayerName(name) }
                    }
                    preview
                    if showColours { colourRow }
                }
            }
            VStack(spacing: 0) {
                keyRow
                if !studio.profiles.isEmpty {
                    Rectangle().fill(palette.separator).frame(height: 1).padding(.leading, 16)
                    loadoutRow
                }
            }
            .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            .shapeAudit(.container, .rounded(16), "player card group")
            HStack(spacing: 10) {
                Text(footnote).font(.system(size: 12)).foregroundStyle(model.playerCardError != nil ? palette.negative : palette.secondary)
                    .fixedSize(horizontal: false, vertical: true).frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityAddTraits(.isStaticText)
                Button("Done") { model.flushPlayerName(name); model.playerCardPresented = false }
                    .prominentAction().controlSize(.large).keyboardShortcut(.defaultAction)
            }
        }
        .padding(18)
        .frame(width: 430)
        .environment(\.palette, palette)
        .onAppear {
            name = model.playerName == PlayerProfile.unnamed ? "" : model.playerName
            model.playerCardError = nil
            if !model.snapshot { model.playerCardStatus = nil }
        }
        .onChange(of: name) { old, value in if old != value { model.schedulePlayerNameSave(value) } }
        .onDisappear { model.flushPlayerName(name) }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Player card")
    }
    private var editable: Bool { !model.gameRunning && (studio.session != nil || model.snapshot) }
    private var footnote: String {
        if let error = model.playerCardError { return error }
        if model.gameRunning { return "The game is running. Change your name after you quit it." }
        if let status = model.playerCardStatus { return status }
        return "Saves to your \(model.playerLoadoutName ?? "Main") loadout as you type."
    }
    private var preview: some View {
        HStack(spacing: 4) {
            Text("Shows as").foregroundStyle(palette.secondary)
            QuakeName(name: name.isEmpty ? "Unknown Soldier" : name).fontWeight(.semibold).lineLimit(1)
            Button(showColours ? "Hide colours" : "Add colours…") { showColours.toggle() }
                .buttonStyle(.plain).fixedSize()
                .font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.link)
                .disabled(!editable)
                .accessibilityHint("Colour codes ^0 to ^9 colour the letters after them.")
        }
        .font(.system(size: 12))
    }
    /// One button per colour code; each adds its code at the end of the name.
    private var colourRow: some View {
        HStack(spacing: 6) {
            ForEach(PlayerNameRules.colours, id: \.code) { colour in
                Button { name += "^\(colour.code)" } label: {
                    Circle().fill(palette.quake(colour.code)).frame(width: 18, height: 18)
                        .overlay(Circle().strokeBorder(palette.wellStroke, lineWidth: 1))
                        .contentShape(Circle())
                }
                .buttonStyle(.plain)
                .help("^\(colour.code) \(colour.name)")
                .accessibilityLabel("Add \(colour.name), ^\(colour.code)")
            }
        }
        .disabled(!editable)
    }
    @ViewBuilder private var keyRow: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 3) {
                Text("CD key").font(.system(size: 13, weight: .medium)).foregroundStyle(palette.text)
                Text(model.keyVerified ? "Stored only on this Mac." : "Needed to join most servers.").font(.system(size: 12)).foregroundStyle(palette.secondary)
            }
            .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
            Spacer(minLength: 8)
            StatusLabel(model.keyVerified ? .ready : .needsChange, model.keyVerified ? "Verified" : "Not entered")
            Button(model.keyVerified ? "Replace…" : "Enter Key…") { model.playerCardPresented = false; model.presentReplaceKey() }
                .glassAction().controlSize(.large)
                .disabled(model.gameRunning)
        }
        // A 28-point button 16 in from the top, bottom and side: outside the group's 16-point corner.
        .padding(.horizontal, 16).frame(minHeight: 60)
    }
    private var loadoutRow: some View {
        HStack(spacing: 12) {
            Text("Loadout for your next game").font(.system(size: 13, weight: .medium)).foregroundStyle(palette.text)
            Spacer(minLength: 8)
            PopUpButton("Loadout for your next game", selection: Binding(get: { studio.activeProfile?.id ?? "" }, set: { id in
                guard !id.isEmpty, id != studio.activeProfile?.id else { return }
                model.useLoadoutForNextGame(id)
                model.readPlayerName()
                name = model.playerName == PlayerProfile.unnamed ? "" : model.playerName
                model.playerCardStatus = nil
            }), options: studio.profiles.map { (value: $0.id, title: $0.displayName) })
            .disabled(model.gameRunning)
        }
        .padding(.horizontal, 16).frame(minHeight: 52)
    }
}

/// The dog tag on the card, stamped with the name as it is typed (plain letters, upper case).
private struct PlayerDogTag: View {
    var name: String
    var engine: GameEngine
    @Environment(\.palette) private var palette
    var body: some View {
        let words = QuakeColors.plain(name.isEmpty ? "Unknown Soldier" : name).uppercased().split(separator: " ").map(String.init)
        let lines = words.count > 2 ? [words.dropLast().joined(separator: " "), words.last ?? ""] : words
        ZStack(alignment: .top) {
            RoundedRectangle(cornerRadius: 15, style: .continuous).fill(Palette.hex(0xb9b4a6))
                .overlay(RoundedRectangle(cornerRadius: 14.5, style: .continuous).strokeBorder(.white.opacity(0.35), lineWidth: 1))
                .frame(width: 68, height: 85).padding(.top, 6)
            Circle().fill(Palette.hex(0x22251b)).frame(width: 11, height: 11).padding(.top, 13.5)
            VStack(spacing: 3) {
                ForEach(Array(lines.prefix(2).enumerated()), id: \.offset) { _, line in
                    Text(line).font(.typewriter(11, bold: true)).foregroundStyle(Palette.hex(0x3a382e)).lineLimit(1).minimumScaleFactor(0.5)
                }
                Spacer(minLength: 0)
                Text(engine == .cod2x ? "COD2X · 1.4" : "COD2 · 1.3").font(.typewriter(8)).foregroundStyle(Palette.hex(0x6f6b5e))
            }
            .frame(width: 60, height: 50).padding(.top, 36)
        }
        .frame(width: 74, height: 94)
        .accessibilityHidden(true)
    }
}
