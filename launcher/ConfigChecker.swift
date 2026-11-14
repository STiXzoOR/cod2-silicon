import Foundation

// Check a Config: what each line of a config will do in one game version. It reads the text with the
// same parser, catalogue, import review and rulesets as the rest of Configs, and changes nothing: a
// fix edits a copy held in memory, never the file it came from.

struct ConfigCheckFinding: Identifiable, Equatable, Sendable {
    /// Won't work; works, but not as written; worth knowing. Fix All covers the first two.
    enum Kind: Int, CaseIterable, Sendable { case broken, different, note }
    enum Fix: Equatable, Sendable { case remove, replace(String) }
    /// The byte offset of the command, which is unique in one config.
    let id: Int
    let range: Range<Int>
    let line: Int
    let kind: Kind
    let title: String
    let detail: String
    /// A few words for the margin beside the line.
    let note: String
    let fix: Fix?
    let fixLabel: String?
}

struct ConfigCheckLine: Identifiable, Equatable, Sendable {
    /// The line's number, from 1.
    let id: Int
    let range: Range<Int>
    /// Safe to show: a password is replaced by dots.
    let text: String
}

/// One line as the page shows it after some fixes were applied.
struct ConfigCheckShownLine: Identifiable, Equatable, Sendable {
    enum Mark: Equatable, Sendable { case finding(ConfigCheckFinding.Kind), removed, changed }
    let id: Int
    let text: String
    let mark: Mark?
    let note: String
}

struct ConfigCheckReport: Sendable {
    let bytes: Data
    let lines: [ConfigCheckLine]
    let findings: [ConfigCheckFinding]
    /// Settings and binds, without comments and blank lines.
    let commands: Int

    func findings(_ kind: ConfigCheckFinding.Kind) -> [ConfigCheckFinding] { findings.filter { $0.kind == kind } }
    /// What Fix All applies: every fault that has a fix. Notes are left for the person.
    var fixable: [ConfigCheckFinding] { findings.filter { $0.kind != .note && $0.fix != nil } }

    /// One line's bytes after these fixes, which all belong to it.
    private func edited(_ line: ConfigCheckLine, _ fixes: [ConfigCheckFinding]) -> [UInt8] {
        var content = Array(bytes[line.range])
        for finding in fixes.sorted(by: { $0.range.lowerBound > $1.range.lowerBound }) {
            let local = (finding.range.lowerBound - line.range.lowerBound)..<(finding.range.upperBound - line.range.lowerBound)
            guard local.lowerBound >= 0, local.upperBound <= content.count else { continue }
            if case .replace(let text) = finding.fix { content.replaceSubrange(local, with: Array((try? ConfigDocument.encode(text)) ?? Data())) }
            else { content.removeSubrange(local) }
        }
        return content
    }
    private static func blank(_ content: [UInt8]) -> Bool { content.allSatisfy { [9, 13, 32, 59].contains($0) } }

    /// The config with these findings' fixes applied. A line left with nothing on it goes, with its
    /// line break.
    func applying(_ ids: Set<Int>) -> Data {
        let fixes = Dictionary(grouping: findings.filter { ids.contains($0.id) && $0.fix != nil }, by: \.line)
        guard !fixes.isEmpty else { return bytes }
        var output = Data()
        for line in lines {
            let end = min(line.range.upperBound + 1, bytes.count)
            guard let here = fixes[line.id] else { output.append(bytes.subdata(in: line.range.lowerBound..<end)); continue }
            let content = edited(line, here)
            if Self.blank(content) { continue }
            output.append(contentsOf: content); output.append(bytes.subdata(in: line.range.upperBound..<end))
        }
        return output
    }

