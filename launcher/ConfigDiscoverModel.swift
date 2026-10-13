import AppKit
import Foundation
import UniformTypeIdentifiers

// Discover and import: the player directory, Saved, a player's detail and the import
// review. Players come only from a verified signed index (the production directory is
// honestly unavailable today); every import goes through the I1/I2 review and one
// guarded transaction. Nothing here invents players, permissions, values or signatures.

enum ConfigDiscoverPage: Hashable { case players, saved, player(String) }

enum ConfigDirectoryState {
    case idle, loading
    /// No signed publication or trust configuration yet: the production state.
    case unavailable(String)
    case loaded(ConfigDirectorySnapshot, refreshError: String?)
    case failed(String)
}

/// One import sheet steps from choosing a source to its review without closing in between.
enum ConfigImportStep: Equatable {
    /// A link, with optional starting text (“http://localhost:” for a server on this Mac).
    case link(String)
    case windows, review
}

struct ConfigDirectoryShelf: Identifiable {
    let id: String
    let entries: [ConfigDirectoryEntry]
}

/// What a source is and who it credits, as the review header shows it.
struct ConfigImportProvenance: Sendable {
    var source: String
    /// A file name or link, shown in Courier.
    var detail: String? = nil
    var signed = false
    var credit: String? = nil
    var permission = false
    var windows = false
    var symbol = "doc.text"
}

// MARK: - Plain presentation of review rows

struct ConfigImportItem: Identifiable, Equatable {
    enum Section: Hashable { case change(String), protected, needsOK, machine, never }
    let id: String
    let section: Section
    let title: String
    let detail: String?
    let labels: [String]
    /// The exact line(s), with credentials already redacted by the safety review.
    let exact: String
    /// The loadout's current line for the same setting, for Show details.
    let previous: String?
    let selectable: Bool
    let selected: Bool
    let risky: Bool
    var isSetting = false
    var isBinding = false
    /// The loadout already has this value, so importing it changes nothing.
    var unchanged = false
    /// What the line would do, in words (risky and blocked lines).
    var risks: [String] = []
}

/// A key's name as people read it: "Mouse 5", "Space", "W".
enum KeyNames {
    private static let names = ["SPACE": "Space", "TAB": "Tab", "ENTER": "Return", "ESCAPE": "Escape", "BACKSPACE": "Backspace",
        "CTRL": "Ctrl", "SHIFT": "Shift", "ALT": "Alt", "UPARROW": "Up Arrow", "DOWNARROW": "Down Arrow", "LEFTARROW": "Left Arrow",
        "RIGHTARROW": "Right Arrow", "MWHEELUP": "Wheel up", "MWHEELDOWN": "Wheel down", "INS": "Insert", "DEL": "Delete",
        "PGUP": "Page Up", "PGDN": "Page Down", "HOME": "Home", "END": "End", "PAUSE": "Pause", "SEMICOLON": "Semicolon", "COMMAND": "Command"]
    static func plain(_ spelling: String) -> String {
        let upper = spelling.uppercased()
        if upper.count == 1 { return upper }
        if upper.hasPrefix("MOUSE"), let number = Int(upper.dropFirst(5)) { return "Mouse \(number)" }
        if upper.hasPrefix("KP_") { return "Keypad " + upper.dropFirst(3).lowercased() }
        if upper.hasPrefix("F"), let number = Int(upper.dropFirst()) { return "F\(number)" }
        return names[upper] ?? upper.prefix(1) + upper.dropFirst().lowercased()
    }
}

enum ConfigImportPresentation {
    /// Outcomes in the second person; the safety review's own summaries are the fallback.
    static func plain(_ risk: ConfigImportRisk) -> String {
        switch risk {
        case .connect: "Joins a server you didn’t choose"
        case .quit: "Closes the game"
        case .remoteConsole: "Sends an administrator command to a server"
        case .identity: "Changes your name in game"
        case .chat: "Sends chat to other players"
        case .fileWrite: "Writes files on your Mac"
        case .unbindAll: "Clears all your key bindings first"
        case .network: "Changes what the game downloads or where it connects"
        case .sideEffect: "Changes the game session or restarts part of the game"
        case .unresolvedExec: "Runs another file that wasn’t included"
        case .unresolvedVariable: "Runs commands that can’t be known in advance"
        case .cycle: "Repeats itself, which can freeze the game"
        case .limit: "Too long for the game, or for this review, to read safely"
        case .malformed: "The game can’t read this line reliably"
        case .negativeWait: "Can stall the game"
        case .script: "Stores or runs a chain of commands"
        case .keyRelease: "Also runs commands when you release the key"
        case .credential: "Contains a password, key or login"
        case .filesystemPath: "Points at a file or folder on a computer"
        case .protectedSetting: "The game protects this setting, so a config can’t change it"
        case .inertName: "Has no effect in CoD2"
        case .unknownSetting: "Creates a helper setting the game doesn’t have"
        case .unknownForwardedCommand: "An unknown command the game may send to the server"
        }
    }

