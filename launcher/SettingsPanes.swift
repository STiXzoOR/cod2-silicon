import SwiftUI
import AppKit

// Owner: ship-settings. The Settings window's seven panes, built against the SettingsWin board: General,
// Display, Game Data, Privacy, Updates, Advanced and About. Every change saves itself (no Save button).

/// A single-row group with a button keeps the button as far from the group's edge as from its top and bottom
/// (the shape audit's rule): the 36-point action size in a row 20 points taller on each side.
private let singleRowWithButton = ControlMetrics.action + 2 * FormMetrics.radius(.panel) + 1

/// A pane's short confirmation where the person acted (“Shaders rebuilt.”), read out by VoiceOver.
struct SettingsFeedbackLine: View {
    @ObservedObject var model: LauncherModel
    var pane: SettingsPane
    var undo: (() -> Void)? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        if let feedback = model.settingsFeedback, feedback.pane == pane {
            HStack(spacing: 8) {
                Label(feedback.text, systemImage: feedback.failed ? "exclamationmark.triangle" : "checkmark")
                    .font(.system(size: 12)).foregroundStyle(feedback.failed ? palette.negative : palette.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isStaticText)
                if feedback.undo, let undo { Button("Undo", action: undo).quietAction() }
            }
            .padding(.horizontal, 6)
            .onAppear { NSAccessibility.post(element: NSApp as Any, notification: .announcementRequested, userInfo: [.announcement: feedback.text, .priority: NSAccessibilityPriorityLevel.medium.rawValue]) }
        }
    }
}

// MARK: - General

struct GeneralPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            FormSection("Game") {
                FormGroup {
                    FormRow("Game version", detail: model.originalAvailable
                            ? "Used when you open the game’s own menu. Joining a server always uses the version that server runs."
                            : "This copy of the app includes CoD2x only. The full release also includes Original 1.3.") { versionControl }
                    FormRow("Hide CoD2 Unbound while the game is running", detail: "It comes back when you quit the game.", isOn: $model.settings.hideWhilePlaying)
                }
            }
            FormSection("Launcher") {
                FormGroup {
                    FormRow("Appearance") {
                        Picker("Appearance", selection: $model.settings.appearance) {
                            ForEach(LauncherAppearance.allCases, id: \.self) { Text($0.title).tag($0) }
                        }
                        .pickerStyle(.segmented).labelsHidden().fixedSize()
                    }
                    FormRow("Refresh the server list when Servers opens",
                            detail: model.settings.refreshServersOnOpen ? "On, the list refreshes each time you open Servers." : "Off, nothing is contacted until you press Refresh.",
                            isOn: $model.settings.refreshServersOnOpen)
                }
            }
            if model.usesLoadouts {
                Banner(.tip, symbol: "rectangle.on.rectangle", title: "Looking for frame rate, mouse, brightness, network, HUD or sound?",
                       message: "They live in your loadouts, so every loadout keeps its own.") {
                    Button("Open Configs", action: model.openConfigs).glassAction()
                }
            } else {
                Banner(.tip, symbol: "rectangle.on.rectangle", title: "Your loadouts aren’t set up yet",
                       message: "Until they are, the frame rate, mouse and sound settings in Display apply to every game." + (model.configStudio?.migrationError.map { " " + $0 } ?? "")) {
                    Button("Open Configs", action: model.openConfigs).glassAction()
                }
            }
        }
    }
    @ViewBuilder private var versionControl: some View {
        if model.originalAvailable {
            // The player's own choice, not a one-match switch to a server's version (Servers may set chosenEngine for that).
            Picker("Game version", selection: Binding(get: { model.installedEngines.contains(model.settings.engine) ? model.settings.engine : .cod2x },
                                                      set: { model.settings.engine = $0 })) {
                ForEach(model.engineOptions, id: \.value) { Text($0.title).tag($0.value) }
            }
            .pickerStyle(.segmented).labelsHidden().fixedSize()
        } else {
            Text(CoD2xFiles.engineTitle).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
            Button("View Release") { NSWorkspace.shared.open(AppUpdate.releases) }.quietAction()
        }
    }
}

// MARK: - Display

