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

struct SetupView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    private enum Focus: Hashable { case folder, key, primary }
    @FocusState private var focus: Focus?
    @State private var dropTarget = false
    private let footerInset: CGFloat = 14
    private var cardRadius: CGFloat { footerInset + ControlMetrics.action / 2 }

    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                VStack(spacing: 10) {
                    Eyebrow(text: "Reporting for duty", size: 14, tracking: 0.4)
                    Text("Set up CoD2 Silicon").font(.system(size: 34, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                    Text("Your game files, your CD key, your Mac.").font(.system(size: 15)).foregroundStyle(palette.secondary)
                }
                StepIndicator(step: model.step, approximate: model.approximate)
                card
            }
            .padding(.top, 36).padding(.bottom, 24)
            .frame(maxWidth: .infinity)
        }
        .background { SetupBackdrop().ignoresSafeArea() }
        .task(id: model.step * 2 + (model.replacingKey ? 1 : 0)) {
            // The new step must be mounted before its focus target can accept focus.
            focus = nil
            try? await Task.sleep(for: .milliseconds(250))
            if !Task.isCancelled { focusStep() }
        }
        .onChange(of: focus) { _, value in SecureKeyEntry.set(value == .key && !model.snapshot) }
        .onDisappear { SecureKeyEntry.set(false) }
        .animation(.easeInOut(duration: reduceMotion ? 0.15 : 0.22), value: model.step)
    }
    private func focusStep() {
        focus = model.step == 0 ? .folder : model.step == 1 && (!model.keyVerified || model.replacingKey) ? .key : .primary
    }
    private var card: some View {
        VStack(alignment: .leading, spacing: 0) {
            VStack(alignment: .leading, spacing: 16) {
                Group {
                    switch model.step {
                    case 0: dataStep
                    case 1: keyStep
                    case 2: shaderStep
                    default: summaryStep
                    }
                }
                .id(model.step).transition(.opacity)
                if !model.setupError.isEmpty {
                    Label(model.setupError, systemImage: "exclamationmark.circle")
                        .font(.system(size: 13)).foregroundStyle(palette.negative).fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(.top, 28).padding(.horizontal, 28)
            HStack(spacing: 8) {
                Button("Back", action: model.backSetup).glassAction().actionControlSize()
                    .disabled(model.step == 0 || model.shaderBusy)
                if model.canCancelSetup {
                    Button("Cancel", action: model.cancelSetup).glassAction().actionControlSize().keyboardShortcut(.cancelAction)
                } else {
                    Button("Quit") { NSApp.terminate(nil) }.glassAction().actionControlSize().keyboardShortcut(.cancelAction)
                }
                Spacer(minLength: 8)
                if model.step == 3 && !model.hasPendingLaunch && !model.approximate {
                    Button("Find a Server") { model.finishSetup(findServer: true) }.glassAction().actionControlSize()
                }
                Button(primaryTitle, action: model.nextSetup)
                    .prominentAction().actionControlSize().keyboardShortcut(.defaultAction)
                    .disabled(!model.setupCanContinue).focused($focus, equals: .primary)
            }
            .padding(.top, 24).padding([.horizontal, .bottom], footerInset)
        }
        .frame(width: 720)
        .contentSurface(RoundedRectangle(cornerRadius: cardRadius, style: .continuous))
        .accessibilityElement(children: .contain)
    }
    private var primaryTitle: String {
        switch model.step {
        case 0: "Continue"
        case 1: model.keyVerified && !model.replacingKey ? "Continue" : "Save Key and Continue"
        case 2: model.shaderBusy ? "Preparing Shaders…" : "Review Setup"
        default: model.setupCompletionTitle
        }
    }
    private func heading(_ title: String, detail: String, symbol: String) -> some View {
        HStack(alignment: .top, spacing: 16) {
            Image(systemName: symbol).font(.system(size: 34, weight: .light)).foregroundStyle(palette.accent)
                .frame(width: 44, height: 44).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 6) {
                Text(title).font(.system(size: 22, weight: .bold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
                Text(detail).font(.system(size: 14)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
    private var dataStep: some View {
        VStack(alignment: .leading, spacing: 16) {
            heading(model.dataError.isEmpty && !model.dataPath.isEmpty ? "Game data found" : "Find your game data",
                    detail: "Choose the Call of Duty 2 folder from your own copy. Your game files stay where they are.", symbol: "folder")
            VStack(alignment: .leading, spacing: 12) {
                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(model.dataError.isEmpty ? (model.dataPath.isEmpty ? "Drop your game folder here" : model.dataPathDisplay) : (model.attemptedDataPath as NSString).abbreviatingWithTildeInPath)
                            .font(.system(size: 14, weight: .medium)).foregroundStyle(palette.text)
                            .textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                            .id(model.dataPath + model.attemptedDataPath + model.dataError)
                        if model.dataError.isEmpty && !model.dataPath.isEmpty {
                            Label("All 16 game archives found", systemImage: "checkmark.circle.fill").foregroundStyle(palette.positive)
                        } else if model.dataError.isEmpty {
                            Text("Or choose a folder below.").foregroundStyle(palette.secondary)
                        }
                    }
                    Spacer(minLength: 0)
                    Button(model.dataPath.isEmpty || !model.dataError.isEmpty ? "Choose Folder…" : "Change…", action: model.pickData)
                        .glassAction().actionControlSize().focused($focus, equals: .folder)
                }
                if !model.dataError.isEmpty {
                    Label(model.dataError, systemImage: "exclamationmark.circle.fill").foregroundStyle(palette.negative)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .font(.system(size: 13)).padding(20)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 18))
            .overlay(RoundedRectangle(cornerRadius: 18).stroke(dropTarget ? palette.accent : palette.wellStroke, lineWidth: dropTarget ? 3 : 1))
            .shapeAudit(.container, .rounded(18), "data well")
            .dropDestination(for: URL.self) { urls, _ in
                guard let url = urls.first else { return false }
                _ = model.chooseData(url)
                return true
            } isTargeted: { dropTarget = $0 }
            Link("Where to find your game data", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/docs/macos-port/game-data.md")!)
                .font(.system(size: 13)).foregroundStyle(palette.link)
        }
    }
    @ViewBuilder private var keyStep: some View {
        if model.keyVerified && !model.replacingKey {
            heading("CD key verified", detail: "Your key is stored on this Mac. You can continue without entering it again.", symbol: "checkmark.shield")
            HStack {
                Label("Stored on this Mac", systemImage: "lock.fill").foregroundStyle(palette.positive)
                Spacer()
                Button("Replace Key…", action: model.replaceKey).glassAction().actionControlSize()
            }
            .font(.system(size: 14)).padding(20).background(palette.well, in: RoundedRectangle(cornerRadius: 18))
        } else {
            heading(model.keyVerified ? "Replace your CD key" : "Your CD key", detail: "Enter the 20-character key that came with your game to unlock multiplayer.", symbol: "key")
            TextField("CD key", text: Binding(get: { KeyFormat.display(model.keyInput) }, set: { model.keyInput = KeyFormat.display($0); model.setupError = "" }),
                      prompt: Text("XXXX XXXX XXXX XXXX XXXX").foregroundColor(palette.tertiary))
                .textFieldStyle(.plain).font(.typewriter(22, bold: true)).tracking(2)
                .foregroundStyle(palette.text).autocorrectionDisabled().focused($focus, equals: .key)
                .padding(.horizontal, 20).frame(height: 54)
                .background(palette.well, in: Capsule())
                .overlay(Capsule().stroke(focus == .key ? palette.accent : palette.wellStroke, lineWidth: focus == .key ? 3 : 1))
                .shapeAudit(.field, .capsule, "key field")
                .onSubmit { if model.keyReady { model.nextSetup() } }
                .accessibilityLabel("CD key, twenty characters")
            let raw = KeyFormat.groups(model.keyInput).joined()
            Label(keyHint(raw), systemImage: raw.count == 20 ? (KeyStore.valid(model.keyInput) ? "checkmark.circle" : "exclamationmark.circle") : "info.circle")
                .font(.system(size: 13)).foregroundStyle(raw.count == 20 ? (KeyStore.valid(model.keyInput) ? palette.positive : palette.negative) : palette.secondary)
                .fixedSize(horizontal: false, vertical: true)
            if model.keyVerified {
                Button("Keep Stored Key", action: model.keepStoredKey).glassAction()
            }
        }
        Label("Only your account can read the saved key. Joining a server uses it to verify your copy of the game.", systemImage: "lock.shield")
            .font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
    }
    private func keyHint(_ raw: String) -> String {
        if raw.isEmpty { return "Spaces and dashes are fine." }
        if raw.count < 20 { return "\(20 - raw.count) characters to go" }
        return KeyStore.valid(model.keyInput) ? "Key verified. Save it to continue." : "This key doesn’t match. Check each character against the key that came with your game."
    }
    private var shaderStep: some View {
        VStack(alignment: .leading, spacing: 16) {
            heading("Original shaders", detail: "Prepare the game’s original lighting, sky and water from your own game files.", symbol: "sun.horizon")
            if model.shaderBusy {
                HStack {
                    Text(model.shaderMessage)
                    Spacer()
                    Text("\(model.shaderCount) of \(LauncherModel.shaderTotal) shaders").monospacedDigit()
                }.font(.system(size: 13)).foregroundStyle(palette.secondary)
                ProgressBar("Shader setup", value: Double(model.shaderCount) / Double(max(1, LauncherModel.shaderTotal)))
                    .accessibilityLabel("Shader setup").accessibilityValue("\(model.shaderCount) of \(LauncherModel.shaderTotal) shaders")
                Text("This usually takes a few seconds. Nothing is downloaded. You can leave setup while it finishes.")
                    .font(.system(size: 13)).foregroundStyle(palette.secondary)
            } else {
                Label(model.approximate ? "Approximate shaders available" : "Original shaders ready", systemImage: model.approximate ? "exclamationmark.triangle" : "checkmark.circle")
                    .foregroundStyle(model.approximate ? palette.caution : palette.positive)
                if model.approximate { shaderFix }
            }
        }
    }
    private var summaryStep: some View {
        VStack(alignment: .leading, spacing: 16) {
            heading("Ready to play", detail: model.approximate ? "You can play now with simpler lighting, or add the original shaders below." : "Your game is ready. Choose what you’d like to do next.", symbol: model.approximate ? "exclamationmark.triangle" : "checkmark.circle")
            VStack(spacing: 0) {
                summaryRow("Game data found", detail: model.dataPathDisplay, symbol: "folder", caution: false)
                Divider().overlay(palette.separator)
                summaryRow("CD key verified", detail: "Stored on this Mac", symbol: "lock.shield", caution: false)
                Divider().overlay(palette.separator)
                summaryRow(model.approximate ? "Approximate shaders" : "Original shaders ready", detail: model.approximate ? "Clouds and much of the lighting detail will be missing." : "Original lighting, sky and water", symbol: model.approximate ? "exclamationmark.triangle" : "sun.horizon", caution: model.approximate)
            }
            .background(palette.well, in: RoundedRectangle(cornerRadius: 18))
            if model.approximate { shaderFix }
            if let address = model.setupServerAddress {
                Label("Next: join \(address)", systemImage: "network").font(.system(size: 13)).foregroundStyle(palette.secondary)
            }
        }
    }
    private func summaryRow(_ title: String, detail: String, symbol: String, caution: Bool) -> some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).frame(width: 20).foregroundStyle(caution ? palette.caution : palette.positive).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 4) {
                Text(title).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }.padding(16).accessibilityElement(children: .ignore)
            .accessibilityLabel("\(title). \(detail)").accessibilityAddTraits(.isStaticText)
    }
    private var shaderFix: some View {
        HStack(spacing: 8) {
            Text("Try again, or use shaders from your Mac copy.").font(.system(size: 12)).foregroundStyle(palette.secondary)
            Spacer(minLength: 0)
            Button("Try Again") { model.prepareShaders() }.glassAction().actionControlSize()
            Button("Choose Mac Copy…", action: model.pickShaders).glassAction().actionControlSize()
        }
    }
}

struct StepIndicator: View {
    var step: Int
    var approximate: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 14) {
            ForEach(Array(["Game data", "CD key", "Shaders", "Summary"].enumerated()), id: \.offset) { index, label in
                let complete = index < step, current = index == step
                let caution = complete && index == 2 && approximate
                HStack(spacing: 10) {
                    ZStack {
                        Circle().fill(caution ? palette.caution : complete ? palette.positive : current ? palette.accent : (palette.dark ? Color.white.opacity(0.1) : Color.black.opacity(0.06)))
                        if !complete && !current { Circle().stroke(palette.dark ? Color.white.opacity(0.18) : Color.black.opacity(0.15), lineWidth: 1) }
                        if complete { Image(systemName: caution ? "exclamationmark.triangle.fill" : "checkmark").font(.system(size: 12, weight: .bold)).foregroundStyle(palette.dark ? Palette.hex(0x10140c) : .white) }
                        else { Text("\(index + 1)").font(.system(size: 13, weight: .bold)).foregroundStyle(caution ? palette.caution : current ? (palette.dark ? palette.prominentText : .white) : palette.secondary) }
                    }
                    .frame(width: 28, height: 28)
                    Text(label).font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(caution ? palette.caution : current ? (palette.dark ? Palette.hex(0xf6efdf) : palette.heading) : complete ? (palette.dark ? Palette.hex(0xcfe3c4) : palette.positive) : palette.secondary)
                    if index < 3 { Rectangle().fill(palette.dark ? Color.white.opacity(0.18) : Color.black.opacity(0.15)).frame(width: 28, height: 1).padding(.leading, 4) }
                }
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Step \(index + 1) of 4, \(label), \(complete ? (index == 2 && approximate ? "approximate" : "complete") : current ? "current" : "not started")")
                .accessibilityAddTraits(.isStaticText)
            }
        }
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
                EllipticalGradient(stops: [.init(color: .black.opacity(0.25), location: 0.35), .init(color: .black.opacity(0.8), location: 1)],
                                   center: UnitPoint(x: 0.5, y: 0.45), startRadiusFraction: 0, endRadiusFraction: 0.75)
            } else {
                EllipticalGradient(stops: [.init(color: palette.ground.opacity(0.35), location: 0.35), .init(color: palette.ground.opacity(0.85), location: 1)],
                                   center: UnitPoint(x: 0.5, y: 0.45), startRadiusFraction: 0, endRadiusFraction: 0.75)
            }
        }
        .accessibilityHidden(true)
    }
}
