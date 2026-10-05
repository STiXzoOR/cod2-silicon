import SwiftUI
import AppKit

// Original fallback art, drawn natively from the approved design's vector data. The
// launcher shows the player's own loading screens instead whenever game data exists.

/// Minimal SVG path-data parser (M L H V C S Q T A Z, absolute and relative).
enum SVGPath {
    static func parse(_ data: String, transform: CGAffineTransform = .identity) -> Path {
        var path = Path()
        var tokens: [String] = []
        var current = ""
        for character in data {
            if character.isLetter && character != "e" {
                if !current.isEmpty { tokens.append(current); current = "" }
                tokens.append(String(character))
            } else if character == "," || character == " " || character == "\n" {
                if !current.isEmpty { tokens.append(current); current = "" }
            } else if character == "-" && !current.isEmpty && !current.hasSuffix("e") {
                tokens.append(current); current = "-"
            } else if character == "." && current.contains(".") {
                tokens.append(current); current = "."
            } else { current.append(character) }
        }
        if !current.isEmpty { tokens.append(current) }
        var index = 0, command: Character = "M"
        var point = CGPoint.zero, start = CGPoint.zero, control = CGPoint.zero, last: Character = " "
        func number() -> CGFloat { defer { index += 1 }; return CGFloat(Double(tokens[index]) ?? 0) }
        func t(_ p: CGPoint) -> CGPoint { p.applying(transform) }
        while index < tokens.count {
            if let letter = tokens[index].first, tokens[index].count == 1, letter.isLetter { command = letter; index += 1 }
            let relative = command.isLowercase
            let base = relative ? point : .zero
            switch Character(command.uppercased()) {
            case "M":
                point = CGPoint(x: base.x + number(), y: base.y + number()); start = point
                path.move(to: t(point)); command = relative ? "l" : "L"
            case "L": point = CGPoint(x: base.x + number(), y: base.y + number()); path.addLine(to: t(point))
            case "H": point.x = (relative ? point.x : 0) + number(); path.addLine(to: t(point))
            case "V": point.y = (relative ? point.y : 0) + number(); path.addLine(to: t(point))
            case "C":
                let c1 = CGPoint(x: base.x + number(), y: base.y + number()), c2 = CGPoint(x: base.x + number(), y: base.y + number())
                point = CGPoint(x: base.x + number(), y: base.y + number()); control = c2
                path.addCurve(to: t(point), control1: t(c1), control2: t(c2))
            case "S":
                let c1 = "CS".contains(last) ? CGPoint(x: 2 * point.x - control.x, y: 2 * point.y - control.y) : point
                let c2 = CGPoint(x: base.x + number(), y: base.y + number())
                point = CGPoint(x: base.x + number(), y: base.y + number()); control = c2
                path.addCurve(to: t(point), control1: t(c1), control2: t(c2))
            case "Q":
                let c = CGPoint(x: base.x + number(), y: base.y + number())
                point = CGPoint(x: base.x + number(), y: base.y + number()); control = c
                path.addQuadCurve(to: t(point), control: t(c))
            case "T":
                let c = "QT".contains(last) ? CGPoint(x: 2 * point.x - control.x, y: 2 * point.y - control.y) : point
                point = CGPoint(x: base.x + number(), y: base.y + number()); control = c
                path.addQuadCurve(to: t(point), control: t(c))
            case "A":
                let rx = abs(number()), ry = abs(number()); _ = number()
                let large = number() != 0, sweep = number() != 0
                let end = CGPoint(x: base.x + number(), y: base.y + number())
                arc(&path, from: point, to: end, rx: rx, ry: ry, large: large, sweep: sweep, transform: transform)
                point = end
            case "Z": path.closeSubpath(); point = start
            default: index += 1
            }
            last = Character(command.uppercased())
        }
        return path
    }
    // Endpoint-to-centre conversion for unrotated elliptical arcs.
    private static func arc(_ path: inout Path, from p0: CGPoint, to p1: CGPoint, rx: CGFloat, ry: CGFloat, large: Bool, sweep: Bool, transform: CGAffineTransform) {
        guard rx > 0, ry > 0, p0 != p1 else { path.addLine(to: p1.applying(transform)); return }
        let dx = (p0.x - p1.x) / 2, dy = (p0.y - p1.y) / 2
        var rx = rx, ry = ry
        let lambda = dx * dx / (rx * rx) + dy * dy / (ry * ry)
        if lambda > 1 { rx *= sqrt(lambda); ry *= sqrt(lambda) }
        let numerator = max(0, rx * rx * ry * ry - rx * rx * dy * dy - ry * ry * dx * dx)
        var factor = sqrt(numerator / (rx * rx * dy * dy + ry * ry * dx * dx))
        if large == sweep { factor = -factor }
        let cx = factor * rx * dy / ry + (p0.x + p1.x) / 2, cy = -factor * ry * dx / rx + (p0.y + p1.y) / 2
        let a0 = atan2((p0.y - cy) / ry, (p0.x - cx) / rx)
        var delta = atan2((p1.y - cy) / ry, (p1.x - cx) / rx) - a0
        if sweep && delta < 0 { delta += 2 * .pi } else if !sweep && delta > 0 { delta -= 2 * .pi }
        let steps = max(4, Int(abs(delta) / (.pi / 16)))
        for step in 1...steps {
            let angle = a0 + delta * CGFloat(step) / CGFloat(steps)
            path.addLine(to: CGPoint(x: cx + rx * cos(angle), y: cy + ry * sin(angle)).applying(transform))
        }
    }
}

