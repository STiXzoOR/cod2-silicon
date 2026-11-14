import Foundation

/// Prepares a published config for the catalogue. The launcher's own import review
/// (ConfigImportSafety) reads it for both game versions; lines it blocks, and lines that say who the
/// player is, what they typed, a secret, a path or a setting of another game, are left out. The rest
/// keeps its order and bytes. There is no second grammar here. Comments go too, so a host's banner
/// never reaches the catalogue, and so do the settings for hosting a server. The summary says what
/// was left out and why.
///
///     clean <published.cfg> <ConfigCatalog.json> <cleaned.cfg>      prints a JSON summary
@main struct DirectoryClean {
    struct Summary: Encodable {
        let kept: Int
        let dropped: Int
        let comments: Int
        /// How many dropped lines carried each reason; a line can carry several.
        let reasons: [String: Int]
        /// The lower-cased names of the settings that were kept, for the entry's facts.
        let settings: [String: String]
        let binds: Int
    }
    /// Left out whatever the review's level: who the player is, what they said, secrets, paths, and
    /// anything that is not a setting of this game. What the review only asks about (network values,
    /// binds with scripts, unbindall, screenshots) stays: every import shows it for an OK anyway.
    static let leftOut: Set<ConfigImportRisk> = [.identity, .chat, .credential, .filesystemPath, .remoteConsole, .connect, .quit,
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
    struct Verdict { var range: Range<Int>; var keep = true; var reasons: Set<String> = []; var comment = false; var command: ConfigCommand }

    static func main() async throws {
        guard CommandLine.arguments.count == 4 else { throw CocoaError(.fileReadInvalidFileName) }
        let source = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let catalog = try ConfigCatalog(data: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[2])))
        let adapter = ConfigImportCatalogAdapter(catalog: catalog)
        var verdicts: [Int: Verdict] = [:]
        var bytes = Data()
        var document: ConfigDocument?
        for dialect in [ConfigDialect.stock, .cod2x] {
            let bundle = try await ConfigImportLoader(catalog: adapter).load(.paste(source), dialect: dialect)
            guard let entry = bundle.files.first(where: { $0.path == bundle.entryPath }) else { throw CocoaError(.fileReadCorruptFile) }
            let serialized = entry.document.serialized()
            if bytes.isEmpty { bytes = serialized; document = entry.document }
            guard bytes == serialized else { throw CocoaError(.fileReadCorruptFile) }
            for candidate in ConfigImportSafety(catalog: adapter).review(bundle).candidates
            where candidate.location.file == bundle.entryPath && candidate.location.context.isEmpty {
                let start = candidate.location.range.lowerBound
                var verdict = verdicts[start] ?? Verdict(range: candidate.location.range, command: candidate.command)
                guard verdict.range == candidate.location.range else { throw CocoaError(.fileReadCorruptFile) }
                if candidate.command.tokens.isEmpty { verdict.comment = true; verdict.keep = false }
                // A setup is settings and keys. A loose command (a stray `wait`, half of a macro) is neither.
                switch candidate.command.kind {
                case .setting, .bind, .unbind, .unbindall: break
                default: verdict.keep = false; if !candidate.command.tokens.isEmpty { verdict.reasons.insert("looseCommand") }
                }
                if candidate.classification == .blocked || !candidate.importable || candidate.reasons.contains(where: { leftOut.contains($0) }) { verdict.keep = false }
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
        var output = Data(), reasons: [String: Int] = [:], settings: [String: String] = [:]
        var kept = 0, dropped = 0, comments = 0, binds = 0
        for verdict in verdicts.values.sorted(by: { $0.range.lowerBound < $1.range.lowerBound }) {
            if verdict.comment { comments += 1; continue }
            guard verdict.keep else {
                dropped += 1
                if ProcessInfo.processInfo.environment["CLEAN_EXPLAIN"] == "1" {
                    let line = verdict.reasons.contains("credential") ? "[credential]" : String(decoding: bytes.subdata(in: verdict.range).prefix(70), as: UTF8.self)
                    FileHandle.standardError.write(Data((verdict.reasons.sorted().joined(separator: ",") + "\t" + line + "\n").utf8))
                }
                for reason in verdict.reasons.isEmpty ? ["notImportable"] : verdict.reasons { reasons[reason, default: 0] += 1 }
                continue
            }
            output.append(bytes.subdata(in: verdict.range)); output.append(10)
            kept += 1
            let tokens = verdict.command.tokens
            if verdict.command.kind == .bind { binds += 1 }
            if verdict.command.kind == .setting, tokens.count > 1, let value = document?.effectiveSetting(named: tokens[1].text) {
                settings[tokens[1].text.lowercased()] = value
            }
        }
        try output.write(to: URL(fileURLWithPath: CommandLine.arguments[3]))
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        FileHandle.standardOutput.write(try encoder.encode(Summary(kept: kept, dropped: dropped, comments: comments, reasons: reasons, settings: settings, binds: binds)) + Data([10]))
    }
}