struct DisplayPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var customFPS = false
    @State private var beforeRestore: GameSettings?
    private var legacy: Bool { !model.usesLoadouts }
    var body: some View {
        SettingsPaneScroll {
            HStack(alignment: .top, spacing: 20) {
                DisplayPreview(model: model).frame(width: 300)
                VStack(alignment: .leading, spacing: 8) {
                FormGroup {
                    FormRow("Display", detail: model.gameMonitor > 0 ? "Chosen by the loadout for the next game" : "The main display") {
                        Text(model.mainDisplay?.name ?? "Main display").font(.system(size: 13)).foregroundStyle(palette.secondary).lineLimit(1)
                    }
                    FormRow("Mode", detail: LauncherModel.displayModeDetail(model.settings.fullscreen)) {
                        PopUpButton("Mode", selection: $model.settings.fullscreen, options: LauncherModel.displayModes)
                    }
                    if model.offersRenderers {
                        FormRow("Graphics", detail: "Metal is how macOS draws today. OpenGL is the older way, kept in case something looks wrong.") {
                            ScopeSwitch("Graphics", selection: $model.settings.renderer, options: [(.metal, "Metal"), (.openGL, "OpenGL")])
                        }
                    }
                    if legacy {
                        VStack(alignment: .leading, spacing: 6) {
                            FormRow("Resolution") {
                                PopUpButton("Resolution", selection: Binding(get: { model.settings.resolution }, set: { model.resolutionAdjustment = nil; model.settings.resolution = $0 }), options: model.resolutionOptions)
                            }
                            Text(model.resolutionHelp).font(.system(size: 12)).foregroundStyle(palette.tertiary)
                                .fixedSize(horizontal: false, vertical: true).padding(.horizontal, 20).padding(.bottom, 12)
                        }
                    }
                }
                if !legacy {
                    Text("The game has no control for these \(model.offersRenderers ? "three" : "two"), so they are set here and apply to every loadout.")
                        .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true).padding(.leading, 6)
                }
                }
            }
            if legacy {
                FormSection("Picture") {
                    FormGroup {
                        FormRow("Vertical sync", detail: "Stops tearing by matching your display. Adds a little delay; most competitive players leave it off.", isOn: $model.settings.vsync)
                        FormRow("Texture sharpness at a distance", detail: "Anisotropic filtering. Higher is sharper and costs almost nothing on Apple silicon.") {
                            PopUpButton("Texture sharpness at a distance", selection: $model.settings.anisotropy,
                                        options: GameSettings.anisotropyLevels.map { ($0, "\($0)×") })
                        }
                    }
                }
            } else {
                // Resolution, vertical sync and the picture belong to each loadout: the game's own Graphics menu
                // changes them too, and the launcher no longer sets them back at every start.
                Banner(.tip, symbol: "square.stack.3d.up", title: "Resolution, vertical sync and picture quality belong to each loadout",
                       message: "Change them in Configs or in the game’s own Graphics menu. Either way they stay as you set them.") {
                    Button("Open Configs", action: model.openResolutionSettings).glassAction().disabled(!model.canNavigate)
                }
            }
            if legacy {
                FormSection("Frame rate") {
                    FrameRateCrates(fps: $model.settings.fps, custom: $customFPS)
                } footer: {
                    Text("Any cap from 0 to 1000 fps, where 0 means no cap. Servers can enforce their own; CoD2x competitive servers keep it between 125 and 250 fps.")
                }
                FormSection("Mouse and sound") { LegacyMouseGroup(model: model) }
            }
            HStack(alignment: .center, spacing: 12) {
                if let adjustment = model.resolutionAdjustment {
                    Text(adjustment).font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                } else if model.settingsFeedback?.pane == .display {
                    SettingsFeedbackLine(model: model, pane: .display) { undoRestore() }
                } else {
                    Label(model.settings == model.savedSettings ? "Saved. Applies the next time the game starts." : "Saving…", systemImage: "checkmark")
                        .font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.horizontal, 6)
                }
                Spacer(minLength: 8)
                Button("Restore Defaults", action: restore).glassAction().disabled(atDefaults)
            }
        }
        .onAppear { customFPS = !FrameCap.isPreset(model.settings.fps); model.refreshDisplay() }
    }
    private var atDefaults: Bool {
        let d = model.defaultSettings, s = model.settings
        let display = s.fullscreen == d.fullscreen && s.renderer == d.renderer && (!legacy || (s.resolution == d.resolution && s.vsync == d.vsync && s.anisotropy == d.anisotropy))
        return display && (!legacy || (s.fps == d.fps && s.rawMouse == d.rawMouse && s.sensitivity == d.sensitivity && s.dpi == d.dpi && s.volume == d.volume))
    }
    private func restore() {
        beforeRestore = model.restoreDisplayDefaults()
        customFPS = !FrameCap.isPreset(model.settings.fps)
        model.settingsFeedback = .init(pane: .display, text: "Defaults restored. Applies the next time the game starts.", undo: true)
    }
    private func undoRestore() {
        guard let previous = beforeRestore else { model.settingsFeedback = nil; return }
        var settings = model.settings
        settings.fullscreen = previous.fullscreen; settings.renderer = previous.renderer
        if legacy { settings.resolution = previous.resolution; settings.vsync = previous.vsync; settings.anisotropy = previous.anisotropy; settings.fps = previous.fps; settings.rawMouse = previous.rawMouse; settings.sensitivity = previous.sensitivity; settings.dpi = previous.dpi; settings.volume = previous.volume }
        model.settings = settings
        customFPS = !FrameCap.isPreset(settings.fps)
        beforeRestore = nil
        model.settingsFeedback = .init(pane: .display, text: "Your settings are back.")
    }
}