    static func groupTitle(_ group: ConfigPurposeGroup, catalog: ConfigCatalog?) -> String {
        catalog?.groups.first { $0.id == group }?.title ?? group.rawValue.capitalized
    }
    /// The order change groups appear in: the eight purpose groups, then bindings and the rest.
    static func groupOrder(catalog: ConfigCatalog?) -> [String] {
        ConfigPurposeGroup.allCases.map { groupTitle($0, catalog: catalog) } + ["Key bindings", "Other commands"]
    }

    /// A binding's action in words: the game's menu name for one action, otherwise what it does.
    static func action(_ text: String?, catalog: ConfigCatalog?) -> String? {
        guard let text else { return nil }
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        if trimmed.isEmpty { return "Nothing" }
        if !trimmed.contains(";"), !trimmed.contains(" "), let command = catalog?.command(named: trimmed) {
            return command.menuLabel ?? command.title
        }
        return "A chain of commands"
    }

    @MainActor static func items(_ session: ConfigImportSession, studio: ConfigStudioModel) -> [ConfigImportItem] {
        let catalog = studio.catalog
        let adapter = catalog.map { ConfigImportCatalogAdapter(catalog: $0) }
        let files = Dictionary(session.review.bundle.files.map { ($0.path, $0) }, uniquingKeysWith: { first, _ in first })
        let selected = session.selectedIDs
        return session.rows.compactMap { row -> ConfigImportItem? in
            let command = row.candidate.command
            guard ![.empty, .comment].contains(command.kind) else { return nil }
            let reasons = row.candidate.reasons
            let secret = reasons.contains(.credential)
            var title = reasons.first.map(plain) ?? "A config command"
            var detail: String?
            var group = "Other commands"
            var previous: String?
            var oldShown: String?, newShown: String?
            var isSetting = false, isBinding = false
            if let file = files[row.candidate.location.file], let adapter,
               let assignment = ConfigImportScript.assignment(command, document: file.document, catalog: adapter) {
                isSetting = true
                let name = assignment.name
                if let spec = studio.spec(named: name) {
                    title = spec.title; group = groupTitle(spec.group, catalog: catalog)
                    oldShown = row.oldValue.map { ConfigValues.display(spec, $0) }
                    newShown = row.newValue.map { ConfigValues.display(spec, $0) }
                } else if let setting = catalog?.setting(named: name) {
                    title = setting.title; group = groupTitle(setting.group, catalog: catalog)
                    oldShown = row.oldValue; newShown = row.newValue
                } else if reasons.contains(.inertName) {
                    title = "A setting from another game"
                    newShown = row.newValue
                } else {
                    title = "A helper setting the config creates"
                    newShown = row.newValue
                }
                if secret { detail = "Hidden, because it holds a password or key." }
                else if let oldShown, let newShown { detail = oldShown == newShown ? "Already \(newShown)" : "\(oldShown) → \(newShown)" }
                else if let newShown { detail = "Set to \(newShown)" }
                if !secret, let old = row.oldValue { previous = "\(name) \"\(old)\"" }
            } else if command.kind == .bind, command.tokens.count > 1 {
                isBinding = true
                let key = KeyNames.plain(command.tokens[1].text)
                group = "Key bindings"
                let new = action(row.newValue, catalog: catalog)
                title = secret ? "\(key) key" : "\(key): \(new ?? "A chain of commands")"
                if secret { detail = "Hidden, because it holds a password or key." }
                else if session.target.isNew { detail = nil }
                else { detail = action(row.oldValue, catalog: catalog).map { "Was \($0)." } ?? "Wasn’t bound before." }
                if !secret, let old = row.oldValue { previous = "bind \(command.tokens[1].text) \"\(old)\"" }
            }
            // Risky and blocked rows say what they would do, beyond the setting's own value.
            let risks = reasons.map(plain).filter { $0 != title }
            let section: ConfigImportItem.Section
            if !row.lockedNames.isEmpty {
                section = .protected
                if isSetting, let oldShown {
                    title = "\(title) stays \(oldShown)"
                    detail = newShown.map { "They use \($0). Unlock it in Settings if you want theirs." } ?? "Locked by you."
                } else {
                    title = isBinding ? "\(title.components(separatedBy: ":").first ?? title) stays as it is" : "\(title) stays as it is"
                    detail = "Locked by you, so this import leaves it alone."
                }
            } else if let note = row.platformNote {
                section = .machine
                detail = note
            } else if !row.canSelect {
                section = .never
                detail = ([detail].compactMap { $0 } + risks).joined(separator: ". ")
            } else if row.candidate.classification == .needsOK {
                section = .needsOK
                let parts = [detail].compactMap { $0 } + risks
                detail = parts.isEmpty ? nil : parts.joined(separator: ". ")
            } else {
                section = .change(group)
            }
            return ConfigImportItem(id: row.id, section: section, title: title, detail: detail,
                                    labels: reasons.contains(.inertName) ? ["Has no effect in CoD2"] : [],
                                    exact: row.candidate.details, previous: previous, selectable: row.canSelect,
                                    selected: selected.contains(row.id), risky: row.candidate.classification == .needsOK,
                                    isSetting: isSetting, isBinding: isBinding,
                                    unchanged: oldShown != nil && oldShown == newShown, risks: reasons.map(plain))
        }
    }
}

