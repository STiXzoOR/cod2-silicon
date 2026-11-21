import AppKit
import SwiftUI

// Owner: share-launcher (0.4.1). Share Your Setup's state: the records of shared loadouts, each
// loadout's cleaned text for the menus, the sheet's states, and Stop Sharing. The views are in
// ConfigShareView.swift; what is sent and how is in ConfigShare.swift.

/// One per launcher folder: the records file and each loadout's cleaned text, so the menus know at
/// once whether a loadout has anything to share.
@MainActor final class ConfigShareCenter: ObservableObject {
    /// The launcher's own folder, where `shared-setups.json` lives; nil keeps everything in memory
    /// (the review harness).
    let home: URL?
    @Published private(set) var records: ConfigShareRecordFile
    @Published private(set) var analyses: [String: Analysis] = [:]
    /// Loadouts whose Stop Sharing is on its way, so the menu doesn't offer it twice.
    @Published private(set) var withdrawing: Set<String> = []
    var client = ConfigShareClient()

    enum Analysis: Equatable {
        case ready(source: String, ConfigShare.Prepared)
        case failed(source: String)
        var prepared: ConfigShare.Prepared? { if case .ready(_, let prepared) = self { prepared } else { nil } }
        var source: String { switch self { case .ready(let source, _), .failed(let source): source } }
    }