/// The small drawing beside Display's choices: the main display with the game's picture on it.
struct DisplayPreview: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 20, style: .continuous)
        // With loadouts the size is the next game's loadout's; the pane itself only chooses the mode.
        let size = SettingsText.size(model.usesLoadouts ? model.nextGameResolution.value : model.settings.resolution) ?? (1920, 1080)
        let windowed = model.settings.fullscreen == "windowed"
        VStack(spacing: 12) {
            ZStack(alignment: .top) {
                VStack(spacing: 0) {
                    RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(palette.secondary, lineWidth: 2)
                        .frame(width: 228, height: 134)
                    Path { path in
                        path.move(to: CGPoint(x: 14, y: 0)); path.addLine(to: CGPoint(x: 54, y: 0))
                        path.addLine(to: CGPoint(x: 60, y: 20)); path.addLine(to: CGPoint(x: 8, y: 20)); path.closeSubpath()
                    }
                    .fill(palette.track).frame(width: 68, height: 20)
                    Capsule().fill(palette.track).frame(width: 88, height: 4)
                }
                screen(windowed: windowed, size: size).padding(.top, 5)
            }
            Text(model.mainDisplay?.name ?? "Main display").font(.system(size: 12)).foregroundStyle(palette.secondary)
                .multilineTextAlignment(.center).fixedSize(horizontal: false, vertical: true)
        }
        .padding(18)
        .frame(maxWidth: .infinity, minHeight: 234)
        .background(palette.panel, in: shape)
        .overlay(shape.strokeBorder(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "display preview")
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(spoken(windowed: windowed, size: size))
        .accessibilityAddTraits(.isImage)
    }
    private func screen(windowed: Bool, size: (width: Int, height: Int)) -> some View {
        let full = CGSize(width: 218, height: 124)
        let native = model.mainDisplay.map { CGSize(width: max(1, $0.nativeWidth), height: max(1, $0.nativeHeight)) } ?? CGSize(width: 16, height: 9)
        let scale = min(full.width / CGFloat(native.width), full.height / CGFloat(native.height))
        let window = windowed ? CGSize(width: min(full.width - 16, max(70, CGFloat(size.width) * scale)), height: min(full.height - 16, max(42, CGFloat(size.height) * scale))) : full
        return ZStack {
            RoundedRectangle(cornerRadius: 6, style: .continuous).fill(windowed ? palette.well : palette.selection)
                .overlay(RoundedRectangle(cornerRadius: 6, style: .continuous).strokeBorder(windowed ? palette.wellStroke : palette.selectionStroke, lineWidth: 1))
                .frame(width: full.width, height: full.height)
            if windowed {
                RoundedRectangle(cornerRadius: 5, style: .continuous).fill(palette.selection)
                    .overlay(RoundedRectangle(cornerRadius: 5, style: .continuous).strokeBorder(palette.selectionStroke, lineWidth: 1))
                    .frame(width: window.width, height: window.height)
            }
            VStack(spacing: 4) {
                Text(model.usesLoadouts ? modeTitle.uppercased() : SettingsText.resolution(model.settings.resolution)).font(.stencil(windowed ? 20 : 26)).foregroundStyle(palette.accent)
                    .lineLimit(1).minimumScaleFactor(0.6)
                Text(caption(windowed: windowed, size: size)).font(.system(size: 11)).foregroundStyle(palette.secondary)
            }
            .frame(width: window.width - 8)
        }
    }
    private var modeTitle: String { LauncherModel.displayModes.first { $0.value == model.settings.fullscreen }?.title ?? "Full Screen" }
    private func caption(windowed: Bool, size: (width: Int, height: Int)) -> String {
        if model.usesLoadouts { return windowed ? "beside your other apps" : "fills the display" }
        let aspect = SettingsText.aspect(width: size.width, height: size.height)
        let mode = LauncherModel.displayModes.first { $0.value == model.settings.fullscreen }?.title.lowercased() ?? "full screen"
        return aspect + " · " + (windowed ? "in a window" : mode)
    }
    private func spoken(windowed: Bool, size: (width: Int, height: Int)) -> String {
        let mode = LauncherModel.displayModes.first { $0.value == model.settings.fullscreen }?.title.lowercased() ?? "full screen"
        let how = windowed ? "the game in a window" : "the game fills the display"
        if model.usesLoadouts { return "Preview: \(how), \(mode), on \(model.mainDisplay?.name ?? "the main display")" }
        return "Preview: \(how), \(size.width) by \(size.height), \(mode), on \(model.mainDisplay?.name ?? "the main display")"
    }
}

/// Before loadouts are set up, the launcher still applies these to every game.
private struct LegacyMouseGroup: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        FormGroup {
            FormRow("Raw mouse input", detail: "Bypasses macOS pointer acceleration.", isOn: $model.settings.rawMouse)
            FormRow("Sensitivity") {
                Slider(value: $model.settings.sensitivity, in: 1...20, step: 0.1).frame(width: 220).accessibilityLabel("Sensitivity")
                    .accessibilityValue(String(format: "%.1f", model.settings.sensitivity))
                Text(String(format: "%.1f", model.settings.sensitivity)).font(.system(size: 14, weight: .semibold)).monospacedDigit().frame(width: 40, alignment: .trailing)
            }
            FormRow("Mouse DPI") {
                TextField("DPI", value: $model.settings.dpi, format: .number.grouping(.never)).textFieldStyle(.plain).multilineTextAlignment(.trailing)
                    .padding(.horizontal, 12).frame(width: 80, height: 28)
                    .background(palette.field, in: Capsule())
                    .overlay(Capsule().stroke(palette.wellStroke, lineWidth: 1))
                    .shapeAudit(.field, .capsule, "DPI field")
                    .accessibilityLabel("Mouse DPI")
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text(MouseMath.label(model.settings.cm360)).font(.stencil(20)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f))
                        .contentTransition(.numericText())
                    Text("cm / 360°").font(.system(size: 13)).foregroundStyle(palette.secondary)
                }
                .frame(width: 140, alignment: .trailing)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("\(MouseMath.label(model.settings.cm360)) centimetres per full turn")
                .accessibilityAddTraits(.isStaticText)
            }
            FormRow("Volume") {
                Slider(value: $model.settings.volume, in: 0...1).frame(width: 220).accessibilityLabel("Game volume")
                Text("\(Int((model.settings.volume * 100).rounded()))%").font(.system(size: 14, weight: .semibold)).monospacedDigit().frame(width: 40, alignment: .trailing)
            }
        }
    }
}