/// A deterministic monochrome noise tile standing in for the design's film grain.
enum Grain {
    @MainActor static let tile: CGImage? = {
        let size = 192
        var state: UInt32 = 0x9e3779b9
        var bytes = [UInt8](repeating: 0, count: size * size * 4)
        for i in 0..<(size * size) {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5
            let v = UInt8(truncatingIfNeeded: 70 + Int(state % 116))
            bytes[i * 4] = v; bytes[i * 4 + 1] = v; bytes[i * 4 + 2] = v; bytes[i * 4 + 3] = 255
        }
        guard let provider = CGDataProvider(data: Data(bytes) as CFData), let space = CGColorSpace(name: CGColorSpace.sRGB) else { return nil }
        return CGImage(width: size, height: size, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: size * 4, space: space,
                       bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue), provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent)
    }()
}

struct GrainOverlay: View {
    var opacity: Double
    var blend: BlendMode
    var body: some View {
        if let tile = Grain.tile {
            Image(decorative: tile, scale: 2).resizable(resizingMode: .tile).blendMode(blend).opacity(opacity).allowsHitTesting(false)
        }
    }
}

private func hex(_ value: UInt32) -> Color { Palette.hex(value) }

// MARK: - Hero scenes (1440 × 620 design space, aspect-fill)