// MARK: - Player detail

struct ConfigPlayerChange: Identifiable, Equatable {
    let id: String
    let title: String
    let detail: String
    let locked: Bool
}

struct ConfigPlayerSummary {
    /// The loadout compared against: the one the next game uses.
    let comparedWith: String?
    let changes: [ConfigPlayerChange]
    let more: Int
    let settings: Int
    let keys: Int
    let needsOK: Int
    let never: Int
}

struct ConfigPlayerDetail {
    enum Phase { case loading, failed(String), ready(ConfigPlayerSummary) }
    let entryID: String
    var phase: Phase
    /// The verified download, reused when the person tries the setup.
    var bundle: ConfigImportBundle?
    /// The reviewed values Compare shows in Expert.
    var values: [String: String]?
}

extension ConfigImportPresentation {
    /// “What changes for you”, from a review that targets the active loadout. Nothing is written.
    @MainActor static func summary(_ session: ConfigImportSession, studio: ConfigStudioModel, comparedWith: ConfigProfileRead?) -> ConfigPlayerSummary {
        let items = items(session, studio: studio)
        let catalog = studio.catalog
        let adapter = catalog.map { ConfigImportCatalogAdapter(catalog: $0) }
        let files = Dictionary(session.review.bundle.files.map { ($0.path, $0) }, uniquingKeysWith: { first, _ in first })
        // What trying it changes: the lines Review ticks by default, and the locks that keep yours.
        let counted = session.rows.filter { ($0.canSelect && $0.candidate.selectedByDefault) || !$0.lockedNames.isEmpty }
        var values: [String: String] = [:]
        for row in counted {
            guard let file = files[row.candidate.location.file], let adapter, let value = row.newValue,
                  let assignment = ConfigImportScript.assignment(row.candidate.command, document: file.document, catalog: adapter) else { continue }
            values[assignment.name] = value
        }
        var guided: [ConfigPlayerChange] = [], others: [ConfigPlayerChange] = [], locked: [ConfigPlayerChange] = []
        for row in counted {
            guard let file = files[row.candidate.location.file], let adapter, !row.candidate.reasons.contains(.credential),
                  let assignment = ConfigImportScript.assignment(row.candidate.command, document: file.document, catalog: adapter),
                  let spec = studio.spec(named: assignment.name), let theirs = row.newValue, values[assignment.name] == theirs else { continue }
            values[assignment.name] = nil  // the last assignment of a name wins; list it once
            let mine = comparedWith.map { studio.state(spec, in: $0.document, metadata: $0.metadata) }
            if let value = mine?.value, ConfigValues.same(spec, value, theirs) { continue }
            let change: ConfigPlayerChange
            if !row.lockedNames.isEmpty, let mine {
                change = .init(id: row.id, title: "Your \(spec.title.lowercased()) stays \(mine.display)",
                               detail: "They use \(ConfigValues.display(spec, theirs)), but you locked yours, so it won’t change.", locked: true)
            } else if spec.name.lowercased() == "sensitivity", let mine, let turn = turnChange(mine: mine, theirs: theirs, spec: spec, session: session, files: files, adapter: adapter, comparedWith: comparedWith, studio: studio) {
                change = .init(id: row.id, title: turn.title, detail: turn.detail, locked: false)
            } else {
                change = .init(id: row.id, title: "\(spec.title): \(ConfigValues.display(spec, theirs))",
                               detail: mine.map { "Yours is \($0.display)." } ?? "Set to their value.", locked: false)
            }
            if change.locked { locked.append(change) } else if spec.setting.guided { guided.append(change) } else { others.append(change) }
        }
        // Locks are always acknowledged; other changes fill the rest of six rows.
        let unlocked = guided + others, room = max(0, 6 - locked.count)
        let more = max(0, unlocked.count - room)
        var changes = Array(unlocked.prefix(room)) + locked
        let keys = items.filter { $0.isBinding && $0.selectable && $0.section != .needsOK }.count
        changes.append(keys == 0
            ? .init(id: "binds", title: "Your binds stay yours", detail: "This setup doesn’t include any keys.", locked: true)
            : .init(id: "binds", title: keys == 1 ? "1 key binding changes" : "\(keys) key bindings change",
                    detail: "You choose which ones to keep when you review the setup.", locked: false))
        return ConfigPlayerSummary(comparedWith: comparedWith?.profile.displayName, changes: changes, more: more,
                                   settings: items.filter { $0.isSetting && $0.selectable }.count, keys: keys,
                                   needsOK: items.filter { $0.section == .needsOK }.count,
                                   never: items.filter { $0.section == .never || $0.section == .machine }.count)
    }