// MARK: - Game Data

struct GameDataPane: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        SettingsPaneScroll {
            VStack(alignment: .leading, spacing: 10) {
                FormGroup {
                    GameFolderRow(model: model)
                    KeyRow(model: model)
                    ShaderRow(model: model)
                }
                if let error = model.dataChangeError { Banner(.error, message: error) { Button("Choose Again…", action: model.changeGameData).glassAction() } }
                if let message = model.shaderRebuildMessage {
                    Label(message, systemImage: model.approximate ? "exclamationmark.triangle" : "checkmark").font(.system(size: 12))
                        .padding(.horizontal, 6).accessibilityAddTraits(.isStaticText)
                }
                SettingsFeedbackLine(model: model, pane: .gameData)
            }
            FormSection("Your files") {
                FormGroup {
                    FormRow("Loadouts, demos and screenshots",
                            detail: PathDisplay.short(model.home.path) + (model.gameData.myFilesBytes.map { " · " + SettingsText.bytes($0) } ?? ""),
                            symbol: "doc") {
                        Button("Show in Finder", action: model.showMyFilesInFinder).glassAction()
                    }
                    FormRow("Game versions installed", detail: model.originalAvailable ? "Both ship inside the app and update with it."
                            : "CoD2x ships inside the app and updates with it. This copy doesn’t include Original 1.3.", symbol: "cpu") {
                        ForEach(Array(model.installedEngineNames.enumerated()), id: \.offset) { index, name in Badge(name, tone: index == 0 ? .accent : .neutral) }
                    }
                    CoD2xFilesRow(model: model)
                }
            }
            FormGroup {
                FormRow("Set up again", detail: "Walks through game data, CD key and shaders once more. Your loadouts, demos and screenshots stay. You can cancel at any step.",
                        minHeight: singleRowWithButton) {
                    Button("Set Up Again…", action: model.requestSetUpAgain).glassAction().actionControlSize().disabled(model.gameRunning || model.shaderBusy)
                }
            }
        }
        .onAppear { model.refreshGameData(); model.refreshCoD2xFiles() }
        .onDisappear { model.dataChangeError = nil; model.shaderRebuildMessage = nil }
    }
}

private struct GameFolderRow: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        let found = model.gameData.found && !model.dataPath.isEmpty
        FormRow("Game data", detail: model.dataPath.isEmpty ? "No game folder chosen yet." : found ? model.dataPathDisplay + " · All 16 game archives · Version 1.3"
                : model.dataPathDisplay + " · Not there now. Is its drive connected?", symbol: "folder") {
            StatusLabel(found ? .ready : .wontWork, found ? "Found" : "Not found")
            Button("Show in Finder", action: model.showGameDataInFinder).glassAction().disabled(!found)
            Button("Change…", action: model.changeGameData).glassAction().disabled(model.gameRunning || model.shaderBusy)
        }
    }
}

private struct KeyRow: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        let ending = model.gameData.keyEnding
        FormRow("CD key", detail: ending.map { SettingsText.maskedKey(lastFour: $0) + " · Stored only on this Mac" } ?? "No key stored. The game needs one to join servers.",
                symbol: "key") {
            StatusLabel(ending == nil ? .wontWork : .ready, ending == nil ? "Missing" : "Verified")
            Button(ending == nil ? "Add Key…" : "Replace…", action: model.presentReplaceKey).glassAction()
        }
    }
}

private struct ShaderRow: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        FormRow("Shaders", detail: model.shaderSummary, symbol: "circle.lefthalf.filled") {
            if model.shaderBusy {
                ProgressView(value: Double(model.shaderCount), total: Double(LauncherModel.shaderTotal)).frame(width: 90)
                    .accessibilityLabel("Building shaders").accessibilityValue("\(model.shaderCount) of \(LauncherModel.shaderTotal)")
            } else {
                StatusLabel(model.approximate ? .needsChange : .ready, model.approximate ? "Approximate" : "Ready")
            }
            Button("Rebuild", action: model.rebuildShaders).glassAction()
                .disabled(model.shaderBusy || model.gameRunning || model.dataPath.isEmpty || !model.gameData.found)
                .help("Build the shaders again from your game files")
        }
    }
}

