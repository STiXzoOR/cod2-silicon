import SwiftUI
import AppKit

struct SetupView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @FocusState private var keyFocused: Bool

    var body: some View {
        ZStack {
            SetupBackdrop()
            VStack(spacing: 0) {
                VStack(spacing: 10) {
                    Eyebrow(text: "Reporting for duty", size: 14, tracking: 0.4, color: palette.dark ? palette.accent : palette.accent)
                    Text("Set up CoD2 Silicon").font(.system(size: 34, weight: .bold)).foregroundStyle(palette.dark ? Palette.hex(0xf6efdf) : palette.heading).accessibilityAddTraits(.isHeader)
                    Text("Three things from your own copy of the game. Nothing leaves this Mac.").font(.system(size: 15)).foregroundStyle(palette.dark ? Palette.hex(0xc9c1ad) : palette.secondary)
                }
                .padding(.top, 70)
                StepIndicator(step: model.step, done: model.step == 2 && model.shaderCount >= 834 && !model.shaderBusy).padding(.top, 34)
                card.padding(.top, 32)
                Spacer(minLength: 24)
            }
            .frame(maxWidth: .infinity)
        }
        .ignoresSafeArea()
    }

    private var card: some View {
        VStack(alignment: .leading, spacing: 22) {
            Group {
                switch model.step {
                case 0: dataStep
                case 1: keyStep
                default: shaderStep
                }
            }
            .transition(reduceMotion ? .opacity : .asymmetric(insertion: .move(edge: .trailing).combined(with: .opacity), removal: .move(edge: .leading).combined(with: .opacity)))
            .id(model.step)
            HStack {
                Button("Back", action: model.backSetup).glassAction().controlSize(.large)
                    .disabled(model.step == 0 || model.shaderBusy)
                Spacer()
                if model.step == 2 && !model.shaderBusy {
                    Button(action: model.nextSetup) {
                        Label(model.approximate ? "Deploy with approximate shaders" : "Deploy", systemImage: "play.fill")
                    }
                    .prominentAction().controlSize(.large).keyboardShortcut(.defaultAction)
                } else {
                    Button("Continue", action: model.nextSetup).prominentAction().controlSize(.large).keyboardShortcut(.defaultAction)
                        .disabled((model.step == 0 && model.dataPath.isEmpty) || (model.step == 1 && !model.keyReady) || model.shaderBusy)
                }
            }
            .padding(.top, 6)
        }
        .padding(.top, 34).padding(.horizontal, 38).padding(.bottom, 30)
        .frame(width: 680)
        .glassSurface(RoundedRectangle(cornerRadius: 30, style: .continuous))
        .animation(reduceMotion ? nil : .launcherSpring, value: model.step)
        .accessibilityElement(children: .contain)
    }

    @ViewBuilder private var dataStep: some View {
        VStack(alignment: .leading, spacing: 20) {
            let found = !model.dataPath.isEmpty
            HStack(alignment: .top, spacing: 18) {
                Image(systemName: found ? "folder.badge.checkmark" : "folder.badge.questionmark").font(.system(size: 40, weight: .light))
                    .symbolRenderingMode(.palette).foregroundStyle(found ? palette.positive : palette.accent, palette.accent)
                    .frame(width: 54, height: 54).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 6) {
                    Text(found ? "Game data found" : "Find your game data").font(.system(size: 21, weight: .bold)).foregroundStyle(palette.text)
                    Text(found ? "We use your installed Call of Duty 2 files where they are. Configs, demos and screenshots get their own folder."
                               : "Choose the Call of Duty 2 folder from your own copy. Windows or Mac 1.3 data works; nothing is copied.")
                        .font(.system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xc9c1ad) : palette.secondary).lineSpacing(5).fixedSize(horizontal: false, vertical: true)
                }
            }
            HStack(alignment: .center) {
                VStack(alignment: .leading, spacing: 10) {
                    Text(found ? model.dataPathDisplay : "No folder chosen").font(.typewriter(15)).foregroundStyle(palette.dark ? Palette.hex(0xefe7d3) : palette.text).lineLimit(1).truncationMode(.middle)
                    Text(found ? "All 16 game archives present · Version 1.3" : "Needs main/iw_00.iwd through iw_15.iwd")
                        .font(.system(size: 13)).foregroundStyle(found ? palette.positive : palette.secondary)
                }
                Spacer()
                Button(found ? "Change…" : "Choose Folder…", action: model.pickData).glassAction()
            }
            .padding(.vertical, 16).padding(.horizontal, 18)
            .background(palette.well, in: RoundedRectangle(cornerRadius: 18, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 18, style: .continuous).stroke(palette.wellStroke, lineWidth: 1))
            HStack(spacing: 4) {
                Text("Don't have the files yet?").foregroundStyle(palette.secondary)
                Link("Where to find your game data", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon#game-data")!).foregroundStyle(palette.link)
            }
            .font(.system(size: 13))
        }
    }

    @ViewBuilder private var keyStep: some View {
        let raw = KeyFormat.groups(model.keyInput).joined()
        HStack(alignment: .center, spacing: 30) {
            StampedDogTag(lines: KeyFormat.tag(model.keyInput))
            VStack(alignment: .leading, spacing: 14) {
                Text("Your CD key").font(.system(size: 21, weight: .bold)).foregroundStyle(palette.text)
                Text("The 20-character key that came with your game. It unlocks multiplayer on official and CoD2x servers.")
                    .font(.system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xc9c1ad) : palette.secondary).lineSpacing(5).fixedSize(horizontal: false, vertical: true)
                TextField("", text: Binding(get: { KeyFormat.display(model.keyInput) }, set: { model.keyInput = KeyFormat.display($0) }),
                          prompt: Text("XXXX XXXX XXXX XXXX XXXX").foregroundColor(palette.dark ? Palette.hex(0x6f6a5d) : palette.tertiary.opacity(0.7)))
                    .textFieldStyle(.plain).font(.typewriter(22, bold: true)).tracking(2.6)
                    .foregroundStyle(palette.dark ? Palette.hex(0xf1ead9) : palette.text)
                    .autocorrectionDisabled().focused($keyFocused)
                    .padding(.horizontal, 18).frame(height: 56)
                    .background(palette.dark ? Color.black.opacity(0.3) : Color.white.opacity(0.55), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(keyFocused ? palette.selectionStroke.opacity(1.5) : palette.wellStroke, lineWidth: keyFocused ? 1.5 : 1))
                    .onSubmit { if model.keyReady { model.nextSetup() } }
                    .accessibilityLabel("CD key, twenty characters")
                    .onAppear { keyFocused = !model.snapshot }
                Text(keyHint(raw)).font(.system(size: 13)).foregroundStyle(raw.count == 20 ? (model.keyReady ? palette.positive : palette.negative) : palette.secondary)
                    .accessibilityAddTraits(.updatesFrequently)
            }
        }
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: "lock.shield").font(.system(size: 16)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary).accessibilityHidden(true)
            Text("Stored only on this Mac, in a file only your account can read. Like the original game, joining a server sends the authorization service a one-way hash of it; the key itself never leaves this Mac.")
                .font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary).lineSpacing(6).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, 14).padding(.horizontal, 16)
        .background(palette.dark ? Color.black.opacity(0.24) : Color.white.opacity(0.4), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(palette.wellStroke.opacity(0.7), lineWidth: 1))
    }
    private func keyHint(_ raw: String) -> String {
        if raw.isEmpty { return "Spaces and dashes are fine." }
        if raw.count < 20 { return "\(20 - raw.count) characters to go" }
        return model.keyReady ? "Checksum verified." : "The checksum doesn't match. Check for a mistyped character."
    }

    @ViewBuilder private var shaderStep: some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack(alignment: .top, spacing: 18) {
                Image(systemName: "circle.lefthalf.filled").font(.system(size: 40, weight: .light)).foregroundStyle(palette.accent).frame(width: 54, height: 54).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 6) {
                    Text("Original shaders").font(.system(size: 21, weight: .bold)).foregroundStyle(palette.text)
                    Text("Lighting and sky exactly as the game shipped, taken from your own Mac copy of Call of Duty 2.")
                        .font(.system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xc9c1ad) : palette.secondary).lineSpacing(5).fixedSize(horizontal: false, vertical: true)
                }
            }
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text(model.approximate && !model.shaderBusy ? "Original Mac shaders weren't found" : model.shaderMessage)
                    Spacer()
                    Text("\(model.shaderCount) / 834").font(.typewriter(13)).monospacedDigit()
                }
                .font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
                ShaderProgressBar(fraction: Double(model.shaderCount) / 834)
                    .accessibilityElement().accessibilityLabel("Shader extraction").accessibilityValue("\(model.shaderCount) of 834")
                if model.approximate && !model.shaderBusy {
                    HStack(spacing: 10) {
                        Text("Approximate shaders change some lighting and skies.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                        Spacer()
                        Button("Choose Mac Copy…", action: model.pickShaders).glassAction()
                    }
                } else {
                    Text("No Mac copy? You can still play with approximate lighting and add it later in Settings.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                }
            }
        }
    }
}

