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
    static var loadoutTitle: Font { .stencil(46) }
    static var discoverTitle: Font { .stencil(60) }
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

struct StudioStatusLabel: View {
    var title: String
    var tone: StudioStatusTone
    @Environment(\.palette) private var palette

    var body: some View {
        Label(title, systemImage: tone.symbol)
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(tone.color(in: palette))
            .fixedSize(horizontal: false, vertical: true)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(title)
            .accessibilityAddTraits(.isStaticText)
    }
}

/// Calm inline content. Interactive actions belong beside this view, not inside
/// its combined accessibility element. Announce successful saves at the caller.
struct StudioBanner: View {
    var title: String
    var detail: String? = nil
    var tone: StudioStatusTone = .information
    @Environment(\.palette) private var palette

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Image(systemName: tone.symbol)
                .foregroundStyle(tone.color(in: palette))
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: StudioMetrics.rowGap) {
                Text(title).font(.system(size: 13, weight: .medium)).foregroundStyle(palette.text)
                if let detail {
                    Text(detail).font(StudioTypography.help).foregroundStyle(palette.secondary)
                }
            }
            .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(StudioMetrics.cardInset)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: StudioMetrics.bannerRadius))
        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.bannerRadius).stroke(palette.panelStroke))
        .accessibilityElement(children: .combine)
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

    var fill: Color {
        switch self {
        case .movement: Palette.hex(0xe2bd72)
        case .communication: Palette.hex(0x7fa6e6)
        case .combat: Palette.hex(0xc46a2f)
        case .game: Palette.hex(0x4a4c42)
        case .unbound: Palette.hex(0x22251e)
        case .unavailable: Palette.hex(0x1a1c17)
        }
    }

    var ink: Color {
        switch self {
        case .movement, .combat: Palette.hex(0x1c170c)
        case .communication: Palette.hex(0x0d1220)
        case .game: Palette.hex(0xf3ecdc)
        case .unbound: Palette.hex(0xbdb5a1)
        case .unavailable: Palette.hex(0xa9a28f)
        }
    }
}

/// A button label, not a custom control. Wrap in a native Button so activation,
/// focus and disabled behavior remain native; the keyboard grid owns navigation.
/// Unavailable keys stay inspectable so the explanation is reachable.
/// Popovers that hold settings sit on an opaque panel, like the rest of Config Studio's content,
/// so their text stays legible over the coloured keyboard and busy rows behind them.
struct StudioPopoverBackground: ViewModifier {
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View {
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
    @Environment(\.palette) private var palette
    @Environment(\.colorSchemeContrast) private var contrast

    var body: some View {
        VStack(spacing: 2) {
            Text(key).font(.system(size: 11, weight: .semibold)).lineLimit(1)
            if let action, height >= 30 { Text(action).font(.system(size: 10)).lineLimit(1) }
        }
        .padding(.horizontal, 3)
        .foregroundStyle(role.ink)
        .frame(width: max(StudioMetrics.minimumHitTarget, width), height: height)
        .background {
            ZStack {
                role.fill
                if role == .unavailable {
                    Canvas { context, size in
                        var path = Path()
                        for offset in stride(from: -size.height, through: size.width, by: 8) {
                            path.move(to: CGPoint(x: offset, y: size.height))
                            path.addLine(to: CGPoint(x: offset + size.height, y: 0))
                        }
                        context.stroke(path, with: .color(.white.opacity(0.10)), lineWidth: 3)
                    }
                    .allowsHitTesting(false).accessibilityHidden(true)
                }
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: StudioMetrics.keyRadius))
        .overlay(RoundedRectangle(cornerRadius: StudioMetrics.keyRadius)
            .strokeBorder(role.ink.opacity(contrast == .increased ? 0.65 : 0.25), lineWidth: 1))
        .overlay {
            if selected {
                // Leave a ground-colored gap: brass directly on a Movement key
                // disappears in dark appearance. Selection never changes layout.
                RoundedRectangle(cornerRadius: StudioMetrics.keyRadius + 4)
                    .strokeBorder(palette.accent, lineWidth: 2).padding(-4)
                    .allowsHitTesting(false)
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(key), \(role == .unavailable ? role.rawValue : action ?? role.rawValue)")
        .accessibilityValue(selected ? "Selected" : "")
    }
}
