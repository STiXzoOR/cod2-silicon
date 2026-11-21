import Foundation

/// Prepares a published config for the catalogue with `ConfigClean` (launcher/ConfigClean.swift), the
/// same cleaning Share Your Setup runs in the launcher: the launcher's own import review reads it for
/// both game versions, and lines it blocks, lines that say who the player is, what they typed, a
/// secret, a path or a setting of another game, comments, the settings for hosting a server and keys
/// that run timed steps are left out. The rest keeps its order and bytes. The summary says what was
/// left out and why.
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

    static func main() async throws {
        guard CommandLine.arguments.count == 4 else { throw CocoaError(.fileReadInvalidFileName) }
        let source = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let catalog = try ConfigCatalog(data: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[2])))
        let result: ConfigClean.Result
        do { result = try await ConfigClean.clean(source, catalog: ConfigImportCatalogAdapter(catalog: catalog)) }
        catch { throw CocoaError(.fileReadCorruptFile) }
        if ProcessInfo.processInfo.environment["CLEAN_EXPLAIN"] == "1" {
            for line in result.leftOut {
                let text = line.reasons.contains("credential") ? "[credential]" : String(decoding: line.text.prefix(70), as: UTF8.self)
                FileHandle.standardError.write(Data((line.reasons.joined(separator: ",") + "\t" + text + "\n").utf8))
            }
        }
        try result.bytes.write(to: URL(fileURLWithPath: CommandLine.arguments[3]))
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        let summary = Summary(kept: result.kept, dropped: result.leftOut.count, comments: result.comments, reasons: result.reasons,
                              settings: result.settings, binds: result.binds)
        FileHandle.standardOutput.write(try encoder.encode(summary) + Data([10]))
    }
}