/// CoD2x's own menus and icons, which the app can't ship: fetched or copied only when the player asks.
private struct CoD2xFilesRow: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        let known = [CoD2xFiles.Message.installed, CoD2xFiles.Message.notInstalled]
        let detail = "CoD2x’s own menus and icons, which can’t ship with the app: download them from the CoD2x project (2.6 MB) or copy them from a Windows game folder with CoD2x."
        FormRow("CoD2x menus and icons", detail: known.contains(model.cod2xFilesMessage) ? detail : detail + "\n" + model.cod2xFilesMessage, symbol: "square.grid.2x2") {
            if model.cod2xFilesBusy {
                ProgressView().controlSize(.small).accessibilityLabel(model.cod2xFilesMessage)
                Button("Cancel", action: model.cancelCoD2xFiles).glassAction()
            } else if model.cod2xFilesInstalled {
                StatusLabel(.ready, "Installed")
                Button("Remove", action: model.removeCoD2xFiles).glassAction().disabled(model.gameRunning)
            } else {
                Button("Copy From Folder…", action: model.chooseCoD2xFolder).glassAction().disabled(model.gameRunning)
                    .help("Copy them from a Windows Call of Duty 2 folder that has CoD2x installed")
                Button("Download", action: model.downloadCoD2xFiles).glassAction().disabled(model.gameRunning)
            }
        }
    }
}

// MARK: - Privacy

struct PrivacyPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            Banner(.tip, symbol: "lock", message: "CoD2 Unbound has no account and sends no reports unless you turn them on. Everything it contacts is listed below.")
            FormSection("Sharing") {
                FormGroup {
                    // Only a build with a report receiver offers it; without one nothing is ever sent.
                    if model.crashReportService != nil {
                        FormRow("Send crash reports to CoD2 Unbound",
                                detail: "If the game crashes or freezes: what failed, your macOS version and Mac model, and the game’s report. Never your CD key or your Mac’s user name.") {
                            Button("See What’s Sent…") { model.privacyDetailsShown = true }.quietAction()
                                .popover(isPresented: $model.privacyDetailsShown, arrowEdge: .bottom) { CrashReportContents(model: model) }
                            SwitchToggle("Send crash reports to CoD2 Unbound", isOn: $model.settings.sendCrashReports)
                        }
                    }
                    FormRow("Send CoD2x crash reports to the CoD2x project",
                            detail: "Faults in CoD2x’s own code, sent by CoD2x to the people who make it, as on Windows. They include folder paths and the list of loaded game files.",
                            isOn: $model.settings.sendCoD2xReports)
                }
            }
            FormSection("On this Mac") {
                FormGroup {
                    FormRow("Match summaries", detail: "Frame rate and connection notes after each match, so Check-up can advise you. Kept on this Mac, the last \(MatchSummaries.recordLimit) matches.",
                            minHeight: singleRowWithButton) {
                        Button("Delete Summaries…", action: model.requestDeleteSummaries).glassAction().actionControlSize()
                            .disabled(model.matchSummaryCount == 0 || model.gameRunning)
                        SwitchToggle("Match summaries", isOn: $model.settings.matchSummariesEnabled)
                    }
                }
            } footer: {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Game logs stay on this Mac: up to 10 sessions and 20 MiB. Passwords and CD keys are removed. Find them in Advanced › Logs.")
                    Text(summaryCount)
                    SettingsFeedbackLine(model: model, pane: .privacy).padding(.horizontal, -6)
                }
            }
            FormSection("What CoD2 Unbound contacts") {
                FormGroup {
                    ForEach(PrivacyContacts.lines, id: \.who) { line in ContactRow(who: line.who, when: line.when) }
                }
            }
        }
        .onAppear { model.refreshMatchCount() }
    }
    private var summaryCount: String {
        let count = model.matchSummaryCount
        let kept = count == 0 ? "No match summaries on this Mac yet." : count == 1 ? "1 match summary on this Mac." : "\(count) match summaries on this Mac."
        return kept + (model.settings.matchSummariesEnabled ? "" : " Turning this off keeps the ones already here.")
    }
}

/// One line of “What CoD2 Unbound contacts”: who, and when.
private struct ContactRow: View {
    var who: String
    var when: String
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 16) {
            Text(who).font(.system(size: 14)).foregroundStyle(palette.text).frame(width: 160, alignment: .leading)
            Text(when).font(.system(size: 13)).foregroundStyle(palette.secondary).lineSpacing(1.5)
                .frame(maxWidth: .infinity, alignment: .leading).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.horizontal, FormMetrics.rowInset(.panel)).padding(.vertical, 11)
        .frame(minHeight: 40)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }
}

/// See What's Sent…: the fields a crash report carries, with this Mac's values (CrashReporting.envelope).
struct CrashReportContents: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("What a crash report holds").font(.system(size: 15, weight: .semibold)).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            VStack(alignment: .leading, spacing: 8) {
                ForEach(model.crashReportFields, id: \.name) { field in
                    HStack(alignment: .firstTextBaseline, spacing: 12) {
                        Text(field.name).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.text).frame(width: 96, alignment: .leading)
                        Text(field.value).font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                    .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                }
            }
            Text("No account, name or CD key goes with it, and a report that seems to hold a CD key is never sent. The report service is told not to record where it came from. Each report is sent once.")
                .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(18)
        .frame(width: 420)
    }
}

// MARK: - Updates

