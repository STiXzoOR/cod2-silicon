import SwiftUI
import AppKit
import Carbon.HIToolbox

/// Protect typed keys from other processes while the entry field has focus.
@MainActor enum SecureKeyEntry {
    private static var active = false
    static func set(_ on: Bool) {
        guard on != active else { return }
        active = on
        _ = on ? EnableSecureEventInput() : DisableSecureEventInput()
    }
}

/// Setup (the thirteen Setup boards): six steps over the dusk scene, each a heading and one card,
/// with Back, the progress dots and the step's actions in the glass bar at the bottom.
struct SetupView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    fileprivate enum Focus: Hashable { case folder, key, name, primary }
    @FocusState private var focus: Focus?
    @State private var dropTarget = false
    static let panelWidth: CGFloat = 660

    var body: some View {
        ZStack(alignment: .bottom) {
            GeometryReader { proxy in
                ScrollView {
                    VStack(spacing: 12) { content }
                        .frame(maxWidth: .infinity)
                        .padding(.horizontal, 24).padding(.top, 40).padding(.bottom, 112)
                        .frame(minHeight: proxy.size.height)
                }
                .scrollBounceBehavior(.basedOnSize)
            }
            SetupBar(model: model, focus: $focus)
                .padding(.bottom, 22).padding(.horizontal, 24)
        }
        .background { SetupBackdrop().ignoresSafeArea() }
        .task(id: model.step * 4 + (model.replacingKey ? 1 : 0) + (model.shaderBusy ? 2 : 0)) {
            // The new step must be mounted before its focus target can accept focus.
            focus = nil
            try? await Task.sleep(for: .milliseconds(250))
            if !Task.isCancelled { focusStep() }
        }
        .onChange(of: focus) { _, value in SecureKeyEntry.set(value == .key && !model.snapshot) }
        .onDisappear { SecureKeyEntry.set(false) }
        .onExitCommand { if model.canCancelSetup { model.cancelSetup() } }
        .animation(.easeInOut(duration: reduceMotion ? 0.15 : 0.22), value: model.step)
    }
    private func focusStep() {
        switch model.setupStep {
        case .data where model.dataPath.isEmpty || !model.dataError.isEmpty: focus = .folder
        case .key where !model.keyVerified || model.replacingKey: focus = .key
        case .yours: focus = .name
        default: focus = .primary
        }
    }
    @ViewBuilder private var content: some View {
        Group {
            switch model.setupStep {
            case .welcome: welcome
            case .data: head(model.dataPath.isEmpty || !model.dataError.isEmpty ? "Find your game data" : "Game data found",
                             "Choose the Call of Duty 2 folder from your own copy. CoD2 Silicon uses the files where they are; nothing is copied.")
                dataPanel
            case .key: head("Enter your CD key", "The 20-character key that came with your game. It unlocks multiplayer on official and CoD2x servers.")
                keyPanel
            case .shaders: head(model.shaderBusy || model.shaderFailed ? "Building the original shaders" : "Original shaders are ready",
                                "Lighting, sky and water as the game draws them on Windows, built on this Mac from your own game files.")
                shaderPanel
            case .yours: head("Make it yours", "Two things most players set first. Skip this and you start with the game’s defaults.")
                yoursPanel
            case .ready: head("You’re ready to play", "Everything the game needs is in place.")
                readyPanel
            }
            if !model.setupError.isEmpty {
                Banner(.error, symbol: "exclamationmark.triangle", message: model.setupError).frame(width: Self.panelWidth)
            }
        }
        .transition(.opacity)
    }

    // MARK: Pieces
    private func head(_ title: String, _ detail: String) -> some View {
        VStack(spacing: 12) {
            Eyebrow(text: "Step \(model.setupStep.number) of \(SetupStep.count) · \(model.setupStep.title)")
            Text(title).font(.system(size: 30, weight: .bold)).foregroundStyle(palette.inkOnArt).multilineTextAlignment(.center)
                .accessibilityAddTraits(.isHeader)
            Text(detail).font(.system(size: 15)).foregroundStyle(palette.inkOnArt.opacity(0.82)).multilineTextAlignment(.center)
                .lineSpacing(3).frame(maxWidth: 560).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.bottom, 14)
    }
    /// The card under each heading (radius 22). Its default 6-point inset keeps a 16-point group or
    /// well concentric; a 28-point button in a corner sits 8 in (`cornered`).
    private func panel<Content: View>(spacing: CGFloat = 12, padding: CGFloat = 6, @ViewBuilder _ content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: spacing) { content() }
            .padding(padding)
            .frame(width: Self.panelWidth, alignment: .leading)
            .contentSurface(RoundedRectangle(cornerRadius: 22, style: .continuous))
    }
    private func group<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        VStack(spacing: 0) { content() }
            .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
            .shapeAudit(.container, .rounded(16), "setup group")
    }
    private func symbolTile(_ name: String, negative: Bool = false) -> some View {
        Image(systemName: name).font(.system(size: 15, weight: .medium)).foregroundStyle(negative ? palette.negative : palette.accent)
            .frame(width: 30, height: 30)
            .background(palette.selection, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            .accessibilityHidden(true)
    }
    /// A group row. With `control`, the row is 60 tall so its 28-point button sits 16 from the top,
    /// bottom and side, clear of the group's 16-point corner.
    private func row<Trailing: View>(_ symbol: String, _ title: String, _ detail: String, mono: Bool = false, negative: Bool = false,
                                     control: Bool = false, @ViewBuilder trailing: () -> Trailing) -> some View {
        HStack(spacing: 12) {
            symbolTile(symbol, negative: negative)
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(mono ? .typewriter(14, bold: true) : .system(size: 14, weight: .semibold))
                    .foregroundStyle(mono ? palette.address : palette.text).lineLimit(mono ? 1 : 2).truncationMode(.middle)
                Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
            .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
            Spacer(minLength: 8)
            trailing()
        }
        .padding(.horizontal, 16).padding(.vertical, 10).frame(minHeight: control ? 60 : 56)
    }
    private func cornered<Content: View>(@ViewBuilder _ content: () -> Content) -> some View { content().padding(2) }
    private func divider() -> some View { Rectangle().fill(palette.separator).frame(height: 1).padding(.leading, 58) }
    private func note(_ text: String, symbol: String? = nil) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            if let symbol { Image(systemName: symbol).font(.system(size: 11)).accessibilityHidden(true) }
            Text(text).fixedSize(horizontal: false, vertical: true)
        }
        .font(.system(size: 12)).foregroundStyle(palette.secondary)
        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
    }

    // MARK: Welcome
    private var welcome: some View {
        VStack(spacing: 12) {
            BrandMark().frame(width: 124, height: 124).shadow(color: .black.opacity(0.35), radius: 18, y: 8)
            Eyebrow(text: "Reporting for duty").padding(.top, 14)
            Text("Welcome to CoD2 Silicon").font(.system(size: 40, weight: .bold)).foregroundStyle(palette.inkOnArt).multilineTextAlignment(.center)
                .accessibilityAddTraits(.isHeader)
            Text("Call of Duty 2, running natively on your Mac. Setup takes about two minutes and uses your own copy of the game.")
                .font(.system(size: 15)).foregroundStyle(palette.inkOnArt.opacity(0.82)).multilineTextAlignment(.center).lineSpacing(3)
                .frame(maxWidth: 560).fixedSize(horizontal: false, vertical: true)
            panel(padding: 6) {
                group {
                    row("folder", "Your Call of Duty 2 folder", "From a Windows, Steam or Mac 1.3 install. Used where it is, never changed.") { EmptyView() }
                    divider()
                    row("key", "Your CD key", "The 20 characters that came with the game. Needed for online play.") { EmptyView() }
                    divider()
                    row("circle.lefthalf.filled", "About a minute for shaders", "Built once on this Mac so the game looks as it does on Windows.") { EmptyView() }
                }
            }
            .padding(.top, 14)
            note("Nothing leaves this Mac during setup.", symbol: "lock").padding(.top, 6)
        }
    }

    // MARK: Game data
    @ViewBuilder private var dataPanel: some View {
        if !model.dataError.isEmpty {
            panel {
                group {
                    row("folder", (model.attemptedDataPath as NSString).abbreviatingWithTildeInPath, "\(model.dataArchivesFound) of 16 game archives found", mono: true, negative: true, control: true) {
                        Button("Choose Folder…", action: model.pickData).glassAction().controlSize(.large).focused($focus, equals: .folder)
                    }
                }
                cornered { Banner(.error, symbol: "exclamationmark.triangle", message: model.dataError) }
            }
            .dropZone(model: model, targeted: $dropTarget)
        } else if !model.dataPath.isEmpty {
            panel {
                group {
                    row("folder", model.dataPathDisplay, model.dataFoundSummary, mono: true) { StatusLabel(.ready, "Game data found") }
                    divider()
                    note("Used where it is. Nothing is copied or changed. Configs, demos and screenshots get their own folder.")
                        .padding(.horizontal, 16).padding(.vertical, 12).frame(maxWidth: .infinity, alignment: .leading)
                }
                cornered { Button("Choose a Different Folder…", action: model.pickData).glassAction().controlSize(.large).focused($focus, equals: .folder) }
            }
            .dropZone(model: model, targeted: $dropTarget)
        } else {
            panel {
                VStack(spacing: 10) {
                    Image(systemName: "folder").font(.system(size: 34, weight: .light)).foregroundStyle(palette.accent).accessibilityHidden(true)
                    Text("Drag your Call of Duty 2 folder here").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.text)
                    Text("It’s the folder that contains “main”. Windows or Mac 1.3 data both work.").font(.system(size: 12)).foregroundStyle(palette.secondary)
                    Button("Choose Folder…", action: model.pickData).glassAction().actionControlSize().focused($focus, equals: .folder).padding(.top, 6)
                }
                .frame(maxWidth: .infinity).padding(.vertical, 28).padding(.horizontal, 20)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .strokeBorder(dropTarget ? palette.accent : palette.wellStroke, style: StrokeStyle(lineWidth: dropTarget ? 2 : 1, dash: [6, 4])))
                .shapeAudit(.container, .rounded(16), "drop zone")
                .accessibilityElement(children: .contain)
                .accessibilityLabel("Drop zone for your Call of Duty 2 folder")
                group {
                    ForEach(Array([("Windows PC", "Copy the “Call of Duty 2” folder from Program Files to this Mac."),
                                   ("Steam", "Right-click the game, then Manage › Browse Local Files."),
                                   ("Mac release", "Open the Call of Duty 2 folder in Applications.")].enumerated()), id: \.offset) { index, item in
                        if index > 0 { Rectangle().fill(palette.separator).frame(height: 1).padding(.leading, 16) }
                        HStack(spacing: 12) {
                            Text(item.0).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text).frame(width: 150, alignment: .leading)
                            Text(item.1).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                            Spacer(minLength: 0)
                        }
                        .padding(.horizontal, 16).frame(minHeight: 44)
                        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                    }
                }
                Link("Don’t have the files yet?", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon#game-data")!)
                    .font(.system(size: 13)).foregroundStyle(palette.link).frame(maxWidth: .infinity).padding(.bottom, 8)
            }
            .dropZone(model: model, targeted: $dropTarget)
        }
    }

    // MARK: CD key
    @ViewBuilder private var keyPanel: some View {
        if model.keyVerified && !model.replacingKey {
            panel {
                group {
                    row("key", "A CD key is already stored on this Mac", "•••• •••• •••• •••• " + (model.storedKeyTail ?? "••••"), control: true) {
                        StatusLabel(.ready, "Verified")
                        Button("Replace…", action: model.replaceKey).glassAction().controlSize(.large)
                    }
                }
                note("Continue keeps this key. You never have to type it again to move on.").padding(.leading, 18).padding(.bottom, 8)
            }
        } else {
            let check = model.keyCheck
            // The 56-point capsule can't be concentric in the card's corner, so it sits 22 in; Paste rides
            // inside its trailing end, concentric with it.
            panel(padding: 22) {
                TextField("CD key", text: Binding(get: { KeyFormat.display(model.keyInput) }, set: { model.keyInput = KeyFormat.display($0); model.setupError = "" }),
                              prompt: Text("XXXX XXXX XXXX XXXX XXXX").foregroundColor(palette.tertiary))
                        .textFieldStyle(.plain).font(.typewriter(22, bold: true)).tracking(2.6)
                        .foregroundStyle(palette.heading).autocorrectionDisabled().focused($focus, equals: .key)
                        .padding(.leading, 24).padding(.trailing, 100).frame(height: 56)
                        .background(palette.well, in: Capsule())
                        .overlay(Capsule().strokeBorder(check == .mismatch ? palette.negative : check == .verified ? palette.positive : focus == .key ? palette.accent : palette.wellStroke,
                                                        lineWidth: focus == .key || check == .mismatch || check == .verified ? 2 : 1))
                        .shapeAudit(.field, .capsule, "key field")
                        .onSubmit { if model.keyReady { model.nextSetup() } }
                        .accessibilityLabel("CD key, twenty characters")
                        .accessibilityValue(check == .mismatch ? "The checksum doesn’t match" : check == .verified ? "Checksum verified" : "")
                        .overlay(alignment: .trailing) {
                            Button("Paste", action: model.pasteKey).glassAction().actionControlSize().padding(.trailing, 10)
                        }
                switch check {
                case .mismatch:
                    Banner(.error, symbol: "exclamationmark.triangle", message: "The checksum doesn’t match. Check for a mistyped character: 0 and O, 1 and I are the usual ones.")
                case .verified:
                    StatusLabel(.ready, "Checksum verified.").padding(.leading, 8)
                case .typing(let left):
                    note("\(left) character\(left == 1 ? "" : "s") to go. Spaces and dashes are fine.").padding(.leading, 8)
                case .blank:
                    note("Spaces and dashes are fine. Find it on the manual, the disc sleeve, or in Steam under Manage › CD keys.").padding(.leading, 8)
                }
                note("Stored only on this Mac, in the game’s own preferences. Sent only to game servers when you join, as the original game does.", symbol: "lock")
                    .padding(.horizontal, 8)
            }
        }
    }

    // MARK: Shaders
    @ViewBuilder private var shaderPanel: some View {
        if model.shaderBusy {
            let counted = model.shaderCount > 0
            panel(padding: 16) {
                VStack(alignment: .leading, spacing: 10) {
                    HStack {
                        Text(model.shaderMessage).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                        Spacer()
                        if counted {
                            Text("\(model.shaderCount) of \(LauncherModel.shaderTotal)").font(.system(size: 13)).foregroundStyle(palette.secondary).monospacedDigit()
                        }
                    }
                    .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                    ProgressBar("Building shaders", value: counted ? Double(model.shaderCount) / Double(LauncherModel.shaderTotal) : nil)
                        .accessibilityValue(counted ? "\(model.shaderCount) of \(LauncherModel.shaderTotal)" : "Starting")
                    note("Built once, on this Mac, from your own game files. Nothing is downloaded. You can keep this window in the background.")
                }
                .padding(8)
            }
        } else if model.shaderFailed {
            panel {
                cornered {
                    Banner(.error, symbol: "exclamationmark.triangle", title: "The original shaders couldn’t be built",
                           message: "The game still runs with approximate shaders, which lose clouds and most lighting detail. If you own the Mac version, its shaders work too.")
                }
                cornered {
                    HStack(spacing: 8) {
                        Button("Try Again") { model.prepareShaders() }.glassAction().controlSize(.large)
                        Button("Choose Mac Copy…", action: model.pickShaders).glassAction().controlSize(.large)
                        Spacer(minLength: 0)
                    }
                }
            }
        } else {
            panel {
                group {
                    row("circle.lefthalf.filled", "Original shaders",
                        model.shaderSeconds.map { "\(LauncherModel.shaderTotal) built in \($0) second\($0 == 1 ? "" : "s"). Kept for next time." }
                            ?? "\(LauncherModel.shaderTotal) built on this Mac earlier. Kept for next time.") { StatusLabel(.ready, "Ready") }
                }
            }
        }
    }

    // MARK: Make it yours
    private var yoursPanel: some View {
        panel(spacing: 16, padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                Text("Name other players see").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6)
                HStack(spacing: 12) {
                    TextField("Name other players see", text: Binding(get: { model.setupName }, set: { value in
                        model.setupName = value; model.setupNameError = PlayerNameRules.problem(value)
                    }), prompt: Text("Unknown Soldier").foregroundStyle(palette.tertiary))
                        .textFieldStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.text).focused($focus, equals: .name)
                        .padding(.horizontal, 16).frame(height: 36)
                        .background(palette.field, in: Capsule())
                        .overlay(Capsule().strokeBorder(model.setupNameError != nil ? palette.negative : focus == .name ? palette.accent.opacity(0.75) : palette.wellStroke,
                                                        lineWidth: focus == .name || model.setupNameError != nil ? 2 : 1))
                        .shapeAudit(.field, .capsule, "name field")
                        .accessibilityLabel("In-game name")
                        .onSubmit { model.nextSetup() }
                    HStack(spacing: 4) {
                        Text("Shows as").foregroundStyle(palette.secondary)
                        QuakeName(name: model.setupName.isEmpty ? "Unknown Soldier" : model.setupName).fontWeight(.semibold).lineLimit(1)
                    }
                    .font(.system(size: 12)).fixedSize()
                    .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                }
                if let error = model.setupNameError { Text(error).font(.system(size: 12)).foregroundStyle(palette.negative).padding(.leading, 6) }
            }
            VStack(alignment: .leading, spacing: 8) {
                Text("How the game fills your screen").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6)
                    .accessibilityAddTraits(.isHeader)
                HStack(alignment: .top, spacing: 10) {
                    ForEach(SetupDisplayMode.allCases) { mode in DisplayModeCrate(mode: mode, selected: model.setupDisplayMode == mode.rawValue) { model.setupDisplayMode = mode.rawValue } }
                }
            }
            note("Both can be changed any time: the name on your player card, the display in Settings.").padding(.leading, 6)
        }
    }

    // MARK: Ready
    private var readyPanel: some View {
        VStack(spacing: 12) {
            panel(padding: 6) {
                group {
                    row("folder", "Game data", "\(model.dataPathDisplay) · Version 1.3") { StatusLabel(.ready, "Found") }
                    divider()
                    if model.keyVerified {
                        row("key", "CD key", "Stored only on this Mac") { StatusLabel(.ready, "Verified") }
                    } else {
                        row("key", "CD key", "Not entered. Most servers need it; add it from your player card.") { StatusLabel(.needsChange, "Skipped") }
                    }
                    divider()
                    if model.approximate {
                        row("circle.lefthalf.filled", "Shaders", "Approximate: clouds and most lighting detail are missing. Rebuild them in Settings › Game Data.") {
                            StatusLabel(.needsChange, "Approximate")
                        }
                    } else {
                        row("circle.lefthalf.filled", "Shaders", "Original, built on this Mac") { StatusLabel(.ready, "Ready") }
                    }
                    divider()
                    row("person", "Player and display", model.setupPlayerSummary) { StatusLabel(.ready, "Set") }
                }
            }
            if let address = model.setupServerAddress {
                note("Next: join \(address)", symbol: "dot.radiowaves.left.and.right")
            }
            note("Run setup again any time from Settings › Game Data.").padding(.top, 6)
        }
    }
}