struct HeroScene: View {
    var scenery: Scenery
    var dark: Bool
    /// Width hidden to the left (under the sidebar): the scene is composed for the whole window.
    var leading: CGFloat = 0
    var body: some View {
        Canvas { context, size in
            let full = size.width + leading
            let scale = max(full / 1440, size.height / 620)
            context.translateBy(x: (full - 1440 * scale) / 2 - leading, y: (size.height - 620 * scale) / 2)
            context.scaleBy(x: scale, y: scale)
            switch (scenery, dark) {
            case (.desert, true): Self.desertDusk(&context)
            case (.desert, false): Self.desertDay(&context)
            case (.europe, true): Self.normandyNight(&context)
            case (.europe, false): Self.normandyDay(&context)
            case (.winter, true): Self.winterDusk(&context)
            case (.winter, false): Self.winterDay(&context)
            }
        }
        .accessibilityHidden(true)
    }
    private static let frame = CGRect(x: 0, y: 0, width: 1440, height: 620)
    private static func sky(_ context: inout GraphicsContext, _ stops: [(UInt32, CGFloat)]) {
        context.fill(Path(frame), with: .linearGradient(Gradient(stops: stops.map { .init(color: hex($0.0), location: $0.1) }),
                                                      startPoint: .zero, endPoint: CGPoint(x: 0, y: 620)))
    }
    private static func fill(_ context: inout GraphicsContext, _ d: String, _ color: UInt32, _ opacity: Double = 1, transform: CGAffineTransform = .identity) {
        context.fill(SVGPath.parse(d, transform: transform), with: .color(hex(color).opacity(opacity)))
    }
    private static func glow(_ context: inout GraphicsContext, _ center: CGPoint, _ radius: CGFloat, _ color: UInt32, _ opacity: Double) {
        context.fill(Path(ellipseIn: CGRect(x: center.x - radius, y: center.y - radius, width: radius * 2, height: radius * 2)),
                     with: .radialGradient(Gradient(colors: [hex(color).opacity(opacity), hex(color).opacity(0)]), center: center, startRadius: 0, endRadius: radius))
    }
    private static func disc(_ context: inout GraphicsContext, _ center: CGPoint, _ radius: CGFloat, _ color: UInt32, _ opacity: Double) {
        context.fill(Path(ellipseIn: CGRect(x: center.x - radius, y: center.y - radius, width: radius * 2, height: radius * 2)), with: .color(hex(color).opacity(opacity)))
    }
    private static func haze(_ context: inout GraphicsContext, _ rect: CGRect, _ color: UInt32, _ opacity: Double, blur: CGFloat) {
        context.drawLayer { layer in
            layer.addFilter(.blur(radius: blur))
            layer.fill(Path(ellipseIn: rect), with: .color(hex(color).opacity(opacity)))
        }
    }
    private static func rects(_ context: inout GraphicsContext, _ items: [(CGFloat, CGFloat, CGFloat, CGFloat)], _ color: UInt32, _ opacity: Double = 1) {
        var path = Path()
        for r in items { path.addRect(CGRect(x: r.0, y: r.1, width: r.2, height: r.3)) }
        context.fill(path, with: .color(hex(color).opacity(opacity)))
    }

