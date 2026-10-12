import Foundation

enum ConfigImportPlatform: String, Codable, Sendable { case native, windows }
enum ConfigImportMode: Sendable { case review, everything }
enum ConfigImportTarget: Sendable {
    case newLoadout(id: String, displayName: String)
    case existing(ConfigProfileRead)
    var isNew: Bool { if case .newLoadout = self { return true }; return false }
}
enum ConfigImportSessionError: Error, Equatable, LocalizedError, Sendable {
    case invalidBundle, unavailableSelection, emptySelection, unsafeComposition, alreadyApplied
    var errorDescription: String? {
        switch self {
        case .invalidBundle: return "The import files are missing, duplicated or use a different config dialect."
        case .unavailableSelection: return "This command is blocked, locked or belongs to machine settings."
        case .emptySelection: return "Select at least one config change."
        case .unsafeComposition: return "These selected commands cannot be safely combined with this loadout. Review their dependencies and scripts."
        case .alreadyApplied: return "This import has already been applied."
        }
    }
}
struct ConfigImportRow: Sendable, Identifiable {
    let candidate: ConfigImportCandidate
    let title: String
    let group: String
    let oldValue: String?
    let newValue: String?
    let lockedNames: [String]
    let platformNote: String?
    var id: String { candidate.id }
    var canSelect: Bool {
        candidate.importable && candidate.classification != .blocked && lockedNames.isEmpty && platformNote == nil
            && ![.empty, .comment].contains(candidate.command.kind)
    }
}
struct ConfigImportReceipt: Sendable {
    let profileID: String
    let historyEntry: ConfigHistoryEntry
}

/// Value-type review state; a view owns/observes this value and applies it once
/// per confirmation. Selection setters cannot grant permission to blocked rows.
struct ConfigImportSession: Sendable {
    let review: ConfigImportReview
    let dialect: ConfigDialect
    let platform: ConfigImportPlatform
    let origin: ConfigOrigin
    private let catalog: any ConfigImportCatalog
    private(set) var mode: ConfigImportMode = .review
    private(set) var target: ConfigImportTarget
    private(set) var selectedIDs: Set<String> = []
    private(set) var receipt: ConfigImportReceipt?

    init(bundle: ConfigImportBundle, dialect: ConfigDialect, catalog: any ConfigImportCatalog, displayName: String,
         platform: ConfigImportPlatform = .native, origin: ConfigOrigin? = nil) throws {
        guard bundle.files.contains(where: { $0.path == bundle.entryPath }),
              Set(bundle.files.map(\.path)).count == bundle.files.count,
              bundle.files.allSatisfy({ $0.document.dialect == dialect }) else { throw ConfigImportSessionError.invalidBundle }
        self.catalog = catalog; self.dialect = dialect; self.platform = platform
        self.origin = origin ?? ConfigOrigin(kind: .imported, source: bundle.source.kind.rawValue)
        review = ConfigImportSafety(catalog: catalog).review(bundle)
        target = .newLoadout(id: "Import-" + String(UUID().uuidString.prefix(24)), displayName: displayName)
        resetSelection()
    }
    var rows: [ConfigImportRow] { makeRows(review: review, baseline: baseline) }
    private var baseline: ConfigProfileRead? { if case .existing(let value) = target { return value }; return nil }
    mutating func setTarget(_ target: ConfigImportTarget) { self.target = target; resetSelection() }
    mutating func setMode(_ mode: ConfigImportMode) { self.mode = mode; resetSelection() }
    mutating func select(_ id: String, selected: Bool) throws {
        guard receipt == nil else { throw ConfigImportSessionError.alreadyApplied }
        guard let row = rows.first(where: { $0.id == id }), row.canSelect else { throw ConfigImportSessionError.unavailableSelection }
        if selected { selectedIDs.insert(id) } else { selectedIDs.remove(id) }
    }
    /// One eligibility review when keeping explicit choices after leaving
    /// Import Everything. Invalid selections leave the review state intact.
    mutating func replaceSelection(_ ids: Set<String>) throws {
        guard receipt == nil else { throw ConfigImportSessionError.alreadyApplied }
        let eligible = Set(rows.filter(\.canSelect).map(\.id))
        guard ids.isSubset(of: eligible) else { throw ConfigImportSessionError.unavailableSelection }
        mode = .review
        selectedIDs = ids
    }
    private mutating func resetSelection() {
        selectedIDs = Set(rows.filter { $0.canSelect && (mode == .everything || $0.candidate.selectedByDefault) }.map(\.id))
    }

