import AppKit
import SwiftUI

// Owner: ship-home-library. The pull-down menus of Home's loadout chip and Library's demo More button:
// a real AppKit menu under the control, so it has native keyboard handling, ticks and shortcuts, and the
// control itself is an ordinary button VoiceOver can name (a SwiftUI Menu drawn as a plain label isn't).

enum PullDownMenu {
    enum Entry {
        case header(String)
        case separator
        /// `detail` sits right-aligned after the title (“250 fps”); `key` with `modifiers` is the shortcut shown.
        case item(_ title: String, detail: String? = nil, checked: Bool = false, enabled: Bool = true,
                  key: String = "", modifiers: NSEvent.ModifierFlags = [], action: @MainActor () -> Void)
    }
    @MainActor private final class Target: NSObject {
        var actions: [@MainActor () -> Void] = []
        @objc func run(_ sender: NSMenuItem) { actions[sender.tag]() }
    }
    /// Shows `entries` under `view` and returns when the menu closes (menu tracking is synchronous).
    @MainActor static func show(_ entries: [Entry], below view: NSView) {
        let menu = NSMenu(), target = Target()
        menu.autoenablesItems = false
        let font = NSFont.menuFont(ofSize: 0)
        // Right-align details on one tab stop past the longest title.
        let widest = entries.reduce(CGFloat(0)) { width, entry in
            guard case .item(let title, _, _, _, _, _, _) = entry else { return width }
            return max(width, (title as NSString).size(withAttributes: [.font: font]).width)
        }
        let tab = NSParagraphStyle.default.mutableCopy() as? NSMutableParagraphStyle
        tab?.tabStops = [NSTextTab(textAlignment: .right, location: widest + 90)]
        for entry in entries {
            switch entry {
            case .header(let title):
                menu.addItem(NSMenuItem.sectionHeader(title: title))
            case .separator:
                menu.addItem(.separator())
            case .item(let title, let detail, let checked, let enabled, let key, let modifiers, let action):
                let row = NSMenuItem(title: title, action: #selector(Target.run(_:)), keyEquivalent: key)
                row.keyEquivalentModifierMask = modifiers
                row.target = target; row.tag = target.actions.count; target.actions.append(action)
                row.state = checked ? .on : .off
                row.isEnabled = enabled
                if let detail, let tab {
                    let text = NSMutableAttributedString(string: title + "\t", attributes: [.font: font, .paragraphStyle: tab])
                    text.append(NSAttributedString(string: detail, attributes: [.font: NSFont.monospacedDigitSystemFont(ofSize: font.pointSize, weight: .regular),
                                                                               .foregroundColor: NSColor.secondaryLabelColor, .paragraphStyle: tab]))
                    row.attributedTitle = text
                    row.setAccessibilityLabel("\(title), \(detail)")
                }
                menu.addItem(row)
            }
        }
        menu.popUp(positioning: nil, at: NSPoint(x: 0, y: view.isFlipped ? view.bounds.height + 6 : -6), in: view)
        withExtendedLifetime(target) {}
    }
}

/// The AppKit view a pull-down menu opens under: put it in the control's background.
final class PullDownAnchorBox { weak var view: NSView? }
struct PullDownAnchor: NSViewRepresentable {
    let box: PullDownAnchorBox
    func makeNSView(context: Context) -> NSView { let view = NSView(); box.view = view; return view }
    func updateNSView(_ view: NSView, context: Context) { box.view = view }
}