    /// Every original line, with what the applied fixes did to it.
    func shown(applied: Set<Int>) -> [ConfigCheckShownLine] {
        let byLine = Dictionary(grouping: findings, by: \.line)
        return lines.map { line in
            let here = byLine[line.id] ?? []
            let fixed = here.filter { applied.contains($0.id) && $0.fix != nil }
            guard !fixed.isEmpty else {
                let first = here.min { $0.kind.rawValue < $1.kind.rawValue }
                return ConfigCheckShownLine(id: line.id, text: line.text, mark: first.map { .finding($0.kind) }, note: first?.note ?? "")
            }
            let content = edited(line, fixed)
            if Self.blank(content) { return ConfigCheckShownLine(id: line.id, text: line.text, mark: .removed, note: "Removed") }
            let label = fixed.count == 1 ? fixed[0].fixLabel.map { $0.hasPrefix("Set to ") ? "Changed to " + $0.dropFirst(7) : $0 == "Rename" ? "Renamed" : "Changed" } : nil
            // A line that still holds a password after a fix beside it keeps its dots.
            let text = here.contains { $0.title.contains("password") && !applied.contains($0.id) } ? line.text : ConfigChecker.display(Data(content))
            return ConfigCheckShownLine(id: line.id, text: text, mark: .changed, note: label ?? "Changed")
        }
    }
}

