import SwiftUI
import AppKit

struct SettingsView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    @State private var customFPS = false

    /// Personal settings (frame cap, mouse, sound, filtering) live in loadouts once CoD2 Silicon has
    /// brought them over. Until then the launcher still applies them, so they stay here.
    private var legacy: Bool { !model.snapshot && model.configStudio?.migrationCompleted != true }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                PageHeader(eyebrow: "Field manual", title: "Settings", subtitle: "This Mac and the app. How the game plays lives in your loadouts.")
                    .padding(.leading, 16).padding(.bottom, 4)
                if legacy {
                    StudioBanner(title: "Your loadouts aren’t set up yet, so the mouse, sound and frame rate settings below still apply to every game.",
                                 detail: model.configStudio?.migrationError, tone: .caution)
                } else {
                    ConfigsLink { model.page = .configs; model.studio.loadCatalog() }
                }
                HStack(alignment: .top, spacing: 22) {
                    VStack(alignment: .leading, spacing: 22) {
                        SettingsSection("Game data") { setupGroup }
                        SettingsSection("Display") { displayGroup }
                        SettingsSection("Renderer") { rendererGroup }
                        if legacy {
                            SettingsSection("Frame rate") {
                                FrameRateCrates(fps: $model.settings.fps, custom: $customFPS)
                            } footer: {
                                Text("Any cap from 0 to 1000, where 0 means no cap. Servers can enforce their own; CoD2x competitive servers keep it between 125 and 250.")
                            }
                        }
                    }
                    VStack(alignment: .leading, spacing: 22) {
                        SettingsSection("Updates") { updatesGroup }
                        SettingsSection("Advanced launch options") { advancedGroup }
                        if legacy {
                            SettingsSection("Mouse and sound") { mouseGroup }
                            SettingsSection("Graphics") { filteringGroup }
                        }
                    }
                }
            }
            .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 24).padding(.top, 30).padding(.bottom, 32)
        }
        .scrollIndicators(.automatic)
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onAppear { customFPS = !FrameCap.isPreset(model.settings.fps) }
        .modifier(SaveToolbar(saved: model.settingsSaved, save: model.saveSettings))
    }

    private var displayGroup: some View {
        SettingsGroup {
            SettingsRow("Resolution") {
                Picker("Resolution", selection: $model.settings.resolution) {
                    ForEach(GameSettings.resolutions, id: \.self) { Text(Self.resolutionLabel($0)).tag($0) }
                }
                .labelsHidden().fixedSize()
            }
            SettingsRow("Fullscreen") {
                Picker("Fullscreen", selection: $model.settings.fullscreen) {
                    Text("Exclusive").tag("exclusive"); Text("Borderless").tag("borderless"); Text("Native Space").tag("spaces"); Text("Window").tag("windowed")
                }
                .pickerStyle(.segmented).labelsHidden().fixedSize()
            }
            SettingsNote(text: Self.modeHint(model.settings.fullscreen))
            SettingsRow("Vertical sync", detail: "Off keeps input latency lowest.") {
                Toggle("Vertical sync", isOn: $model.settings.vsync).toggleStyle(.switch).labelsHidden()
            }
        }
    }

    private var mouseGroup: some View {
        SettingsGroup {
            SettingsRow("Raw mouse input", detail: "Bypasses macOS pointer acceleration.") {
                Toggle("Raw mouse input", isOn: $model.settings.rawMouse).toggleStyle(.switch).labelsHidden()
            }
            SettingsRow("Sensitivity", fixedLabel: 120) {
                Slider(value: $model.settings.sensitivity, in: 1...20, step: 0.1).accessibilityLabel("Sensitivity")
                    .accessibilityValue(String(format: "%.1f", model.settings.sensitivity))
                Text(String(format: "%.1f", model.settings.sensitivity)).font(.system(size: 14, weight: .semibold)).monospacedDigit().frame(width: 40, alignment: .trailing)
            }
            SettingsRow("Mouse DPI") {
                TextField("DPI", value: $model.settings.dpi, format: .number.grouping(.never)).textFieldStyle(.plain).multilineTextAlignment(.trailing)
                    .padding(.horizontal, 12).frame(width: 80, height: ControlMetrics.height(.large))
                    .background(palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.6), in: Capsule())
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

    private var rendererGroup: some View {
        SettingsGroup {
            SettingsRow("Renderer", detail: "Classic OpenGL is the reference look today. Metal 4 arrives in 0.3, with MetalFX upscaling and HDR on displays that support it.") {
                Picker("Renderer", selection: .constant("gl")) {
                    Text("Classic OpenGL").tag("gl"); Text("Metal 4").tag("metal").selectionDisabled()
                }
                .pickerStyle(.segmented).labelsHidden().fixedSize()
                .accessibilityHint("Metal 4 is coming in version 0.3")
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

    private var updatesGroup: some View {
        SettingsGroup {
            SettingsRow("CoD2 Silicon \(model.version)", detail: model.updateMessage,
                        minHeight: ControlMetrics.height(.regular) + 2 * SettingsMetrics.radius + 1) {
                HStack(spacing: 10) {
                    if let url = model.releaseURL { Link("View Release", destination: url).font(.system(size: 13)).foregroundStyle(palette.link) }
                    Button(model.checkingUpdates ? "Checking…" : "Check Now", action: model.checkUpdates).glassAction().disabled(model.checkingUpdates)
                }
            }
        }
    }

    private var advancedGroup: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Extra options passed to the game when it starts, one per line. For experts.")
                .font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            TextEditor(text: $model.settings.advanced)
                .font(.typewriter(13)).scrollContentBackgroundHidden()
                .padding(.horizontal, 8).padding(.vertical, 8)
                .frame(height: 112)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).stroke(palette.wellStroke, lineWidth: 1))
                .shapeAudit(.inset, .rounded(12), "dvar editor")
                .accessibilityLabel("Extra launch options")
            Text(legacy ? "Paths, passwords and your CD key are managed by the launcher and can't be set here."
                        : "Paths, passwords and your CD key are managed by the launcher and can't be set here. Settings that belong to a loadout are changed in Configs; the game ignores them here.")
                .font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, 14).padding(.horizontal, SettingsMetrics.radius)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .shapeAudit(.container, .rounded(SettingsMetrics.radius), "advanced panel")
    }

    private var setupGroup: some View {
        SettingsGroup {
            // Tall enough that the button keeps the group's corner radius from its top and bottom
            // (plus the point the group hides under its first hairline).
            SettingsRow(model.dataPathDisplay.isEmpty ? "Not found" : model.dataPathDisplay, detail: model.approximate ? "Approximate shaders · add your Mac copy for the original look" : "Original shaders verified",
                        typewriter: true, minHeight: ControlMetrics.height(.regular) + 2 * SettingsMetrics.radius + 1) {
                Button("Set Up Again…", action: model.restartSetup).glassAction()
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

/// Where personal settings went: a calm link row into Configs.
private struct ConfigsLink: View {
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

/// The save note sits on the toolbar without its own glass; Save is the view's one prominent action.
private struct SaveToolbar: ViewModifier {
    var saved: Bool
    var save: () -> Void
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar {
                ToolbarSpacer(.flexible)
                ToolbarItem(placement: .automatic) { note }.sharedBackgroundVisibility(.hidden)
                ToolbarItem(placement: .automatic) { button.prominentAction() }
            }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar { ToolbarItem(placement: .automatic) { Spacer() }; ToolbarItemGroup(placement: .automatic) { note; button.prominentAction() } }
    }
    private var note: some View {
        Text(saved ? "Saved. Applies on next launch" : "Applies on next launch").font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize()
    }
    private var button: some View {
        Button("Save", action: save).keyboardShortcut("s", modifiers: .command)
    }
}

private extension View {
    @ViewBuilder func scrollContentBackgroundHidden() -> some View {
        self.scrollContentBackground(.hidden)
    }
    @ViewBuilder func selectionDisabled() -> some View {
        if #available(macOS 14.0, *) { self.selectionDisabled(true) } else { self }
    }
}

struct SettingsSection<Content: View, Footer: View>: View {
    var title: String
    @ViewBuilder var content: Content
    @ViewBuilder var footer: Footer
    @Environment(\.palette) private var palette
    init(_ title: String, @ViewBuilder content: () -> Content, @ViewBuilder footer: () -> Footer) {
        self.title = title; self.content = content(); self.footer = footer()
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6).accessibilityAddTraits(.isHeader)
            content
            footer.font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.leading, 6)
        }
    }
}
extension SettingsSection where Footer == EmptyView {
    init(_ title: String, @ViewBuilder content: () -> Content) { self.init(title, content: content, footer: { EmptyView() }) }
}

/// Settings panels' corner radius, which rows also keep as their side margin, so no control
/// reaches into a corner.
enum SettingsMetrics {
    static let radius: CGFloat = 20
}

/// Grouped settings rows on an opaque panel, separated by hairlines.
struct SettingsGroup<Content: View>: View {
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        // Every row draws a hairline above itself; shifting up one point hides the first under the clip.
        VStack(spacing: 0) { content }
        .padding(.top, -1)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(SettingsMetrics.radius), "settings group")
    }
}

