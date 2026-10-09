import Foundation

private final class GameGate: @unchecked Sendable {
    private let lock = NSLock()
    private var running = false
    func set(_ value: Bool) { lock.lock(); running = value; lock.unlock() }
    func get() -> Bool { lock.lock(); defer { lock.unlock() }; return running }
}
@main struct ConfigCheckupTests {
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: \(message)") }
    }
    static func rejects<E: Error & Equatable>(_ expected: E, _ operation: () throws -> Void) {
        do { try operation(); fatalError("FAIL: expected \(expected)") }
        catch let error as E { require(error == expected, "expected \(expected), got \(error)") }
        catch { fatalError("FAIL: unexpected \(error)") }
    }
    static func main() throws {
        let resources = URL(fileURLWithPath: CommandLine.arguments[1])
        let model = try ConfigCheckup(catalog: ConfigCatalog(data: Data(contentsOf: resources.appendingPathComponent("ConfigCatalog.json"))),
                                     rules: ConfigRules(data: Data(contentsOf: resources.appendingPathComponent("ConfigRulesets.json"))),
                                     optimizer: ConfigOptimizer(data: Data(contentsOf: resources.appendingPathComponent("ConfigOptimizer.json"))))
        let home = URL(fileURLWithPath: ProcessInfo.processInfo.environment["COD2_CHECKUP_TEST_SCRATCH"]!).appendingPathComponent("profiles")
        let gate = GameGate()
        let store = try ConfigProfileStore(homeURL: home, dialect: .cod2x, gameIsRunning: { gate.get() })
        let original = Data("// keep bytes\r\nseta com_maxfps \"333\"\r\nseta m_filter \"1\"\r\nseta cl_mouseAccel \"2\"\r\nseta rate \"5000\"\r\nseta snaps \"20\"\r\nseta cl_maxpackets \"30\"\r\nseta r_picmip \"3\"\r\nseta r_drawSun \"1\"\r\nseta cg_fov \"90\"\r\nbind F1 \"wait; +attack\"\r\n".utf8)
        var metadata = ConfigStudioMetadata(displayName: "Test")
        metadata.locks = ["CL_MOUSEACCEL"]
        let profile = try store.create(id: "test", displayName: "Test", document: ConfigDocument(data: original, dialect: .cod2x), metadata: metadata)
        let report = model.evaluate(profile: profile, rulesetIDs: ["zpam-4.08"], tuneFor: .competitive)
        require(report.suggestions.contains { $0.area == .movement && $0.changes.contains { $0.setting == "com_maxfps" && $0.newValue == "250" } }, "competitive movement suggests exactly 250")
        require(report.suggestions.contains { $0.area == .aim && $0.changes.contains { $0.setting == "m_filter" && $0.newValue == "0" } }, "aim smoothing suggestion")
        require(report.suggestions.contains { $0.area == .visibility }, "visibility has evidence-based advice")
        require(report.suggestions.first { $0.area == .visibility }?.changes.contains { $0.setting == "r_picmip_manual" && $0.newValue == "1" } == true, "texture advice explicitly enables manual mode so startup will preserve the reviewed level")
        let proposed = report.suggestions.flatMap(\.changes).map { $0.setting.lowercased() }
        require(!proposed.contains("cl_mouseaccel"), "case-insensitive locks suppress advice")
        require(Set(proposed).isDisjoint(with: ["rate", "snaps", "cl_maxpackets", "r_drawsun", "g_competitive", "cg_fov", "cg_fovscale", "cg_fovmin"]), "server-owned and FOV never proposed")
        require(report.dials.count == 4 && report.dials.allSatisfy { $0.statusText == "Not measured" && $0.value == nil }, "absent data is Not measured on all four dials")
        require(report.suggestions.allSatisfy { !$0.why.isEmpty && !$0.notice.isEmpty && !$0.sources.isEmpty && $0.evidence.allSatisfy { $0.kind != .measurement } }, "general advice carries evidence without measured claims")
        require(report.policy.contains { $0.setting == "wait" && $0.status == .violation }, "wait in an effective binding is flagged")
        let unknown = model.evaluate(profile: profile, rulesetIDs: ["pam"], tuneFor: .competitive)
        require(unknown.suggestions.isEmpty && !unknown.limitations.isEmpty, "unknown rules never give positive advice")
        let publicReport = model.evaluate(profile: profile, rulesetIDs: ["public-cod2x"], tuneFor: .balanced)
        require(publicReport.suggestions.contains { $0.area == .connection && $0.changes.contains { $0.setting == "snaps" && $0.newValue == "40" } }, "public CoD2x connection uses 40 snaps")
        let stockStore = try ConfigProfileStore(homeURL: home, dialect: .stock)
        let stockReport = model.evaluate(profile: try stockStore.read(id: "test"), rulesetIDs: ["stock-1.3"], tuneFor: .balanced)
        require(stockReport.suggestions.flatMap(\.changes).contains { $0.setting == "snaps" && $0.newValue == "30" }, "stock connection uses 30 snaps")
        require(stockReport.suggestions.flatMap(\.changes).contains { $0.setting == "cl_maxpackets" && $0.newValue == "100" }, "stock packet cap is 100")
        let stockTarget = model.evaluate(profile: profile, rulesetIDs: ["stock-1.3"], tuneFor: .balanced, targetMode: .stock)
        require(stockTarget.suggestions.flatMap(\.changes).contains { $0.setting == "snaps" && $0.newValue == "30" }, "target client rules can differ from config parser dialect")
        let measured = model.evaluate(profile: profile, rulesetIDs: ["public-cod2x"], tuneFor: .bestLooking,
                                      measurements: .init(frameTimeP99MS: 8, mousePollingHz: 1000, pingMS: 42, packetLossPercent: nil, source: "fixture match"))
        require(measured.dials.first { $0.area == .movement }?.value == 8, "frame timing uses the explicit measurement")
        require(measured.dials.first { $0.area == .visibility }?.statusText == "Not measured", "ping/timing do not invent visibility scores")
        let invalid = model.evaluate(profile: profile, rulesetIDs: ["public-cod2x"], tuneFor: .balanced,
                                     measurements: .init(frameTimeP99MS: .nan, mousePollingHz: -1, pingMS: .infinity, source: "bad"))
        require(invalid.dials.allSatisfy { $0.value == nil }, "invalid measurements remain absent")
        let allIDs = Set(publicReport.suggestions.map(\.id))
        require(model.evaluate(profile: profile, rulesetIDs: ["public-cod2x"], tuneFor: .balanced, dismissedSuggestionIDs: allIDs).suggestions.isEmpty, "Not now filters reviewed rule identities")
        require(try store.read(id: "test").data == original && store.history(id: "test").count == 1, "review and dismissal write nothing")
        let receipt = try model.apply(report: publicReport, suggestionIDs: allIDs, store: store)
        let applied = try store.read(id: "test")
        require(try receipt.historyEntry.source == .checkup && store.history(id: "test").count == 2, "one confirmed Apply is one Check-up transaction")
        require(applied.metadata == metadata && applied.document.effectiveSetting(named: "cl_mouseAccel") == "2", "Apply preserves metadata and locked values")
        require(applied.document.effectiveSetting(named: "rate") == "25000" && applied.document.effectiveSetting(named: "snaps") == "40" && applied.document.effectiveSetting(named: "cl_maxpackets") == "125", "Apply publishes exact connection changes")
        require(applied.document.effectiveSetting(named: "cg_fov") == "90" && applied.document.effectiveBind(key: "F1") == "wait; +attack", "advice preserves protected values and binds")
        rejects(ConfigProfileError.conflict) { _ = try model.apply(report: publicReport, suggestionIDs: allIDs, store: store) }
        gate.set(true)
        rejects(ConfigProfileError.gameRunning) { _ = try model.undo(receipt: receipt, rulesetIDs: ["public-cod2x"], store: store) }
        gate.set(false)
        rejects(ConfigCheckupError.protectedSetting("rate")) { _ = try model.undo(receipt: receipt, rulesetIDs: ["zpam-4.08"], store: store) }
        let restored = try model.undo(receipt: receipt, rulesetIDs: ["public-cod2x"], store: store)
        require(try restored.source == .restore && store.history(id: "test").count == 3, "one Undo is one store restore transaction")
        require(try store.read(id: "test").data == original && store.read(id: "test").metadata == metadata, "Undo restores exact original bytes and metadata")
        let fresh = model.evaluate(profile: try store.read(id: "test"), rulesetIDs: ["public-cod2x"], tuneFor: .balanced)
        gate.set(true)
        rejects(ConfigProfileError.gameRunning) { _ = try model.apply(report: fresh, suggestionIDs: Set(fresh.suggestions.map(\.id)), store: store) }
        gate.set(false)
        rejects(ConfigCheckupError.emptyAction) { _ = try model.apply(report: fresh, suggestionIDs: [], store: store) }
        rejects(ConfigCheckupError.staleSuggestion) { _ = try model.apply(report: fresh, suggestionIDs: ["invented"], store: store) }
        let path = home.appendingPathComponent("main/players/test/config_mp.cfg")
        try PrivateFile.write(original + Data("// external revision\n".utf8), to: path)
        rejects(ConfigProfileError.conflict) { _ = try model.apply(report: fresh, suggestionIDs: Set(fresh.suggestions.map(\.id)), store: store) }
        require(try Data(contentsOf: path) == original + Data("// external revision\n".utf8), "stale Apply cannot overwrite disk")
        let scripted = try store.create(id: "scripted", displayName: "Scripted", document: ConfigDocument(data: original + Data("exec missing.cfg\n".utf8), dialect: .cod2x))
        let scriptReport = model.evaluate(profile: scripted, rulesetIDs: ["public-cod2x"], tuneFor: .balanced)
        require(scriptReport.suggestions.isEmpty && scriptReport.policy.allSatisfy { $0.status != .ok }, "unresolved scripts never certify direct settings")
        let unbound = try store.create(id: "unbound", displayName: "Unbound", document: ConfigDocument(data: original + Data("unbind F1\n".utf8), dialect: .cod2x))
        require(!model.evaluate(profile: unbound, rulesetIDs: ["zpam-4.08"], tuneFor: .competitive).policy.contains { $0.setting == "wait" && $0.status == .violation }, "obsolete binds do not create a wait violation")
        let unspecified = model.evaluate(profile: profile, rulesetIDs: ["public-cod2x"], tuneFor: .balanced, measurements: .init(frameTimeP99MS: 8, mousePollingHz: 1000, pingMS: 42))
        require(unspecified.dials.allSatisfy { $0.statusText == "Not measured" }, "numbers without provenance are not measurements")
        var textureLocks = ConfigStudioMetadata(displayName: "Texture lock"); textureLocks.locks = ["R_PICMIP_MANUAL"]
        let textureRead = try store.create(id: "texture-lock", displayName: "Texture lock", document: ConfigDocument(data: original + Data("seta r_picmip_manual \"0\"\n".utf8), dialect: .cod2x), metadata: textureLocks)
        require(!model.evaluate(profile: textureRead, rulesetIDs: ["public-cod2x"], tuneFor: .bestLooking).suggestions.contains { $0.area == .visibility }, "manual-mode lock blocks the entire texture change")
        let metaTarget = try store.create(id: "metadata", displayName: "Metadata", document: ConfigDocument(data: original, dialect: .cod2x))
        let metaReport = model.evaluate(profile: metaTarget, rulesetIDs: ["public-cod2x"], tuneFor: .balanced)
        let metaReceipt = try model.apply(report: metaReport, suggestionIDs: ["unfiltered-aim"], store: store)
        let metaApplied = try store.read(id: "metadata")
        var changedMetadata = metaApplied.metadata; changedMetadata.locks = ["m_filter"]
        try PrivateFile.write(JSONEncoder().encode(changedMetadata), to: home.appendingPathComponent("main/players/metadata/studio.json"))
        rejects(ConfigProfileError.conflict) { _ = try model.undo(receipt: metaReceipt, rulesetIDs: ["public-cod2x"], store: store) }
        require(try store.read(id: "metadata").data == metaApplied.data, "stale Undo preserves config after a metadata revision")
        let targetRead = try store.create(id: "target-mode", displayName: "Target", document: ConfigDocument(data: original, dialect: .cod2x))
        let targetReport = model.evaluate(profile: targetRead, rulesetIDs: ["stock-1.3"], tuneFor: .balanced, targetMode: .stock)
        let stockReceipt = try model.apply(report: targetReport, suggestionIDs: ["connection-snapshots"], store: store)
        require(stockReceipt.changes[0].newValue == "30", "Apply keeps the reviewed target mode")
        _ = try model.undo(receipt: stockReceipt, rulesetIDs: ["stock-1.3"], store: store, targetMode: .stock)
        require(try store.read(id: "target-mode").data == original, "Undo supports an explicit target while preserving parser dialect")
        try everyShippedSuggestion(model, store: store)
        print("PASS: ConfigCheckup (typed review, every optimizer rule, guarded disk Apply/Undo)")
    }
    static func everyShippedSuggestion(_ model: ConfigCheckup, store: ConfigProfileStore) throws {
        for rule in model.optimizer.rules {
            let initial = rule.setting == "com_maxfps" ? "333" : rule.setting == "r_picmip" ? "3" : rule.setting == "cl_mouseAccel" ? "2" : rule.setting == "m_filter" ? "1" : "10"
            let document = ConfigDocument(data: Data("seta \(rule.setting) \"\(initial)\"\n".utf8), dialect: .cod2x)
            let profile = try store.create(id: rule.id, displayName: rule.title, document: document)
            let policy = rule.setting == "com_maxfps" ? "cod2x-competitive" : "public-cod2x"
            let report = model.evaluate(profile: profile, rulesetIDs: [policy], tuneFor: rule.tuneFor[0])
            guard let suggestion = report.suggestions.first(where: { $0.id == rule.id }) else { fatalError("FAIL: shipped rule never triggers: \(rule.id)") }
            require(suggestion.changes.count == (rule.area == .visibility ? 2 : 1) && suggestion.changes[0].newValue == rule.suggest["cod2x"], "authored exact proposed values include texture manual mode")
            if rule.area == .visibility { require(suggestion.changes[1].setting == "r_picmip_manual" && suggestion.changes[1].newValue == "1", "texture choice persists through renderer setup") }
            let receipt = try model.apply(report: report, suggestionIDs: [rule.id], store: store)
            require(try store.read(id: rule.id).document.effectiveSetting(named: rule.setting) == rule.suggest["cod2x"], "each rule writes its exact reviewed value")
            let next = model.evaluate(profile: try store.read(id: rule.id), rulesetIDs: [policy], tuneFor: rule.tuneFor[0])
            require(!next.suggestions.contains { $0.id == rule.id }, "satisfied rule does not repeat")
            _ = try model.undo(receipt: receipt, rulesetIDs: [policy], store: store)
            require(try store.read(id: rule.id).data == document.serialized(), "each shipped rule has byte-exact Undo")
        }
    }
}
