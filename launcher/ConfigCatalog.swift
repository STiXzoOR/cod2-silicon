import Foundation

/// Immutable registration metadata and authored copy. This does not read a
/// player's config or determine the live rules of a connected server.
enum ConfigCatalogMode: String, Codable, CaseIterable, Sendable {
    case stock, cod2x
}

enum ConfigPurposeGroup: String, Codable, CaseIterable, Sendable {
    case frame, aim, net, vis, hud, audio, gfx, other
}

enum ConfigCatalogAvailability: String, Codable, Sendable {
    case registered, dispatchOnly, referenceOnly, scriptOrMod, unavailable
}

enum ConfigCatalogOrigin: String, Codable, Sendable {
    case stock, cod2x, port
}

enum ConfigCatalogChange: String, Codable, Sendable {
    case unchanged, changed, added
}

enum ConfigCatalogType: String, Codable, Sendable {
    case bool, int, float, string, color, vec2, vec3, vec4
    case enumeration = "enum"
}

indirect enum ConfigCatalogValue: Codable, Hashable, Sendable {
    case null, bool(Bool), number(Double), string(String)
    case array([ConfigCatalogValue])

    init(from decoder: any Decoder) throws {
        let container = try decoder.singleValueContainer()
        if container.decodeNil() { self = .null }
        else if let value = try? container.decode(Bool.self) { self = .bool(value) }
        else if let value = try? container.decode(Double.self), value.isFinite { self = .number(value) }
        else if let value = try? container.decode(String.self) { self = .string(value) }
        else { self = .array(try container.decode([ConfigCatalogValue].self)) }
    }

    func encode(to encoder: any Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .null: try container.encodeNil()
        case .bool(let value): try container.encode(value)
        case .number(let value): try container.encode(value)
        case .string(let value): try container.encode(value)
        case .array(let value): try container.encode(value)
        }
    }
}

struct ConfigCatalogFact: Codable, Hashable, Sendable {
    enum Status: String, Codable, Sendable {
        case literal, dynamic, ambiguous
    }
    let status: Status
    let value: ConfigCatalogValue?
    let expression: String?

    /// A disagreement between sites remains a disagreement. File order does
    /// not establish which registration executes first in the game.
    static func consensus(_ facts: [Self]) -> Self {
        guard let first = facts.first, facts.allSatisfy({ $0 == first }) else {
            return Self(status: .ambiguous, value: nil, expression: nil)
        }
        return first
    }
}

struct ConfigCatalogControl: Codable, Hashable, Sendable {
    enum Kind: String, Codable, Sendable {
        case segmented, slider, menu, colour, text, number, vector
        case toggle = "switch"
    }
    let kind: Kind
    let step: Double?
    let unit: String?
}

struct ConfigCatalogGroup: Codable, Hashable, Sendable {
    let id: ConfigPurposeGroup
    let title: String
}

struct ConfigSettingSite: Codable, Hashable, Sendable {
    let name: String
    let path: String
    let line: Int
    let mechanism: String
    let type: ConfigCatalogType
    let defaultValue: ConfigCatalogFact
    let minimum: ConfigCatalogFact?
    let maximum: ConfigCatalogFact?
    /// Enum defaults are constructor indices. Dynamic display choices have
    /// no invented strings; literal choices retain their source ordering.
    let enumValues: ConfigCatalogFact?
    let flags: Int?
    let flagCandidates: [Int]?
    let flagsExpression: String?

    enum CodingKeys: String, CodingKey {
        case name, path, line, mechanism, type, minimum, maximum, enumValues
        case flags, flagCandidates, flagsExpression
        case defaultValue = "default"
    }

    var protectionKnown: Bool {
        flags != nil || !(flagCandidates ?? []).isEmpty
    }
    var conservativeFlags: Int {
        (flagCandidates ?? []).reduce(flags ?? 0, |)
    }

    var valid: Bool {
        func validFact(_ fact: ConfigCatalogFact, literal: (ConfigCatalogValue) -> Bool) -> Bool {
            switch fact.status {
            case .literal: return fact.value.map(literal) == true
            case .dynamic: return fact.value == nil && !(fact.expression ?? "").isEmpty
            case .ambiguous: return false // Consensus only; not a registration-site fact.
            }
        }
        func number(_ value: ConfigCatalogValue) -> Bool {
            if case .number(let number) = value { return number.isFinite }
            return false
        }
        let validDefault = validFact(defaultValue) { value in
            switch (type, value) {
            case (.bool, .bool), (.string, .string): return true
            case (.int, .number(let n)), (.enumeration, .number(let n)):
                return n.isFinite && n.rounded(.towardZero) == n
            case (.float, .number(let n)): return n.isFinite
            case (.vec2, .array(let values)): return values.count == 2 && values.allSatisfy(number)
            case (.vec3, .array(let values)): return values.count == 3 && values.allSatisfy(number)
            case (.vec4, .array(let values)), (.color, .array(let values)):
                return values.count == 4 && values.allSatisfy(number)
            default: return false
            }
        }
        return validDefault && [minimum, maximum].compactMap { $0 }.allSatisfy {
            validFact($0, literal: number)
        } && enumValues.map {
            validFact($0) { value in
                guard case .array(let values) = value else { return false }
                return !values.isEmpty && values.allSatisfy { if case .string = $0 { return true }; return false }
            }
        } != false
    }
}