    // North Africa at dusk: the design's Toujane hero.
    private static let desertTown: [(CGFloat, CGFloat, CGFloat, CGFloat)] = [(610, 444, 72, 80), (682, 420, 58, 104), (740, 434, 92, 90), (832, 404, 50, 120),
        (882, 428, 112, 96), (994, 398, 66, 126), (1060, 304, 22, 220), (1054, 296, 34, 9), (1070, 268, 2, 12), (1082, 422, 80, 102),
        (1162, 390, 58, 134), (1220, 416, 96, 108), (1316, 400, 60, 124), (1376, 432, 64, 92)]
    private static let desertWindows: [(CGFloat, CGFloat, CGFloat, CGFloat)] = [(704, 444, 6, 9), (772, 456, 6, 9), (904, 446, 6, 9), (1012, 414, 6, 9),
        (1100, 440, 6, 9), (1180, 406, 6, 9), (1250, 440, 6, 9), (1334, 416, 6, 9)]
    private static func desertSilhouette(_ context: inout GraphicsContext, ink: UInt32, palm: UInt32) {
        rects(&context, desertTown, ink)
        disc(&context, CGPoint(x: 1071, y: 288), 10, ink, 1)
        fill(&context, "M760 434 a14 14 0 0 1 28 0 Z", ink)
        fill(&context, "M1236 416 a16 16 0 0 1 32 0 Z", ink)
        context.stroke(SVGPath.parse("M583 478 C 578 430 584 392 598 352"), with: .color(hex(palm)), style: StrokeStyle(lineWidth: 7, lineCap: .round))
        for frond in ["M598 352 C 570 340 548 346 528 362", "M598 352 C 584 326 560 316 538 318", "M598 352 C 612 324 638 316 660 322",
                      "M598 352 C 628 344 652 352 668 370", "M598 352 C 600 330 596 312 586 300"] {
            context.stroke(SVGPath.parse(frond), with: .color(hex(palm)), style: StrokeStyle(lineWidth: 5, lineCap: .round))
        }
    }
    private static let duneBack = "M0 438 Q 210 400 430 424 T 870 408 T 1440 420 V620 H0Z"
    private static let duneFront = "M0 476 Q 270 450 540 466 T 1060 452 T 1440 462 V620 H0Z"
    private static let ridge = "M0 566 L 110 552 L 170 560 L 290 542 L 372 556 L 470 548 L 540 566 L 640 560 L 760 574 L 900 566 L 1040 578 L 1200 570 L 1440 580 V620 H0Z"
    static func desertDusk(_ context: inout GraphicsContext) {
        sky(&context, [(0x0a1214, 0), (0x1b2826, 0.34), (0x4c3f27, 0.6), (0x9c6733, 0.78), (0xc88846, 0.9)])
        glow(&context, CGPoint(x: 1252, y: 262), 170, 0xf6cf86, 0.75)
        disc(&context, CGPoint(x: 1252, y: 262), 40, 0xf1c47d, 0.92)
        haze(&context, CGRect(x: 220, y: 414, width: 1520, height: 116), 0xd08f4a, 0.16, blur: 18)
        fill(&context, duneBack, 0x3a2f1f, 0.6)
        fill(&context, duneFront, 0x251e14, 0.85)
        desertSilhouette(&context, ink: 0x15120c, palm: 0x120f0a)
        rects(&context, desertWindows, 0xe9b45f, 0.85)
        fill(&context, ridge, 0x0a0907)
    }
    static func desertDay(_ context: inout GraphicsContext) {
        sky(&context, [(0xb3bfbf, 0), (0xd6d2bf, 0.5), (0xead9b6, 0.8)])
        glow(&context, CGPoint(x: 1252, y: 262), 170, 0xfff4d8, 0.7)
        disc(&context, CGPoint(x: 1252, y: 262), 40, 0xfff1cf, 0.95)
        haze(&context, CGRect(x: 640, y: 130, width: 720, height: 90), 0xffffff, 0.3, blur: 14)
        fill(&context, duneBack, 0xc9a878, 0.65)
        fill(&context, duneFront, 0xb99a6a, 0.85)
        desertSilhouette(&context, ink: 0x86704e, palm: 0x6e5a3c)
        haze(&context, CGRect(x: 380, y: 440, width: 1200, height: 90), 0xf1e2c2, 0.35, blur: 16)
        fill(&context, ridge, 0xa98e64)
    }

