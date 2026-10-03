// Fixed devmap mp_toujane intro view, 1280x720, stock dm, default renderer.
// The lower right world surfaces must not turn magenta. SDK only; no game data.
import Foundation
import CoreGraphics
import ImageIO
func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8)); exit(1)
}
guard CommandLine.arguments.count == 2 else { fail("usage: toujane_rgb.swift screenshot.jpg") }
guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: CommandLine.arguments[1]) as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil),
      image.width == 1280, image.height == 720 else { fail("Expected 1280x720 Toujane intro screenshot") }
var pixels = [UInt8](repeating: 0, count: image.width * image.height * 4)
pixels.withUnsafeMutableBytes { buffer in
    guard let context = CGContext(data: buffer.baseAddress, width: image.width, height: image.height,
                                  bitsPerComponent: 8, bytesPerRow: image.width * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { fail("Cannot decode RGB") }
    context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
}
var magenta = 0, count = 0, sum = [0.0, 0.0, 0.0]
// Avoid the black UI border, text, and scroll bar; include sunlit buildings.
for y in 400..<620 {
    for x in 740..<1050 {
        let i = (y * image.width + x) * 4
        let r = Double(pixels[i]), g = Double(pixels[i + 1]), b = Double(pixels[i + 2])
        for c in 0..<3 { sum[c] += Double(pixels[i + c]) }
        if r > g * 1.2 + 4 && b > g * 1.2 + 4 { magenta += 1 }
        count += 1
    }
}
let fraction = Double(magenta) / Double(count)
let result: [String: Any] = ["magenta_fraction": fraction, "mean_rgb": sum.map { $0 / Double(count) },
                            "passed": fraction < 0.03 && sum[0] > sum[2]]
print(String(data: try JSONSerialization.data(withJSONObject: result, options: [.sortedKeys]), encoding: .utf8)!)
guard fraction < 0.03 && sum[0] > sum[2] else { fail("FAIL Toujane world RGB sanity: directional coefficients rendered as colors") }
