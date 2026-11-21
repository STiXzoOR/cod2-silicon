import Foundation

/// Prepares a config for other players: the catalogue tool (`tools/config-directory/Clean.swift`)
/// cleans a published config with it, and Share Your Setup cleans a loadout with it, so both leave out
/// exactly the same lines. The launcher's own import review (ConfigImportSafety) reads the text for
/// both game versions; lines it blocks, and lines that say who the player is, what they typed, a
/// secret, a path or a setting of another game, are left out. The rest keeps its order and bytes.
/// There is no second grammar here. Comments go too, so a host's banner never reaches the
/// catalogue, and so do the settings for hosting a server and keys that run timed steps.
/// Foundation only: the catalogue tool compiles it with the review's sources and nothing else.
enum ConfigClean {
    /// A line that was left out, with the review's reasons (`ConfigImportRisk` raw values, plus
    /// `hosting`, `macro`, `looseCommand` and `notImportable`).
    struct LeftOut: Equatable, Sendable {
        /// Where the line was in the text that was cleaned.
        var range: Range<Int>
        /// The line's bytes as written.
        var text: Data
        var reasons: [String]
        /// The command's tokens, so a caller can show a line without its secret.
        var tokens: [String]
    }
    struct Result: Equatable, Sendable {
        /// The kept lines, each ending in a newline.
        var bytes: Data
        /// Kept setting lines (`set`, `seta`, `sets`, `setu`).
        var settingLines: Int
        /// Kept `bind` lines.
        var binds: Int
        /// Kept lines of every kind.
        var kept: Int
        var comments: Int
        var leftOut: [LeftOut]
        /// The lower-cased names of the settings that were kept, with their effective values.
        var settings: [String: String]
        /// How many left-out lines carried each reason; a line can carry several.
        var reasons: [String: Int] {
            var counts: [String: Int] = [:]
            for line in leftOut { for reason in line.reasons { counts[reason, default: 0] += 1 } }
            return counts
        }
    }
    enum Failure: Error, Equatable { case unreadable, differentReadings }

    /// Left out whatever the review's level: who the player is, what they said, secrets, paths, and
    /// anything that is not a setting of this game. What the review only asks about (network values,
    /// binds with scripts, unbindall, screenshots) stays: every import shows it for an OK anyway.
    static let leftOutRisks: Set<ConfigImportRisk> = [.identity, .chat, .credential, .filesystemPath, .remoteConsole, .connect, .quit,
                                                      .unknownSetting, .inertName, .protectedSetting, .malformed, .limit, .cycle,
                                                      .negativeWait, .unresolvedExec, .unresolvedVariable]
    /// Settings for hosting a server (its name, team names, game type, download address, proxy) are
    /// not part of how someone plays, and they are free text that a page's visitors wrote.
    static func hosting(_ name: String) -> Bool {
        let name = name.lowercased()
        return ["sv_", "g_", "scr_", "ui_", "net_socks", "rcon"].contains { name.hasPrefix($0) }
    }
    // A key that runs timed steps (`wait`) is a macro: rapid fire, a jump sequence, a quick switch.
    // The catalogue holds how people set the game up, so those stay with their source page.
    private struct Verdict { var range: Range<Int>; var keep = true; var reasons: Set<String> = []; var comment = false; var command: ConfigCommand }

    static func clean(_ source: Data, catalog: any ConfigImportCatalog) async throws -> Result {
        var verdicts: [Int: Verdict] = [:]
        var bytes = Data()
        var document: ConfigDocument?
        for dialect in [ConfigDialect.stock, .cod2x] {
            let bundle = try await ConfigImportLoader(catalog: catalog).load(.paste(source), dialect: dialect)
            guard let entry = bundle.files.first(where: { $0.path == bundle.entryPath }) else { throw Failure.unreadable }
            let serialized = entry.document.serialized()
            if bytes.isEmpty { bytes = serialized; document = entry.document }
            guard bytes == serialized else { throw Failure.differentReadings }
            for candidate in ConfigImportSafety(catalog: catalog).review(bundle).candidates
            where candidate.location.file == bundle.entryPath && candidate.location.context.isEmpty {
                let start = candidate.location.range.lowerBound
                var verdict = verdicts[start] ?? Verdict(range: candidate.location.range, command: candidate.command)
                guard verdict.range == candidate.location.range else { throw Failure.differentReadings }
                if candidate.command.tokens.isEmpty { verdict.comment = true; verdict.keep = false }
                // A setup is settings and keys. A loose command (a stray `wait`, half of a macro) is neither.
                switch candidate.command.kind {
                case .setting, .bind, .unbind, .unbindall: break
                default: verdict.keep = false; if !candidate.command.tokens.isEmpty { verdict.reasons.insert("looseCommand") }
                }
                if candidate.classification == .blocked || !candidate.importable || candidate.reasons.contains(where: { leftOutRisks.contains($0) }) { verdict.keep = false }
                verdict.reasons.formUnion(candidate.reasons.map(\.rawValue))
                if candidate.command.kind == .setting, candidate.command.tokens.count > 1, hosting(candidate.command.tokens[1].text) {
                    verdict.keep = false; verdict.reasons.insert("hosting")
                }
                if candidate.command.kind == .bind, let script = candidate.command.tokens.last?.text.lowercased() {
                    let words = script.split(whereSeparator: { !($0.isLetter || $0.isNumber || $0 == "_") }).map(String.init)
                    if words.contains(where: hosting) { verdict.keep = false; verdict.reasons.insert("hosting") }
                    if words.contains("wait") { verdict.keep = false; verdict.reasons.insert("macro") }
                }
                verdicts[start] = verdict
            }
        }
        var result = Result(bytes: Data(), settingLines: 0, binds: 0, kept: 0, comments: 0, leftOut: [], settings: [:])
        for verdict in verdicts.values.sorted(by: { $0.range.lowerBound < $1.range.lowerBound }) {
            if verdict.comment { result.comments += 1; continue }
            let tokens = verdict.command.tokens
            guard verdict.keep else {
                result.leftOut.append(LeftOut(range: verdict.range, text: bytes.subdata(in: verdict.range),
                                              reasons: verdict.reasons.isEmpty ? ["notImportable"] : verdict.reasons.sorted(),
                                              tokens: tokens.map(\.text)))
                continue
            }
            result.bytes.append(bytes.subdata(in: verdict.range)); result.bytes.append(10)
            result.kept += 1
            if verdict.command.kind == .bind { result.binds += 1 }
            if verdict.command.kind == .setting { result.settingLines += 1 }
            if verdict.command.kind == .setting, tokens.count > 1, let value = document?.effectiveSetting(named: tokens[1].text) {
                result.settings[tokens[1].text.lowercased()] = value
            }
        }
        return result
    }
}