/// One of the three display choices on Make it yours: a card that is a radio button.
private struct DisplayModeCrate: View {
    var mode: SetupDisplayMode
    var selected: Bool
    var choose: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        Button(action: choose) {
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 6) {
                    Text(mode.title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading).fixedSize()
                    Spacer(minLength: 0)
                    if mode == .borderless { Badge("Recommended", tone: .accent) }
                }
                Text(mode.detail).font(.system(size: 11)).foregroundStyle(palette.secondary).lineSpacing(1).fixedSize(horizontal: false, vertical: true)
            }
            .padding(.horizontal, 14).padding(.vertical, 14)
            .frame(maxWidth: .infinity, minHeight: 84, alignment: .topLeading)
            .background(selected ? palette.selection : palette.crate, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(selected ? palette.selectionStroke : palette.wellStroke, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
        .buttonStyle(.plain)
        .shapeAudit(.container, .rounded(16), "display choice")
        .accessibilityLabel("\(mode.title)\(mode == .borderless ? ", recommended" : ""). \(mode.detail)")
        .accessibilityAddTraits(selected ? [.isSelected] : [])
        .accessibilityHint("How the game fills your screen")
    }
}

/// The bottom bar: Back, one dot per step with “Step N of 6”, then Cancel (Set Up Again), the step's
/// second action and its default action.
private struct SetupBar: View {
    @ObservedObject var model: LauncherModel
    var focus: FocusState<SetupView.Focus?>.Binding
    @Environment(\.palette) private var palette
    var body: some View {
        let step = model.setupStep
        HStack(spacing: 8) {
            if step != .welcome {
                Button("Back", action: model.backSetup).glassAction().actionControlSize().disabled(model.shaderBusy)
            }
            HStack(spacing: 6) {
                ForEach(SetupStep.allCases) { item in
                    Capsule().fill(item == step ? palette.accent : item.rawValue < step.rawValue ? palette.positive : palette.track)
                        .frame(width: item == step ? 26 : 14, height: 5)
                }
            }
            .padding(.leading, 10)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("Step \(step.number) of \(SetupStep.count), \(step.title)")
            .accessibilityAddTraits(.isStaticText)
            Text("Step \(step.number) of \(SetupStep.count)").font(.system(size: 12)).foregroundStyle(palette.secondary).padding(.leading, 8)
                .accessibilityHidden(true)
            Spacer(minLength: 8)
            if model.canCancelSetup {
                Button("Cancel", action: model.cancelSetup).glassAction().actionControlSize().keyboardShortcut(.cancelAction)
            }
            if let alternate = model.setupAlternateTitle {
                Button(alternate, action: model.alternateSetup).glassAction().actionControlSize()
            }
            Button(model.setupPrimaryTitle, action: model.nextSetup)
                .prominentAction().actionControlSize().keyboardShortcut(.defaultAction)
                .disabled(!model.setupCanContinue).focused(focus, equals: .primary)
        }
        // 36-point buttons sit 10 in from every edge of the 56-point capsule.
        .padding(.horizontal, 10).frame(height: 56)
        .frame(maxWidth: 860)
        .glassSurface(Capsule())
    }
}

