import SwiftUI
import AppKit

// Owner: ship-settings. The Settings window's panes. Their content moved here unchanged from the old
// Settings and About pages; ship-settings rebuilds each pane against the SettingsWin board.

/// Personal settings (frame cap, mouse, sound, filtering) live in loadouts once CoD2 Silicon has
/// brought them over. Until then the launcher still applies them, so Display keeps them.
@MainActor private func legacy(_ model: LauncherModel) -> Bool { !model.usesLoadouts }

// MARK: - General

struct GeneralPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            if legacy(model) {
                StudioBanner(title: "Your loadouts aren’t set up yet, so the mouse, sound and frame rate settings in Display still apply to every game.",
                             detail: model.configStudio?.migrationError, tone: .caution)
            } else {
                ConfigsLink { model.showMainWindow(); model.navigate(to: .configs) }
            }
            Text("Changes save automatically and apply from your next game.")
                .font(.system(size: 12)).foregroundStyle(palette.secondary).padding(.leading, 6)
            if let error = model.settingsSaveError {
                Banner(.error, message: error) { Button("Try Again", action: model.saveSettings).glassAction() }
            }
            SettingsSection("Game") { gameGroup }
        }
        .onAppear { model.refreshCoD2xFiles() }
    }

    /// CoD2x is built into the engine, so the app carries two and starts the one chosen here.
    private var gameGroup: some View {
        SettingsGroup {
            SettingsRow("Version", detail: model.originalAvailable ? "Original 1.3 can only join Stock 1.3 servers." : "This copy includes CoD2x only. Download the full app to use Original 1.3.") {
                if model.originalAvailable {
                    Picker("Game version", selection: Binding(get: { model.chosenEngine }, set: { model.settings.engine = $0 })) {
                        Text("CoD2x").tag(GameEngine.cod2x); Text("Original 1.3").tag(GameEngine.original)
                    }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
                } else {
                    Text("CoD2x").font(.system(size: 13, weight: .semibold))
                    Link("View Release", destination: AppUpdate.releases).font(.system(size: 13)).foregroundStyle(palette.link)
                }
            }
            cod2xFilesRow
            SettingsNote(text: model.chosenEngine == .original ? "Only CoD2x uses these files. Choose CoD2x above to change them."
                : "CoD2x’s own menus and icons. Downloaded from the CoD2x project’s release on GitHub (2.6\u{00A0}MB) and kept on this Mac.")
        }
    }

    /// CoD2x's menus and icons, which the app doesn't ship: fetched or copied only when the player asks.
    private var cod2xFilesRow: some View {
        let original = model.chosenEngine == .original
        return SettingsRow("CoD2x files", detail: model.cod2xFilesMessage) {
            HStack(spacing: 10) {
                if model.cod2xFilesBusy { ProgressView().controlSize(.small).accessibilityLabel(model.cod2xFilesMessage) }
                if model.cod2xFilesBusy {
                    Button("Cancel", action: model.cancelCoD2xFiles).glassAction()
                } else if model.cod2xFilesInstalled {
                    Button("Remove", action: model.removeCoD2xFiles).glassAction()
                } else {
                    Button("Choose Folder…", action: model.chooseCoD2xFolder).glassAction()
                        .help("Copy them from a Windows Call of Duty 2 folder that has CoD2x installed")
                    Button("Download", action: model.downloadCoD2xFiles).glassAction()
                }
            }
            .fixedSize()
            .disabled(original && !model.cod2xFilesBusy)
            .accessibilityHint(original ? "Only CoD2x uses these files" : "")
        }
    }
}

/// Where personal settings went: a calm link row into Configs.
struct ConfigsLink: View {
    var open: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        Button(action: open) {
            HStack(spacing: 16) {
                Image(systemName: LauncherPage.configs.symbol).font(.system(size: 16)).foregroundStyle(palette.accent)
                    .frame(width: 36, height: 36).background(palette.selection, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 3) {
                    Text("Looking for frame rate, mouse, brightness, network, HUD or sound?").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading)
                    Text("They moved into your loadouts, so every loadout keeps its own.").font(.system(size: 12)).foregroundStyle(palette.secondary)
                }
                Spacer(minLength: 8)
                Text("Open Configs ›").font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.link)
            }
            .padding(.vertical, 14).padding(.horizontal, SettingsMetrics.radius)
            .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
            .shapeAudit(.container, .rounded(SettingsMetrics.radius), "configs link")
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Frame rate, mouse, brightness, network, HUD and sound moved into your loadouts. Open Configs.")
    }
}