    /// The values Compare shows: the lines Review ticks by default, including ones a lock keeps,
    /// by lowercased setting name (the last assignment wins). Credentials never appear.
    @MainActor static func comparisonValues(_ session: ConfigImportSession, studio: ConfigStudioModel) -> [String: String] {
        guard let catalog = studio.catalog else { return [:] }
        let adapter = ConfigImportCatalogAdapter(catalog: catalog)
        let files = Dictionary(session.review.bundle.files.map { ($0.path, $0) }, uniquingKeysWith: { first, _ in first })
        var values: [String: String] = [:]
        for row in session.rows where row.candidate.selectedByDefault && (row.canSelect || !row.lockedNames.isEmpty) && !row.candidate.reasons.contains(.credential) {
            guard let file = files[row.candidate.location.file], let value = row.newValue,
                  let assignment = ConfigImportScript.assignment(row.candidate.command, document: file.document, catalog: adapter) else { continue }
            values[assignment.name.lowercased()] = value
        }
        return values
    }

    /// Turning speed scales with mouse speed while the turn rate per count stays the same. A
    /// distance per full turn needs the person's own DPI; without it, only the ratio is claimed.
    @MainActor private static func turnChange(mine: ConfigSettingState, theirs: String, spec: ConfigSettingSpec, session: ConfigImportSession,
                                              files: [String: ConfigImportFile], adapter: ConfigImportCatalogAdapter,
                                              comparedWith: ConfigProfileRead?, studio: ConfigStudioModel) -> (title: String, detail: String)? {
        guard let old = mine.number, let new = ConfigValues.number(theirs), old > 0, new > 0 else { return nil }
        let yaw = comparedWith?.document.effectiveSetting(named: "m_yaw").flatMap(ConfigValues.number) ?? 0.022
        let theirYaw = session.rows.compactMap { row -> Double? in
            guard row.canSelect, let file = files[row.candidate.location.file],
                  let assignment = ConfigImportScript.assignment(row.candidate.command, document: file.document, catalog: adapter),
                  assignment.name == "m_yaw" else { return nil }
            return row.newValue.flatMap(ConfigValues.number)
        }.last ?? yaw
        guard abs(theirYaw - yaw) < 1e-9 else { return nil }
        let ratio = new / old
        guard abs(ratio - 1) >= 0.005 else { return nil }
        let percent = Int((abs(1 - ratio) * 100).rounded())
        let title = "You’ll turn about \(percent)% \(ratio < 1 ? "slower" : "faster")"
        if let dpi = comparedWith?.metadata.dpi,
           let a = MouseMath.centimetresPer360(dpi: dpi, sensitivity: new, yaw: yaw), let b = MouseMath.centimetresPer360(dpi: dpi, sensitivity: old, yaw: yaw) {
            return (title, "A full turn takes about \(MouseMath.label(a)) cm of mouse movement instead of \(MouseMath.label(b)) cm.")
        }
        return (title, "Mouse speed \(ConfigValues.display(spec, theirs)) instead of \(mine.display). Add your mouse DPI in Guided to see the distance for a full turn.")
    }
}

// MARK: - Import review store

/// One review in the sheet. It owns the I2 session value; Apply is its only write.
@MainActor final class ConfigImportReviewModel: ObservableObject, Identifiable {
    enum Phase { case loading, failed(String), ready }
    let id = UUID()
    let provenance: ConfigImportProvenance
    let displayName: String
    @Published private(set) var phase: Phase = .loading
    @Published private(set) var session: ConfigImportSession? { didSet { refreshItems() } }
    @Published private(set) var items: [ConfigImportItem] = []
    @Published var showDetails = false
    @Published var applyError: String?
    @Published private(set) var receipt: ConfigImportReceipt?
    private let studio: ConfigStudioModel
    private var task: Task<Void, Never>?
    private var newLoadoutID = ""

    init(studio: ConfigStudioModel, displayName: String, provenance: ConfigImportProvenance) {
        self.studio = studio; self.displayName = displayName; self.provenance = provenance
    }

    /// Reads and reviews the source off the main thread; a new loadout is the default destination.
    func start(adapter: ConfigImportCatalogAdapter, platform: ConfigImportPlatform, origin: ConfigOrigin?,
               load: @escaping @Sendable () async throws -> ConfigImportBundle) {
        let dialect = studio.dialect, name = displayName
        let newID = ConfigStudioModel.identifier(for: name, existing: studio.profiles.map(\.id))
        newLoadoutID = newID
        task = Task { [weak self] in
            do {
                let bundle = try await load()
                let built = try await Task.detached(priority: .userInitiated) { () throws -> ConfigImportSession in
                    var session = try ConfigImportSession(bundle: bundle, dialect: dialect, catalog: adapter, displayName: name,
                                                          platform: platform, origin: origin)
                    session.setTarget(.newLoadout(id: newID, displayName: name))
                    return session
                }.value
                guard let self, !Task.isCancelled else { return }
                self.session = built
                self.phase = .ready
            } catch is CancellationError {
            } catch {
                self?.fail(Self.message(for: error))
            }
        }
    }
    func fail(_ message: String) { phase = .failed(message) }
    func cancel() { task?.cancel(); task = nil }
    /// Review only: the harness and tests install a reviewed session directly.
    func install(_ session: ConfigImportSession) {
        if case .newLoadout(let id, _) = session.target { newLoadoutID = id }
        self.session = session; phase = .ready
    }

    private func refreshItems() { items = session.map { ConfigImportPresentation.items($0, studio: studio) } ?? [] }