enum ConfigChecker {
    /// Reads `data` for one game version and, when given, one set of server rules.
    static func check(_ data: Data, dialect: ConfigDialect, ruleset: String?, catalog: ConfigCatalog, rules: ConfigRules?) async throws -> ConfigCheckReport {
        let adapter = ConfigImportCatalogAdapter(catalog: catalog)
        let bundle = try await ConfigImportLoader(catalog: adapter).load(.paste(data), dialect: dialect)
        guard let file = bundle.files.first(where: { $0.path == bundle.entryPath }) else { throw ConfigImportError.missingEntry }
        let bytes = file.document.serialized()
        let mode: ConfigCatalogMode = dialect == .cod2x ? .cod2x : .stock
        let game = dialect == .cod2x ? "CoD2x" : "the original 1.3"

        // Lines, split as the game splits them.
        var ranges: [Range<Int>] = [], start = 0
        for (offset, byte) in bytes.enumerated() where byte == 10 { ranges.append(start..<offset); start = offset + 1 }
        if start < bytes.count { ranges.append(start..<bytes.count) }
        func line(of offset: Int) -> Int {
            var low = 0, high = ranges.count - 1
            while low < high { let middle = (low + high + 1) / 2; if ranges[middle].lowerBound <= offset { low = middle } else { high = middle - 1 } }
            return low + 1
        }

        let candidates = ConfigImportSafety(catalog: adapter).review(bundle).candidates
            .filter { $0.location.file == bundle.entryPath && $0.location.context.isEmpty && !$0.command.tokens.isEmpty }
        // The last direct assignment of each setting wins; earlier ones are repeats.
        var last: [String: (offset: Int, line: Int)] = [:]
        for candidate in candidates { if let name = assignment(candidate.command)?.name.lowercased() {
            last[name] = (candidate.location.range.lowerBound, line(of: candidate.location.range.lowerBound))
        } }
        let policy = ruleset.flatMap { id in rules?.ruleset(id: id).map { (id: id, title: $0.title, rules: rules?.resolvedRules(id: id) ?? []) } }

        var findings: [ConfigCheckFinding] = [], secret: [Int: String] = [:]
        for candidate in candidates {
            let range = candidate.location.range, number = line(of: range.lowerBound)
            let reasons = Set(candidate.reasons)
            if reasons.contains(.credential) {
                // The line with its secret replaced by dots: the command and what it is set on stay readable.
                secret[number] = candidate.command.tokens.prefix(2).map(\.text).joined(separator: " ") + " \"••••••••\""
            }
            func add(_ kind: ConfigCheckFinding.Kind, _ title: String, _ detail: String, _ note: String, fix: ConfigCheckFinding.Fix? = .remove, label: String? = "Remove") {
                findings.append(.init(id: range.lowerBound, range: range, line: number, kind: kind, title: title, detail: detail, note: note, fix: fix, fixLabel: fix == nil ? nil : label))
            }
            let set = assignment(candidate.command)
            let verb = candidate.command.tokens[0].text
            func rewritten(_ name: String, _ value: String) -> ConfigCheckFinding.Fix { .replace("\(verb) \(name) \"\(value)\"") }

            if reasons.contains(.malformed) || candidate.command.tokenizationFailure != nil {
                add(.broken, "The game can’t read this line", "A quote is left open or the line is too long, so the game would read something else than what is written.", "Can’t be read")
            } else if let set, reasons.contains(.unknownSetting) || reasons.contains(.inertName) || catalog.setting(named: set.name)?.variant(for: mode) == nil {
                if let known = catalog.setting(named: set.name), known.variant(for: mode) == nil, !known.variants.isEmpty {
                    add(.broken, "“\(set.name)” isn’t a setting of \(game)", "Only \(dialect == .cod2x ? "the original 1.3" : "CoD2x") has it. This line does nothing here.", "Not in \(dialect == .cod2x ? "CoD2x" : "1.3")")
                } else if let near = nearest(set.name, in: catalog, mode: mode) {
                    add(.broken, "Unknown setting “\(set.name)”", "Probably \(near), which is a setting of this game. As written, the line does nothing.", "Unknown setting. Did you mean \(near)?",
                        fix: rewritten(near, set.value), label: "Rename")
                } else {
                    add(.broken, "“\(set.name)” isn’t a Call of Duty 2 setting", "It may belong to another game or to a mod. Without that, the line does nothing.", "Not a CoD2 setting")
                }
            } else if let set, let winner = last[set.name.lowercased()], winner.offset != range.lowerBound {
                add(.different, "\(set.name) is set twice", "Line \(winner.line) sets it again and the later line wins, so this one changes nothing.", "Set again on line \(winner.line)")
            } else if reasons.contains(.credential) {
                add(.note, "A password is stored on this line", "Anyone you share this file with would see it.", "Contains a password")
            } else if let set, let setting = catalog.setting(named: set.name), let variant = setting.variant(for: mode) {
                let flags = Set(variant.flagNames)
                let number = ConfigRules.number(set.value)
                let minimum = literal(variant.minimum), maximum = literal(variant.maximum)
                if !flags.contains("archive"), !flags.isDisjoint(with: ["readOnly", "writeProtected"]) {
                    add(.different, "The game sets \(set.name) itself", "It is read-only, so the game ignores this line.", "Read-only")
                } else if flags.contains("cheatProtected"), let standard = standard(variant.defaultValue), !ConfigRules.valuesEqual(standard, set.value) {
                    add(.different, "\(set.name) only changes with cheats on", "On a server with cheats off the game puts it back to \(standard).", "Only works with cheats on")
                } else if let number, let minimum, number < minimum {
                    add(.different, "\(set.name) is below the limit", limits(set.name, minimum, maximum), "Below the limit of \(text(minimum))",
                        fix: rewritten(set.name, text(minimum)), label: "Set to \(text(minimum))")
                } else if let number, let maximum, number > maximum {
                    add(.different, "\(set.name) is above the limit", limits(set.name, minimum, maximum), "Above the limit of \(text(maximum))",
                        fix: rewritten(set.name, text(maximum)), label: "Set to \(text(maximum))")
                } else if let policy, let rule = policy.rules.first(where: { $0.setting.lowercased() == set.name.lowercased() && !$0.advisory && $0.constraint.kind != .ownership }),
                          rules?.evaluate(setting: set.name, value: set.value, rulesetIDs: [policy.id], mode: mode).status == .violation {
                    let constraint = rule.constraint
                    switch constraint.kind {
                    case .range:
                        let low = constraint.minimum, high = constraint.maximum
                        var target: Double? = number
                        if let value = target, let low, value < low { target = low }
                        if let value = target, let high, value > high { target = high }
                        let span = span(low, high, both: { "between \($0) and \($1)" }, least: { "at \($0) or more" }, most: { "at \($0) or less" })
                        add(.note, "\(short(policy.title)) limits \(set.name)", "Those rules keep it \(span). Yours is \(set.value).", "\(short(policy.title)) keeps this \(span)",
                            fix: target.map { rewritten(set.name, text($0)) }, label: target.map { "Set to \(text($0))" })
                    case .exact:
                        add(.note, "\(short(policy.title)) sets \(set.name)", "Those rules need \(constraint.value ?? "their own value"). Yours is \(set.value).", "\(short(policy.title)) needs \(constraint.value ?? "its own value")",
                            fix: constraint.value.map { rewritten(set.name, $0) }, label: constraint.value.map { "Set to \($0)" })
                    default:
                        add(.note, "\(short(policy.title)) doesn’t allow this value", "Yours is \(set.value)." + (constraint.resetValue.map { " A server with those rules uses \($0)." } ?? ""),
                            "\(short(policy.title)) doesn’t allow \(set.value)", fix: constraint.resetValue.map { rewritten(set.name, $0) }, label: constraint.resetValue.map { "Set to \($0)" })
                    }
                }
            }
        }
        let lines = ranges.enumerated().map { index, range in
            ConfigCheckLine(id: index + 1, range: range, text: secret[index + 1] ?? display(bytes.subdata(in: range)))
        }
        let commands = candidates.filter { [.setting, .bind].contains($0.command.kind) }.count
        return ConfigCheckReport(bytes: bytes, lines: lines, findings: findings.sorted { $0.range.lowerBound < $1.range.lowerBound }, commands: commands)
    }