    init(home: URL?) {
        self.home = home
        records = home.map(ConfigShareRecordFile.load(home:)) ?? .init()
    }
    /// The model's own center, made on first use and kept as long as the model (the review harness
    /// makes a model per screen, so no screen sees another's records).
    nonisolated(unsafe) private static var key: UInt8 = 0
    static func center(for model: LauncherModel) -> ConfigShareCenter {
        if let center = objc_getAssociatedObject(model, &key) as? ConfigShareCenter { return center }
        let center = ConfigShareCenter(home: model.snapshot ? nil : model.home)
        objc_setAssociatedObject(model, &key, center, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
        return center
    }

    func record(_ loadout: String) -> ConfigShareRecord? { records.record(loadout) }
    func analysis(_ loadout: String) -> Analysis? { analyses[loadout.lowercased()] }
    var rememberedName: String { records.name ?? "" }

    func remember(_ record: ConfigShareRecord, loadout: String) throws {
        var next = records
        next.remember(record, loadout: loadout)
        if let home { try next.save(home: home) }
        records = next
    }
    func forget(_ loadout: String) throws {
        var next = records
        next.forget(loadout)
        if let home { try next.save(home: home) }
        records = next
    }

    /// Cleans every loadout whose text changed since it was last cleaned. Cheap when nothing changed.
    func refresh(_ studio: ConfigStudioModel) {
        guard let session = studio.session, let catalog = studio.catalog else { return }
        for profile in session.profiles {
            guard let read = try? session.readProfile(profile.id) else { continue }
            analyze(read, catalog: catalog)
        }
    }
    func analyze(_ read: ConfigProfileRead, catalog: ConfigCatalog) {
        let text = ConfigShare.loadoutText(read), key = read.profile.id.lowercased()
        let source = ConfigShare.sha256(text)
        guard analyses[key]?.source != source, !pending.contains(key + source) else { return }
        pending.insert(key + source)
        Task {
            let result: Analysis
            do { result = .ready(source: source, try await ConfigShare.prepare(text, catalog: ConfigImportCatalogAdapter(catalog: catalog))) }
            catch { result = .failed(source: source) }
            pending.remove(key + source)
            analyses[key] = result
        }
    }
    private var pending: Set<String> = []
    /// The review harness: a loadout's analysis without cleaning anything.
    func setAnalysis(_ analysis: Analysis, loadout: String) { analyses[loadout.lowercased()] = analysis }

    /// Why `loadout` can't be shared now, in the sentence the Player Setups menu shows; nil when it can.
    /// Unknown until its text has been cleaned, and then nothing stops it here: the sheet cleans it again.
    func refusal(_ profile: ConfigProfile, studio: ConfigStudioModel) -> String? {
        if studio.isReadOnly { return "Sharing is off while Call of Duty 2 is running." }
        if record(profile.id) != nil { return "\(profile.displayName) is already shared. To send it again, choose Stop Sharing from its menu first." }
        switch analysis(profile.id) {
        case .failed: return "\(profile.displayName) couldn’t be read for sharing. Open it in Configs, then try again."
        case .ready(_, let prepared):
            switch prepared.problem {
            case .nothingToShare: return "\(profile.displayName) still has the game’s own settings. Change something first, then share it."
            case .tooLarge: return "\(profile.displayName) is larger than Player Setups takes: 64 KB or 3,000 lines."
            case nil: return nil
            }
        case nil: return nil
        }
    }

    /// Stop Sharing…, after its alert: withdraws the request, then forgets the record. A withdraw that
    /// fails says so and keeps the record, so Stop Sharing can be chosen again.
    func withdraw(_ profile: ConfigProfile, studio: ConfigStudioModel) {
        guard let record = record(profile.id), !withdrawing.contains(profile.id.lowercased()) else { return }
        let key = profile.id.lowercased(), name = profile.displayName, client = client
        withdrawing.insert(key)
        studio.notice = "Stopping sharing \(name)…"
        Task {
            let reply = await client.withdraw(request: record.request, token: record.token)
            withdrawing.remove(key)
            let failure: String?
            switch reply {
            case .withdrawn:
                failure = forgetting(profile.id, said: "\(name) is no longer shared.", studio: studio)
            case .removal:
                failure = forgetting(profile.id, said: "\(name) comes off Player Setups with the next catalogue update.", studio: studio)
            case .forbidden:
                failure = "cod2unbound.com doesn’t recognise this request any more, so nothing was taken back. To have the setup removed, use Ask to remove or correct this setup on its page in Player Setups."
            case .unreachable, .cancelled:
                failure = "Couldn’t reach cod2unbound.com, so the setup is still shared. Check your connection, then choose Stop Sharing again."
            }
            if let failure {
                studio.notice = nil
                studio.failure = ConfigStudioFailure(title: "Couldn’t stop sharing \(name)", message: failure, retry: nil)
                studio.announce("Couldn’t stop sharing \(name). \(failure)")
            }
        }
    }
    private func forgetting(_ loadout: String, said: String, studio: ConfigStudioModel) -> String? {
        do { try forget(loadout) } catch {
            return "The request was taken back, but CoD2 Unbound couldn’t update its record. Check that your CoD2 Unbound folder is writable."
        }
        studio.notice = said
        studio.announce(said)
        return nil
    }
}

/// How the Share sheet opens: which loadout, and, for the review harness only, a state to show.
struct ConfigShareStart {
    var loadout: String
    var preset: ConfigShareSheetModel.Preset? = nil
}

/// One Share sheet: the loadout's cleaned text, the name, and sending.
@MainActor final class ConfigShareSheetModel: ObservableObject {
    enum Phase: Equatable {
        case preparing, ready, sending
        /// Not sent: no answer, a refusal the player can't act on, or an unexpected reply. Try Again.
        case offline
        case limit(everyone: Bool), already, tooLarge
        /// Nothing left to share once cleaned, or the text couldn't be read.
        case nothingToShare, unreadable
    }
    struct Preset {
        var phase: Phase
        var name: String
        var prepared: ConfigShare.Prepared
        var preview = false
    }
    let loadout: ConfigProfile
    let dialect: ConfigDialect
    @Published var name: String
    @Published private(set) var phase: Phase = .preparing
    @Published private(set) var prepared: ConfigShare.Prepared?
    @Published var previewShown = false
    private var task: Task<Void, Never>?
    private let center: ConfigShareCenter
    private let app: String
    /// Called with the record once the service has it.
    var sent: (ConfigShareRecord) -> Void = { _ in }

    init(loadout: ConfigProfile, dialect: ConfigDialect, center: ConfigShareCenter, app: String) {
        self.loadout = loadout; self.dialect = dialect; self.center = center; self.app = app
        name = center.rememberedName
    }

    func start(read: ConfigProfileRead?, catalog: ConfigCatalog?, preset: Preset?) {
        if let preset {
            name = preset.name; prepared = preset.prepared; phase = preset.phase; previewShown = preset.preview
            return
        }
        guard let read, let catalog else { phase = .unreadable; return }
        let text = ConfigShare.loadoutText(read)
        task = Task {
            do {
                let result = try await ConfigShare.prepare(text, catalog: ConfigImportCatalogAdapter(catalog: catalog))
                prepared = result
                switch result.problem {
                case .nothingToShare: phase = .nothingToShare
                case .tooLarge: phase = .tooLarge
                case nil: phase = .ready
                }
            } catch { phase = .unreadable }
        }
    }