struct UpdatesPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        SettingsPaneScroll {
            UpdateCard(model: model)
            if model.updateError { Banner(.error, message: model.updateMessage) }
            SettingsFeedbackLine(model: model, pane: .updates)
            FormGroup {
                FormRow("Check for updates automatically", detail: "Once a day, when the launcher is open. Nothing is downloaded until you choose Install.",
                        isOn: $model.settings.checkUpdatesAutomatically)
            }
            FormSection("Game versions") {
                FormGroup {
                    FormRow(CoD2xFiles.engineTitle, detail: "Included in this release. A newer CoD2x arrives with an app update.") { StatusLabel(.ready, "Included") }
                    FormRow("Original 1.3", detail: model.originalAvailable ? "The final official patch. It doesn’t change." : "Not included in this copy of the app.") {
                        Text(model.originalAvailable ? "Final" : "Not included").font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.secondary)
                    }
                }
            }
            Button(action: model.openReleaseNotes) {
                HStack(spacing: 6) { Text("Release notes for every version"); Image(systemName: "arrow.up.forward.square").imageScale(.small) }
            }
            .quietAction().help(AppUpdate.releases.absoluteString)
        }
    }
}

/// The Updates pane's status: what's installed, what was found, and the one next action.
struct UpdateCard: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    private static let cardHeight: CGFloat = 104
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 20, style: .continuous)
        HStack(spacing: 18) {
            BrandMark().frame(width: 64, height: 64).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 4) {
                Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading).fixedSize(horizontal: false, vertical: true)
                Text(subtitle).font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
            HStack(spacing: 10) { actions }
        }
        // The buttons sit as far from the right edge as from the top and bottom (concentric with the card).
        .padding(.leading, 20).padding(.trailing, (Self.cardHeight - ControlMetrics.action) / 2)
        .frame(maxWidth: .infinity, minHeight: Self.cardHeight)
        .background(palette.panel, in: shape)
        .overlay(shape.strokeBorder(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "update status")
        .accessibilityElement(children: .contain)
    }
    private var checked: String { model.lastUpdateCheck.map { "Last checked " + SettingsText.when($0) + "." } ?? "Not checked yet." }
    private var title: String {
        switch model.updateState {
        case .notChecked: "CoD2 Unbound \(model.version)"
        case .checking: "Checking for updates…"
        case .upToDate: "CoD2 Unbound \(model.version) is up to date"
        case .ready(let version, let installable): installable ? "CoD2 Unbound \(version) is ready to install" : "CoD2 Unbound \(version.isEmpty ? "update" : version) is available"
        case .installing: "Installing CoD2 Unbound \(model.updateOffer?.version ?? "")"
        case .failed: model.updateFailedStage == .install ? "Couldn’t install the update" : "Couldn’t check for updates"
        }
    }
    private var subtitle: String {
        switch model.updateState {
        case .notChecked: checked
        case .checking: "Asking GitHub for the latest release."
        case .upToDate: checked
        case .ready(_, let installable):
            installable ? "A \(SettingsText.bytes(Int64(model.updateOffer?.archiveSize ?? 0))) download. Installing takes a few seconds and reopens the launcher."
                : "It can’t be installed from here. Download it from its release page."
        case .installing(let message): message
        case .failed: model.updateFailedStage == .install ? "You still have \(model.version), unchanged." : "\(checked) You have \(model.version)."
        }
    }
    @ViewBuilder private var actions: some View {
        switch model.updateState {
        case .checking, .installing:
            ProgressView().controlSize(.small).accessibilityLabel(model.updateMessage)
            if model.canCancelUpdate { Button("Cancel", action: model.cancelUpdate).glassAction().actionControlSize() }
        case .ready(_, let installable):
            if !model.updateNotes.isEmpty { Button("What’s New", action: model.showWhatsNew).glassAction().actionControlSize() }
            if installable { Button("Install and Relaunch", action: model.installUpdate).prominentAction().actionControlSize().disabled(model.gameRunning) }
            else { Button("Open Release Page") { NSWorkspace.shared.open(model.releaseURL ?? AppUpdate.releases) }.prominentAction().actionControlSize() }
        case .failed:
            Button("Try Again", action: model.retryUpdate).glassAction().actionControlSize()
        case .notChecked, .upToDate:
            Button("Check Now", action: model.checkUpdates).glassAction().actionControlSize()
        }
    }
}