    static func display(_ data: Data) -> String {
        var text = ConfigDocument.text(data)
        if text.hasSuffix("\r") { text.removeLast() }
        return text.replacingOccurrences(of: "\t", with: "    ")
    }
    /// `seta name value…`: the name and the value as the game joins it.
    static func assignment(_ command: ConfigCommand) -> (name: String, value: String)? {
        guard command.kind == .setting, command.tokens.count > 2 else { return nil }
        return (command.tokens[1].text, command.tokens.dropFirst(2).map(\.text).joined(separator: " "))
    }
    static func text(_ number: Double) -> String {
        number == number.rounded() && abs(number) < 1e15 ? String(Int64(number)) : String(format: "%g", number)
    }
    private static func short(_ title: String) -> String { title.components(separatedBy: " / ").last ?? title }
    private static func literal(_ fact: ConfigCatalogFact?) -> Double? {
        guard let fact, fact.status == .literal, case .number(let number)? = fact.value else { return nil }
        return number
    }
    private static func standard(_ fact: ConfigCatalogFact) -> String? {
        guard fact.status == .literal, let value = fact.value else { return nil }
        switch value {
        case .number(let number): return text(number)
        case .bool(let on): return on ? "1" : "0"
        case .string(let string): return string
        default: return nil
        }
    }
    /// A range in words, whichever of its two ends is known.
    private static func span(_ low: Double?, _ high: Double?, both: (String, String) -> String, least: (String) -> String, most: (String) -> String) -> String {
        if let low, let high { return both(text(low), text(high)) }
        if let low { return least(text(low)) }
        if let high { return most(text(high)) }
        return "in its own range"
    }
    private static func limits(_ name: String, _ minimum: Double?, _ maximum: Double?) -> String {
        let span = span(minimum, maximum, both: { "\($0) to \($1)" }, least: { "\($0) or more" }, most: { "\($0) or less" })
        return "\(name) takes \(span). The game refuses a value outside that and keeps the one it had."
    }
    /// The one catalogue name a typo most likely meant: a single closest name within two edits.
    static func nearest(_ name: String, in catalog: ConfigCatalog, mode: ConfigCatalogMode) -> String? {
        let typed = Array(name.lowercased().utf8)
        guard typed.count >= 4 else { return nil }
        let allowed = typed.count < 7 ? 1 : 2
        var best: [String] = [], bestDistance = allowed + 1
        for setting in catalog.settings where setting.variant(for: mode) != nil {
            let other = Array(setting.name.lowercased().utf8)
            guard abs(other.count - typed.count) <= allowed else { continue }
            let distance = edits(typed, other, limit: allowed)
            if distance < bestDistance { best = [setting.name]; bestDistance = distance } else if distance == bestDistance { best.append(setting.name) }
        }
        return best.count == 1 && bestDistance >= 1 ? best[0] : nil
    }
    /// Edit distance with adjacent swaps, cut off once it passes `limit`.
    private static func edits(_ a: [UInt8], _ b: [UInt8], limit: Int) -> Int {
        var previous2 = [Int](repeating: 0, count: b.count + 1), previous = Array(0...b.count), current = previous
        for i in 1...a.count {
            current[0] = i
            var smallest = current[0]
            for j in 1...b.count {
                let cost = a[i - 1] == b[j - 1] ? 0 : 1
                current[j] = min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost)
                if i > 1, j > 1, a[i - 1] == b[j - 2], a[i - 2] == b[j - 1] { current[j] = min(current[j], previous2[j - 2] + 1) }
                smallest = min(smallest, current[j])
            }
            if smallest > limit { return limit + 1 }
            previous2 = previous; previous = current
        }
        return previous[b.count]
    }
}