private extension View {
    /// Setup's data step accepts a folder dropped anywhere on its card.
    func dropZone(model: LauncherModel, targeted: Binding<Bool>) -> some View {
        dropDestination(for: URL.self) { urls, _ in
            guard let url = urls.first else { return false }
            _ = model.chooseData(url)
            return true
        } isTargeted: { targeted.wrappedValue = $0 }
    }
}

/// The setup screen's full-bleed dusk (or daylight) scene, behind a heavier vignette.
struct SetupBackdrop: View {
    @Environment(\.palette) private var palette
    var body: some View {
        ZStack {
            HeroScene(scenery: .desert, dark: palette.dark)
            GrainOverlay(opacity: palette.dark ? 0.13 : 0.1, blend: palette.dark ? .overlay : .multiply)
            if palette.dark {
                EllipticalGradient(stops: [.init(color: .black.opacity(0.55), location: 0), .init(color: .black.opacity(0.45), location: 0.6), .init(color: .black.opacity(0.25), location: 1)],
                                   center: UnitPoint(x: 0.5, y: 0.46), startRadiusFraction: 0, endRadiusFraction: 0.7)
            } else {
                EllipticalGradient(stops: [.init(color: palette.ground.opacity(0.75), location: 0), .init(color: palette.ground.opacity(0.55), location: 0.6), .init(color: palette.ground.opacity(0.3), location: 1)],
                                   center: UnitPoint(x: 0.5, y: 0.46), startRadiusFraction: 0, endRadiusFraction: 0.7)
            }
        }
        .accessibilityHidden(true)
    }
}
