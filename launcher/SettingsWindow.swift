import SwiftUI
import AppKit

// Owner: ship-settings. The Settings window: the second window scene (id "settings", 900 wide on the
// SettingsWin board), opened by ⌘, , the app menu and `model.openSettings(_:)`. Seven panes behind a labelled
// symbol bar; the window's title is the pane's name, it reopens on the pane it was left on, and in the app it
// takes each pane's height (scrolling only when a pane is taller than the screen). Every change saves itself.

enum SettingsWindowMetrics {
    /// The board's size; the review harness renders 900 × 700 and, as its minimum, 900 × 560.
    static let width: CGFloat = 900, height: CGFloat = 700, minimumHeight: CGFloat = 560
    /// The shortest the window gets when it fits a pane.
    static let fittedMinimum: CGFloat = 300
}

/// Where a pane reports its content's height, so the window can take it.
private struct SettingsPaneHeightKey: EnvironmentKey { static let defaultValue: Binding<CGFloat>? = nil }
extension EnvironmentValues {
    var settingsPaneHeight: Binding<CGFloat>? {
        get { self[SettingsPaneHeightKey.self] }
        set { self[SettingsPaneHeightKey.self] = newValue }
    }
}

struct SettingsWindowRoot: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.colorScheme) private var scheme
    @Environment(\.colorSchemeContrast) private var contrast
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var headerHeight: CGFloat = 0
    @State private var paneHeight: CGFloat = 0
    var body: some View {
        let palette = Palette.of(scheme, contrast: contrast)
        VStack(spacing: 0) {
            VStack(spacing: 0) {
                SettingsTabBar(selection: $model.settingsPane)
                Rectangle().fill(palette.separator).frame(height: 1)
                notices
            }
            .onGeometryChange(for: CGFloat.self, of: { $0.size.height }) { headerHeight = $0 }
            SettingsPaneView(model: model, pane: model.settingsPane)
                .id(model.settingsPane)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .environment(\.settingsPaneHeight, $paneHeight)
        }
        .background { paneShortcuts }
        .capsuleControls()
        .background(palette.ground)
        .background(SettingsWindowReader(model: model))
        .navigationTitle(model.settingsPane.title)
        .appSheets(model, sheet: $model.settingsSheet, confirmation: $model.settingsConfirmation)
        .onDisappear { model.flushSettings() }
        .onChange(of: model.settingsPane) { model.rememberSettingsPane(); model.settingsFeedback = nil }
        .onChange(of: headerHeight + paneHeight) { fitWindow() }
        // The first layout happens before the window is on screen; fit it once it is.
        .onReceive(NotificationCenter.default.publisher(for: NSWindow.didBecomeMainNotification)) { note in
            if let window = note.object as? NSWindow, window === model.settingsWindow { fitWindow() }
        }
        .frame(width: SettingsWindowMetrics.width)
        .frame(minHeight: SettingsWindowMetrics.fittedMinimum, idealHeight: SettingsWindowMetrics.height, maxHeight: .infinity)
        .environment(\.palette, palette)
    }

    /// While the game runs, and when saving fails: said once, above every pane.
    @ViewBuilder private var notices: some View {
        if model.gameRunning || model.settingsSaveError != nil {
            VStack(spacing: 10) {
                if model.gameRunning {
                    Banner(.neutral, symbol: "gamecontroller", message: "Call of Duty 2 is running. Changes you make here apply the next time it starts.")
                }
                if let error = model.settingsSaveError {
                    Banner(.error, message: error) { Button("Try Again", action: model.saveSettings).glassAction().controlSize(.large) }
                }
            }
            .padding(.horizontal, 28).padding(.top, 16)
        }
    }

    /// ⌃Tab and ⌃⇧Tab step through the panes, as tabs do everywhere on the Mac.
    private var paneShortcuts: some View {
        HStack {
            Button("Next Pane") { step(1) }.keyboardShortcut(.tab, modifiers: .control)
            Button("Previous Pane") { step(-1) }.keyboardShortcut(.tab, modifiers: [.control, .shift])
        }
        .opacity(0).accessibilityHidden(true).allowsHitTesting(false)
    }
    private func step(_ offset: Int) {
        let panes = SettingsPane.allCases
        guard let index = panes.firstIndex(of: model.settingsPane) else { return }
        model.settingsPane = panes[(index + offset + panes.count) % panes.count]
    }

    /// The app's Settings window takes the pane's height, keeping its top edge where it is. The review harness
    /// renders fixed sizes instead.
    private func fitWindow() {
        guard !model.snapshot, paneHeight > 0, let window = model.settingsWindow, window.isVisible, !window.inLiveResize else { return }
        let screen = window.screen?.visibleFrame ?? NSScreen.main?.visibleFrame ?? .zero
        let chrome = window.frame.height - window.contentLayoutRect.height
        let content = min(max(SettingsWindowMetrics.fittedMinimum, (headerHeight + paneHeight).rounded(.up)), screen.height - chrome - 24)
        let height = content + chrome
        guard abs(window.frame.height - height) > 1 else { return }
        var frame = window.frame
        frame.origin.y += frame.height - height
        frame.size.height = height
        if frame.minY < screen.minY { frame.origin.y = screen.minY }
        window.setFrame(frame, display: true, animate: !reduceMotion)
    }
}