    /// What is wrong with the typed name, said under the field.
    var nameProblem: String? { ConfigShareName.problem(name) }
    var busy: Bool { phase == .sending }
    /// Share Setup (or Try Again) is on: a good name, a cleaned text, and not refused already.
    var canSend: Bool {
        guard ConfigShareName.valid(name) != nil, prepared != nil else { return false }
        return phase == .ready || phase == .offline
    }
    var actionTitle: String { phase == .sending ? "Sending…" : phase == .offline ? "Try Again" : "Share Setup" }

    /// The refusal said in the sheet, in the board's words.
    var refusal: String? {
        switch phase {
        case .offline: "Couldn’t reach cod2unbound.com, so nothing was sent. Check your connection."
        case .limit(false): "Three setups were shared from this network today, which is the most in a day. Try again tomorrow."
        case .limit(true): "Player Setups has taken as many new setups as can be looked at today. Try again tomorrow."
        case .already: "This exact setup is already in Player Setups, or waiting to be looked at."
        case .tooLarge: "\(loadout.displayName) is larger than Player Setups takes: 64 KB or 3,000 lines."
        case .nothingToShare: "\(loadout.displayName) still has the game’s own settings. Change something first, then share it."
        case .unreadable: "\(loadout.displayName) couldn’t be read for sharing. Close this, open the loadout in Configs, then try again."
        case .preparing, .ready, .sending: nil
        }
    }

    func send() {
        guard canSend, let prepared, let name = ConfigShareName.valid(name) else { return }
        phase = .sending
        let body = ConfigShare.requestBody(prepared, name: name, dialect: dialect, app: app)
        let client = center.client, digest = prepared.digest
        task = Task {
            let reply = await client.share(body)
            guard !Task.isCancelled else { return }
            switch reply {
            case .sent(let receipt):
                let record = ConfigShareRecord(request: receipt.request, url: receipt.url, token: receipt.token,
                                               sent: ConfigShareRecordFile.day(Date()), sha256: digest, name: name)
                // The request exists either way; a record that can't be written only loses Stop Sharing.
                try? center.remember(record, loadout: loadout.id)
                phase = .ready
                sent(record)
            case .already: phase = .already
            case .limit(let everyone): phase = .limit(everyone: everyone)
            case .tooLarge: phase = .tooLarge
            case .unreachable: phase = .offline
            case .cancelled: phase = .ready
            }
        }
    }
    /// Cancel: stops a request on its way; nothing is remembered.
    func cancel() { task?.cancel(); task = nil }
}

// MARK: - LauncherModel entry points

@MainActor extension LauncherModel {
    var shareCenter: ConfigShareCenter { ConfigShareCenter.center(for: self) }

    /// The loadout's More menu and Player Setups: Share is on when the game isn't running, the
    /// settings catalogue is loaded, the loadout isn't shared already and has something to share.
    func canShareLoadout(_ profile: ConfigProfile) -> Bool {
        canNavigate && studio.session != nil && studio.catalog != nil && shareCenter.refusal(profile, studio: studio) == nil
    }
    /// Opens the Share sheet for `id` over Configs.
    func shareLoadout(_ id: String) {
        guard let profile = studio.profiles.first(where: { $0.id.caseInsensitiveCompare(id) == .orderedSame }), canShareLoadout(profile) else { return }
        showMainWindow()
        present(.shareSetup(ConfigShareStart(loadout: profile.id)))
    }
    /// View Request: the public request in the browser.
    func viewShareRequest(_ id: String) {
        guard let record = shareCenter.record(id) else { return }
        NSWorkspace.shared.open(record.url)
    }
    /// Stop Sharing…: its alert, then the withdraw.
    func requestStopSharing(_ id: String) {
        guard let profile = studio.profiles.first(where: { $0.id.caseInsensitiveCompare(id) == .orderedSame }), shareCenter.record(id) != nil else { return }
        confirm(stopSharingConfirmation(profile))
    }
    /// The Stop Sharing alert (Sheets board, stopSharing).
    func stopSharingConfirmation(_ profile: ConfigProfile) -> ConfirmationRequest {
        ConfirmationRequest(title: "Stop sharing “\(profile.displayName)”?",
                            message: "It is taken off Player Setups for everyone. Copies other players already imported stay on their Macs.",
                            actionTitle: "Stop Sharing", destructive: false) { [weak self] in
            guard let self else { return }
            self.shareCenter.withdraw(profile, studio: self.studio)
        }
    }