struct ConfigSettingVariant: Codable, Hashable, Sendable {
    let sites: [ConfigSettingSite]
    let flags: Int
    let flagNames: [String]
    let flagsDynamic: Bool
    let serverControlled: Bool
    private let editableHint: Bool

    enum CodingKeys: String, CodingKey {
        case sites, flags, flagNames, flagsDynamic, serverControlled
        case editableHint = "editable"
    }

    var type: ConfigCatalogType? {
        guard let first = sites.first?.type, sites.allSatisfy({ $0.type == first }) else { return nil }
        return first
    }
    var defaultValue: ConfigCatalogFact { .consensus(sites.map(\.defaultValue)) }
    var minimum: ConfigCatalogFact? { commonOptional(sites.map(\.minimum)) }
    var maximum: ConfigCatalogFact? { commonOptional(sites.map(\.maximum)) }
    var enumValues: ConfigCatalogFact? { commonOptional(sites.map(\.enumValues)) }

    private func commonOptional(_ facts: [ConfigCatalogFact?]) -> ConfigCatalogFact? {
        guard facts.contains(where: { $0 != nil }) else { return nil }
        guard facts.allSatisfy({ $0 != nil }) else {
            return ConfigCatalogFact(status: .ambiguous, value: nil, expression: nil)
        }
        return .consensus(facts.compactMap { $0 })
    }

    /// Re-derive restrictions from sites as well as the resource hint. A bad
    /// editable hint can never expose a cheat/write-protected/read-only control.
    var editable: Bool {
        editableHint && !sites.isEmpty && type != nil && flags & 0xd0 == 0 &&
            sites.allSatisfy { $0.protectionKnown && $0.conservativeFlags & 0xd0 == 0 }
    }
}

struct ConfigCatalogSetting: Codable, Hashable, Sendable {
    let name: String
    let title: String
    let summary: String
    let details: String
    let group: ConfigPurposeGroup
    let control: ConfigCatalogControl
    let availability: ConfigCatalogAvailability
    let variants: [String: ConfigSettingVariant]
    let origins: [ConfigCatalogOrigin]
    let cod2x: ConfigCatalogChange
    let sources: [String]
    let guided: Bool
    let menuExposed: Bool?
    let effectSources: [String]?
    let runtimeNotes: [String]?

    func variant(for mode: ConfigCatalogMode) -> ConfigSettingVariant? { variants[mode.rawValue] }
    func isEditable(in mode: ConfigCatalogMode) -> Bool {
        availability == .registered && variant(for: mode)?.editable == true
    }
}

struct ConfigCommandSite: Codable, Hashable, Sendable {
    let name: String
    let path: String
    let line: Int
    let mechanism: String
}

struct ConfigCommandVariant: Codable, Hashable, Sendable {
    let sites: [ConfigCommandSite]
}

struct ConfigCatalogCommand: Codable, Hashable, Sendable {
    let name: String
    let title: String
    let summary: String
    let details: String
    let group: ConfigPurposeGroup
    let availability: ConfigCatalogAvailability
    let variants: [String: ConfigCommandVariant]
    let origins: [ConfigCatalogOrigin]
    let cod2x: ConfigCatalogChange
    let sources: [String]
    let bindable: Bool
    let menuLabel: String?
    let effectSources: [String]?

    func variant(for mode: ConfigCatalogMode) -> ConfigCommandVariant? { variants[mode.rawValue] }
}

struct ConfigDynamicRegistration: Codable, Hashable, Sendable {
    let path: String
    let line: Int
    let mechanism: String
    let function: String
    let nameExpression: String
}

enum ConfigCatalogError: Error, Equatable, Sendable {
    case missingResource
    case unsupportedSchema(Int)
    case invalidGroups
    case duplicateName(String)
    case invalidEntry(String)
}

struct ConfigCatalog: Codable, Sendable {
    let schemaVersion: Int
    let sourceRevision: String
    let groups: [ConfigCatalogGroup]
    let settings: [ConfigCatalogSetting]
    let commands: [ConfigCatalogCommand]
    let dynamicRegistrations: [String: [ConfigDynamicRegistration]]
    private let settingsByName: [String: ConfigCatalogSetting]
    private let commandsByName: [String: ConfigCatalogCommand]

