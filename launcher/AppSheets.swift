import SwiftUI

// The app's sheets (AllSheets board), presented from the main window (`model.sheet`) or the Settings
// window (`model.settingsSheet`); `model.present(_:)` picks the window in front. Shared file: a unit
// adds its case in its own group below and its view in its own sheets file (ServerSheets.swift,
// SettingsSheets.swift, ShellSheets.swift), then one line in `AppSheetView`.
enum AppSheet: Identifiable {
    // MARK: ship-servers (ServerSheets.swift)
    /// File › Connect to Address… (⌘L); `address` prefills the field (a pasted or linked address).
    case connect(address: String?)
    /// A passworded server: asks before joining.
    case password(server: String)
    /// Joining a CoD2x server while Original 1.3 is chosen.
    case switchVersion(EngineRequest)

    // MARK: ship-settings (SettingsSheets.swift)
    case replaceKey
    case whatsNew
    /// Home’s What’s New in full: every release and news line (HomeView.swift).
    case news
    case reportProblem
    case acknowledgements

    // MARK: ship-shell (ShellSheets.swift)
    case keyboardShortcuts
    case gameCouldNotStart(reason: String)
    /// Once, for people set up before Setup asked: Send crash reports?
    case crashReportsQuestion

    // MARK: ship-configs (ConfigSheets.swift)
    /// Loadout › New Loadout… (⌘N) and Configs' New Loadout row.
    case newLoadout
    /// Loadout › Rename… for the loadout Configs shows.
    case renameLoadout

    var id: String {
        switch self {
        case .connect: "connect"
        case .password(let server): "password-" + server
        case .switchVersion(let request): "switch-version-" + request.id
        case .replaceKey: "replace-key"
        case .whatsNew: "whats-new"
        case .news: "news"
        case .reportProblem: "report-problem"
        case .acknowledgements: "acknowledgements"
        case .keyboardShortcuts: "keyboard-shortcuts"
        case .gameCouldNotStart: "game-could-not-start"
        case .crashReportsQuestion: "crash-reports-question"
        case .newLoadout: "new-loadout"
        case .renameLoadout: "rename-loadout"
        }
    }
}

/// The view for each sheet. Both windows present through this, so a sheet looks the same over either.
struct AppSheetView: View {
    @ObservedObject var model: LauncherModel
    var sheet: AppSheet
    var body: some View {
        switch sheet {
        case .connect(let address): ConnectSheet(model: model, address: address)
        case .password(let server): PasswordSheet(model: model, address: server)
        case .switchVersion(let request): SwitchVersionSheet(model: model, request: request)
        case .replaceKey: ReplaceKeySheet(model: model)
        case .whatsNew: WhatsNewSheet(model: model)
        case .news: HomeNewsSheet(model: model)
        case .reportProblem: ReportProblemSheet(model: model)
        case .acknowledgements: AcknowledgementsSheet(model: model)
        case .keyboardShortcuts: KeyboardShortcutsSheet(model: model)
        case .gameCouldNotStart(let reason): GameCouldNotStartSheet(model: model, reason: reason)
        case .crashReportsQuestion: CrashReportsQuestionSheet(model: model)
        case .newLoadout: NewLoadoutSheet(model: model)
        case .renameLoadout: RenameLoadoutSheet(model: model)
        }
    }
}

extension View {
    /// Hosts the app's sheets and single-decision alerts on a window's root view.
    func appSheets(_ model: LauncherModel, sheet: Binding<AppSheet?>, confirmation: Binding<ConfirmationRequest?>) -> some View {
        self.sheet(item: sheet) { AppSheetView(model: model, sheet: $0).sheetShapeAudit() }
            .confirmationAlert(confirmation)
    }
}
