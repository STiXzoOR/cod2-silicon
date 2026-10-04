// Static fallback for icon G: a stenciled 2 over a raised star on olive steel.
// Used only where actool cannot compile tools/cod2x/icon-source/CoD2 Silicon.icon
// (Command Line Tools without Xcode). The geometry matches the .icon layers;
// no game data, fonts, logos, or third-party artwork are used.
import AppKit

let output = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let space = CGColorSpace(name: CGColorSpace.sRGB)!
func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: space, components: [CGFloat(hex >> 16 & 255) / 255, CGFloat(hex >> 8 & 255) / 255, CGFloat(hex & 255) / 255, alpha])!
}
func gradient(_ a: UInt32, _ b: UInt32) -> CGGradient {
    CGGradient(colorsSpace: space, colors: [color(a), color(b)] as CFArray, locations: [0, 1])!
}
let star: [CGPoint] = [(512, 160), (592.8, 408.7), (854.4, 408.8), (642.8, 562.5), (723.6, 811.2),
                       (512, 657.5), (300.4, 811.2), (381.2, 562.5), (169.6, 408.8), (431.2, 408.7)].map { CGPoint(x: $0.0, y: $0.1) }
let centre = CGPoint(x: 512, y: 520)
func numeral() -> CGPath {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: 352, y: 404))
    p.addCurve(to: CGPoint(x: 516, y: 246), control1: CGPoint(x: 352, y: 300), control2: CGPoint(x: 428, y: 246))
    p.addCurve(to: CGPoint(x: 676, y: 392), control1: CGPoint(x: 612, y: 246), control2: CGPoint(x: 676, y: 306))
    p.addCurve(to: CGPoint(x: 566, y: 566), control1: CGPoint(x: 676, y: 470), control2: CGPoint(x: 628, y: 516))
    p.addLine(to: CGPoint(x: 372, y: 740)); p.addLine(to: CGPoint(x: 700, y: 740))
    return p.copy(strokingWithWidth: 124, lineCap: .butt, lineJoin: .miter, miterLimit: 10)
}
func bridges(_ c: CGContext) {
    for (rect, angle, pivot) in [(CGRect(x: 402, y: 214, width: 34, height: 96), -38.0, CGPoint(x: 419, y: 262)),
                                 (CGRect(x: 452, y: 612, width: 34, height: 110), 42.0, CGPoint(x: 469, y: 667)),
                                 (CGRect(x: 548, y: 676, width: 30, height: 118), 0.0, CGPoint(x: 563, y: 735))] {
        c.saveGState()
        c.translateBy(x: pivot.x, y: pivot.y); c.rotate(by: angle * .pi / 180); c.translateBy(x: -pivot.x, y: -pivot.y)
        c.fill(rect)
        c.restoreGState()
    }
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
        // macOS 13-25 icon grid: an 824-point rounded tile with room for its shadow.
        let tile = CGPath(roundedRect: CGRect(x: 100, y: 100, width: 824, height: 824), cornerWidth: 185, cornerHeight: 185, transform: nil)
        c.saveGState(); c.setShadow(offset: CGSize(width: 0, height: 10), blur: 24, color: color(0x000000, 0.35))
        c.addPath(tile); c.setFillColor(color(0x323819)); c.fillPath(); c.restoreGState()
        c.saveGState(); c.addPath(tile); c.clip()
        c.drawLinearGradient(gradient(0x6f7a44, 0x323819), start: CGPoint(x: 512, y: 100), end: CGPoint(x: 718, y: 924), options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
        c.addPath(CGPath(roundedRect: CGRect(x: 168, y: 168, width: 688, height: 688), cornerWidth: 132, cornerHeight: 132, transform: nil))
        c.setFillColor(color(0x000000, 0.06)); c.fillPath()
        c.addPath(CGPath(roundedRect: CGRect(x: 170, y: 170, width: 684, height: 684), cornerWidth: 130, cornerHeight: 130, transform: nil))
        c.setStrokeColor(color(0x8d9760, 0.8)); c.setLineWidth(6); c.strokePath()
        // Raised star: lit and shaded facets around the centre.
        for index in 0..<10 {
            let a = star[index], b = star[(index + 1) % 10]
            c.saveGState()
            c.beginPath(); c.move(to: centre); c.addLine(to: a); c.addLine(to: b); c.closePath(); c.clip()
            let lit = index % 2 == 1
            c.drawLinearGradient(lit ? gradient(0xd9c78f, 0x9c8a4e) : gradient(0x7d6d33, 0x3f3716),
                                 start: CGPoint(x: 169, y: 160), end: CGPoint(x: 855, y: 811), options: [])
            c.restoreGState()
        }
        if pixels >= 64 {
            c.setFillColor(color(0xb7ad86))
            for point in [(236.0, 236.0), (788.0, 236.0), (236.0, 788.0), (788.0, 788.0)] {
                c.fillEllipse(in: CGRect(x: point.0 - 16, y: point.1 - 16, width: 32, height: 32))
            }
        }
        // Stencilled numeral: soft shadow, cream paint, then the three bridges cut through.
        c.beginTransparencyLayer(auxiliaryInfo: nil)
        c.saveGState(); c.setShadow(offset: CGSize(width: 0, height: 14), blur: 22, color: color(0x000000, 0.38))
        c.addPath(numeral()); c.setFillColor(color(0xecdfb4)); c.fillPath(); c.restoreGState()
        c.setBlendMode(.clear); bridges(c)
        c.endTransparencyLayer()
        c.restoreGState()
        NSGraphicsContext.restoreGraphicsState()
        let suffix = scale == 2 ? "@2x" : ""
        try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
    }
}
