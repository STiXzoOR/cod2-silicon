import AppKit
import CoreGraphics
import Foundation

let pid = Int32(CommandLine.arguments[1])!
let mode = CGDisplayCopyDisplayMode(CGMainDisplayID())!
let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as! [[String: Any]]
let owned = windows.filter { ($0[kCGWindowOwnerPID as String] as? NSNumber)?.int32Value == pid }
let result: [String: Any] = [
    "active": NSWorkspace.shared.frontmostApplication?.processIdentifier == pid,
    "display": [mode.width, mode.height, mode.pixelWidth, mode.pixelHeight],
    "mouse": [NSEvent.mouseLocation.x, NSEvent.mouseLocation.y],
    "windows": owned.map { ["bounds": $0[kCGWindowBounds as String]!,
                             "layer": $0[kCGWindowLayer as String]!,
                             "alpha": $0[kCGWindowAlpha as String]!] }
]
print(String(data: try JSONSerialization.data(withJSONObject: result, options: [.sortedKeys]), encoding: .utf8)!)