    /// Preview is also the final revalidation step. No raw exec is emitted, and
    /// re-review of the assembled document catches omitted vstr definitions,
    /// target-provided unsafe values, and parser-boundary changes.
    func preview() throws -> (document: ConfigDocument, metadata: ConfigStudioMetadata) {
        guard receipt == nil else { throw ConfigImportSessionError.alreadyApplied }
        let selected = rows.filter { selectedIDs.contains($0.id) }
        guard !selected.isEmpty else { throw ConfigImportSessionError.emptySelection }
        guard selected.count == selectedIDs.count, selected.allSatisfy(\.canSelect),
              baseline?.document.dialect == nil || baseline?.document.dialect == dialect else {
            throw ConfigImportSessionError.unavailableSelection
        }
        var bytes = baseline?.data ?? Data()
        let before = ConfigDocument(data: bytes, dialect: dialect)
        var ranges: [(ConfigImportRow, Range<Int>)] = []
        for row in selected {
            guard let file = review.bundle.files.first(where: { $0.path == row.candidate.location.file }) else { throw ConfigImportSessionError.invalidBundle }
            if !bytes.isEmpty { bytes.append(10) }
            let start = bytes.count
            bytes.append(file.document.serialized().subdata(in: row.candidate.location.range))
            ranges.append((row, start..<bytes.count)); bytes.append(10)
        }
        guard bytes.count <= 65535 else { throw ConfigImportError.tooLarge }
        let document = ConfigDocument(data: bytes, dialect: dialect)
        let composed = ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: "composed.cfg",
            files: [.init(path: "composed.cfg", data: bytes, dialect: dialect)]))
        let priorReview = baselineReview(baseline)
        // Original logical commands must retain their exact tokens too.
        for command in before.commands where !command.tokens.isEmpty {
            guard document.commands.contains(where: { $0.range.lowerBound == command.range.lowerBound && $0.tokens.map(\.value) == command.tokens.map(\.value) }) else {
                throw ConfigImportSessionError.unsafeComposition
            }
        }
        for (row, range) in ranges {
            guard let actual = composed.candidates.first(where: { $0.location.range.lowerBound == range.lowerBound }),
                  actual.command.tokens.map(\.value) == row.candidate.command.tokens.map(\.value),
                  actual.importable, actual.classification.rawValue <= row.candidate.classification.rawValue,
                  lockedNames(actual, baseline: baseline, priorReview: priorReview).isEmpty else { throw ConfigImportSessionError.unsafeComposition }
        }
        var metadata = baseline?.metadata ?? ConfigStudioMetadata(displayName: {
            if case .newLoadout(_, let name) = target { return name }; return "Imported"
        }())
        if baseline == nil { metadata.origin = origin }
        // Only reviewed direct values belong in provenance; never copy secret
        // source text, arbitrary URLs or unselected values into the sidecar.
        for row in selected {
            if let file = review.bundle.files.first(where: { $0.path == row.candidate.location.file }),
               let assignment = ConfigImportScript.assignment(row.candidate.command, document: file.document, catalog: catalog) {
                metadata.lastImportedValues[assignment.name] = ConfigDocument.text(assignment.value)
            }
        }
        try metadata.validate()
        return (document, metadata)
    }
    @MainActor mutating func apply(to store: ConfigProfileStore) throws -> ConfigImportReceipt {
        guard receipt == nil else { throw ConfigImportSessionError.alreadyApplied }
        guard store.dialect == dialect else { throw ConfigImportSessionError.invalidBundle }
        if let baseline { guard try store.read(id: baseline.profile.id).version == baseline.version else { throw ConfigProfileError.conflict } }
        let prepared = try preview()
        let entry: ConfigHistoryEntry, id: String
        switch target {
        case .existing(let read):
            id = read.profile.id
            entry = try store.update(id: id, document: prepared.document, expected: read.version, metadata: prepared.metadata,
                                     source: .import, summary: "Imported reviewed config")
        case .newLoadout(let newID, let name):
            id = newID
            _ = try store.create(id: id, displayName: name, document: prepared.document, metadata: prepared.metadata,
                                 source: .import, summary: "Imported reviewed config")
            guard let created = try store.history(id: id).first else { throw ConfigProfileError.invalidHistory }; entry = created
        }
        let result = ConfigImportReceipt(profileID: id, historyEntry: entry); receipt = result; return result
    }
    @MainActor mutating func apply(to studio: ConfigStudioSession) throws -> ConfigImportReceipt {
        guard receipt == nil else { throw ConfigImportSessionError.alreadyApplied }
        guard studio.configDialect == dialect else { throw ConfigImportSessionError.invalidBundle }
        if let baseline { guard try studio.readImportTarget(id: baseline.profile.id).version == baseline.version else { throw ConfigProfileError.conflict } }
        let prepared = try preview()
        let id: String, name: String
        switch target {
        case .newLoadout(let newID, let displayName): id = newID; name = displayName
        case .existing(let read): id = read.profile.id; name = read.metadata.displayName
        }
        let entry = try studio.commitReviewedImport(id: id, displayName: name, document: prepared.document,
                                                     expected: baseline?.version, metadata: prepared.metadata)
        let result = ConfigImportReceipt(profileID: id, historyEntry: entry); receipt = result; return result
    }

    private func makeRows(review: ConfigImportReview, baseline: ConfigProfileRead?) -> [ConfigImportRow] {
        let priorReview = baselineReview(baseline)
        return review.candidates.map { candidate in
            let tokens = candidate.command.tokens
            let file = review.bundle.files.first { $0.path == candidate.location.file }!
            let assignment = ConfigImportScript.assignment(candidate.command, document: file.document, catalog: catalog)
            let binding = candidate.command.kind == .bind ? ConfigImportScript.binding(candidate.command, document: file.document) : nil
            let key = tokens.count > 1 ? tokens[1].text : ""
            let title: String, group: String, old: String?, new: String?
            if let assignment {
                title = "Set " + assignment.name; group = "Settings"
                old = candidate.reasons.contains(.credential) ? nil : baseline?.document.effectiveSetting(named: assignment.name)
                new = candidate.reasons.contains(.credential) ? nil : ConfigDocument.text(assignment.value)
            } else if let binding {
                title = "Bind " + key; group = "Bindings"
                let secret = priorReview.contains { previous in
                    previous.command.kind == .bind && previous.command.tokens.count > 1
                        && ConfigDocument.keyNumber(previous.command.tokens[1].text) == ConfigDocument.keyNumber(key)
                        && previous.reasons.contains(.credential)
                }
                old = secret ? nil : baseline?.document.effectiveBind(key: key)
                new = candidate.reasons.contains(.credential) ? nil : ConfigDocument.text(binding)
            } else { title = candidate.reasons.first?.summary ?? "Config command"; group = "Commands"; old = nil; new = nil }
            return .init(candidate: candidate, title: title, group: group, oldValue: old, newValue: new,
                         lockedNames: lockedNames(candidate, baseline: baseline, priorReview: priorReview), platformNote: platformNote(candidate))
        }
    }
    private func baselineReview(_ baseline: ConfigProfileRead?) -> [ConfigImportCandidate] {
        guard let baseline else { return [] }
        return ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: "target.cfg",
            files: [.init(path: "target.cfg", data: baseline.data, dialect: dialect)])).candidates
    }
    private func platformNote(_ candidate: ConfigImportCandidate) -> String? {
        let commands = [candidate.command] + candidate.effects.map(\.command)
        for command in commands {
            let tokens = command.tokens
            let first = tokens.first?.text.lowercased() ?? ""
            let writesSetting = command.kind == .setting || ["toggle", "togglep", "reset", "setfromdvar", "increase", "decrease"].contains(first)
            let name = writesSetting && tokens.count > 1 ? tokens[1].text.lowercased() : first
            if ["r_mode", "r_customwidth", "r_customheight", "r_fullscreen", "r_borderless", "r_displayrefresh", "r_aspectratio", "r_swapinterval"].contains(name) {
                return "Choose display settings for this Mac in launcher Settings. Imported resolution and display modes are omitted."
            }
            if GameSettings.machineDvarNames.contains(name) || name == "r_rendererpreference" {
                return "This value belongs to this Mac's launcher settings. Choose it there instead of importing a machine-specific value."
            }
            if platform == .windows && ["in_mouse", "in_joystick", "m_rawinput", "cl_mouseaccel_windows", "snd_provider", "s_provider"].contains(name) {
                return "This Windows input or driver choice has no verified Mac translation. Keep the native device settings."
            }
            if [.bind, .unbind].contains(command.kind), tokens.count > 1 {
                guard let key = ConfigDocument.keyNumber(tokens[1].text), key > 0 else { return "This key has no supported native mapping. Choose a key in Binds." }
                if platform == .windows && key == 150 { return "Command is a Mac key, rather than a verified Windows mapping. Choose the intended key in Binds." }
            }
        }
        return nil
    }
    private func lockedNames(_ candidate: ConfigImportCandidate, baseline: ConfigProfileRead?, priorReview: [ConfigImportCandidate]) -> [String] {
        guard let baseline else { return [] }
        var touched = Set<String>(), keys = Set<Int>(), allBinds = false
        for command in [candidate.command] + candidate.effects.map(\.command) {
            let tokens = command.tokens
            guard let first = tokens.first?.text.lowercased() else { continue }
            touched.insert(first)
            if command.kind == .setting || ["toggle", "togglep", "reset", "setfromdvar", "increase", "decrease"].contains(first) {
                if tokens.count > 1 { touched.insert(tokens[1].text.lowercased()) }
            }
            if [.bind, .unbind].contains(command.kind), tokens.count > 1, let key = ConfigDocument.keyNumber(tokens[1].text) {
                keys.insert(key)
                if let old = baseline.document.effectiveBindBytes(key: tokens[1].text) {
                    for oldCommand in ConfigDocument(data: old, dialect: dialect).commands {
                        if let name = oldCommand.tokens.first?.text.lowercased() { touched.insert(name) }
                    }
                }
            }
            if command.kind == .unbindall { allBinds = true }
        }
        // Changing a helper also changes every existing bind which reaches that
        // helper, including indirect vstr chains. Protect the old action and key
        // identity before the imported definition can make either disappear.
        for previous in priorReview where previous.command.kind == .bind && previous.command.tokens.count > 1 {
            guard let key = ConfigDocument.keyNumber(previous.command.tokens[1].text) else { continue }
            let changesDependency = previous.dependencies.contains { $0.kind == .vstr && touched.contains($0.name.lowercased()) }
            if keys.contains(key) || changesDependency {
                keys.insert(key)
                for effect in previous.effects {
                    if let name = effect.command.tokens.first?.text.lowercased() { touched.insert(name) }
                }
            }
        }
        touched.formUnion(touched.filter { $0.hasPrefix("-") }.map { "+" + $0.dropFirst() })
        return baseline.metadata.locks.sorted().filter { lock in
            let name = lock.lowercased()
            if name.hasPrefix("bind:"), let key = ConfigDocument.keyNumber(String(name.dropFirst(5))) { return allBinds || keys.contains(key) }
            let action = name.hasPrefix("action:") ? String(name.dropFirst(7)) : name
            return touched.contains(action) || (allBinds && (name.hasPrefix("action:") || catalog.command(named: action, dialect: dialect) != nil))
        }
    }
}
