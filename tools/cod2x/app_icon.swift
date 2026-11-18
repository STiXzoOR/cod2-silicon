// Static fallback for the app icon: the gold star with the cream 2 cut out of it, on the olive plate.
// Used only where actool cannot compile tools/cod2x/icon-source/CoD2 Unbound.icon (Command Line
// Tools without Xcode). The shapes come from launcher/BrandShapes.swift, which the bundler puts in
// front of this file; no game data, fonts, logos or third-party artwork are used.
import AppKit

let output = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let space = CGColorSpace(name: CGColorSpace.sRGB)!
func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: space, components: [CGFloat(hex >> 16 & 255) / 255, CGFloat(hex >> 8 & 255) / 255, CGFloat(hex & 255) / 255, alpha])!
}
func gradient(_ pair: (top: UInt32, base: UInt32)) -> CGGradient {
    CGGradient(colorsSpace: space, colors: [color(pair.top), color(pair.base)] as CFArray, locations: [0, 1])!
}
/// "M x y L x y … Z" rings, which is all the brand's shapes use.
func outline(_ data: String) -> CGPath {
    let path = CGMutablePath()
    var numbers: [CGFloat] = [], command: Character = "M"
    for token in data.split(separator: " ") {
        var text = Substring(token)
        if let first = text.first, first.isLetter {
            if first == "Z" { path.closeSubpath(); continue }
            command = first; text = text.dropFirst()
        }
        guard let value = Double(text) else { continue }
        numbers.append(CGFloat(value))
        guard numbers.count == 2 else { continue }
        let point = CGPoint(x: numbers[0], y: numbers[1])
        if command == "M" { path.move(to: point) } else { path.addLine(to: point) }
        numbers = []; command = "L"
    }
    return path
}
let star = outline(BrandShapes.star), two = outline(BrandShapes.two)
/// The 2 with its corners softened: the outline, then a round-joined stroke of it, each filled on its own.
func paintTwo(_ c: CGContext, stroke width: CGFloat) {
    c.addPath(two); c.fillPath()
    c.addPath(two.copy(strokingWithWidth: width, lineCap: .round, lineJoin: .round, miterLimit: 10)); c.fillPath()
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
        c.addPath(tile); c.setFillColor(color(BrandShapes.plate.base)); c.fillPath(); c.restoreGState()
        c.saveGState(); c.addPath(tile); c.clip()
        c.drawLinearGradient(gradient(BrandShapes.plate), start: CGPoint(x: 512, y: 100), end: CGPoint(x: 512, y: 924), options: [])
        // The mark is drawn for a 1024 plate; here the plate is the 824 tile.
        c.translateBy(x: 100, y: 100); c.scaleBy(x: 824.0 / 1024, y: 824.0 / 1024)
        c.beginTransparencyLayer(auxiliaryInfo: nil)
        c.saveGState(); c.addPath(star); c.clip()
        c.drawLinearGradient(gradient(BrandShapes.gold), start: CGPoint(x: 512, y: BrandShapes.starTop), end: CGPoint(x: 512, y: BrandShapes.starBase), options: [])
        c.restoreGState()
        c.setBlendMode(.clear); paintTwo(c, stroke: BrandShapes.cutStroke)
        c.endTransparencyLayer()
        c.saveGState(); c.setShadow(offset: CGSize(width: 0, height: -12 * factor), blur: 24 * factor, color: color(0x000000, 0.4))
        c.beginTransparencyLayer(auxiliaryInfo: nil)
        c.setFillColor(color(BrandShapes.cream.top)); paintTwo(c, stroke: BrandShapes.twoStroke)
        c.setBlendMode(.sourceIn)
        c.drawLinearGradient(gradient(BrandShapes.cream), start: CGPoint(x: 512, y: BrandShapes.twoTop), end: CGPoint(x: 512, y: BrandShapes.twoBase),
                             options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
        c.endTransparencyLayer()
        c.restoreGState()
        c.restoreGState()
        NSGraphicsContext.restoreGraphicsState()
        let suffix = scale == 2 ? "@2x" : ""
        try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
    }
}