/// The pane switcher under the title: a symbol over a name, the selected one tinted, as the board draws it.
struct SettingsTabBar: View {
    @Binding var selection: SettingsPane
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 2) {
            ForEach(SettingsPane.allCases) { pane in
                let selected = pane == selection
                Button { selection = pane } label: {
                    VStack(spacing: 4) {
                        Image(systemName: pane.symbol).font(.system(size: 20, weight: .light)).frame(height: 22)
                        Text(pane.title).font(.system(size: 11, weight: selected ? .semibold : .regular)).lineLimit(1)
                    }
                    .foregroundStyle(selected ? palette.accent : palette.secondary)
                    .frame(width: 84, height: 50)
                    // The chosen pane: a neutral fill with its name in the accent (.ptab.on), as on a Mac settings toolbar.
                    .background(selected ? palette.button : .clear, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                    .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                }
                .buttonStyle(.plain)
                // Like a settings toolbar, the tabs don't take the window's first keyboard focus; ⌃Tab steps through them.
                .focusable(false)
                .help(pane.title)
                .accessibilityLabel(pane.title)
                .accessibilityAddTraits(selected ? [.isSelected] : [])
            }
        }
        .padding(.vertical, 8)
        .frame(maxWidth: .infinity)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Settings sections")
    }
}

struct SettingsPaneView: View {
    @ObservedObject var model: LauncherModel
    var pane: SettingsPane
    var body: some View {
        switch pane {
        case .general: GeneralPane(model: model)
        case .display: DisplayPane(model: model)
        case .gameData: GameDataPane(model: model)
        case .privacy: PrivacyPane(model: model)
        case .updates: UpdatesPane(model: model)
        case .advanced: AdvancedPane(model: model)
        case .about: AboutPane(model: model)
        }
    }
}

/// A pane's scrolling body with the board's padding (20 top, 28 sides, 26 bottom). It reports its content's
/// height so the window can fit it.
struct SettingsPaneScroll<Content: View>: View {
    // MARK: - ship-play: an overflow cue for About's real, longer acknowledgement list.
    var showOverflow = false
    @State private var moreBelow = false
    @Environment(\.palette) private var palette
    @ViewBuilder var content: Content
    @Environment(\.settingsPaneHeight) private var height
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) { content }
                .padding(.top, 20).padding(.horizontal, 28).padding(.bottom, 26)
                .frame(maxWidth: .infinity, alignment: .leading)
                .onGeometryChange(for: CGFloat.self, of: { $0.size.height }) { height?.wrappedValue = $0 }
        }
        .scrollIndicators(.automatic)
        .onScrollGeometryChange(for: Bool.self) { $0.contentSize.height > $0.visibleRect.maxY + 1 } action: { _, value in moreBelow = value }
        .overlay(alignment: .bottom) {
            if showOverflow && moreBelow {
                LinearGradient(colors: [palette.ground.opacity(0), palette.ground], startPoint: .top, endPoint: .bottom)
                    .frame(height: 24).allowsHitTesting(false).accessibilityHidden(true)
            }
        }
    }
}

/// Keeps sheets, panels and `model.settingsInFront` attached to this window.
private struct SettingsWindowReader: NSViewRepresentable {
    let model: LauncherModel
    func makeNSView(context: Context) -> WindowView { WindowView(model: model) }
    func updateNSView(_ view: WindowView, context: Context) { view.model = model; if let window = view.window { model.settingsWindow = window } }
    final class WindowView: NSView {
        var model: LauncherModel
        init(model: LauncherModel) { self.model = model; super.init(frame: .zero) }
        required init?(coder: NSCoder) { nil }
        override func viewDidMoveToWindow() { super.viewDidMoveToWindow(); if let window { model.settingsWindow = window } }
    }
}
