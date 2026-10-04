import SwiftUI
import AppKit
import CoreText

// Colour tokens from the approved design. Dark is the field-dusk palette; light is
// paper and olive. Every text/background pair used on screen meets 4.5:1.
struct Palette {
    var ground: Color, panel: Color, panelStroke: Color, crate: Color, well: Color, wellStroke: Color
    var text: Color, heading: Color, secondary: Color, tertiary: Color, body: Color
    var accent: Color, numeral: Color, link: Color, address: Color, hint: Color
    var prominentTop: Color, prominentBottom: Color, prominentText: Color, prominentTint: Color
    var positive: Color, negative: Color, separator: Color, selection: Color, selectionStroke: Color
    var chipText: Color, chipIcon: Color, glassStroke: Color, glassFill: Color, track: Color
    var dark: Bool

    static func of(_ scheme: ColorScheme, contrast: ColorSchemeContrast = .standard) -> Palette {
        let base = scheme == .dark ? dark : light
        return contrast == .increased ? base.increasedContrast() : base
    }
    /// Increase Contrast: secondary text steps up a level and edges get firmer.
    func increasedContrast() -> Palette {
        var p = self
        p.secondary = dark ? Self.hex(0xd6cfbd) : Self.hex(0x3a382e)
        p.tertiary = dark ? Self.hex(0xbdb5a1) : Self.hex(0x4d4a3f)
        p.body = p.secondary; p.hint = p.secondary
        p.panelStroke = dark ? .white.opacity(0.22) : Self.hex(0x504628).opacity(0.4)
        p.glassStroke = dark ? .white.opacity(0.3) : Self.hex(0x504628).opacity(0.35)
        p.wellStroke = p.panelStroke; p.separator = p.panelStroke
        p.selectionStroke = p.accent
        return p
    }
    static let dark = Palette(
        ground: hex(0x0d0f0c), panel: hex(0x141711), panelStroke: .white.opacity(0.06), crate: hex(0x11140f),
        well: .black.opacity(0.26), wellStroke: .white.opacity(0.10),
        text: hex(0xece6d6), heading: hex(0xf3ecdc), secondary: hex(0xa9a28f), tertiary: hex(0x8f8877), body: hex(0xa9a28f),
        accent: hex(0xe2bd72), numeral: hex(0xe8e0cc), link: hex(0xe2bd72), address: hex(0xb9a982), hint: hex(0xa59e8b),
        prominentTop: hex(0xe8c478), prominentBottom: hex(0xc4984c), prominentText: hex(0x1c170c), prominentTint: hex(0xd6ad62),
        positive: hex(0x9fc58d), negative: hex(0xff7a6b), separator: .white.opacity(0.055),
        selection: hex(0xe2bd72).opacity(0.10), selectionStroke: hex(0xe2bd72).opacity(0.45),
        chipText: hex(0xe2dbc9), chipIcon: hex(0xcfc6b1), glassStroke: .white.opacity(0.11), glassFill: hex(0x1a1c16).opacity(0.46),
        track: .white.opacity(0.16), dark: true)
    static let light = Palette(
        ground: hex(0xefe8d8), panel: hex(0xf7f2e6), panelStroke: hex(0x504628).opacity(0.12), crate: hex(0xf3ede0),
        well: hex(0x504628).opacity(0.06), wellStroke: hex(0x504628).opacity(0.14),
        text: hex(0x26251d), heading: hex(0x1f1e17), secondary: hex(0x615e51), tertiary: hex(0x6b6757), body: hex(0x5d5a4d),
        accent: hex(0x6b4a12), numeral: hex(0x2c2b22), link: hex(0x8a5a12), address: hex(0x7a5a1c), hint: hex(0x67645a),
        prominentTop: hex(0x5c6a33), prominentBottom: hex(0x46522a), prominentText: hex(0xfff8e8), prominentTint: hex(0x55622f),
        positive: hex(0x3a7236), negative: hex(0xb3261e), separator: hex(0x504628).opacity(0.10),
        selection: hex(0x5c6a33).opacity(0.10), selectionStroke: hex(0x5c6a33).opacity(0.45),
        chipText: hex(0x33312a), chipIcon: hex(0x55523f), glassStroke: .white.opacity(0.75), glassFill: hex(0xfcf8ee).opacity(0.56),
        track: hex(0x504628).opacity(0.16), dark: false)