    var mode: ConfigImportMode { session?.mode ?? .review }
    var targetID: String? { if case .existing(let read)? = session?.target { return read.profile.id }; return nil }
    var targetName: String {
        if case .existing(let read)? = session?.target { return read.metadata.displayName }
        return displayName
    }
    func setMode(_ mode: ConfigImportMode) {
        guard var session, session.mode != mode else { return }
        session.setMode(mode); self.session = session; applyError = nil
    }
    /// Choosing a destination never changes which loadout the next game uses.
    func setTarget(_ id: String?) {
        guard var session, id != targetID else { return }
        if let id {
            guard let live = studio.session else { return }
            do { session.setTarget(.existing(try live.readImportTarget(id: id))) }
            catch { applyError = "Couldn’t read that loadout. \(Self.message(for: error))"; return }
        } else {
            session.setTarget(.newLoadout(id: newLoadoutID, displayName: displayName))
        }
        self.session = session; applyError = nil
    }
    func toggle(_ id: String) {
        guard var session, let item = items.first(where: { $0.id == id }), item.selectable else { return }
        do {
            if session.mode == .everything {
                // Unticking in Import everything returns to Review with everything else still ticked.
                let keep = session.selectedIDs.subtracting([id])
                try session.replaceSelection(keep)
            } else {
                try session.select(id, selected: !session.selectedIDs.contains(id))
            }
            self.session = session; applyError = nil
        } catch { applyError = Self.message(for: error) }
    }

    var selectedCount: Int { session?.selectedIDs.count ?? 0 }
    var eligibleCount: Int { items.filter(\.selectable).count }
    var riskySelected: [ConfigImportItem] { items.filter { $0.selected && $0.risky } }
    var riskyCount: Int { items.filter { $0.risky && $0.selectable }.count }
    /// The button names the selected changes.
    var applyTitle: String {
        let count = selectedCount, risky = riskySelected.count
        if count == 0 { return "Apply Nothing" }
        if mode == .everything { return "Import All \(count)" + (risky > 0 ? ", Including \(risky) Risky" : "") }
        return "Apply \(count) Change\(count == 1 ? "" : "s")" + (risky > 0 ? ", Including \(risky) Risky" : "")
    }
    /// Concrete consequences of the risky lines that would be imported, once each.
    var consequences: [String] {
        var seen = Set<String>(), result: [String] = []
        for item in riskySelected {
            let inert = ConfigImportPresentation.plain(.inertName)
            let risks = item.risks.filter { $0 != inert }
            for text in risks.isEmpty ? ["\(item.title) has no effect in CoD2"] : risks where seen.insert(text).inserted { result.append(text) }
        }
        return result
    }
    var summary: String {
        let lines = items.filter { if case .change = $0.section { return true }; return $0.section == .needsOK }
        let same = lines.filter(\.unchanged).count, changes = lines.count - same
        var parts = ["\(changes) change\(changes == 1 ? "" : "s"), grouped by what they do."]
        if same > 0 { parts.append("\(same) line\(same == 1 ? " matches" : "s match") what you have already.") }
        let protected = items.filter { $0.section == .protected }.count
        if protected > 0 { parts.append(protected == 1 ? "1 setting is locked, so it stays yours." : "\(protected) settings are locked, so they stay yours.") }
        let never = items.filter { $0.section == .never }.count
        if never > 0 { parts.append("\(never) line\(never == 1 ? " is" : "s are") never imported.") }
        return parts.joined(separator: " ")
    }

    /// One guarded transaction. A failure keeps the review open and the loadout's bytes as they were.
    @discardableResult func apply(undo: UndoManager?) -> ConfigImportReceipt? {
        guard var session, receipt == nil else { return nil }
        guard let live = studio.session else { applyError = "Configs aren’t available, so nothing can be imported."; return nil }
        guard !studio.isReadOnly else {
            applyError = "Configs are read-only while Call of Duty 2 is running. Quit the game, then import."; return nil
        }
        // A loadout made while this review was open may own the proposed folder name now.
        if case .newLoadout(let id, let name) = session.target,
           studio.profiles.contains(where: { $0.id.caseInsensitiveCompare(id) == .orderedSame }) {
            let keep = session.selectedIDs
            newLoadoutID = ConfigStudioModel.identifier(for: name, existing: studio.profiles.map(\.id))
            session.setTarget(.newLoadout(id: newLoadoutID, displayName: name))
            for id in session.selectedIDs where !keep.contains(id) { try? session.select(id, selected: false) }
            for id in keep { try? session.select(id, selected: true) }
        }
        do {
            let result = try session.apply(to: live)
            self.session = session
            receipt = result; applyError = nil
            if case .existing = session.target, let undo {
                let entry = result.historyEntry, id = result.profileID
                undo.registerUndo(withTarget: studio) { studio in ConfigImportReviewModel.undoImport(studio: studio, profileID: id, entry: entry) }
                undo.setActionName("Import")
            }
            return result
        } catch {
            applyError = Self.message(for: error)
            return nil
        }
    }