    // Normandy: the design's Carentan hero by day, and a lamplit night version.
    private static let houses = ["M660 470 V438 L688 414 L716 438 V470Z", "M716 470 V444 L748 420 L780 444 V470Z", "M800 470 V430 L840 398 L880 430 V470Z",
        "M896 372 L917 290 L938 372Z", "M934 470 V420 L980 392 L1026 420 V470Z", "M1040 470 V440 L1070 418 L1100 440 V470Z",
        "M1112 470 V428 L1150 400 L1188 428 V470Z", "M1210 470 V446 L1236 426 L1262 446 V470Z", "M1280 470 V436 L1320 408 L1360 436 V470Z"]
    private static let hillBack = "M0 440 Q 260 412 520 430 T 1040 418 T 1440 428 V620 H0Z"
    private static let hillFront = "M0 482 Q 300 458 600 474 T 1160 462 T 1440 470 V620 H0Z"
    private static func village(_ context: inout GraphicsContext, ink: UInt32, bush: UInt32) {
        for house in houses { fill(&context, house, ink) }
        rects(&context, [(900, 372, 34, 98), (916, 270, 2, 22), (910, 278, 14, 2)], ink)
        var bushes = Path()
        bushes.addEllipse(in: CGRect(x: 564, y: 444, width: 92, height: 52))
        bushes.addEllipse(in: CGRect(x: 1334, y: 436, width: 120, height: 60))
        bushes.addEllipse(in: CGRect(x: 490, y: 464, width: 140, height: 44))
        context.fill(bushes, with: .color(hex(bush)))
    }
    private static let field = "M0 540 Q 200 520 420 534 T 860 528 T 1440 540 V620 H0Z"
    private static let road = "M0 560 C 240 548 480 572 720 558 S 1200 552 1440 566"
    static func normandyDay(_ context: inout GraphicsContext) {
        sky(&context, [(0x9fb0b4, 0), (0xcfd3c8, 0.5), (0xe7e0cd, 0.8)])
        haze(&context, CGRect(x: 580, y: 120, width: 840, height: 120), 0xffffff, 0.35, blur: 14)
        haze(&context, CGRect(x: 280, y: 80, width: 720, height: 80), 0xffffff, 0.28, blur: 14)
        fill(&context, hillBack, 0x8c9677, 0.65)
        fill(&context, hillFront, 0x5f6b45, 0.85)
        village(&context, ink: 0x3b4130, bush: 0x2e3426)
        fill(&context, field, 0x46502f)
        context.stroke(SVGPath.parse(road), with: .color(hex(0xd9cfb2).opacity(0.7)), lineWidth: 10)
    }
    static func normandyNight(_ context: inout GraphicsContext) {
        sky(&context, [(0x090f12, 0), (0x16201f, 0.38), (0x2c3428, 0.66), (0x4f4f37, 0.86)])
        glow(&context, CGPoint(x: 1236, y: 196), 150, 0xe8e4d6, 0.16)
        disc(&context, CGPoint(x: 1236, y: 196), 28, 0xe8e4d6, 0.85)
        haze(&context, CGRect(x: 580, y: 120, width: 840, height: 120), 0xc9cdbf, 0.06, blur: 14)
        fill(&context, hillBack, 0x2b3222, 0.7)
        fill(&context, hillFront, 0x1d2317, 0.9)
        village(&context, ink: 0x0f120c, bush: 0x0b0e09)
        rects(&context, [(682, 446, 6, 9), (760, 450, 6, 9), (836, 440, 6, 9), (912, 392, 5, 8), (974, 432, 6, 9), (1064, 448, 6, 9),
                         (1150, 438, 6, 9), (1232, 452, 6, 9), (1316, 444, 6, 9)], 0xe9b45f, 0.8)
        fill(&context, field, 0x13170e)
        context.stroke(SVGPath.parse(road), with: .color(hex(0x4a4636).opacity(0.55)), lineWidth: 10)
    }

    // Eastern Front: broken roofs, chimneys and an onion-domed church in snow.
    private static let ruins = "M0 76 H10 V60 L14 64 L18 58 V76 H28 V54 H34 L36 58 L40 52 V76 H52 V64 H60 V58 L64 62 V76 H78 V50 L82 54 L86 48 V76 H100 V62 H112 V76 H124 V56 L128 60 V76 H140 V66 H160 V100 H0Z"
    private static let ruinsTransform = CGAffineTransform(a: 5.25, b: 0, c: 0, d: 4.3, tx: 600, ty: 524 - 76 * 4.3)
    private static let church = "M1046 524 V352 H1060 V330 C 1060 318 1053 312 1053 302 C 1053 294 1060 290 1063 282 C 1066 290 1073 294 1073 302 C 1073 312 1066 318 1066 330 V352 H1080 V524 Z"
    private static let snowField = "M0 500 Q 300 484 620 496 T 1180 488 T 1440 494 V620 H0Z"
    private static func ruinedCity(_ context: inout GraphicsContext, ink: UInt32) {
        fill(&context, ruins, ink, transform: ruinsTransform)
        fill(&context, church, ink)
        rects(&context, [(1062, 262, 2, 20), (1057, 268, 12, 2), (712, 392, 10, 48), (1260, 404, 10, 40)], ink)
    }
    static func winterDusk(_ context: inout GraphicsContext) {
        sky(&context, [(0x0b0f14, 0), (0x1a2430, 0.4), (0x3b4856, 0.7), (0x6f7c88, 0.9)])
        glow(&context, CGPoint(x: 1196, y: 246), 150, 0xdfe6ec, 0.16)
        disc(&context, CGPoint(x: 1196, y: 246), 34, 0xdfe6ec, 0.55)
        fill(&context, duneBack, 0x2a3139, 0.7)
        fill(&context, duneFront, 0x1c2229, 0.9)
        ruinedCity(&context, ink: 0x0d1014)
        rects(&context, [(690, 470, 6, 9), (804, 456, 6, 9), (958, 478, 6, 9), (1186, 466, 6, 9), (1334, 486, 6, 9)], 0xe9b45f, 0.55)
        fill(&context, snowField, 0x3a434c, 0.55)
        fill(&context, ridge, 0x08090b)
        var snow = Path(); var state: UInt32 = 7
        for _ in 0..<70 {
            state = state &* 1103515245 &+ 12345; let x = CGFloat(state % 1440)
            state = state &* 1103515245 &+ 12345; let y = CGFloat(state % 560)
            snow.addEllipse(in: CGRect(x: x, y: y, width: 2.4, height: 2.4))
        }
        context.fill(snow, with: .color(.white.opacity(0.10)))
    }
    static func winterDay(_ context: inout GraphicsContext) {
        sky(&context, [(0xa9b4bc, 0), (0xcfd5d8, 0.5), (0xe6e8e6, 0.8)])
        glow(&context, CGPoint(x: 1196, y: 246), 150, 0xffffff, 0.45)
        disc(&context, CGPoint(x: 1196, y: 246), 34, 0xffffff, 0.75)
        fill(&context, duneBack, 0xb8c1c7, 0.65)
        fill(&context, duneFront, 0x98a2aa, 0.85)
        ruinedCity(&context, ink: 0x4a5058)
        fill(&context, snowField, 0xe9ecee)
        fill(&context, ridge, 0xc9cfd3)
    }
}