// MARK: - Display

struct DisplayPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var customFPS = false
    var body: some View {
        SettingsPaneScroll {
            SettingsSection("Display") { displayGroup }
            if legacy(model) {
                SettingsSection("Frame rate") {
                    FrameRateCrates(fps: $model.settings.fps, custom: $customFPS)
                } footer: {
                    Text("Any cap from 0 to 1000 fps, where 0 means no cap. Servers can enforce their own; CoD2x competitive servers keep it between 125 and 250 fps.")
                }
                SettingsSection("Mouse and sound") { mouseGroup }
                SettingsSection("Graphics") { filteringGroup }
            }
        }
        .onAppear { customFPS = !FrameCap.isPreset(model.settings.fps) }
    }

    private var displayGroup: some View {
        SettingsGroup {
            SettingsRow("Resolution") {
                Picker("Resolution", selection: $model.settings.resolution) {
                    ForEach(GameSettings.resolutions, id: \.self) { Text(Self.resolutionLabel($0)).tag($0) }
                }
                .labelsHidden().fixedSize()
            }
            SettingsRow("Display mode") {
                PopUpButton("Display mode", selection: $model.settings.fullscreen,
                            options: [("exclusive", "Exclusive"), ("borderless", "Borderless"), ("spaces", "macOS full screen"), ("windowed", "Window")])
            }
            SettingsNote(text: Self.modeHint(model.settings.fullscreen))
            SettingsRow("Vertical sync", detail: "Off keeps input latency lowest.") {
                SwitchToggle("Vertical sync", isOn: $model.settings.vsync)
            }
        }
    }

    private var mouseGroup: some View {
        SettingsGroup {
            SettingsRow("Raw mouse input", detail: "Bypasses macOS pointer acceleration.") {
                SwitchToggle("Raw mouse input", isOn: $model.settings.rawMouse)
            }
            SettingsRow("Sensitivity", fixedLabel: 120) {
                Slider(value: $model.settings.sensitivity, in: 1...20, step: 0.1).accessibilityLabel("Sensitivity")
                    .accessibilityValue(String(format: "%.1f", model.settings.sensitivity))
                Text(String(format: "%.1f", model.settings.sensitivity)).font(.system(size: 14, weight: .semibold)).monospacedDigit().frame(width: 40, alignment: .trailing)
            }
            SettingsRow("Mouse DPI") {
                TextField("DPI", value: $model.settings.dpi, format: .number.grouping(.never)).textFieldStyle(.plain).multilineTextAlignment(.trailing)
                    .padding(.horizontal, 12).frame(width: 80, height: ControlMetrics.height(.large))
                    .background(palette.field, in: Capsule())
                    .overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1))
                    .shapeAudit(.field, .capsule, "DPI field")
                    .accessibilityLabel("Mouse DPI")
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text(MouseMath.label(model.settings.cm360)).font(.stencil(20)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f))
                        .contentTransition(.numericText())
                    Text("cm / 360°").font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.secondary)
                }
                .frame(width: 150, alignment: .trailing)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("\(MouseMath.label(model.settings.cm360)) centimetres per full turn")
            }
            SettingsRow("Volume", fixedLabel: 120) {
                Slider(value: $model.settings.volume, in: 0...1).accessibilityLabel("Game volume")
                Text("\(Int((model.settings.volume * 100).rounded()))%").font(.system(size: 14, weight: .semibold)).monospacedDigit().frame(width: 40, alignment: .trailing)
            }
        }
    }

    private var filteringGroup: some View {
        SettingsGroup {
            SettingsRow("Anisotropic filtering") {
                Picker("Anisotropic filtering", selection: $model.settings.anisotropy) {
                    ForEach(GameSettings.anisotropyLevels, id: \.self) { Text("\($0)×").tag($0) }
                }
                .labelsHidden().fixedSize()
            }
        }
    }

    static func resolutionLabel(_ value: String) -> String {
        let pretty = value.replacingOccurrences(of: "x", with: " × ")
        return ["3840x2160": " (4K)", "5120x2880": " (5K)", "6016x3384": " (6K)"][value].map { pretty + $0 } ?? pretty
    }
    static func modeHint(_ mode: String) -> String {
        switch mode {
        case "borderless": "A borderless window over the whole display. Fast switching between apps."
        case "spaces": "macOS fullscreen in its own Space. Required for Game Mode."
        case "windowed": "A regular window at the chosen size."
        default: "Exclusive fullscreen at the chosen resolution. Lowest latency."
        }
    }
}