    /// Undo restores the state saved just before the import, as a new History transaction.
    static func undoImport(studio: ConfigStudioModel, profileID: String, entry: ConfigHistoryEntry) {
        guard let session = studio.session else { return }
        do {
            try session.focus(profileID)
            guard session.current?.version == studio.restoreTarget(entry, revision: .after) else {
                studio.failure = ConfigStudioFailure(title: "Nothing to undo", message: "This loadout changed after the import. Pick a snapshot in History instead.", retry: nil)
                return
            }
            try session.restore(snapshotID: entry.id, revision: .before)
            studio.announce("Undid the import.")
        } catch {
            studio.failure = ConfigStudioFailure(title: "Couldn’t undo the import", message: error.localizedDescription, retry: nil)
        }
    }

    static func message(for error: Error) -> String {
        switch error {
        case ConfigProfileError.gameRunning: "Configs are read-only while Call of Duty 2 is running. Quit the game, then import."
        case ConfigProfileError.conflict: "This loadout changed while you were reviewing. Close the review and start the import again."
        case ConfigImportSessionError.emptySelection: "Tick at least one change to import."
        case ConfigImportSessionError.unsafeComposition:
            "These lines can’t be combined safely with this loadout. Leave out the ones that run commands, or import into a new loadout."
        case ConfigImportSessionError.alreadyApplied: "This import is already done."
        case ConfigImportError.nonText: "That isn’t config text. Choose a .cfg file or plain text, not a web page or a binary file."
        case ConfigImportError.tooLarge: "This config is longer than the 65,535 characters the game reads."
        case ConfigImportError.invalidURL: "Use an https:// link, a GitHub file or gist, or http://localhost for a server on this Mac."
        case ConfigImportError.httpStatus(let status): "The server answered with error \(status). Check the link and try again."
        case ConfigDirectoryError.profileMismatch, ConfigDirectoryError.identityMismatch:
            "The downloaded setup doesn’t match its signed catalog entry, so it wasn’t opened."
        case let error as URLError where error.code == .notConnectedToInternet: "You’re offline. Connect to the internet and try again."
        default: error.localizedDescription
        }
    }
}

// MARK: - Discover store

@MainActor final class ConfigDiscoverModel: ObservableObject {
    let studio: ConfigStudioModel
    private(set) var directory: ConfigDirectory
    @Published var page: ConfigDiscoverPage?
    @Published private(set) var state: ConfigDirectoryState = .idle
    @Published var query = ""
    @Published private(set) var saved: [ConfigDirectorySavedEntry] = []
    @Published private(set) var detail: ConfigPlayerDetail?
    @Published private(set) var review: ConfigImportReviewModel?
    @Published var step: ConfigImportStep?
    /// Messages outside a sheet (an empty clipboard, Saved failing) go to the launcher's notice.
    var notify: @MainActor (String) -> Void = { _ in }
    /// How the Windows step finds profiles, given the folders the person chose: the bounded search
    /// of mounted drives. The review harness supplies known candidates instead and never scans.
    var findWindowsProfiles: @Sendable ([URL]) async -> (WindowsProfileDiscovery, searched: [String]) = { folders in
        await Task.detached(priority: .userInitiated) { ConfigDiscoverModel.searchWindowsProfiles(folders) }.value
    }
    private var detailTask: Task<Void, Never>?

    init(studio: ConfigStudioModel, directory: ConfigDirectory) {
        self.studio = studio; self.directory = directory
    }

    /// The review harness swaps in a signed fixture directory (preview mode only).
    func use(_ directory: ConfigDirectory) {
        detailTask?.cancel()
        self.directory = directory; state = .idle; saved = []; detail = nil
    }

    var snapshot: ConfigDirectorySnapshot? { if case .loaded(let snapshot, _) = state { return snapshot }; return nil }
    var entries: [ConfigDirectoryEntry] { snapshot?.index.entries ?? [] }
    func entry(_ id: String) -> ConfigDirectoryEntry? { entries.first { $0.id == id } }
    func isSaved(_ id: String) -> Bool { saved.contains { $0.id == id } }
    var matches: [ConfigDirectoryEntry] {
        let query = query.trimmingCharacters(in: .whitespaces)
        guard !query.isEmpty else { return entries }
        return entries.filter { entry in
            ([entry.player, entry.team ?? "", entry.role, entry.era, entry.feel] + entry.styleTags).contains { $0.localizedCaseInsensitiveContains(query) }
        }
    }
    /// Shelves follow the signed entries' own eras, in the index's order. No invented shelves.
    var shelves: [ConfigDirectoryShelf] {
        var order: [String] = [], groups: [String: [ConfigDirectoryEntry]] = [:]
        for entry in matches {
            if groups[entry.era] == nil { order.append(entry.era) }
            groups[entry.era, default: []].append(entry)
        }
        return order.map { ConfigDirectoryShelf(id: $0, entries: groups[$0] ?? []) }
    }

