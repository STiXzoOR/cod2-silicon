import SwiftUI

enum LauncherPage: String, CaseIterable, Identifiable {
    case home = "Play", servers = "Servers", settings = "Settings", media = "Library", about = "About"
    var id: String { rawValue }
    var symbol: String {
        switch self { case .home: "play.fill"; case .servers: "network"; case .settings: "slider.horizontal.3"; case .media: "photo.on.rectangle.angled"; case .about: "info.circle" }
    }
}

extension View {
    @ViewBuilder func launcherGlass() -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) { self.glassEffect(.regular, in: RoundedRectangle(cornerRadius: 16)) }
        else { self.background(.regularMaterial, in: RoundedRectangle(cornerRadius: 16)) }
        #else
        self.background(.regularMaterial, in: RoundedRectangle(cornerRadius: 16))
        #endif
    }
}
struct Eyebrow: View {
    var text: String
    var body: some View { Text(text.uppercased()).font(.system(.caption, design: .rounded).weight(.semibold)).tracking(2.2).foregroundStyle(.secondary) }
}
struct PageHeading: View {
    var title: String; var subtitle: String
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.system(size: 34, weight: .bold, design: .rounded)).accessibilityAddTraits(.isHeader)
            Text(subtitle).font(.body).foregroundStyle(.secondary)
        }
    }
}
struct Panel<Content: View>: View {
    var title: String
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if !title.isEmpty { Text(title).font(.title3.weight(.semibold)).accessibilityAddTraits(.isHeader) }
            content
        }.padding(20).frame(maxWidth: .infinity, alignment: .leading)
            .background(.background.opacity(0.75), in: RoundedRectangle(cornerRadius: 20))
            .overlay(RoundedRectangle(cornerRadius: 20).stroke(.primary.opacity(0.07), lineWidth: 1))
    }
}
struct QuakeName: View {
    var name: String
    @Environment(\.colorScheme) private var scheme
    private func color(_ code: Int) -> Color {
        switch code {
        case 0: .gray
        case 8: .orange
        case 9: .secondary
        case 1: .red
        case 2: scheme == .dark ? .green : Color(red: 0.05, green: 0.43, blue: 0.2)
        case 3: scheme == .dark ? .yellow : Color(red: 0.55, green: 0.34, blue: 0.02)
        case 4: .blue
        case 5: scheme == .dark ? .cyan : Color(red: 0.0, green: 0.43, blue: 0.55)
        case 6: .purple
        default: .primary
        }
    }
    var body: some View {
        // A single attributed Text preserves natural wrapping and VoiceOver order.
        Text(attributed).accessibilityLabel(QuakeColors.plain(name))
    }
    private var attributed: AttributedString {
        var result = AttributedString()
        for run in QuakeColors.runs(name) { var part = AttributedString(run.text); part.foregroundColor = color(run.code); result.append(part) }
        return result
    }
}
struct TerrainArtwork: View {
    @Environment(\.colorScheme) private var scheme
    var body: some View {
        Canvas { context, size in
            let dark = scheme == .dark
            let sky = Gradient(colors: dark ? [Color(red: 0.09, green: 0.2, blue: 0.24), Color(red: 0.19, green: 0.31, blue: 0.32)] : [Color(red: 0.72, green: 0.85, blue: 0.84), Color(red: 0.9, green: 0.88, blue: 0.76)])
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .linearGradient(sky, startPoint: .zero, endPoint: CGPoint(x: size.width, y: size.height)))
            let sun = CGRect(x: size.width * 0.7, y: size.height * 0.12, width: size.height * 0.3, height: size.height * 0.3)
            context.fill(Path(ellipseIn: sun), with: .color(Color(red: 0.94, green: 0.78, blue: 0.48).opacity(dark ? 0.5 : 0.9)))
            for layer in 0..<5 {
                var path = Path(); path.move(to: CGPoint(x: 0, y: size.height))
                for index in 0...90 {
                    let x = Double(index) / 90
                    let y = 0.47 + Double(layer) * 0.105 + sin(x * 7 + Double(layer) * 1.3) * 0.07 + sin(x * 19 + Double(layer)) * 0.02
                    path.addLine(to: CGPoint(x: x * size.width, y: y * size.height))
                }
                path.addLine(to: CGPoint(x: size.width, y: size.height)); path.closeSubpath()
                let t = Double(layer)
                context.fill(path, with: .color(Color(red: 0.18 - t * 0.018, green: 0.4 - t * 0.045, blue: 0.4 - t * 0.04).opacity(dark ? 0.85 : 0.65)))
            }
            for index in 0..<9 {
                let y = size.height * (0.74 + Double(index) * 0.032)
                var contour = Path(); contour.move(to: CGPoint(x: 0, y: y))
                contour.addCurve(to: CGPoint(x: size.width, y: y - 25), control1: CGPoint(x: size.width * 0.3, y: y - 50), control2: CGPoint(x: size.width * 0.75, y: y + 40))
                context.stroke(contour, with: .color(.white.opacity(0.08)), lineWidth: 1)
            }
        }.accessibilityHidden(true)
    }
}
struct Metric: View {
    var value: String; var label: String; var symbol: String
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).font(.title2).foregroundStyle(Color.accentColor)
            VStack(alignment: .leading, spacing: 4) { Text(value).font(.title3.weight(.semibold)); Text(label).font(.caption).foregroundStyle(.secondary) }
        }
    }
}