/// Home's release status and actions, as before the redesign (ship-home-library may replace this with its pill).
struct UpdateStatusView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .top, spacing: 10) {
                if model.checkingUpdates || model.installingUpdate { ProgressView().controlSize(.small).accessibilityLabel(model.updateMessage) }
                else { Image(systemName: model.updateError ? "exclamationmark.triangle" : model.releaseURL != nil ? "arrow.down.circle" : model.updateWasChecked ? "checkmark.circle" : "arrow.clockwise").foregroundStyle(model.updateError ? palette.negative : palette.secondary).accessibilityHidden(true) }
                VStack(alignment: .leading, spacing: 4) {
                    Text("CoD2 Unbound \(model.version)").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.heading)
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
                .help("Download this version, check it and replace the app. CoD2 Unbound restarts.")
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
    @State private var copied: (text: String, copied: Bool)?
    var body: some View {
        SettingsPaneScroll {
            FormSection("Extra launch options") { LaunchOptionsEditor(model: model) }
            FormSection("Troubleshooting") {
                FormGroup {
                    FormRow("Logs", detail: "The last 10 game sessions, up to 20 MiB on this Mac. Show in Finder selects the latest.") {
                        Button("Show in Finder", action: model.showLogsInFinder).glassAction()
                        Button("Copy Last Game Log") { copied = model.copyLastGameLog() }.glassAction()
                    }
                    FormRow("Warnings you dismissed", detail: warningsDetail) {
                        Button("Reset Warnings", action: model.resetWarnings).glassAction().disabled(model.settings.dismissedWarnings.isEmpty)
                    }
                    FormRow("Launcher settings", detail: "Returns everything in this window to how it started. Loadouts, demos, screenshots and your CD key stay.") {
                        Button("Reset Settings…", action: model.requestResetSettings).destructiveAction()
                    }
                }
            } footer: {
                VStack(alignment: .leading, spacing: 6) {
                    if let copied {
                        Label(copied.text, systemImage: copied.copied ? "doc.on.clipboard" : "info.circle").foregroundStyle(palette.secondary)
                            .accessibilityAddTraits(.isStaticText)
                    }
                    SettingsFeedbackLine(model: model, pane: .advanced).padding(.horizontal, -6)
                }
            }
        }
        .onChange(of: copied?.text) { if let text = copied?.text { NSAccessibility.post(element: NSApp as Any, notification: .announcementRequested, userInfo: [.announcement: text]) } }
    }
    private var warningsDetail: String {
        let count = model.settings.dismissedWarnings.count
        return "Brings back every “Don’t ask again” prompt." + (count == 0 ? " None are turned off now." : count == 1 ? " One is turned off now." : " \(count) are turned off now.")
    }
}

/// Extra launch options: one per line, kept as typed, checked as you type.
private struct LaunchOptionsEditor: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var cleared: String?
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 20, style: .continuous)
        let text = model.advancedDraft ?? model.settings.advanced
        VStack(alignment: .leading, spacing: 10) {
            TextEditor(text: Binding(get: { model.advancedDraft ?? model.settings.advanced }, set: model.editAdvancedOptions))
                .font(.typewriter(13)).scrollContentBackground(.hidden)
                .autocorrectionDisabled()
                .padding(.horizontal, 8).padding(.vertical, 8)
                .frame(height: 96)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).stroke(model.advancedError == nil ? palette.wellStroke : palette.negative, lineWidth: 1))
                .shapeAudit(.inset, .rounded(12), "dvar editor")
                .accessibilityLabel("Extra launch options, one per line")
            if let error = model.advancedError {
                Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 12)).foregroundStyle(palette.negative)
                    .fixedSize(horizontal: false, vertical: true).accessibilityAddTraits(.isStaticText)
                Text("Your last valid options still apply.").font(.system(size: 12)).foregroundStyle(palette.secondary)
            } else if !model.advancedIgnoredLines.isEmpty {
                // MARK: - ship-integrate
                let lines = model.advancedIgnoredLines.map(String.init).joined(separator: ", ")
                let subject = model.advancedIgnoredLines.count == 1 ? "The option on line" : "The options on lines"
                let explanation = model.advancedIgnoredLines.count == 1
                    ? "comes from your loadout and is ignored here. Change it in Configs."
                    : "come from your loadout and are ignored here. Change them in Configs."
                Label("\(subject) \(lines) \(explanation)",
                      systemImage: "info.circle").font(.system(size: 12)).foregroundStyle(palette.caution).fixedSize(horizontal: false, vertical: true)
                    .accessibilityAddTraits(.isStaticText)
            }
            HStack(alignment: .center, spacing: 12) {
                Text(cleared == nil ? "Passed to the game when it starts, one per line. For experts: a wrong option can stop the game from starting." : "Cleared.")
                    .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                if let previous = cleared {
                    Button("Undo") { model.editAdvancedOptions(previous); cleared = nil }.glassAction()
                } else {
                    Button("Clear") { cleared = text; model.editAdvancedOptions("") }.glassAction().disabled(text.isEmpty)
                }
            }
        }
        .padding(20)
        .background(palette.panel, in: shape)
        .overlay(shape.strokeBorder(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "launch options")
    }
}

// MARK: - About