    func open(_ page: ConfigDiscoverPage) {
        self.page = page
        if case .player(let id) = page { loadDetail(id) }
        Task { await loadIfNeeded() }
    }
    func loadIfNeeded() async {
        if case .idle = state { await reload() }
    }
    func reload() async {
        state = .loading
        let directory = self.directory
        do {
            let result = try await directory.load()
            if let snapshot = result.snapshot {
                state = .loaded(snapshot, refreshError: result.refreshError)
                saved = (try? directory.savedEntries(snapshot: snapshot)) ?? []
            } else {
                state = .unavailable(result.unavailableReason ?? "The player directory isn’t available.")
                saved = []
            }
        } catch {
            state = .failed(ConfigImportReviewModel.message(for: error)); saved = []
        }
    }

    func setSaved(_ id: String, _ value: Bool) {
        guard let snapshot else { return }
        do {
            try directory.setSaved(entryID: id, saved: value, snapshot: snapshot)
            saved = try directory.savedEntries(snapshot: snapshot)
            studio.announce(value ? "Saved for later." : "Removed from Saved.")
        } catch { notify("Couldn’t update Saved. \(error.localizedDescription)") }
    }

    func loadDetail(_ id: String) {
        detailTask?.cancel()
        detail = ConfigPlayerDetail(entryID: id, phase: .loading)
        guard let snapshot, let entry = entry(id) else {
            detail?.phase = .failed(snapshot == nil ? "The player directory isn’t available." : "This player is no longer in the directory.")
            return
        }
        guard let catalog = catalog() else { detail?.phase = .failed("The settings catalog is missing or damaged. Reinstall CoD2 Silicon to restore it."); return }
        let directory = self.directory, dialect = studio.dialect, adapter = ConfigImportCatalogAdapter(catalog: catalog)
        let active = try? studio.session?.readActive()
        let platform: ConfigImportPlatform = entry.platform == .windows ? .windows : .native
        detailTask = Task { [weak self] in
            do {
                let bundle = try await directory.profile(entryID: id, snapshot: snapshot, dialect: dialect, catalog: adapter)
                let session = try await Task.detached(priority: .userInitiated) { () throws -> ConfigImportSession in
                    var session = try ConfigImportSession(bundle: bundle, dialect: dialect, catalog: adapter, displayName: entry.player, platform: platform)
                    if let active { session.setTarget(.existing(active)) }
                    return session
                }.value
                guard let self, !Task.isCancelled else { return }
                self.detail = ConfigPlayerDetail(entryID: id, phase: .ready(ConfigImportPresentation.summary(session, studio: self.studio, comparedWith: active)),
                                                 bundle: bundle, values: ConfigImportPresentation.comparisonValues(session, studio: self.studio))
            } catch is CancellationError {
            } catch {
                guard let self, self.detail?.entryID == id else { return }
                self.detail?.phase = .failed(ConfigImportReviewModel.message(for: error))
            }
        }
    }

    /// Compare needs the player's setup fetched, verified and reviewed on this page.
    func canCompare(_ id: String) -> Bool {
        guard let detail, detail.entryID == id, detail.values != nil, entry(id) != nil, case .ready = detail.phase else { return false }
        return true
    }
    /// Opens Expert with this player's reviewed values as the comparison. Nothing is written;
    /// Take Theirs is still one guarded edit per setting.
    func compareInExpert(_ entry: ConfigDirectoryEntry) {
        guard canCompare(entry.id), let values = detail?.values else { return }
        studio.compare(with: ConfigPlayerComparison(entryID: entry.id, name: entry.player,
                                                    provenance: "Players catalog · signed entry · credit \(entry.credit)", values: values))
        page = nil
        studio.announce("Comparing with \(entry.player) in Expert.")
    }

    private func catalog() -> ConfigCatalog? {
        if studio.catalog == nil { studio.loadCatalogNow() }
        return studio.catalog
    }

    // MARK: Sources

    /// Opens a review sheet at once; the source is read and reviewed while it shows progress.
    func beginImport(displayName: String, provenance: ConfigImportProvenance, platform: ConfigImportPlatform = .native,
                     origin: ConfigOrigin? = nil, load: @escaping @Sendable (ConfigImportCatalogAdapter, ConfigDialect) async throws -> ConfigImportBundle) {
        guard studio.session != nil else { notify("Configs aren’t available, so nothing can be imported."); return }
        review?.cancel()
        let review = ConfigImportReviewModel(studio: studio, displayName: displayName, provenance: provenance)
        self.review = review
        step = .review
        guard let catalog = catalog() else { review.fail("The settings catalog is missing or damaged. Reinstall CoD2 Silicon to restore it."); return }
        let adapter = ConfigImportCatalogAdapter(catalog: catalog), dialect = studio.dialect
        review.start(adapter: adapter, platform: platform, origin: origin) { try await load(adapter, dialect) }
    }

    /// Closes the import sheet. An unconfirmed review writes nothing.
    func closeImport() {
        review?.cancel()
        step = nil
        review = nil
    }
    /// After a confirmed import: show the loadout it made or changed. It doesn't become the active one.
    func finishImport(_ receipt: ConfigImportReceipt, name: String, existing: Bool) {
        closeImport()
        if let session = studio.session { try? session.focus(receipt.profileID) }
        studio.tab = .settings
        page = nil
        studio.announce(existing ? "Imported into \(name). History has the version from before." : "Imported as a new loadout, \(name). Your other loadouts are unchanged.")
    }
    /// The review harness opens a reviewed session directly (preview mode only).
    func present(_ review: ConfigImportReviewModel) { self.review = review; step = .review }