private struct Hairline: ViewModifier {
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View { content.overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) } }
}

struct SettingsRow<Control: View>: View {
    var title: String
    var detail: String?
    var fixedLabel: CGFloat?
    var typewriter = false
    /// In the design but waiting on the Metal renderer (0.3): labelled, dimmed and inert.
    var upcoming = false
    var minHeight: CGFloat = 52
    @ViewBuilder var control: Control
    @Environment(\.palette) private var palette
    init(_ title: String, detail: String? = nil, fixedLabel: CGFloat? = nil, typewriter: Bool = false, upcoming: Bool = false, minHeight: CGFloat = 52,
         @ViewBuilder control: () -> Control) {
        self.title = title; self.detail = detail; self.fixedLabel = fixedLabel; self.typewriter = typewriter; self.upcoming = upcoming
        self.minHeight = minHeight; self.control = control()
    }
    var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(title).font(typewriter ? .typewriter(14) : .system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text).lineLimit(1)
                        .opacity(upcoming ? 0.55 : 1)
                    if upcoming {
                        Text("0.3").font(.stencil(11, heavy: false)).tracking(1.2).foregroundStyle(palette.accent)
                            .padding(.horizontal, 6).padding(.vertical, 1)
                            .overlay(Capsule().stroke(palette.selectionStroke, lineWidth: 1))
                            .accessibilityLabel("Upcoming in version 0.3")
                    }
                }
                if let detail { Text(detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true).opacity(upcoming ? 0.7 : 1) }
            }
            .frame(width: fixedLabel, alignment: .leading)
            .frame(maxWidth: fixedLabel == nil ? .infinity : nil, alignment: .leading)
            control.disabled(upcoming).opacity(upcoming ? 0.45 : 1)
        }
        .padding(.horizontal, SettingsMetrics.radius).padding(.vertical, detail == nil ? 10 : 9)
        .frame(minHeight: minHeight)
        .accessibilityElement(children: .contain)
        .modifier(Hairline())
    }
}