    private struct Resource: Codable {
        let schemaVersion: Int
        let sourceRevision: String
        let groups: [ConfigCatalogGroup]
        let settings: [ConfigCatalogSetting]
        let commands: [ConfigCatalogCommand]
        let dynamicRegistrations: [String: [ConfigDynamicRegistration]]
    }

    init(data: Data) throws { self = try JSONDecoder().decode(Self.self, from: data) }

    init(from decoder: any Decoder) throws {
        let resource = try Resource(from: decoder)
        guard resource.schemaVersion == 1 else { throw ConfigCatalogError.unsupportedSchema(resource.schemaVersion) }
        guard resource.groups.count == ConfigPurposeGroup.allCases.count,
              Set(resource.groups.map(\.id)) == Set(ConfigPurposeGroup.allCases) else {
            throw ConfigCatalogError.invalidGroups
        }
        let modes = Set(ConfigCatalogMode.allCases.map(\.rawValue))
        var settingsIndex: [String: ConfigCatalogSetting] = [:]
        var commandsIndex: [String: ConfigCatalogCommand] = [:]
        for entry in resource.settings {
            guard !entry.name.isEmpty, !entry.title.isEmpty, !entry.summary.isEmpty, !entry.details.isEmpty,
                  !entry.sources.isEmpty, Set(entry.variants.keys).isSubset(of: modes),
                  entry.availability == .registered ? !entry.variants.isEmpty : entry.variants.isEmpty,
                  entry.variants.values.allSatisfy({ !$0.sites.isEmpty && $0.sites.allSatisfy {
                      $0.valid && $0.name.lowercased() == entry.name.lowercased() &&
                          !$0.path.isEmpty && $0.line > 0 && !$0.mechanism.isEmpty
                  } }) else {
                throw ConfigCatalogError.invalidEntry(entry.name)
            }
            guard settingsIndex.updateValue(entry, forKey: entry.name.lowercased()) == nil else {
                throw ConfigCatalogError.duplicateName(entry.name)
            }
        }
        for entry in resource.commands {
            let actual = entry.availability == .registered || entry.availability == .dispatchOnly
            guard !entry.name.isEmpty, !entry.title.isEmpty, !entry.summary.isEmpty, !entry.details.isEmpty,
                  !entry.sources.isEmpty, Set(entry.variants.keys).isSubset(of: modes),
                  actual ? !entry.variants.isEmpty : entry.variants.isEmpty,
                  entry.variants.values.allSatisfy({ !$0.sites.isEmpty && $0.sites.allSatisfy {
                      $0.name.lowercased() == entry.name.lowercased() &&
                          !$0.path.isEmpty && $0.line > 0 && !$0.mechanism.isEmpty
                  } }) else {
                throw ConfigCatalogError.invalidEntry(entry.name)
            }
            guard commandsIndex.updateValue(entry, forKey: entry.name.lowercased()) == nil else {
                throw ConfigCatalogError.duplicateName(entry.name)
            }
        }
        schemaVersion = resource.schemaVersion
        sourceRevision = resource.sourceRevision
        groups = resource.groups
        settings = resource.settings
        commands = resource.commands
        dynamicRegistrations = resource.dynamicRegistrations
        settingsByName = settingsIndex
        commandsByName = commandsIndex
    }

    func encode(to encoder: any Encoder) throws {
        try Resource(schemaVersion: schemaVersion, sourceRevision: sourceRevision, groups: groups,
                     settings: settings, commands: commands, dynamicRegistrations: dynamicRegistrations).encode(to: encoder)
    }

    static func load(bundle: Bundle = .main) throws -> Self {
        guard let url = bundle.url(forResource: "ConfigCatalog", withExtension: "json") ??
                bundle.url(forResource: "ConfigCatalog", withExtension: "json", subdirectory: "Resources") else {
            throw ConfigCatalogError.missingResource
        }
        return try Self(data: Data(contentsOf: url))
    }

    func setting(named name: String) -> ConfigCatalogSetting? { settingsByName[name.lowercased()] }
    func command(named name: String) -> ConfigCatalogCommand? { commandsByName[name.lowercased()] }
    func settings(in group: ConfigPurposeGroup) -> [ConfigCatalogSetting] { settings.filter { $0.group == group } }
    func commands(in group: ConfigPurposeGroup) -> [ConfigCatalogCommand] { commands.filter { $0.group == group } }
    func guidedSettings(in group: ConfigPurposeGroup, mode: ConfigCatalogMode) -> [ConfigCatalogSetting] {
        settings.filter { $0.group == group && $0.guided && $0.isEditable(in: mode) }
    }
}