// MARK: - Game Data

struct GameDataPane: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        SettingsPaneScroll {
            FormSection("Game data") {
                FormGroup {
                    // Tall enough that the buttons keep the group's corner radius from its top and bottom
                    // (plus the point the group hides under its first hairline).
                    FormRow(model.dataPathDisplay.isEmpty ? "Not found" : model.dataPathDisplay,
                            detail: model.approximate ? "Run setup again to build the original ones." : nil, monospacedTitle: true,
                            minHeight: ControlMetrics.height(.regular) + 2 * FormMetrics.radius(.panel) + 1) {
                        StatusLabel(model.approximate ? .needsChange : .ready, model.approximate ? "Approximate shaders" : "Original shaders verified")
                        SymbolButton("folder", label: "Show game data in Finder", action: model.showGameDataInFinder).disabled(model.dataPath.isEmpty)
                        Button("Set Up Again…", action: model.setUpAgain).glassAction()
                    }
                }
            }
        }
    }
}

// MARK: - Privacy

struct PrivacyPane: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        SettingsPaneScroll {
            SettingsSection("Privacy") { privacyGroup } footer: {
                Text("Turning off match summaries keeps the ones already on this Mac.").fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    /// The game's local match summaries (`main/stats/matches.jsonl`), passed to it at launch.
    private var privacyGroup: some View {
        SettingsGroup {
            SettingsRow("Local match summaries",
                        detail: "On by default. Keeps your last \(MatchSummaries.recordLimit) matches on this Mac for Check-up: map, mode, frame times, ping and missed server updates. Never sent anywhere.") {
                SwitchToggle("Local match summaries", isOn: $model.settings.matchSummariesEnabled)
            }
            // Only a build with a report receiver offers it; without one nothing is ever sent.
            if model.crashReportService != nil {
                SettingsRow("Send crash reports to CoD2 Silicon",
                            detail: "Off by default. Sends crashes and freezes to CoD2 Silicon’s Sentry: what failed, where in the game, macOS version and Mac model. Never your CD key or home folder’s name.", titleLines: 2) {
                    SwitchToggle("Send crash reports to CoD2 Silicon", isOn: $model.settings.sendCrashReports)
                }
            }
            cod2xReportsRow
        }
    }

    /// CoD2x's own crash reports, which the CoD2x engine sends the way CoD2x does once the player agrees.
    private var cod2xReportsRow: some View {
        let original = model.chosenEngine == .original
        return SettingsRow("Send CoD2x crash reports to the CoD2x project",
                           detail: original ? "Only CoD2x sends these. Choose CoD2x in General › Version to change this."
                               : "Off by default. Sends faults in CoD2x’s own code to the CoD2x project, as CoD2x does. Includes settings-folder paths and the list of loaded files.", titleLines: 2) {
            SwitchToggle("Send CoD2x crash reports to the CoD2x project", isOn: $model.settings.sendCoD2xReports)
                .disabled(original)
                .accessibilityHint(original ? "Only CoD2x sends these reports" : "")
        }
    }
}

// MARK: - Updates

struct UpdatesPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            SettingsSection("Updates") {
                UpdateStatusView(model: model)
                    .padding(SettingsMetrics.radius).contentSurface(RoundedRectangle(cornerRadius: SettingsMetrics.radius))
            } footer: {
                Text("Nothing is downloaded or installed until you choose Install Update.")
            }
        }
    }
}