    /// Colours for Quake ^0–^9 codes, tuned per appearance for legibility.
    func quake(_ code: Int) -> Color {
        let darkCodes: [Int: UInt32] = [0: 0x8a8a80, 1: 0xff7a6b, 2: 0x86d993, 3: 0xf2d35b, 4: 0x7fb0ff, 5: 0x62d4e8, 6: 0xe895ea]
        let lightCodes: [Int: UInt32] = [0: 0x5f5f58, 1: 0xb3261e, 2: 0x2f7a3e, 3: 0x7d5c00, 4: 0x2a5db0, 5: 0x006a76, 6: 0x8e3a91]
        guard let value = (dark ? darkCodes : lightCodes)[code] else { return text }
        return Self.hex(value)
    }
    static func hex(_ value: UInt32) -> Color {
        Color(.sRGB, red: Double(value >> 16 & 255) / 255, green: Double(value >> 8 & 255) / 255, blue: Double(value & 255) / 255)
    }
}

private struct PaletteKey: EnvironmentKey { static let defaultValue = Palette.dark }
private struct UsesGlassKey: EnvironmentKey { static let defaultValue = LiquidGlass.available }
extension EnvironmentValues {
    var palette: Palette { get { self[PaletteKey.self] } set { self[PaletteKey.self] = newValue } }
    /// Liquid Glass on macOS 26 SDK/runtime; the review harness can force the 13–25 material fallback.
    var usesGlass: Bool { get { self[UsesGlassKey.self] } set { self[UsesGlassKey.self] = newValue } }
}

enum LiquidGlass {
    static var available: Bool {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) { return true }
        #endif
        return false
    }
}

// SIL OFL 1.1 typefaces bundled in Resources/Fonts. Bundles also declare
// ATSApplicationFontsPath; registering here covers bare development builds.
enum LauncherFonts {
    static let stencilHeavy = "BigShouldersStencilDisplay-ExtraBold"
    static let stencilBold = "BigShouldersStencilDisplay-Bold"
    static let typewriter = "CourierPrime-Regular"
    static let typewriterBold = "CourierPrime-Bold"
    @MainActor static func register() {
        guard NSFont(name: stencilHeavy, size: 12) == nil else { return }
        var folders = [Bundle.main.resourceURL?.appendingPathComponent("Fonts")]
        folders.append(Bundle.main.executableURL?.deletingLastPathComponent().appendingPathComponent("Fonts"))
        for folder in folders.compactMap({ $0 }) {
            let files = (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)) ?? []
            for file in files where file.pathExtension.lowercased() == "ttf" {
                CTFontManagerRegisterFontsForURL(file as CFURL, .process, nil)
            }
            if NSFont(name: stencilHeavy, size: 12) != nil { return }
        }
    }
}

extension Font {
    /// Big Shoulders Stencil Display for display titles, eyebrows and numerals.
    static func stencil(_ size: CGFloat, heavy: Bool = true) -> Font {
        let name = heavy ? LauncherFonts.stencilHeavy : LauncherFonts.stencilBold
        if NSFont(name: name, size: size) != nil { return .custom(name, fixedSize: size) }
        return .system(size: size, weight: heavy ? .black : .heavy).width(.condensed)
    }
    /// Courier Prime for timestamps, addresses, file names and the CD key.
    static func typewriter(_ size: CGFloat, bold: Bool = false) -> Font {
        let name = bold ? LauncherFonts.typewriterBold : LauncherFonts.typewriter
        if NSFont(name: name, size: size) != nil { return .custom(name, fixedSize: size) }
        return .system(size: size, weight: bold ? .bold : .regular, design: .monospaced)
    }
}

// MARK: - Glass: functional layer only (navigation, toolbars, buttons, floating panels).

