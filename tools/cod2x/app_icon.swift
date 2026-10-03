// Original geometric icon, generated locally; no game artwork is embedded.
import AppKit
let output = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
        let p = CGFloat(pixels)
        NSColor(calibratedRed: 0.12, green: 0.16, blue: 0.18, alpha: 1).setFill()
        NSBezierPath(roundedRect: NSRect(x: 0, y: 0, width: p, height: p), xRadius: p * 0.2, yRadius: p * 0.2).fill()
        NSColor(calibratedRed: 0.85, green: 0.74, blue: 0.49, alpha: 1).setStroke()
        let ring = NSBezierPath(ovalIn: NSRect(x: p * 0.1, y: p * 0.1, width: p * 0.8, height: p * 0.8))
        ring.lineWidth = p * 0.025; ring.stroke()
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: p * 0.36, weight: .bold),
            .foregroundColor: NSColor(calibratedRed: 0.92, green: 0.86, blue: 0.7, alpha: 1)]
        let text = "C2" as NSString
        let extent = text.size(withAttributes: attributes)
        text.draw(at: NSPoint(x: (p - extent.width) / 2, y: (p - extent.height) / 2), withAttributes: attributes)
        NSGraphicsContext.restoreGraphicsState()
        let suffix = scale == 2 ? "@2x" : ""
        try bitmap.representation(using: .png, properties: [:])!.write(to:
            output.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
    }
}