/// The same release status and actions wherever someone checks for an update.
struct UpdateStatusView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .top, spacing: 10) {
                if model.checkingUpdates || model.installingUpdate { ProgressView().controlSize(.small).accessibilityLabel(model.updateMessage) }
                else { Image(systemName: model.updateError ? "exclamationmark.triangle" : model.releaseURL != nil ? "arrow.down.circle" : model.updateWasChecked ? "checkmark.circle" : "arrow.clockwise").foregroundStyle(model.updateError ? palette.negative : palette.secondary).accessibilityHidden(true) }
                VStack(alignment: .leading, spacing: 4) {
                    Text("CoD2 Silicon \(model.version)").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading)
                    Text(model.updateMessage).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                }
            }
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 10) { actions }
                VStack(alignment: .leading, spacing: 10) { actions }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .contain)
    }
    @ViewBuilder private var actions: some View {
        if model.updateOffer != nil {
            Button("Install Update", action: model.installUpdate).glassAction().disabled(model.installingUpdate || model.checkingUpdates)
                .help("Download this version, check it and replace the app. CoD2 Silicon restarts.")
        } else {
            Button(model.checkingUpdates ? "Checking…" : "Check for Updates…", action: model.checkUpdates).glassAction().disabled(model.checkingUpdates)
        }
        if let url = model.releaseURL { Link("View Release", destination: url).font(.system(size: 13)).foregroundStyle(palette.link) }
    }
}

// MARK: - Advanced

struct AdvancedPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            SettingsSection("Advanced launch options") { advancedGroup }
        }
    }

    private var advancedGroup: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Extra options passed to the game when it starts, one per line. For experts.")
                .font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            TextEditor(text: Binding(get: { model.advancedDraft ?? model.settings.advanced }, set: model.editAdvancedOptions))
                .font(.typewriter(13)).scrollContentBackground(.hidden)
                .padding(.horizontal, 8).padding(.vertical, 8)
                .frame(height: 112)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).stroke(palette.wellStroke, lineWidth: 1))
                .shapeAudit(.inset, .rounded(12), "dvar editor")
                .accessibilityLabel("Extra launch options")
            if let error = model.advancedError {
                Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                    .fixedSize(horizontal: false, vertical: true)
                Text("Your last valid options still apply.").font(.system(size: 12)).foregroundStyle(palette.secondary)
            }
            Text(legacy(model) ? "Paths, passwords and your CD key are managed by the launcher and can't be set here."
                        : "Paths, passwords and your CD key are managed by the launcher and can't be set here. Settings that belong to a loadout are changed in Configs; the game ignores them here.")
                .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, 14).padding(.horizontal, SettingsMetrics.radius)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(SettingsMetrics.radius), "advanced panel")
    }
}

// MARK: - About (moved from the old About page)