    // MARK: Review harness
    /// Share Your Setup's review-harness states: each sheet state, the Sent sheet, the Stop Sharing
    /// alert, and Player Setups with its Share Your Setup… button. Sample data, as on the boards.
    func previewShare(_ screen: String) -> Bool {
        let presets: [String: (ConfigShareSheetModel.Phase, String)] = [
            "configs-share-empty": (.ready, ""), "configs-share-sheet": (.ready, "morrow"), "configs-share-sending": (.sending, "morrow"),
            "configs-share-preview": (.ready, "morrow"), "configs-share-offline": (.offline, "morrow"),
            "configs-share-limit": (.limit(everyone: false), "morrow"), "configs-share-already": (.already, "morrow"),
            "configs-share-nothing": (.nothingToShare, "morrow"),
        ]
        if let (phase, name) = presets[screen] {
            page = .configs; studio.level = .guided; studio.group = .frame
            let prepared = phase == .nothingToShare ? ConfigSharePreview.nothing : ConfigSharePreview.prepared
            sheet = .shareSetup(ConfigShareStart(loadout: "Main", preset: .init(phase: phase, name: name, prepared: prepared, preview: screen == "configs-share-preview")))
            return true
        }
        switch screen {
        case "configs-share-sent":
            page = .configs; studio.level = .guided; studio.group = .frame
            sheet = .shareSent(record: ConfigSharePreview.record, loadout: "Main")
        case "configs-share-stop-confirm":
            page = .configs; studio.level = .guided; studio.group = .frame
            try? shareCenter.remember(ConfigSharePreview.record, loadout: "Main")
            confirmation = stopSharingConfirmation(ConfigProfile(id: "Main", displayName: "Main", isActive: true))
        case "configs-home-share", "discover-player-shared": page = .configs; discover.page = .home
        default: return false
        }
        return true
    }
}

/// Sample data for the review harness, as on the boards (generic values, no real player).
enum ConfigSharePreview {
    static let sentLines = ["seta sensitivity \"2.6\"", "seta cl_mouseAccel \"0\"", "seta m_filter \"0\"", "seta cg_fov \"80\"", "seta com_maxfps \"250\"",
                            "seta cl_maxpackets \"100\"", "seta rate \"25000\"", "seta snaps \"30\"", "seta r_gamma \"1.4\"", "seta r_picmip \"2\"",
                            "seta cg_drawCrosshair \"1\"", "seta cg_crosshairAlpha \"0.8\"", "seta cg_hudChatPosition \"5 204\"", "bind W \"+forward\"",
                            "bind S \"+back\"", "bind SPACE \"+gostand\"", "bind MOUSE1 \"+attack\"", "bind MOUSE2 \"+speed_throw\"",
                            "bind SHIFT \"+holdbreath\"", "bind CTRL \"+prone\""]
    static let leftOut: [ConfigShare.LeftOutLine] = [
        .init(id: 0, shown: "seta name \"^3Sgt. ^7Morrow\"", reasons: ["identity"]),
        .init(id: 1, shown: "seta rconpassword …", reasons: ["credential"]),
        .init(id: 2, shown: "bind F1 \"say good game\"", reasons: ["chat"]),
        .init(id: 3, shown: "bind F5 \"connect 203.0.113.24:28960\"", reasons: ["connect"]),
        .init(id: 4, shown: "seta sv_hostname \"Scrim night\"", reasons: ["hosting"]),
        .init(id: 5, shown: "bind MOUSE4 \"+attack;wait;-attack\"", reasons: ["macro"]),
        .init(id: 6, shown: "exec practice.cfg", reasons: ["looseCommand", "unresolvedExec"]),
    ]
    static var prepared: ConfigShare.Prepared {
        ConfigShare.Prepared(bytes: Data((sentLines.joined(separator: "\n") + "\n").utf8), settings: 13, binds: 7, leftOut: leftOut,
                             reasons: Set(leftOut.flatMap(\.reasons)).sorted(), lines: sentLines.count)
    }
    static var nothing: ConfigShare.Prepared {
        let lines = Array(sentLines.prefix(4))
        return ConfigShare.Prepared(bytes: Data((lines.joined(separator: "\n") + "\n").utf8), settings: 4, binds: 0, leftOut: [Array(leftOut)[0]],
                                    reasons: ["identity"], lines: lines.count)
    }
    static let record = ConfigShareRecord(request: 128, url: URL(string: ConfigShareClient.pulls + "128")!, token: String(repeating: "0", count: 64),
                                          sent: "2026-11-21", sha256: String(repeating: "0", count: 64), name: "morrow")
}