// MARK: - Thumbnails (160 × 100 design space)

struct SceneThumbnail: View {
    var scenery: Scenery
    var dark: Bool
    private struct Look { var sky: (UInt32, UInt32); var sun: UInt32; var sunOpacity: Double; var ground: UInt32; var ink: UInt32; var skyline: String }
    private static let skylines: [Scenery: String] = [
        .desert: "M0 78 H14 V66 H26 V72 H38 V60 H52 V70 H60 V56 H63 V44 H67 V56 H70 V68 H86 V62 H100 V74 H112 V64 H128 V72 H144 V66 H160 V100 H0Z",
        .europe: "M0 74 L10 66 L20 74 V72 H24 L34 62 L44 72 H50 V60 L56 52 L62 60 V72 H70 L72 50 L74 36 L76 50 L78 72 H90 L100 64 L110 72 H120 L128 66 L136 72 H160 V100 H0Z",
        .winter: "M0 76 H10 V60 L14 64 L18 58 V76 H28 V54 H34 L36 58 L40 52 V76 H52 V64 H60 V58 L64 62 V76 H78 V50 L82 54 L86 48 V76 H100 V62 H112 V76 H124 V56 L128 60 V76 H140 V66 H160 V100 H0Z",
    ]
    private var look: Look {
        let skyline = Self.skylines[scenery] ?? ""
        switch (scenery, dark) {
        case (.desert, true): return Look(sky: (0x1e2a2c, 0xc58a4c), sun: 0xf2c67a, sunOpacity: 0.9, ground: 0x3e3121, ink: 0x17130d, skyline: skyline)
        case (.europe, true): return Look(sky: (0x3c4a52, 0x9aa69f), sun: 0xe8e4d6, sunOpacity: 0.35, ground: 0x33402a, ink: 0x1a1e16, skyline: skyline)
        case (.winter, true): return Look(sky: (0x5d6873, 0xc9cdd0), sun: 0xffffff, sunOpacity: 0.25, ground: 0xd6dade, ink: 0x23272b, skyline: skyline)
        case (.desert, false): return Look(sky: (0xd9c8a4, 0xc58a4c), sun: 0xfff1cf, sunOpacity: 0.9, ground: 0xb99a6a, ink: 0x5a4630, skyline: skyline)
        case (.europe, false): return Look(sky: (0x9fb0b4, 0xd8d6c8), sun: 0xffffff, sunOpacity: 0.4, ground: 0x5f6b45, ink: 0x3b4130, skyline: skyline)
        case (.winter, false): return Look(sky: (0xaeb7bf, 0xe3e6e8), sun: 0xffffff, sunOpacity: 0.5, ground: 0xeef0f1, ink: 0x4a5058, skyline: skyline)
        }
    }
    var body: some View {
        Canvas { context, size in
            let look = look
            let scale = max(size.width / 160, size.height / 100)
            context.translateBy(x: (size.width - 160 * scale) / 2, y: (size.height - 100 * scale) / 2)
            context.scaleBy(x: scale, y: scale)
            context.fill(Path(CGRect(x: 0, y: 0, width: 160, height: 100)), with: .linearGradient(Gradient(colors: [hex(look.sky.0), hex(look.sky.1)]), startPoint: .zero, endPoint: CGPoint(x: 0, y: 100)))
            context.fill(Path(ellipseIn: CGRect(x: 111, y: 41, width: 18, height: 18)), with: .color(hex(look.sun).opacity(look.sunOpacity)))
            context.fill(Path(CGRect(x: 0, y: 74, width: 160, height: 26)), with: .color(hex(look.ground)))
            context.fill(SVGPath.parse(look.skyline), with: .color(hex(look.ink)))
        }
        .accessibilityHidden(true)
    }
}

