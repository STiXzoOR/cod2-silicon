import Foundation
import ImageIO
import CoreGraphics

let url = URL(fileURLWithPath: CommandLine.arguments[1])
guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fatalError("Cannot read captured frame")
}
let w = image.width, h = image.height
var pixels = [UInt8](repeating: 0, count: w * h * 4)
pixels.withUnsafeMutableBytes { bytes in
    let context = CGContext(data: bytes.baseAddress, width: w, height: h,
        bitsPerComponent: 8, bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    // CGImage drawing into this bitmap stores the screenshot's top row first.
    context.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
}
var sum = 0.0, count = 0
for y in (h * 70 / 100)..<(h * 95 / 100) {
    for x in (w / 4)..<(w * 3 / 4) {
        let i = (y * w + x) * 4
        sum += 0.2126 * Double(pixels[i]) + 0.7152 * Double(pixels[i+1]) + 0.0722 * Double(pixels[i+2])
        count += 1
    }
}
let result: [String: Any] = ["width": w, "height": h, "floor_luma": sum / Double(count)]
print(String(data: try JSONSerialization.data(withJSONObject: result, options: [.sortedKeys]), encoding: .utf8)!)