struct ShaderProgressBar: View {
    var fraction: Double
    @Environment(\.palette) private var palette
    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(palette.dark ? Color.white.opacity(0.1) : Color.black.opacity(0.08))
                Capsule().fill(LinearGradient(colors: palette.dark ? [Palette.hex(0xb8893f), Palette.hex(0xe8c47e)] : [palette.prominentBottom, palette.prominentTop], startPoint: .leading, endPoint: .trailing))
                    .frame(width: max(10, proxy.size.width * min(1, max(0, fraction))))
                    .opacity(fraction > 0 ? 1 : 0)
            }
        }
        .frame(height: 10)
        .animation(.easeOut(duration: 0.3), value: fraction)
    }
}

struct StepIndicator: View {
    var step: Int
    var done: Bool
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 14) {
            ForEach(Array(["Game data", "CD key", "Shaders"].enumerated()), id: \.offset) { index, label in
                let complete = index < step || (done && index == 2), current = index == step && !complete
                HStack(spacing: 10) {
                    ZStack {
                        Circle().fill(complete ? palette.positive : current ? palette.accent : (palette.dark ? Color.white.opacity(0.1) : Color.black.opacity(0.06)))
                        if !complete && !current { Circle().stroke(palette.dark ? Color.white.opacity(0.18) : Color.black.opacity(0.15), lineWidth: 1) }
                        if complete { Image(systemName: "checkmark").font(.system(size: 12, weight: .bold)).foregroundStyle(palette.dark ? Palette.hex(0x10140c) : .white) }
                        else { Text("\(index + 1)").font(.system(size: 13, weight: .bold)).foregroundStyle(current ? (palette.dark ? palette.prominentText : .white) : palette.secondary) }
                    }
                    .frame(width: 28, height: 28)
                    Text(label).font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(current ? (palette.dark ? Palette.hex(0xf6efdf) : palette.heading) : complete ? (palette.dark ? Palette.hex(0xcfe3c4) : palette.positive) : palette.secondary)
                    if index < 2 { Rectangle().fill(palette.dark ? Color.white.opacity(0.18) : Color.black.opacity(0.15)).frame(width: 56, height: 1).padding(.leading, 4) }
                }
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Step \(index + 1), \(label), \(complete ? "complete" : current ? "current" : "not started")")
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