/// The player's own loading screen for a map when available, otherwise the original fallback art.
struct MapArt: View {
    var map: String
    var image: NSImage?
    var hero = false
    @Environment(\.colorScheme) private var scheme
    var body: some View {
        if let image {
            Color.clear.overlay(Image(nsImage: image).resizable().interpolation(.high).aspectRatio(contentMode: .fill)).clipped().accessibilityHidden(true)
        } else if hero {
            HeroScene(scenery: MapCatalog.scenery(map), dark: scheme == .dark)
        } else {
            SceneThumbnail(scenery: MapCatalog.scenery(map), dark: scheme == .dark)
        }
    }
}

// MARK: - Marks

/// Icon G at sidebar scale: stencilled 2 over a raised star on an olive plate.
struct BrandMark: View {
    var body: some View {
        Canvas { context, size in
            let s = size.width / 824
            context.scaleBy(x: s, y: s)
            context.translateBy(x: -100, y: -100)
            let tile = Path(roundedRect: CGRect(x: 100, y: 100, width: 824, height: 824), cornerRadius: 185, style: .continuous)
            context.fill(tile, with: .linearGradient(Gradient(colors: [hex(0x6f7a44), hex(0x323819)]), startPoint: CGPoint(x: 512, y: 100), endPoint: CGPoint(x: 718, y: 924)))
            let points: [CGPoint] = [(512, 160), (592.8, 408.7), (854.4, 408.8), (642.8, 562.5), (723.6, 811.2), (512, 657.5), (300.4, 811.2), (381.2, 562.5), (169.6, 408.8), (431.2, 408.7)].map { CGPoint(x: $0.0, y: $0.1) }
            for index in 0..<10 {
                var facet = Path(); facet.move(to: CGPoint(x: 512, y: 520)); facet.addLine(to: points[index]); facet.addLine(to: points[(index + 1) % 10]); facet.closeSubpath()
                // Quieter facets than the full icon so the numeral leads at sidebar scale.
                context.fill(facet, with: .color(hex(index % 2 == 1 ? 0x9a8b55 : 0x4c4320)))
            }
            context.drawLayer { layer in
                layer.stroke(SVGPath.parse("M352 404 C 352 300 428 246 516 246 C 612 246 676 306 676 392 C 676 470 628 516 566 566 L 372 740 L 700 740"),
                             with: .color(hex(0xecdfb4)), style: StrokeStyle(lineWidth: 124, lineJoin: .miter))
                layer.blendMode = .destinationOut
                for (rect, angle, pivot) in [(CGRect(x: 452, y: 612, width: 34, height: 110), 42.0, CGPoint(x: 469, y: 667)),
                                             (CGRect(x: 548, y: 676, width: 30, height: 118), 0.0, CGPoint(x: 563, y: 735))] {
                    let turn = CGAffineTransform(translationX: pivot.x, y: pivot.y).rotated(by: angle * .pi / 180).translatedBy(x: -pivot.x, y: -pivot.y)
                    layer.fill(Path(rect).applying(turn), with: .color(.black))
                }
            }
        }
        .accessibilityHidden(true)
    }
}

