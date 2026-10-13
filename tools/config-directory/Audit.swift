import Foundation

/// The standalone repository's CI compiles these exact production models from
/// a reviewed, pinned engine checkout. No alternate Python config grammar.
@main struct DirectoryAudit {
    struct Counts: Encodable {
        let safe: Int
        let needsOK: Int
        let blocked: Int
        let importable: Int
        let commands: Int
    }
    static func main() async throws {
        guard CommandLine.arguments.count == 3 else {
            throw CocoaError(.fileReadInvalidFileName)
        }
        let file = URL(fileURLWithPath: CommandLine.arguments[1])
        let catalog = try ConfigCatalog(data: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[2])))
        let adapter = ConfigImportCatalogAdapter(catalog: catalog)
        var result: [String: Counts] = [:]
        for dialect in [ConfigDialect.stock, .cod2x] {
            let data = try Data(contentsOf: file)
            let bundle = try await ConfigImportLoader(catalog: adapter).load(.paste(data), dialect: dialect)
            let candidates = ConfigImportSafety(catalog: adapter).review(bundle).candidates
            result[dialect.rawValue] = .init(safe: candidates.filter { $0.classification == .safe }.count,
                needsOK: candidates.filter { $0.classification == .needsOK }.count,
                blocked: candidates.filter { $0.classification == .blocked }.count,
                importable: candidates.filter(\.importable).count, commands: candidates.count)
        }
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        FileHandle.standardOutput.write(try encoder.encode(result) + Data([10]))
    }
}