struct SettingsNote: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.system(size: 12)).foregroundStyle(palette.tertiary).frame(maxWidth: .infinity, minHeight: 44, alignment: .leading).padding(.horizontal, SettingsMetrics.radius)
            .modifier(Hairline())
    }
}

/// Ammo-crate frame-cap quick picks (`FrameCap.presets`) and a custom value from 0 (no cap) to 1000.
struct FrameRateCrates: View {
    @Binding var fps: Int
    @Binding var custom: Bool
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        HStack(spacing: 10) {
            ForEach(FrameCap.presets, id: \.value) { preset in
                crate("\(preset.value)", preset.caption, selected: !custom && fps == preset.value) { fps = preset.value; custom = false }
            }
            Button { withAnimation(reduceMotion ? nil : .launcherSpring) { custom = true } } label: {
                VStack(alignment: .leading, spacing: 6) {
                    if custom {
                        TextField("FPS", value: $fps, format: .number.grouping(.never)).textFieldStyle(.plain)
                            .font(.stencil(34)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f)).frame(height: 34)
                            .onSubmit { fps = min(1000, max(0, fps)) }
                            .accessibilityLabel("Custom frame cap, 0 to 1000, 0 for no cap")
                    } else {
                        Text("—").font(.stencil(34)).foregroundStyle(palette.numeral).frame(height: 34)
                    }
                    Text(custom ? FrameCap.customCaption(fps) : "Custom value").font(.system(size: 11)).foregroundStyle(palette.secondary)
                }
                .crateStyle(selected: custom, palette: palette)
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Custom frame cap")
            .accessibilityAddTraits(custom ? .isSelected : [])
        }
    }
    private func crate(_ number: String, _ caption: String, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 6) {
                Text(number).font(.stencil(34)).tracking(0.68).foregroundStyle(selected ? (palette.dark ? palette.accent : Palette.hex(0x4e5b2f)) : palette.numeral).frame(height: 34)
                Text(caption).font(.system(size: 11)).foregroundStyle(palette.secondary).lineSpacing(1.5).fixedSize(horizontal: false, vertical: true)
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
