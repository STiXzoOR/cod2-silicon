import SwiftUI
import AppKit

struct SettingsView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    @State private var customFPS = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 26) {
                PageHeader(eyebrow: "Field manual", title: "Settings").padding(.leading, 16)
                HStack(alignment: .top, spacing: 22) {
                    VStack(alignment: .leading, spacing: 22) {
                        SettingsSection("Frame rate") {
                            FrameRateCrates(fps: $model.settings.fps, custom: $customFPS)
                        } footer: {
                            Text("Servers can enforce their own cap. Competitive CoD2x servers usually allow 125 to 250.")
                        }
                        SettingsSection("Display") { displayGroup }
                        SettingsSection("Mouse and sound") { mouseGroup }
                    }
                    VStack(alignment: .leading, spacing: 22) {
                        SettingsSection("Graphics") { graphicsGroup }
                        SettingsSection("Advanced") { advancedGroup }
                        SettingsSection("Game data") { setupGroup }
                    }
                }
            }
            .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 24).padding(.top, 30).padding(.bottom, 32)
        }
        .scrollIndicators(.automatic)
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onAppear { customFPS = ![333, 250, 125].contains(model.settings.fps) }
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
                    .padding(.horizontal, 10).frame(width: 80, height: 30)
                    .background(palette.dark ? Color.white.opacity(0.07) : Color.white.opacity(0.6), in: RoundedRectangle(cornerRadius: 9, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 9, style: .continuous).stroke(palette.wellStroke, lineWidth: 1))
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

    private var graphicsGroup: some View {
        SettingsGroup {
            SettingsRow("Renderer", detail: "Metal 4 arrives with the native renderer in 0.3. Classic OpenGL is the reference look.") {
                Picker("Renderer", selection: .constant("gl")) {
                    Text("Metal 4").tag("metal").selectionDisabled(); Text("Classic OpenGL").tag("gl")
                }
                .pickerStyle(.segmented).labelsHidden().fixedSize()
                .accessibilityHint("Metal 4 is upcoming")
            }
            SettingsRow("Render scale", fixedLabel: 150, upcoming: true) {
                Slider(value: .constant(1), in: 0.5...2).accessibilityLabel("Render scale, upcoming in 0.3")
                Text("100%").font(.system(size: 14, weight: .semibold)).frame(width: 48, alignment: .trailing)
            }
            SettingsRow("MetalFX upscaling", detail: "Renders below the target size and upscales on the GPU.", upcoming: true) {
                Toggle("MetalFX upscaling, upcoming in 0.3", isOn: .constant(false)).toggleStyle(.switch).labelsHidden()
            }
            SettingsRow("HDR (extended range)", detail: "On displays that support EDR. Classic look stays the default.", upcoming: true) {
                Toggle("HDR, upcoming in 0.3", isOn: .constant(false)).toggleStyle(.switch).labelsHidden()
            }
            SettingsRow("Anisotropic filtering") {
                Picker("Anisotropic filtering", selection: $model.settings.anisotropy) {
                    ForEach(GameSettings.anisotropyLevels, id: \.self) { Text("\($0)×").tag($0) }
                }
                .labelsHidden().fixedSize()
            }
        }
    }

    private var advancedGroup: some View {
        VStack(alignment: .leading, spacing: 10) {
            (Text("Extra console variables, one ") + Text("dvar value").font(.typewriter(13)) + Text(" per line"))
                .font(.system(size: 13)).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            TextEditor(text: $model.settings.advanced)
                .font(.typewriter(13)).scrollContentBackgroundHidden()
                .padding(.horizontal, 8).padding(.vertical, 8)
                .frame(height: 112)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).stroke(palette.wellStroke, lineWidth: 1))
                .accessibilityLabel("Extra console variables")
            Text("Paths, passwords and your CD key are managed by the launcher and can't be set here.").font(.system(size: 12)).foregroundStyle(palette.tertiary)
        }
        .padding(.vertical, 14).padding(.horizontal, 18)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
    }

    private var setupGroup: some View {
        SettingsGroup {
            SettingsRow(model.dataPathDisplay.isEmpty ? "Not found" : model.dataPathDisplay, detail: model.approximate ? "Approximate shaders · add your Mac copy for the original look" : "Original shaders verified", typewriter: true) {
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
                ToolbarItem(placement: .automatic) { button.buttonStyle(.glassProminent).tint(palette.prominentTint).foregroundStyle(palette.prominentText) }
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

/// Grouped settings rows on an opaque panel, separated by hairlines.
struct SettingsGroup<Content: View>: View {
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        // Every row draws a hairline above itself; shifting up one point hides the first under the clip.
        VStack(spacing: 0) { content }
        .padding(.top, -1)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
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
    @ViewBuilder var control: Control
    @Environment(\.palette) private var palette
    init(_ title: String, detail: String? = nil, fixedLabel: CGFloat? = nil, typewriter: Bool = false, upcoming: Bool = false, @ViewBuilder control: () -> Control) {
        self.title = title; self.detail = detail; self.fixedLabel = fixedLabel; self.typewriter = typewriter; self.upcoming = upcoming; self.control = control()
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
        .padding(.horizontal, 18).padding(.vertical, detail == nil ? 10 : 9)
        .frame(minHeight: 52)
        .accessibilityElement(children: .contain)
        .modifier(Hairline())
    }
}

struct SettingsNote: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.system(size: 12)).foregroundStyle(palette.tertiary).frame(maxWidth: .infinity, minHeight: 44, alignment: .leading).padding(.horizontal, 18)
            .modifier(Hairline())
    }
}

/// Settings that exist in the design but wait on the Metal renderer: visibly disabled and labelled.
/// Ammo-crate frame-rate presets: 333, 250, 125 and a custom value.
struct FrameRateCrates: View {
    @Binding var fps: Int
    @Binding var custom: Bool
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        HStack(spacing: 10) {
            crate("333", "Classic competitive physics", selected: !custom && fps == 333) { fps = 333; custom = false }
            crate("250", "Common CoD2x server cap", selected: !custom && fps == 250) { fps = 250; custom = false }
            crate("125", "Original default feel", selected: !custom && fps == 125) { fps = 125; custom = false }
            Button { withAnimation(reduceMotion ? nil : .launcherSpring) { custom = true } } label: {
                VStack(alignment: .leading, spacing: 6) {
                    if custom {
                        TextField("FPS", value: $fps, format: .number.grouping(.never)).textFieldStyle(.plain)
                            .font(.stencil(34)).foregroundStyle(palette.dark ? palette.accent : Palette.hex(0x4e5b2f)).frame(height: 34)
                            .accessibilityLabel("Custom frame cap")
                    } else {
                        Text("—").font(.stencil(34)).foregroundStyle(palette.numeral).frame(height: 34)
                    }
                    Text("Custom value").font(.system(size: 11)).foregroundStyle(palette.secondary)
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
    }
}