struct AboutPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            HStack(alignment: .center, spacing: 24) {
                BrandMark().frame(width: 96, height: 96).shadow(color: .black.opacity(palette.dark ? 0.45 : 0.18), radius: 16, y: 8)
                VStack(alignment: .leading, spacing: 8) {
                    Text("COD2 SILICON").font(.stencil(56)).tracking(1.1).lineLimit(1).minimumScaleFactor(0.6).foregroundStyle(palette.dark ? palette.heading : Palette.hex(0x1f2214))
                    Text("VERSION \(model.version) · MACOS 26 OR LATER").font(.typewriter(13)).foregroundStyle(palette.dark ? Palette.hex(0xd9c79a) : palette.address)
                    Text("Call of Duty 2 multiplayer, native on Apple silicon. Plays on stock 1.3 and CoD2x 1.4.6.8 servers.")
                        .font(.system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xddd5c3) : Palette.hex(0x3a382e))
                        .frame(maxWidth: 520, alignment: .leading).fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(.leading, 6)
            HStack(alignment: .top, spacing: 16) {
                ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
                    PanelHeading(title: "Updates", action: nil, perform: nil)
                    UpdateStatusView(model: model).padding(.top, 12)
                    Text("Nothing is downloaded or installed until you choose Install Update.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
                        .fixedSize(horizontal: false, vertical: true).padding(.top, 12)
                    Spacer(minLength: 0)
                }
                ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
                    PanelHeading(title: "Credits", action: "Full credits") { NSWorkspace.shared.open(Credits.full) }
                    VStack(alignment: .leading, spacing: 10) {
                        ForEach(Credits.entries, id: \.name) { credit($0.name, $0.detail) }
                    }
                    .padding(.top, 14)
                    Spacer(minLength: 0)
                }
                ContentPanel(radius: 20, padding: EdgeInsets(top: 20, leading: 20, bottom: 20, trailing: 20)) {
                    PanelHeading(title: "Licence", action: "Notice") { NSWorkspace.shared.open(Credits.notice) }
                    Text("MIT covers this project's own changes. Upstream opencod2 keeps its authors' rights, and third-party licences apply.")
                        .font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(3).padding(.top, 12).fixedSize(horizontal: false, vertical: true)
                    Text("Call of Duty is a trademark of Activision. This independent project isn't affiliated with Activision, Infinity Ward, Aspyr or CoD2x. Bring your own licensed game.")
                        .font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(3).padding(.top, 10).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                }
            }
            .fixedSize(horizontal: false, vertical: true)
        }
        .background(alignment: .top) {
            HeroBackdrop(map: "mp_carentan", image: nil).frame(height: 260).clipped().opacity(0.55).allowsHitTesting(false)
        }
    }
    private func credit(_ name: String, _ detail: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(name).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
            Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary)
        }
        .accessibilityElement(children: .combine)
    }
}

// MARK: - Shared with Config Studio (moved from the old Settings page)

/// Ammo-crate frame-cap quick picks (`FrameCap.presets`) and a custom value from 0 (no cap) to 1000.
struct FrameRateCrates: View {
    @Binding var fps: Int
    @Binding var custom: Bool
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 104), spacing: 10)], alignment: .leading, spacing: 10) {
            ForEach(FrameCap.presets, id: \.value) { preset in
                crate("\(preset.value)", preset.caption, selected: !custom && fps == preset.value) { fps = preset.value; custom = false }
            }
            if custom {
                VStack(alignment: .leading, spacing: 6) {
                    TextField("FPS", value: Binding(get: { fps }, set: { fps = min(1000, max(0, $0)) }), format: .number.grouping(.never)).textFieldStyle(.plain)
                        .font(.stencil(34)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f)).frame(height: 34)
                        .accessibilityLabel("Custom frame cap, 0 to 1000 frames per second, 0 for no cap")
                    Text("fps · \(FrameCap.customCaption(fps))").font(.system(size: 11)).foregroundStyle(palette.secondary)
                }
                .crateStyle(selected: true, palette: palette)
            } else {
                Button { withAnimation(reduceMotion ? nil : .launcherSpring) { custom = true } } label: {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("—").font(.stencil(34)).foregroundStyle(palette.numeral).frame(height: 34)
                        Text("Custom value").font(.system(size: 11)).foregroundStyle(palette.secondary)
                    }
                    .crateStyle(selected: false, palette: palette)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Custom frame cap")
            }
        }
    }
    private func crate(_ number: String, _ caption: String, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 6) {
                Text(number).font(.stencil(34)).tracking(0.68).foregroundStyle(selected ? (palette.dark ? palette.accent : Palette.hex(0x4e5b2f)) : palette.numeral).frame(height: 34)
                Text("fps · \(caption)").font(.system(size: 11)).foregroundStyle(palette.secondary).lineSpacing(1.5).fixedSize(horizontal: false, vertical: true)
            }
            .crateStyle(selected: selected, palette: palette)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("\(number) frames per second, \(caption)")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

private extension View {
    func crateStyle(selected: Bool, palette: Palette) -> some View {
        self.padding(.vertical, 14).padding(.horizontal, 16)
            .frame(maxWidth: .infinity, minHeight: 112, alignment: .topLeading)
            .background(selected ? palette.selection : palette.crate, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(selected ? palette.selectionStroke.opacity(1.3) : (palette.dark ? Color.white.opacity(0.09) : palette.panelStroke), lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
            .shapeAudit(.container, .rounded(16), "crate")
    }
}
