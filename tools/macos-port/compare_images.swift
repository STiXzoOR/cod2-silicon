// SDK-only screenshot comparison: A | absolute RGB difference (3x) | B.
// swift tools/macos-port/compare_images.swift A.jpg B.jpg /outside/repo/diff.png
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8)); exit(1)
}
func read(_ path: String) -> (Int, Int, [UInt8]) {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else { fail("Cannot read " + path) }
    var pixels = [UInt8](repeating: 0, count: image.width * image.height * 4)
    pixels.withUnsafeMutableBytes { buffer in
        guard let context = CGContext(data: buffer.baseAddress, width: image.width, height: image.height,
                                      bitsPerComponent: 8, bytesPerRow: image.width * 4,
                                      space: CGColorSpaceCreateDeviceRGB(),
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { fail("Cannot decode RGB") }
        context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
    }
    return (image.width, image.height, pixels)
}
guard CommandLine.arguments.count == 4 else { fail("usage: compare_images.swift A.jpg B.jpg output.png") }
let a = read(CommandLine.arguments[1]), b = read(CommandLine.arguments[2])
guard a.0 == b.0 && a.1 == b.1 else { fail("Image dimensions differ") }
let output = URL(fileURLWithPath: CommandLine.arguments[3]).standardizedFileURL
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().standardizedFileURL
guard !output.path.hasPrefix(root.path + "/") else { fail("Store game imagery outside the repository") }
let width = a.0, height = a.1
var panel = [UInt8](repeating: 255, count: width * 3 * height * 4)
var absolute = 0.0, squared = 0.0, changed = 0
for y in 0..<height {
    for x in 0..<width {
        let source = (y * width + x) * 4
        let left = (y * width * 3 + x) * 4
        var largest = 0
        for c in 0..<3 {
            let difference = abs(Int(a.2[source + c]) - Int(b.2[source + c]))
            absolute += Double(difference); squared += Double(difference * difference)
            largest = max(largest, difference)
            panel[left + c] = a.2[source + c]
            panel[left + width * 4 + c] = UInt8(min(255, difference * 3))
            panel[left + width * 8 + c] = b.2[source + c]
        }
        if largest > 32 { changed += 1 }
    }
}
panel.withUnsafeMutableBytes { buffer in
    guard let context = CGContext(data: buffer.baseAddress, width: width * 3, height: height,
                                  bitsPerComponent: 8, bytesPerRow: width * 3 * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue),
          let image = context.makeImage(),
          let destination = CGImageDestinationCreateWithURL(output as CFURL, UTType.png.identifier as CFString, 1, nil)
    else { fail("Cannot create comparison") }
    CGImageDestinationAddImage(destination, image, nil)
    guard CGImageDestinationFinalize(destination) else { fail("Cannot save comparison") }
}
let count = Double(width * height * 3)
let result: [String: Any] = ["width": width, "height": height, "rgb_mae_255": absolute / count,
                            "rgb_rmse_255": sqrt(squared / count),
                            "pixels_over_32_fraction": Double(changed) / Double(width * height),
                            "panel": output.path]
print(String(data: try JSONSerialization.data(withJSONObject: result, options: [.sortedKeys]), encoding: .utf8)!)