struct GlassSurface<S: InsettableShape>: ViewModifier {
    var shape: S
    var tint: Color? = nil
    var interactive = false
    @Environment(\.usesGlass) private var usesGlass
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, #available(macOS 26.0, *) {
            // The system adapts glass itself for Reduce Transparency and Increase Contrast.
            content.glassEffect(Glass.regular.tint(tint).interactive(interactive), in: shape)
        } else { fallback(content) }
        #else
        fallback(content)
        #endif
    }
    private func fallback(_ content: Content) -> some View {
        content
            .background(reduceTransparency ? palette.panel : palette.glassFill, in: shape)
            .background(.ultraThinMaterial, in: shape)
            .overlay(shape.stroke(palette.glassStroke, lineWidth: 1))
            .overlay(shape.inset(by: 1).stroke(LinearGradient(colors: [.white.opacity(palette.dark ? 0.12 : 0.6), .clear], startPoint: .top, endPoint: .center), lineWidth: 1).allowsHitTesting(false))
            .shadow(color: .black.opacity(palette.dark ? 0.32 : 0.12), radius: 20, y: 12)
    }
}
extension View {
    func glassSurface<S: InsettableShape>(_ shape: S, tint: Color? = nil, interactive: Bool = false) -> some View {
        modifier(GlassSurface(shape: shape, tint: tint, interactive: interactive))
    }
    func glassCapsule(interactive: Bool = false) -> some View { glassSurface(Capsule(), interactive: interactive) }
    /// The one emphasized action per view: brass in dark, olive in light.
    func prominentAction() -> some View { modifier(ProminentAction()) }
    /// Secondary actions: neutral glass buttons.
    func glassAction() -> some View { modifier(GlassAction()) }
}

struct GlassContainer<Content: View>: View {
    var spacing: CGFloat? = nil
    @ViewBuilder var content: Content
    @Environment(\.usesGlass) private var usesGlass
    var body: some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, #available(macOS 26.0, *) { GlassEffectContainer(spacing: spacing) { content } } else { content }
        #else
        content
        #endif
    }
}

private struct ProminentAction: ViewModifier {
    @Environment(\.usesGlass) private var usesGlass
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, #available(macOS 26.0, *) {
            content.buttonStyle(.glassProminent).tint(palette.prominentTint).foregroundStyle(palette.prominentText)
        } else { content.buttonStyle(ProminentFallbackStyle(palette: palette)) }
        #else
        content.buttonStyle(ProminentFallbackStyle(palette: palette))
        #endif
    }
}

private struct GlassAction: ViewModifier {
    @Environment(\.usesGlass) private var usesGlass
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, #available(macOS 26.0, *) { content.buttonStyle(.glass).foregroundStyle(palette.text) }
        else { content.buttonStyle(GlassFallbackStyle(palette: palette)) }
        #else
        content.buttonStyle(GlassFallbackStyle(palette: palette))
        #endif
    }
}

/// macOS 13–25: the brass (or olive) gradient capsule from the design.
struct ProminentFallbackStyle: ButtonStyle {
    var palette: Palette
    @Environment(\.isEnabled) private var isEnabled
    @Environment(\.controlSize) private var controlSize
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: controlSize == .large ? 17 : 14, weight: .semibold))
            .foregroundStyle(palette.prominentText)
            .padding(.horizontal, controlSize == .large ? 28 : 18)
            .frame(minHeight: controlSize == .large ? 52 : 38)
            .background(LinearGradient(colors: [palette.prominentTop, palette.prominentBottom], startPoint: .top, endPoint: .bottom), in: Capsule())
            .overlay(Capsule().stroke(.white.opacity(palette.dark ? 0.45 : 0.3), lineWidth: 1))
            .overlay(Capsule().inset(by: 1).stroke(LinearGradient(colors: [.white.opacity(0.55), .clear], startPoint: .top, endPoint: .center), lineWidth: 1))
            .shadow(color: palette.prominentBottom.opacity(0.32), radius: 13, y: 10)
            .brightness(configuration.isPressed ? -0.06 : 0)
            .opacity(isEnabled ? 1 : 0.45)
            .contentShape(Capsule())
    }
}

/// macOS 13–25: a neutral translucent capsule.
struct GlassFallbackStyle: ButtonStyle {
    var palette: Palette
    @Environment(\.isEnabled) private var isEnabled
    @Environment(\.controlSize) private var controlSize
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: controlSize == .large ? 17 : 13, weight: .semibold))
            .foregroundStyle(palette.text)
            .padding(.horizontal, controlSize == .large ? 26 : 14)
            .frame(minHeight: controlSize == .large ? 52 : 34)
            .background(palette.dark ? Color.white.opacity(configuration.isPressed ? 0.16 : 0.10) : Color.white.opacity(configuration.isPressed ? 0.72 : 0.55), in: Capsule())
            .background(.ultraThinMaterial, in: Capsule())
            .overlay(Capsule().stroke(palette.dark ? Color.white.opacity(0.16) : Color.white.opacity(0.85), lineWidth: 1))
            .opacity(isEnabled ? 1 : 0.35)
            .contentShape(Capsule())
    }
}