    func chooseFile() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true; panel.canChooseDirectories = false; panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [UTType(filenameExtension: "cfg"), UTType(filenameExtension: "bak"), UTType.plainText].compactMap { $0 }
        panel.message = "Choose a Call of Duty 2 config. Files it runs from the same folder are reviewed with it."
        panel.prompt = "Review"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        importFile(url)
    }
    func importFile(_ url: URL) {
        beginImport(displayName: Self.name(for: url), provenance: .init(source: "File", detail: url.lastPathComponent, symbol: "doc.text"),
                    origin: ConfigOrigin(kind: .imported, source: url.lastPathComponent)) { adapter, dialect in
            try await ConfigImportLoader(catalog: adapter).load(.file(url), dialect: dialect)
        }
    }
    /// The usual install folders on browsable mounted drives, and the chosen folders. Names what it looked on.
    nonisolated static func searchWindowsProfiles(_ folders: [URL]) -> (WindowsProfileDiscovery, searched: [String]) {
        let volumes = FileManager.default.mountedVolumeURLs(includingResourceValuesForKeys: [.volumeNameKey, .volumeIsBrowsableKey], options: []) ?? []
        let names = volumes.compactMap { url -> String? in
            guard let values = try? url.resourceValues(forKeys: [.volumeNameKey, .volumeIsBrowsableKey]), values.volumeIsBrowsable != false else { return nil }
            return values.volumeName
        }
        return (WindowsProfiles().discover(mountedVolumes: volumes, selectedFolders: folders), names + folders.map(\.lastPathComponent))
    }
    func importWindows(_ candidate: WindowsProfileCandidate) {
        let detail = candidate.profileName + "/" + candidate.configURL.lastPathComponent
        beginImport(displayName: candidate.profileName, provenance: .init(source: "Windows profile", detail: detail, windows: true, symbol: "pc"),
                    platform: .windows, origin: ConfigOrigin(kind: .imported, source: "Windows profile \(candidate.profileName)")) { adapter, dialect in
            try await ConfigImportLoader(catalog: adapter).load(.file(candidate.configURL), dialect: dialect)
        }
    }
    func importLink(_ url: URL) {
        let host = url.host?.lowercased() ?? ""
        let source = host.hasSuffix("github.com") || host.hasSuffix("githubusercontent.com") ? "GitHub"
            : ["localhost", "127.0.0.1"].contains(host) ? "This Mac’s server" : "Link"
        let shown = (url.host ?? "") + url.path
        beginImport(displayName: Self.name(for: url), provenance: .init(source: source, detail: shown, symbol: "link"),
                    origin: ConfigOrigin(kind: .imported, source: url.absoluteString)) { adapter, dialect in
            try await ConfigImportLoader(catalog: adapter).load(.remote(url), dialect: dialect)
        }
    }
    /// Shift-Command-V: the clipboard's text goes straight to review, byte for byte as Latin-1.
    func importPasteboard() {
        guard let text = NSPasteboard.general.string(forType: .string), !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            notify("The clipboard has no config text to review."); return
        }
        importText(text, dropped: false)
    }
    func importText(_ text: String, dropped: Bool) {
        let data: Data
        do { data = try ConfigDocument.encode(text) } catch {
            notify("That text has characters Call of Duty 2 can’t store in a config."); return
        }
        beginImport(displayName: dropped ? "Dropped config" : "Pasted config",
                    provenance: .init(source: dropped ? "Dropped text" : "Pasted text", symbol: "doc.on.clipboard"),
                    origin: ConfigOrigin(kind: .imported, source: dropped ? "drop" : "paste")) { adapter, dialect in
            try await ConfigImportLoader(catalog: adapter).load(dropped ? .drop(data) : .paste(data), dialect: dialect)
        }
    }
    /// Try this setup: a signed entry, downloaded by its exact digest, reviewed like any other source.
    func tryEntry(_ entry: ConfigDirectoryEntry) {
        guard let snapshot else { return }
        let cached = detail?.entryID == entry.id ? detail?.bundle : nil
        let directory = self.directory
        beginImport(displayName: entry.player,
                    provenance: .init(source: "Players catalog", signed: true, credit: entry.credit, permission: true,
                                      windows: entry.platform == .windows, symbol: "person.2"),
                    platform: entry.platform == .windows ? .windows : .native,
                    origin: ConfigOrigin(kind: .imported, source: entry.sourceURL.absoluteString, credit: entry.credit)) { adapter, dialect in
            if let cached { return cached }
            return try await directory.profile(entryID: entry.id, snapshot: snapshot, dialect: dialect, catalog: adapter)
        }
    }

    static func name(for url: URL) -> String {
        let base = url.deletingPathExtension().lastPathComponent
        let name = base.hasSuffix(".cfg") ? String(base.dropLast(4)) : base
        return name.isEmpty || name == "raw" ? "Imported config" : name
    }
}
