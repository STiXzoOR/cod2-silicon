import SwiftUI

// Presentation only. These values and views never read profiles, save settings,
// infer rule compliance, or supply sample players/measurements.
enum StudioMetrics {
    static let loadoutColumn: CGFloat = 252
    static let groupColumn: CGFloat = 160
    static let inspectorColumn: CGFloat = 280
    static let pageInset: CGFloat = 24
    static let pageTop: CGFloat = 28
    static let panelInset: CGFloat = 20
    static let cardInset: CGFloat = 16
    static let sectionGap: CGFloat = 14
    static let rowGap: CGFloat = 6
    static let minimumHitTarget: CGFloat = 28
    static let panelRadius: CGFloat = 20
    static let cardRadius: CGFloat = 18
    static let bannerRadius: CGFloat = 14
    static let keyRadius: CGFloat = 6
}

enum StudioTypography {
    static var groupTitle: Font { .system(size: 20, weight: .bold) }
    static var rowTitle: Font { .system(size: 14) }
    static var help: Font { .system(size: 12) }
    static var consoleName: Font { .typewriter(11) }
}

/// The caller supplies the status words: unknown data must never become "Good".
enum StudioStatusTone {
    case positive, caution, negative, unknown, information

    var symbol: String {
        switch self {
        case .positive: "checkmark.circle"
        case .caution: "exclamationmark.triangle"
        case .negative: "exclamationmark.circle"
        case .unknown: "minus.circle"
        case .information: "info.circle"
        }
    }

    func color(in palette: Palette) -> Color {
        switch self {
        case .positive: palette.positive
        case .caution: palette.caution
        case .negative: palette.negative
        case .unknown, .information: palette.secondary
        }
    }
}

/// A readable label for a native Toggle, Slider, Picker or text field. Supply a
/// console name only in Expert. Controls keep their own label/value semantics.
struct StudioSettingLabel: View {
    var title: String
    var summary: String? = nil
    var consoleName: String? = nil
    @Environment(\.palette) private var palette

    var body: some View {
        VStack(alignment: .leading, spacing: StudioMetrics.rowGap) {
            Text(title).font(StudioTypography.rowTitle).foregroundStyle(palette.text)
            if let consoleName {
                Text(consoleName).font(StudioTypography.consoleName).foregroundStyle(palette.address)
            }
            if let summary {
                Text(summary).font(StudioTypography.help).foregroundStyle(palette.secondary)
            }
        }
        .fixedSize(horizontal: false, vertical: true)
    }
}

/// A noninteractive comparison readout. Blue is reinforced by a diamond and the
/// source's name. The parent owns the Take theirs action and lock enforcement.
struct StudioCompareValue: View {
    var value: String
    var source: String
    @Environment(\.palette) private var palette

    var body: some View {
        Label("\(value) · \(source)", systemImage: "diamond")
            .font(StudioTypography.help).monospacedDigit()
            .foregroundStyle(palette.compare)
            .fixedSize(horizontal: false, vertical: true)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("\(value), \(source)’s choice")
            .accessibilityAddTraits(.isStaticText)
    }
}

enum StudioKeyRole: String, CaseIterable {
    case movement = "Movement"
    case communication = "Communication"
    case combat = "Combat"
    case game = "Game & UI"
    case unbound = "Not bound"
    case unavailable = "The game can’t use this key"

    /// A quiet tint of a token, laid over the panel (the Binds boards' colour-mix into --panel):
    /// movement blue, communication green, combat red, game and interface grey.
    func fill(_ palette: Palette) -> Color {
        switch self {
        case .movement: palette.compare.opacity(0.24)
        case .communication: palette.positive.opacity(0.24)
        case .combat: palette.negative.opacity(0.22)
        case .game: palette.secondary.opacity(0.22)
        case .unbound: palette.well
        case .unavailable: .clear
        }
    }

