import Foundation

/// F4 registration flags are conservative across every possible site. Unknown
/// protection flags cannot be treated as evidence of a safe archived setting.
struct ConfigImportCatalogAdapter: ConfigImportCatalog {
    let catalog: ConfigCatalog
    func setting(named name: String, dialect: ConfigDialect) -> ConfigImportSetting? {
        guard let entry = catalog.setting(named: name),
              let variant = entry.variant(for: dialect == .stock ? .stock : .cod2x) else { return nil }
        let flags = variant.sites.reduce(variant.flags) { $0 | $1.conservativeFlags }
        return .init(archived: flags & 1 != 0 && variant.sites.allSatisfy { $0.protectionKnown && $0.conservativeFlags & 1 != 0 },
                     cheatProtected: flags & 128 != 0, readOnly: flags & 64 != 0,
                     writeProtected: flags & 16 != 0 || !variant.sites.allSatisfy(\.protectionKnown))
    }
    func command(named name: String, dialect: ConfigDialect) -> ConfigImportCommandInfo? {
        guard let entry = catalog.command(named: name),
              entry.variant(for: dialect == .stock ? .stock : .cod2x) != nil else { return nil }
        return .init(bindable: entry.bindable)
    }
}
