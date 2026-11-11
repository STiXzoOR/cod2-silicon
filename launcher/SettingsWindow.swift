import SwiftUI
import AppKit

// Owner: ship-settings. The Settings window: the second window scene (id "settings", 900 × 700 on the
// SettingsWin board), opened by ⌘, , the app menu and `model.openSettings(_:)`. Seven panes picked
// from the tab bar; the window's title is the pane's name. Its sheets and alerts attach to it.

enum SettingsWindowMetrics {
    static let width: CGFloat = 900, height: CGFloat = 700, minimumHeight: CGFloat = 560
}

struct SettingsWindowRoot: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.colorScheme) private var scheme
    @Environment(\.colorSchemeContrast) private var contrast
    var body: some View {
        let palette = Palette.of(scheme, contrast: contrast)
        VStack(spacing: 0) {
            SettingsTabBar(selection: $model.settingsPane)
            Rectangle().fill(palette.separator).frame(height: 1)
            SettingsPaneView(model: model, pane: model.settingsPane)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .capsuleControls()
        .background(palette.ground)
        .background(SettingsWindowReader(model: model))
        .navigationTitle(model.settingsPane.title)
        // While the game runs, settings wait for it to close, as the old page did.
        .disabled(model.gameRunning)
        .appSheets(model, sheet: $model.settingsSheet, confirmation: $model.settingsConfirmation)
        .onDisappear { model.flushSettings() }
        .frame(width: SettingsWindowMetrics.width)
        .frame(minHeight: SettingsWindowMetrics.minimumHeight, idealHeight: SettingsWindowMetrics.height, maxHeight: .infinity)
        .environment(\.palette, palette)
    }
}

/// The pane switcher under the title: a symbol over a name, the selected one tinted.
struct SettingsTabBar: View {
    @Binding var selection: SettingsPane
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(spacing: 2) {
            ForEach(SettingsPane.allCases) { pane in
                let selected = pane == selection
                Button { selection = pane } label: {
                    VStack(spacing: 4) {
                        Image(systemName: pane.symbol).font(.system(size: 17)).frame(height: 22)
                        Text(pane.title).font(.system(size: 11, weight: selected ? .semibold : .regular)).lineLimit(1)
                    }
                    .foregroundStyle(selected ? palette.accent : palette.secondary)
                    .frame(width: 84, height: 50)
                    .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                    .contentShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
                }
                .buttonStyle(.plain)
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

/// A pane's scrolling body with the board's padding (20 top, 28 sides, 26 bottom).
struct SettingsPaneScroll<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) { content }
                .padding(.top, 20).padding(.horizontal, 28).padding(.bottom, 26)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .scrollIndicators(.automatic)
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
