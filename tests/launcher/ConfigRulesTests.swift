import Foundation

@main struct ConfigRulesTests {
    static func require(_ condition: Bool, _ message: String) {
        if !condition { fatalError("FAIL: \(message)") }
    }
    static func main() throws {
        let rules = try ConfigRules(data: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1])))
        require(rules.ruleset(id: "zpam-4.08") != nil, "missing primary-source zPAM 4.08 rules")
        func check(_ id: String, _ name: String, _ value: String?, _ status: ConfigRuleStatus, mode: ConfigCatalogMode = .cod2x) {
            let result = rules.evaluate(setting: name, value: value, rulesetIDs: [id], mode: mode)
            require(result.status == status, "\(id) \(name)=\(value ?? "absent"): expected \(status), got \(result.status)")
        }
        for id in ["cod2x-competitive", "zpam-4.08"] {
            for cap in ["125", "250"] { check(id, "COM_MAXFPS", cap, .ok) }
            for cap in ["0", "40", "124", "251", "333", "1000", "125.5", "1.25e2", "NaN", "250junk"] { check(id, "com_maxfps", cap, .violation) }
            check(id, "com_maxfps", nil, .unknown)
            for (name, value) in [("rate", "25000"), ("snaps", "40"), ("cl_maxpackets", "125"), ("sc_enable", "0"), ("fx_sort", "1"), ("mss_q3fs", "1")] {
                check(id, name, value, .ok)
                check(id, name, "999", .violation)
            }
            check(id, "wait", "1", .violation)
        }
        for (name, value) in [("com_maxfps", "125"), ("rate", "25000"), ("snaps", "30"), ("cl_maxpackets", "100")] {
            check("zpam-3.36", name, value, .ok, mode: .stock)
        }
        check("zpam-3.36", "com_maxfps", "40", .violation, mode: .stock)
        check("zpam-3.36", "snaps", "40", .violation, mode: .stock)
        check("zpam-3.36", "cl_maxpackets", "125", .violation, mode: .stock)
        for id in ["zpam-3.36", "zpam-4.08"] {
            for name in ["r_drawSun", "cg_viewsize", "cl_pitchspeed", "cl_yawspeed", "cg_hudDamageIconInScope", "cg_tracerChance", "cg_hudGrenadePointerPivot"] {
                require(rules.isServerOwned(name.uppercased(), rulesetIDs: [id]), "\(id) must preserve server-owned \(name)")
            }
        }
        require(rules.isServerOwned("snaps", rulesetIDs: ["zpam-3.36"]), "zPAM 3 pushes snapshots")
        require(!rules.isServerOwned("com_maxfps", rulesetIDs: ["zpam-4.08"]), "legal frame cap remains a player choice")
        check("stock-1.3", "snaps", "40", .violation, mode: .stock)
        check("public-cod2x", "snaps", "40", .ok)
        for id in ["unknown", "pam", "CAL", "zpam-4.99"] { check(id, "com_maxfps", "250", .unknown) }
        check("zpam-4.08", "com_maxfps", "250", .unknown, mode: .stock)
        check("stock-1.3", "unresearched", "1", .unknown, mode: .stock)
        for (name, ids) in [("fx_sort", ["esl-eu-2008", "wwcl-2007"]), ("cg_hudDamageIconInScope", ["esl-eu-2008", "zpam-3.36"]), ("cl_maxpackets", ["polish-2026", "zpam-4.08"])] {
            let modes: ConfigCatalogMode = name == "cl_maxpackets" ? .cod2x : .stock
            require(rules.evaluate(setting: name, value: "1", rulesetIDs: ids, mode: modes).status == .conflict, "unresolved contradiction for \(name)")
        }
        // Expected boundary outcomes exercise every authored constraint, not just decoder coverage.
        var count = 0
        for policy in rules.rulesets {
            require(!policy.sources.isEmpty && !policy.scope.isEmpty, "policy must declare its evidence and coverage")
            for rule in policy.rules {
                count += 1
                require(!rule.sources.isEmpty && rule.sources.allSatisfy { !$0.sourceURL.isEmpty && !$0.sourceDate.isEmpty && !$0.quoteReference.isEmpty && !$0.quote.isEmpty }, "every rule has dated primary evidence")
                let kind = rule.constraint.kind
                let status: ConfigRuleStatus = rule.advisory ? .warning : .ok
                if kind == .range {
                    check(policy.id, rule.setting, String(rule.constraint.minimum!), status, mode: policy.mode)
                    check(policy.id, rule.setting, String(rule.constraint.maximum!), status, mode: policy.mode)
                    check(policy.id, rule.setting, String(rule.constraint.maximum! + 1), rule.advisory ? .warning : .violation, mode: policy.mode)
                } else if kind == .exact {
                    check(policy.id, rule.setting, rule.constraint.value!, status, mode: policy.mode)
                    check(policy.id, rule.setting, "__invalid__", rule.advisory ? .warning : .violation, mode: policy.mode)
                } else if kind == .disabledCommand {
                    check(policy.id, rule.setting, "used", .violation, mode: policy.mode)
                } else if kind == .ownership {
                    require(rules.isServerOwned(rule.setting, rulesetIDs: [policy.id]), "owned rule must guard writes")
                } else if kind == .forbiddenValues {
                    check(policy.id, rule.setting, rule.constraint.forbiddenValues!.first!, .violation, mode: policy.mode)
                }
            }
        }
        print("PASS: ConfigRules (\(rules.rulesets.count) policies, \(count) authored rules, primary boundaries and contradictions)")
    }
}