/// Small dog tag beside the player's name in the sidebar.
struct MiniDogTag: View {
    @Environment(\.palette) private var palette
    var body: some View {
        Canvas { context, _ in
            context.fill(Path(roundedRect: CGRect(x: 2, y: 4, width: 26, height: 32), cornerRadius: 7), with: .color(hex(palette.dark ? 0xb9b4a6 : 0xa6a294)))
            context.fill(Path(ellipseIn: CGRect(x: 12.4, y: 7.4, width: 5.2, height: 5.2)), with: .color(palette.dark ? hex(0x22251b) : hex(0xefe8d8)))
            for (y, width) in [(18.0, 16.0), (23, 11), (28, 14)] {
                context.fill(Path(roundedRect: CGRect(x: 7, y: y, width: width, height: 2.4), cornerRadius: 1.2), with: .color(hex(0x6f6b5e)))
            }
        }
        .frame(width: 30, height: 38)
        .accessibilityHidden(true)
    }
}

/// The setup dog tag, stamped with the key as it is typed.
struct StampedDogTag: View {
    var lines: [String]
    var body: some View {
        ZStack(alignment: .topLeading) {
            Canvas { context, _ in
                context.stroke(SVGPath.parse("M66 6 C 40 6 30 16 30 24"), with: .color(hex(0x8d8877)), style: StrokeStyle(lineWidth: 3, lineCap: .round, dash: [1, 6]))
                let tag = Path(roundedRect: CGRect(x: 14, y: 22, width: 104, height: 146), cornerRadius: 26, style: .continuous)
                context.fill(tag, with: .linearGradient(Gradient(stops: [.init(color: hex(0xe6e2d6), location: 0), .init(color: hex(0xa9a597), location: 0.5), .init(color: hex(0xd4d0c3), location: 1)]),
                                                      startPoint: CGPoint(x: 14, y: 22), endPoint: CGPoint(x: 118, y: 168)))
                context.stroke(tag, with: .color(.black.opacity(0.25)), lineWidth: 1)
                context.fill(Path(ellipseIn: CGRect(x: 59, y: 33, width: 14, height: 14)), with: .color(hex(0x1a1c16)))
                context.stroke(SVGPath.parse("M22 40 C 40 30 70 28 96 34"), with: .color(.white.opacity(0.55)), style: StrokeStyle(lineWidth: 3, lineCap: .round))
            }
            VStack(alignment: .leading, spacing: 5) {
                ForEach(Array(lines.enumerated()), id: \.offset) { _, line in
                    Text(line).font(.typewriter(12, bold: true)).tracking(0.6).foregroundStyle(hex(0x4d4a40)).fixedSize()
                }
            }
            .padding(.leading, 28).padding(.top, 67)
        }
        .frame(width: 132, height: 176)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Dog tag stamped with the key as you type it")
    }
}

/// Four signal bars for a server's ping.
struct PingBars: View {
    var level: Int
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .bottom, spacing: 2) {
            ForEach(1...4, id: \.self) { bar in
                RoundedRectangle(cornerRadius: 1).fill(bar <= level ? color : palette.track).frame(width: 3, height: CGFloat(3 + bar * 3))
            }
        }
        .frame(height: 15, alignment: .bottom)
        .accessibilityHidden(true)
    }
    private var color: Color { level >= 3 ? palette.positive : level == 2 ? palette.accent : palette.negative }
}

/// A server name with its Quake colour codes, as one Text for wrapping and VoiceOver.
struct QuakeName: View {
    var name: String
    @Environment(\.palette) private var palette
    var body: some View { Text(attributed).accessibilityLabel(QuakeColors.plain(name)) }
    private var attributed: AttributedString {
        var result = AttributedString()
        for run in QuakeColors.runs(name) { var part = AttributedString(run.text); part.foregroundColor = palette.quake(run.code); result.append(part) }
        return result
    }
}