struct AboutPane: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var copied = false
    var body: some View {
        SettingsPaneScroll(showOverflow: true) {
            HStack(alignment: .center, spacing: 22) {
                BrandMark().frame(width: 112, height: 112).shadow(color: .black.opacity(palette.dark ? 0.45 : 0.18), radius: 16, y: 8).accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 8) {
                    Text("COD2 UNBOUND").font(.stencil(50)).tracking(1).lineLimit(1).minimumScaleFactor(0.6).foregroundStyle(palette.heading)
                        .accessibilityLabel("CoD2 Unbound").accessibilityAddTraits(.isHeader)
                    HStack(spacing: 8) {
                        HStack(spacing: 6) {
                            Text("Version \(model.version)").font(.system(size: 14)).foregroundStyle(palette.secondary)
                            if !model.build.isEmpty { Text("(\(model.build))").font(.typewriter(13)).foregroundStyle(palette.tertiary) }
                        }
                        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                        Button { model.copyVersionInfo(); copied = true } label: {
                            Label(copied ? "Copied" : "Copy Version Info", systemImage: copied ? "checkmark" : "doc.on.doc")
                        }
                        .glassAction().controlSize(.small)
                        .help("Copy the version, game versions, macOS, Mac model, shaders and display, as Report a Problem shows them")
                        .task(id: copied) { if copied { try? await Task.sleep(for: .seconds(2)); copied = false } }
                    }
                    Text("Call of Duty 2 multiplayer, on every platform. Free and open source. You bring your own copy of the game.")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).frame(maxWidth: 520, alignment: .leading).fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(.horizontal, 4).padding(.top, 4)
            AboutFacts(model: model)
            HStack(spacing: 8) {
                Button(action: model.openReleaseNotes) { AboutLabel("Release Notes", "doc.text") }.glassAction().actionControlSize()
                Button(action: model.openWebsite) { AboutLabel("Website", "globe") }.glassAction().actionControlSize()
                Button(action: model.reportProblem) { AboutLabel("Report a Problem…", "ladybug") }.glassAction().actionControlSize()
                Button(action: model.showAcknowledgements) { AboutLabel("Acknowledgements", "person.2") }.glassAction().actionControlSize()
                Spacer(minLength: 8)
                Button("Check for Updates…") { model.settingsPane = .updates; model.checkUpdates() }.glassAction().actionControlSize()
                    .disabled(model.checkingUpdates || model.installingUpdate)
            }
            FormSection("Built on") {
                FormGroup {
                    ForEach(AboutCredits.rows(metal: model.offersRenderers), id: \.name) { row in CreditRow(name: row.name, what: row.what, licence: row.licence) }
                }
            }
            Text("Call of Duty is a registered trademark of Activision Publishing, Inc. CoD2 Unbound is a community project and is not affiliated with or endorsed by Activision. No game content is included.")
                .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true).padding(.horizontal, 6)
        }
    }
}

/// An About button's label: its symbol in a fixed box, so every button in the row is the same height.
private struct AboutLabel: View {
    var title: String
    var symbol: String
    init(_ title: String, _ symbol: String) { self.title = title; self.symbol = symbol }
    var body: some View {
        Label { Text(title) } icon: { Image(systemName: symbol).imageScale(.small).frame(width: 16, height: 14) }
    }
}

/// About's three facts. The graphics line says what the next game draws with: Metal, or OpenGL and whether
/// the player's original shaders are built.
private struct AboutFacts: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 20, style: .continuous)
        let versions = model.installedEngineNames
        HStack(spacing: 0) {
            fact("Game versions", versions.joined(separator: " · "), versions.count > 1 ? "Both included" : "Original 1.3 isn’t in this copy", edge: false)
            fact("Requires", "macOS 26 or later", "Apple silicon", edge: true)
            if model.drawsWithMetal {
                fact("Graphics", "Metal", "OpenGL stays as a choice in Settings", edge: true)
            } else {
                fact("Graphics", model.approximate ? "OpenGL, approximate shaders" : "OpenGL, original look",
                     model.approximate ? "Setup builds the original ones from your game"
                        : model.offersRenderers ? "Metal is a choice in Settings" : "Metal is planned, not in this release", edge: true)
            }
        }
        .background(palette.panel, in: shape)
        .overlay(shape.strokeBorder(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(20), "about facts")
    }
    private func fact(_ key: String, _ value: String, _ detail: String, edge: Bool) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(key.uppercased()).font(.system(size: 11, weight: .semibold)).tracking(0.8).foregroundStyle(palette.tertiary)
            Text(value).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
            Text(detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, 14).padding(.horizontal, 20)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .overlay(alignment: .leading) { if edge { Rectangle().fill(palette.separator).frame(width: 1) } }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }
}

/// About › Built on: what the app is made from, in short (Acknowledgements has every licence in full).
enum AboutCredits {
    /// `metal`: this app carries the engines that draw with Metal, and so the library they use.
    static func rows(metal: Bool) -> [(name: String, what: String, licence: String?)] {
        var rows: [(name: String, what: String, licence: String?)] = [
            ("opencod2", "The reconstructed engine this port is built on.", "No licence"),
            ("CoD2x", "The community patch multiplayer runs on. Rebuilt from how it behaves; none of its code ships.", nil),
            ("vkd3d", "Compiles the game’s own shaders on your Mac.", "LGPL-2.1"),
            ("SDL", "Windows, input and sound for the game.", "Zlib"),
            ("Typefaces", "Big Shoulders Stencil and Courier Prime.", "OFL-1.1"),
        ]
        if metal { rows.insert(("mtld3d", "Draws the game with Metal.", "Zlib"), at: 3) }
        return rows
    }
}

private struct CreditRow: View {
    var name: String
    var what: String
    var licence: String?
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .center, spacing: 16) {
            Text(name).font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text).frame(width: 170, alignment: .leading)
                .fixedSize(horizontal: false, vertical: true)
            Text(what).font(.system(size: 13)).foregroundStyle(palette.secondary).frame(maxWidth: .infinity, alignment: .leading)
                .fixedSize(horizontal: false, vertical: true)
            if let licence { Badge(licence) }
        }
        .padding(.horizontal, FormMetrics.rowInset(.panel)).padding(.vertical, 11)
        .frame(minHeight: 42)
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
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
