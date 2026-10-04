// Original stencil plate. This static fallback matches the layered CoD2Silicon.icon.
// No game data, fonts, logos, or third-party artwork are used.
import AppKit

let output = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
func color(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat) -> CGColor {
    NSColor(srgbRed: r, green: g, blue: b, alpha: 1).cgColor
}
func number() -> CGPath {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: 304, y: 242)); p.addLine(to: CGPoint(x: 620, y: 242))
    p.addQuadCurve(to: CGPoint(x: 720, y: 342), control: CGPoint(x: 720, y: 242))
    p.addLine(to: CGPoint(x: 720, y: 426))
    p.addQuadCurve(to: CGPoint(x: 680, y: 506), control: CGPoint(x: 720, y: 474))
    p.addLine(to: CGPoint(x: 394, y: 716)); p.addLine(to: CGPoint(x: 720, y: 716))
    p.addLine(to: CGPoint(x: 720, y: 794)); p.addLine(to: CGPoint(x: 304, y: 794))
    p.addLine(to: CGPoint(x: 304, y: 696)); p.addLine(to: CGPoint(x: 624, y: 456))
    p.addLine(to: CGPoint(x: 624, y: 342))
    p.addQuadCurve(to: CGPoint(x: 608, y: 326), control: CGPoint(x: 624, y: 326))
    p.addLine(to: CGPoint(x: 304, y: 326)); p.closeSubpath()
    return p
}
func bridge(_ c: CGContext) {
    c.fill(CGRect(x: 448, y: 230, width: 24, height: 110))
    c.beginPath(); c.move(to: CGPoint(x: 432, y: 444)); c.addLine(to: CGPoint(x: 456, y: 426))
    c.addLine(to: CGPoint(x: 646, y: 674)); c.addLine(to: CGPoint(x: 622, y: 692)); c.closePath(); c.fillPath()
    c.fill(CGRect(x: 560, y: 700, width: 24, height: 110))
}
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        let graphics = NSGraphicsContext(bitmapImageRep: bitmap)!
        NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
        let c = graphics.cgContext; let factor = CGFloat(pixels) / 1024
        c.translateBy(x: 0, y: CGFloat(pixels)); c.scaleBy(x: factor, y: -factor)
        let tile = CGPath(roundedRect: CGRect(x: 24, y: 24, width: 976, height: 976), cornerWidth: 224, cornerHeight: 224, transform: nil)
        c.addPath(tile); c.clip()
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let backdrop = CGGradient(colorsSpace: space, colors: [color(0.42, 0.46, 0.33), color(0.23, 0.28, 0.19)] as CFArray, locations: [0, 1])!
        c.drawLinearGradient(backdrop, start: CGPoint(x: 512, y: 24), end: CGPoint(x: 512, y: 1000), options: [])
        let plate = CGPath(roundedRect: CGRect(x: 96, y: 96, width: 832, height: 832), cornerWidth: 160, cornerHeight: 160, transform: nil)
        c.saveGState(); c.setShadow(offset: CGSize(width: 0, height: 12), blur: 28, color: color(0.12, 0.14, 0.10)); c.addPath(plate); c.setFillColor(color(0.34, 0.38, 0.27)); c.fillPath(); c.restoreGState()
        c.saveGState(); c.addPath(plate); c.clip()
        let steel = CGGradient(colorsSpace: space, colors: [color(0.39, 0.44, 0.31), color(0.28, 0.32, 0.22)] as CFArray, locations: [0, 1])!
        c.drawLinearGradient(steel, start: CGPoint(x: 512, y: 96), end: CGPoint(x: 512, y: 928), options: [])
        c.restoreGState(); c.addPath(plate); c.setStrokeColor(color(0.53, 0.57, 0.43)); c.setLineWidth(3); c.strokePath()
        c.saveGState(); c.addPath(number()); c.clip()
        let inlay = CGGradient(colorsSpace: space, colors: [color(0.87, 0.82, 0.65), color(0.73, 0.69, 0.53)] as CFArray, locations: [0, 1])!
        c.drawLinearGradient(inlay, start: CGPoint(x: 512, y: 242), end: CGPoint(x: 512, y: 794), options: [])
        c.restoreGState(); c.saveGState(); c.addPath(number()); c.clip()
        c.setFillColor(color(0.33, 0.38, 0.26)); bridge(c); c.restoreGState()
        if pixels >= 128 {
            c.setFillColor(color(0.71, 0.67, 0.54))
            for point in [(212.0, 212.0), (812.0, 212.0), (212.0, 812.0), (812.0, 812.0)] {
                c.fillEllipse(in: CGRect(x: point.0 - 11, y: point.1 - 11, width: 22, height: 22))
            }
        }
        NSGraphicsContext.restoreGraphicsState()
        let suffix = scale == 2 ? "@2x" : ""
        try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
    }
}
