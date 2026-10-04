// Original horizon and compass illustration, drawn without game assets.
import AppKit
let output = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale, p = CGFloat(pixels)
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
        let inset = p * 0.025
        let tile = NSBezierPath(roundedRect: NSRect(x: inset, y: inset, width: p - inset * 2, height: p - inset * 2), xRadius: p * 0.215, yRadius: p * 0.215)
        tile.addClip()
        NSGradient(starting: NSColor(calibratedRed: 0.04, green: 0.18, blue: 0.24, alpha: 1), ending: NSColor(calibratedRed: 0.28, green: 0.56, blue: 0.57, alpha: 1))!.draw(in: tile, angle: 90)
        let sun = NSBezierPath(ovalIn: NSRect(x: p * 0.57, y: p * 0.53, width: p * 0.26, height: p * 0.26))
        NSGradient(starting: NSColor(calibratedRed: 1, green: 0.9, blue: 0.65, alpha: 1), ending: NSColor(calibratedRed: 0.97, green: 0.64, blue: 0.31, alpha: 1))!.draw(in: sun, angle: 90)
        for layer in 0..<4 {
            let hill = NSBezierPath(); hill.move(to: .zero)
            for index in 0...80 {
                let x = CGFloat(index) / 80
                let y = 0.42 - CGFloat(layer) * 0.065 + sin(x * 7 + CGFloat(layer) * 1.3) * 0.065 + sin(x * 18 + CGFloat(layer)) * 0.018
                hill.line(to: NSPoint(x: x * p, y: y * p))
            }
            hill.line(to: NSPoint(x: p, y: 0)); hill.close()
            NSColor(calibratedRed: 0.16 - CGFloat(layer) * 0.025, green: 0.38 - CGFloat(layer) * 0.055, blue: 0.38 - CGFloat(layer) * 0.05, alpha: 1).setFill(); hill.fill()
        }
        for index in 0..<6 {
            let contour = NSBezierPath(); let y = p * (0.13 + CGFloat(index) * 0.033)
            contour.move(to: NSPoint(x: 0, y: y))
            contour.curve(to: NSPoint(x: p, y: y + p * 0.02), controlPoint1: NSPoint(x: p * 0.3, y: y + p * 0.07), controlPoint2: NSPoint(x: p * 0.7, y: y - p * 0.035))
            NSColor.white.withAlphaComponent(0.11).setStroke(); contour.lineWidth = max(0.5, p * 0.0015); contour.stroke()
        }
        let shadow = NSShadow(); shadow.shadowColor = .black.withAlphaComponent(0.25); shadow.shadowBlurRadius = p * 0.035; shadow.shadowOffset = NSSize(width: 0, height: -p * 0.02); shadow.set()
        let compass = NSBezierPath()
        let points: [(CGFloat, CGFloat)] = [(0.43,0.82),(0.48,0.57),(0.68,0.5),(0.48,0.45),(0.43,0.2),(0.38,0.45),(0.18,0.5),(0.38,0.57)]
        compass.move(to: NSPoint(x: points[0].0 * p, y: points[0].1 * p))
        for point in points.dropFirst() { compass.line(to: NSPoint(x: point.0 * p, y: point.1 * p)) }; compass.close()
        NSColor(calibratedRed: 0.94, green: 0.96, blue: 0.88, alpha: 0.95).setFill(); compass.fill()
        let fold = NSBezierPath(); fold.move(to: NSPoint(x: p * 0.43, y: p * 0.82)); fold.line(to: NSPoint(x: p * 0.43, y: p * 0.5)); fold.line(to: NSPoint(x: p * 0.48, y: p * 0.57)); fold.close()
        NSColor(calibratedRed: 0.66, green: 0.84, blue: 0.8, alpha: 1).setFill(); fold.fill()
        NSGraphicsContext.restoreGraphicsState()
        let suffix = scale == 2 ? "@2x" : ""
        try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
    }
}