    func edge(_ palette: Palette) -> Color {
        switch self {
        case .movement: palette.compare.opacity(0.55)
        case .communication: palette.positive.opacity(0.55)
        case .combat: palette.negative.opacity(0.5)
        case .game: palette.secondary.opacity(0.45)
        case .unbound, .unavailable: palette.wellStroke
        }
    }

    func ink(_ palette: Palette) -> Color {
        switch self {
        case .movement, .communication, .combat, .game: palette.text
        case .unbound, .unavailable: palette.tertiary
        }
    }
}

/// A button label, not a custom control. Wrap in a native Button so activation,
/// focus and disabled behavior remain native; the keyboard grid owns navigation.
/// Unavailable keys stay inspectable so the explanation is reachable.
/// Popovers that hold settings: on macOS 26 the system draws its own popover background, which
/// adapts to what's behind it. Earlier versions, and Reduce Transparency, keep the opaque panel of
/// Config Studio's content, so text stays legible over the coloured keyboard and busy rows.
struct StudioPopoverBackground: ViewModifier {
    @Environment(\.palette) private var palette
    @Environment(\.usesGlass) private var usesGlass
    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if usesGlass, !reduceTransparency, #available(macOS 26.0, *) { content } else { panel(content) }
        #else
        panel(content)
        #endif
    }
    @ViewBuilder private func panel(_ content: Content) -> some View {
        if #available(macOS 13.3, *) { content.presentationBackground(palette.panel) } else { content.background(palette.panel) }
    }
}
extension View {
    func studioPopover() -> some View { modifier(StudioPopoverBackground()) }
}

struct StudioKeyCap: View {
    var key: String
    var action: String? = nil
    var role: StudioKeyRole
    var selected = false
    var width: CGFloat = 44
    /// Half-height arrow keys are shorter and drop the caption.
    var height: CGFloat = 36
    /// The inspector's large cap: the label centred and bigger.
    var large = false
    @Environment(\.palette) private var palette
    @Environment(\.colorSchemeContrast) private var contrast

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: large ? 12 : StudioMetrics.keyRadius, style: .continuous)
        // The label at the top left and what the key does under it, as on the Binds board.
        VStack(alignment: .leading, spacing: 2) {
            Text(key).font(.system(size: large ? 22 : 11, weight: large ? .bold : .semibold)).lineLimit(1)
            if let action, height >= 30, !large {
                Spacer(minLength: 0)
                Text(action).font(.system(size: 11, weight: .medium)).lineLimit(1).foregroundStyle(role.ink(palette).opacity(0.8))
            }
        }
        .padding(.horizontal, large || height < 30 ? 0 : 6).padding(.vertical, large || height < 30 ? 0 : 5)
        .foregroundStyle(role.ink(palette))
        .frame(width: max(StudioMetrics.minimumHitTarget, width), height: height,
               alignment: large || height < 30 ? .center : .topLeading)
        .background {
            ZStack {
                palette.panel
                role.fill(palette)
                if role == .unavailable {
                    Canvas { context, size in
                        var path = Path()
                        for offset in stride(from: -size.height, through: size.width, by: 6) {
                            path.move(to: CGPoint(x: offset, y: size.height))
                            path.addLine(to: CGPoint(x: offset + size.height, y: 0))
                        }
                        context.stroke(path, with: .color(palette.track), lineWidth: 1.5)
                    }
                    .allowsHitTesting(false).accessibilityHidden(true)
                }
            }
        }
        .clipShape(shape)
        .overlay(shape.strokeBorder(contrast == .increased ? role.ink(palette).opacity(0.65) : role.edge(palette), lineWidth: 1))
        .overlay {
            if selected {
                // A ground-coloured gap between the key and the accent ring. Selection never changes layout.
                RoundedRectangle(cornerRadius: StudioMetrics.keyRadius + 4, style: .continuous)
                    .strokeBorder(palette.accent, lineWidth: 2).padding(-4)
                    .allowsHitTesting(false)
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(key), \(role == .unavailable ? role.rawValue : action ?? role.rawValue)")
        .accessibilityValue(selected ? "Selected" : "")
    }
}
