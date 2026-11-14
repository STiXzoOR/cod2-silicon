import Foundation

@main struct ConfigCheckerTests {
    static func require(_ condition: @autoclosure () -> Bool, _ message: String) {
        if !condition() { FileHandle.standardError.write(Data("FAIL: \(message)\n".utf8)); exit(1) }
    }
    static func main() async throws {
        let resources = URL(fileURLWithPath: CommandLine.arguments[1])
        let catalog = try ConfigCatalog(data: Data(contentsOf: resources.appendingPathComponent("ConfigCatalog.json")))
        let rules = try ConfigRules(data: Data(contentsOf: resources.appendingPathComponent("ConfigRulesets.json")))
        let text = [
            "// a sample config",                     // 1
            "seta com_maxfps \"333\"",                // 2  zPAM keeps it between 125 and 250
            "seta sensitivity \"1.4\"",               // 3
            "seta cl_maxpakets \"100\"",              // 4  typo
            "seta rate \"25000\"",                    // 5
            "seta snaps \"40\"",                      // 6  above the limit
            "seta cg_fov \"90\"",                     // 7  set again on line 14
            "seta r_vsync \"0\"",                     // 8  another game's
            "seta r_picmip \"3\"",                    // 9
            "seta m_pitch \"0.022\"",                 // 10 repeat
            "seta m_pitch \"0.022\"",                 // 11
            "bind MOUSE1 \"+attack\"",                // 12
            "bind F9 \"rcon login synthetic-secret\"",// 13 password
            "seta cg_fov \"90\"; seta m_rinput \"1\"" // 14 cheats, and a CoD2x-only setting
        ].joined(separator: "\r\n") + "\r\n"
        let data = Data(text.utf8)
        let report = try await ConfigChecker.check(data, dialect: .stock, ruleset: "zpam-3.36", catalog: catalog, rules: rules)
        func finding(_ line: Int) -> ConfigCheckFinding? { report.findings.first { $0.line == line } }
        for finding in report.findings { print(finding.line, finding.kind, "|", finding.title, "|", finding.detail, "|", finding.note, "|", finding.fixLabel ?? "-") }
        require(report.lines.count == 14 && report.bytes == data, "every line is listed and the bytes are untouched")
        require(finding(4)?.kind == .broken && finding(4)?.fix == .replace("seta cl_maxpackets \"100\""), "a typo offers the one close name")
        require(finding(6)?.kind == .different && finding(6)?.fix == .replace("seta snaps \"30\""), "a value above the limit offers the limit")
        require(finding(7)?.title == "cg_fov is set twice", "the earlier of two assignments is the repeat")
        require(report.findings.contains { $0.line == 14 && $0.title.contains("cheats") } && report.findings.contains { $0.line == 14 && $0.title.contains("m_rinput") },
                "the later cg_fov is the one checked for cheats; a CoD2x-only setting is named")
        require(finding(8)?.kind == .broken && finding(8)?.fix == .remove, "another game's setting is removed, not renamed")
        require(finding(2)?.kind == .note && finding(2)?.fixLabel == "Set to 250" && finding(2)?.fix == .replace("seta com_maxfps \"250\""), "a zPAM limit is a note with its own fix")
        require(finding(10)?.kind == .different && finding(11) == nil, "only the earlier m_pitch is marked")
        require(finding(13)?.kind == .note && !report.lines[12].text.contains("synthetic-secret"), "a password is a note and is never shown")
        require(finding(3) == nil && finding(5) == nil && finding(9) == nil && finding(12) == nil, "sound lines have no finding")
        require(!report.findings.map { $0.title + $0.detail + $0.note }.joined().contains("synthetic-secret"), "no finding repeats a password")

        let cod2x = try await ConfigChecker.check(data, dialect: .cod2x, ruleset: nil, catalog: catalog, rules: rules)
        require(!cod2x.findings.contains { $0.title.contains("m_rinput") } && !cod2x.findings.contains { $0.line == 2 }, "CoD2x has m_rinput; without rules there is no rules note")

        let fixed = report.applying(Set(report.fixable.map(\.id)))
        let fixedText = String(decoding: fixed, as: UTF8.self)
        require(fixedText.contains("seta cl_maxpackets \"100\"") && fixedText.contains("seta snaps \"30\"") && !fixedText.contains("r_vsync"), "Fix All renames, limits and removes")
        require(fixedText.contains("seta com_maxfps \"333\"") && fixedText.contains("rcon login"), "Fix All leaves notes alone")
        require(fixedText.components(separatedBy: "seta m_pitch").count == 2 && !fixedText.contains("\r\n\r\n"), "a removed line takes its line break with it")
        let again = try await ConfigChecker.check(fixed, dialect: .stock, ruleset: "zpam-3.36", catalog: catalog, rules: rules)
        require(again.fixable.isEmpty && again.findings.allSatisfy { $0.kind == .note }, "after Fix All only notes remain")
        let shown = report.shown(applied: Set(report.fixable.map(\.id)))
        require(shown[3].mark == .changed && shown[5].note == "Changed to 30" && shown[7].mark == .removed && shown[1].mark == .finding(.note), "the page shows what each fix did")
        require(report.applying([]) == data, "no fix, no change")
        require(ConfigChecker.nearest("cl_maxpakets", in: catalog, mode: .stock) == "cl_maxpackets" && ConfigChecker.nearest("zz_nothing_alike", in: catalog, mode: .stock) == nil, "only a close name is offered")
        print("PASS: config checker findings, fixes and display")
    }
}
