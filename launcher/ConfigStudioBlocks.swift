import SwiftUI

// Owner: ship-configs. Config Studio's status and banner views on the shared blocks (Components.swift).
// Kept apart from ConfigStudioStyle.swift, which the model tests compile without the view layer.

extension StudioStatusTone {
    /// The shared status vocabulary (Foundations): Good, Watch, Needs a change, Not measured.
    var kind: StatusKind {
        switch self { case .positive: .ready; case .caution: .needsChange; case .negative: .wontWork; case .unknown, .information: .notMeasured }
    }
    /// Banners: errors for problems, a soft tip for cautions, neutral otherwise.
    var banner: BannerTone {
        switch self { case .negative: .error; case .caution: .tip; default: .neutral }
    }
}

/// A status in words with the shared glyph and colour (StatusLabel). Short statuses use
/// `StatusLabel` itself; this one wraps a longer note instead of truncating it.
struct StudioStatusLabel: View {
    var title: String
    var tone: StudioStatusTone
    @Environment(\.palette) private var palette

    var body: some View {
        if title.count <= 32 && tone != .information {
            StatusLabel(tone.kind, title)
        } else {
            Label {
                Text(title).fixedSize(horizontal: false, vertical: true)
            } icon: {
                Image(systemName: tone == .information ? "info.circle" : tone.kind.symbol).font(.system(size: 13, weight: .semibold))
            }
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(tone == .information ? palette.secondary : tone.kind.color(palette))
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(title)
            .accessibilityAddTraits(.isStaticText)
        }
    }
}

/// Calm inline content on the shared Banner. Interactive actions belong beside this view, not
/// inside its combined accessibility element. Announce successful saves at the caller.
struct StudioBanner: View {
    var title: String
    var detail: String? = nil
    var tone: StudioStatusTone = .information

    var body: some View {
        let symbol = tone == .positive ? "checkmark.circle" : tone == .caution ? "exclamationmark.triangle" : nil
        if let detail { Banner(tone.banner, symbol: symbol, title: title, message: detail) }
        else { Banner(tone.banner, symbol: symbol, message: title) }
    }
}
