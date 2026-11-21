import Foundation
import CryptoKit

// Share Your Setup (ShareSetup board, 0.4.1): what is sent, how it is cleaned, the service's replies
// and the record that turns Share into Stop Sharing. No views here; ConfigShareView.swift draws the
// sheets and ConfigShareModel.swift runs them. The contract with the service is
// `POST /api/configs/share` and `POST /api/configs/withdraw` on cod2unbound.com (or `COD2_SHARE_URL`).

enum ConfigShare {
    /// The service takes a config of at least 10 lines, at most 65,535 bytes and 3,000 lines, each
    /// line at most 1,024 bytes.
    static let minimumLines = 10
    static let maximumBytes = 65535
    static let maximumLines = 3000
    static let maximumLineBytes = 1024

    /// What the game loads for a loadout: its `config_mp.cfg`, then the advanced binds' companion
    /// file, which the launch runs with `+exec` straight after.
    static func loadoutText(_ read: ConfigProfileRead) -> Data {
        var bytes = read.version.config
        guard let chain = read.version.chain, !chain.isEmpty else { return bytes }
        if !bytes.isEmpty && bytes.last != 10 && bytes.last != 13 { bytes.append(10) }
        bytes.append(chain)
        return bytes
    }

    /// The game version the loadout was last used with, as the service names it.
    static func game(_ dialect: ConfigDialect) -> String { dialect == .stock ? "stock" : "cod2x" }

    static func sha256(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }

    /// A line that is not sent, with its reasons and the text to show for it.
    struct LeftOutLine: Equatable, Sendable, Identifiable {
        var id: Int
        /// The line as written, or its command and name only when it holds a secret.
        var shown: String
        var reasons: [String]
        /// The reason in plain words, for the See What’s Sent… sheet.
        var because: String { ConfigShareReasons.words(reasons) }
    }

    enum Problem: Equatable, Sendable {
        /// Fewer than ten lines are left once cleaned: the game's own settings, or close to them.
        case nothingToShare
        /// More than the service takes.
        case tooLarge
    }

    /// A loadout cleaned for sharing: exactly what would be sent, and what was left out.
    struct Prepared: Equatable, Sendable {
        var bytes: Data
        var settings: Int
        var binds: Int
        var leftOut: [LeftOutLine]
        /// Every reason any left-out line carried, sorted, for the request's `left.reasons`.
        var reasons: [String]
        var lines: Int
        var digest: String { ConfigShare.sha256(bytes) }
        var text: String { String(decoding: bytes, as: UTF8.self) }
        var problem: Problem? {
            if lines < ConfigShare.minimumLines { return .nothingToShare }
            if bytes.count > ConfigShare.maximumBytes || lines > ConfigShare.maximumLines { return .tooLarge }
            return nil
        }
    }

    /// Cleans a loadout's text with the catalogue's own cleaning (`ConfigClean`), then leaves out any
    /// kept line the service would refuse, so a player is never refused for a line they can't see.
    static func prepare(_ source: Data, catalog: any ConfigImportCatalog) async throws -> Prepared {
        let cleaned = try await ConfigClean.clean(source, catalog: catalog)
        // The review's lines in the order they were written, then any line the service would refuse.
        var left: [(shown: String, reasons: [String])] = cleaned.leftOut.map { line in
            (shown(text: line.text, tokens: line.tokens, reasons: line.reasons), line.reasons)
        }
        var bytes = Data(), settings = 0, binds = 0, lines = 0
        for line in cleaned.bytes.split(separator: 10, omittingEmptySubsequences: false).dropLast() {
            let data = Data(line)
            let tokens = ServiceRules.tokenize(String(decoding: data, as: UTF8.self))
            if let reason = ServiceRules.refusal(data) {
                left.append((shown(text: data, tokens: tokens ?? [], reasons: [reason]), [reason]))
                continue
            }
            bytes.append(data); bytes.append(10); lines += 1
            switch tokens?.first?.lowercased() {
            case "set", "seta", "sets", "setu": settings += 1
            case "bind": binds += 1
            default: break
            }
        }
        let leftOut = left.enumerated().map { LeftOutLine(id: $0.offset, shown: $0.element.shown, reasons: $0.element.reasons) }
        let reasons = Set(left.flatMap(\.reasons)).sorted()
        return Prepared(bytes: bytes, settings: settings, binds: binds, leftOut: leftOut, reasons: reasons, lines: lines)
    }

    /// A left-out line as the preview shows it. A line with a secret shows its command and name only.
    static func shown(text: Data, tokens: [String], reasons: [String]) -> String {
        if reasons.contains("credential") {
            return (tokens.prefix(2) + ["…"]).joined(separator: " ")
        }
        let line = String(decoding: text, as: UTF8.self).replacingOccurrences(of: "\r", with: "").replacingOccurrences(of: "\n", with: " ")
        return line.count > 200 ? String(line.prefix(200)) + "…" : line
    }

    /// The request body (`v` 1).
    struct RequestBody: Codable, Equatable, Sendable {
        struct Left: Codable, Equatable, Sendable { var lines: Int; var reasons: [String] }
        var v = 1
        var name: String
        var config: String
        var game: String
        var app: String
        var left: Left
    }
    static func requestBody(_ prepared: Prepared, name: String, dialect: ConfigDialect, app: String) -> RequestBody {
        RequestBody(name: name, config: prepared.text, game: game(dialect), app: appVersion(app),
                    left: .init(lines: min(prepared.leftOut.count, 5000), reasons: Array(prepared.reasons.prefix(16))))
    }
    /// The service takes `major.minor.patch`; a development build's longer version is cut to that.
    static func appVersion(_ version: String) -> String {
        let parts = version.split(separator: ".").prefix(3).map { String($0.prefix { $0.isNumber }) }
        guard parts.count == 3, parts.allSatisfy({ !$0.isEmpty && $0.count <= 4 }) else { return "0.0.0" }
        return parts.joined(separator: ".")
    }

    /// The rules the service applies to every line (its `checkConfig`), so the launcher never sends a
    /// line it would refuse. Each refusal names the review reason it matches.
    enum ServiceRules {
        static let hostingPrefixes = ["sv_", "g_", "scr_", "ui_", "net_socks", "rcon"]
        static let scriptWords: [String: String] = ["say": "chat", "say_team": "chat", "sayteam": "chat", "connect": "connect",
                                                    "reconnect": "connect", "rcon": "remoteConsole", "wait": "macro", "exec": "unresolvedExec",
                                                    "quit": "quit"]

        /// Why the service would refuse `line`, or nil when it takes it.
        static func refusal(_ line: Data) -> String? {
            var line = line
            if line.last == 13 { line.removeLast() }
            guard line.count <= maximumLineBytes, line.allSatisfy({ $0 == 9 || (32...126).contains($0) }) else { return "notAccepted" }
            let text = String(decoding: line, as: UTF8.self)
            if text.trimmingCharacters(in: .whitespaces).isEmpty { return nil }
            guard let tokens = tokenize(text), let first = tokens.first?.lowercased() else { return "notAccepted" }
            switch first {
            case "set", "seta", "sets", "setu":
                guard tokens.count >= 3, tokens[1].range(of: "^[A-Za-z0-9_]+$", options: .regularExpression) != nil else { return "notAccepted" }
                return setting(tokens[1]) ?? script(tokens.dropFirst(2).joined(separator: " "))
            case "bind":
                guard tokens.count >= 2, key(tokens[1]) else { return "notAccepted" }
                return script(tokens.dropFirst(2).joined(separator: " "))
            case "unbind": return tokens.count == 2 && key(tokens[1]) ? nil : "notAccepted"
            case "unbindall": return tokens.count == 1 ? nil : "notAccepted"
            default: return "notAccepted"
            }
        }
        static func key(_ token: String) -> Bool {
            (1...32).contains(token.utf8.count) && token.utf8.allSatisfy { (33...126).contains($0) }
        }
        /// Settings for hosting, the player's identity and secrets.
        static func setting(_ name: String) -> String? {
            let lower = name.lowercased()
            if hostingPrefixes.contains(where: lower.hasPrefix) { return "hosting" }
            if lower == "name" || lower == "cl_guid" { return "identity" }
            if lower == "password" || lower.contains("password") || lower.contains("cdkey") { return "credential" }
            if lower.split(separator: "_", omittingEmptySubsequences: false).contains(where: { $0 == "key" || $0 == "keys" }) { return "credential" }
            return nil
        }
        /// Text that says something, joins a server, runs a file, waits or quits.
        static func script(_ text: String) -> String? {
            let words = text.lowercased().split(whereSeparator: { !(("a"..."z").contains($0) || ("0"..."9").contains($0) || $0 == "_") }).map(String.init)
            for word in words {
                if let reason = scriptWords[word] { return reason }
                if word.hasPrefix("rcon") { return "remoteConsole" }
                if let reason = setting(word) { return reason }
            }
            return nil
        }
        /// A line split as the game splits it, or nil when it holds `;`, `//` or `/*` outside quotes.
        static func tokenize(_ line: String) -> [String]? {
            let characters = Array(line)
            var tokens: [String] = [], index = 0
            func comment(_ at: Int) -> Bool { characters[at] == "/" && at + 1 < characters.count && (characters[at + 1] == "/" || characters[at + 1] == "*") }
            while index < characters.count {
                let character = characters[index]
                if character == " " || character == "\t" { index += 1; continue }
                if character == "\"" {
                    guard let end = characters[(index + 1)...].firstIndex(of: "\"") else { return nil }
                    tokens.append(String(characters[(index + 1)..<end])); index = end + 1; continue
                }
                if character == ";" || comment(index) { return nil }
                var end = index
                while end < characters.count, !" \t\";".contains(characters[end]) {
                    if comment(end) { break }
                    end += 1
                }
                tokens.append(String(characters[index..<end])); index = end
            }
            return tokens
        }
    }
}

/// The name a setup is credited to (the contract's rules): trimmed, runs of spaces made one, 1 to 32
/// characters, each a letter or digit of any script, a space, or one of `- _ . | [ ] ( ) ! ^ ~ * ' + =`.
enum ConfigShareName {
    static let marks: Set<Character> = ["-", "_", ".", "|", "[", "]", "(", ")", "!", "^", "~", "*", "'", "+", "="]
    static func cleaned(_ name: String) -> String {
        name.precomposedStringWithCanonicalMapping.trimmingCharacters(in: CharacterSet(charactersIn: " "))
            .replacingOccurrences(of: " {2,}", with: " ", options: .regularExpression)
    }
    /// The name to send, or nil when it can't be.
    static func valid(_ name: String) -> String? {
        let name = cleaned(name)
        return problem(name) == nil && !name.isEmpty ? name : nil
    }
    /// What is wrong with a typed name, said under the field; nil for an empty name (Share Setup is
    /// simply off) and for a good one.
    static func problem(_ name: String) -> String? {
        let name = cleaned(name)
        guard !name.isEmpty else { return nil }
        if name.unicodeScalars.count > 32 { return "A name can be up to 32 characters." }
        let allowed = name.unicodeScalars.allSatisfy { scalar in
            scalar == " " || marks.contains(Character(scalar)) || scalar.properties.generalCategory.isLetter || scalar.properties.generalCategory.isNumber
        }
        return allowed ? nil : "A name can hold letters, digits, spaces and - _ . | [ ] ( ) ! ^ ~ * ' + =."
    }
}

private extension Unicode.GeneralCategory {
    var isLetter: Bool {
        switch self { case .uppercaseLetter, .lowercaseLetter, .titlecaseLetter, .modifierLetter, .otherLetter: true; default: false }
    }
    var isNumber: Bool {
        switch self { case .decimalNumber, .letterNumber, .otherNumber: true; default: false }
    }
}

/// The review's reasons in plain words, for the lines See What’s Sent… lists as left out.
enum ConfigShareReasons {
    /// The most telling reason first: what a line says about the player before how it is written.
    static let order = ["identity", "credential", "chat", "connect", "remoteConsole", "filesystemPath", "hosting", "macro", "quit",
                        "unresolvedExec", "unresolvedVariable", "cycle", "negativeWait", "protectedSetting", "inertName",
                        "unknownSetting", "malformed", "limit", "looseCommand", "notAccepted", "notImportable"]
    static func word(_ reason: String) -> String? {
        switch reason {
        case "identity": "Your name or who you are in the game"
        case "credential": "A password, key or login"
        case "chat": "Chat or a message to other players"
        case "connect": "Joins or leaves a server"
        case "remoteConsole": "A command for a server’s admin"
        case "filesystemPath": "A file path on your Mac"
        case "hosting": "A setting for hosting a server"
        case "macro": "A key that runs timed steps"
        case "quit": "Closes the game"
        case "unresolvedExec": "Runs another file"
        case "unresolvedVariable": "Runs text that can’t be read ahead"
        case "cycle": "Runs itself in a loop"
        case "negativeWait": "Can stall the game"
        case "protectedSetting": "A setting the game protects"
        case "inertName": "Does nothing in CoD2"
        case "unknownSetting": "A helper or a setting CoD2 doesn’t have"
        case "malformed": "The game can’t read it reliably"
        case "limit": "Too long for the game"
        case "looseCommand": "Not a setting or a key bind"
        case "notAccepted": "Player Setups takes only settings and key binds"
        case "notImportable": "The import review doesn’t take it"
        default: nil
        }
    }
    static func words(_ reasons: [String]) -> String {
        let known = order.filter(reasons.contains)
        if let first = known.first, let text = word(first) { return text }
        return reasons.compactMap { ConfigImportRisk(rawValue: $0)?.summary }.first ?? "The import review doesn’t take it"
    }
}

// MARK: - The service

/// What a share request ended as, in the sheet's terms.
enum ConfigShareReply: Equatable, Sendable {
    case sent(ConfigShareReceipt)
    /// 409: the same bytes are listed or waiting.
    case already
    /// 429: three today from this network, or the day's total for everyone.
    case limit(everyone: Bool)
    /// 413: more than the service takes.
    case tooLarge
    /// No answer, a 400, a 503 or anything unexpected: nothing was sent, and Try Again is offered.
    case unreachable
    case cancelled
}
struct ConfigShareReceipt: Codable, Equatable, Sendable {
    var request: Int
    var url: URL
    var id: String
    var token: String
}
enum ConfigWithdrawReply: Equatable, Sendable {
    case withdrawn
    /// It was listed: a request to take it off was opened, and it leaves with the next catalogue update.
    case removal(URL)
    /// 403: the service doesn't know this request or this key.
    case forbidden
    case unreachable
    case cancelled
}

struct ConfigShareClient: Sendable {
    static let production = URL(string: "https://cod2unbound.com")!
    static let pulls = "https://github.com/cod2unbound/cod2-configs/pull/"
    var base: URL
    var timeout: TimeInterval = 20

    /// `COD2_SHARE_URL` when it names an http or https address (tests point it at a local stand-in),
    /// else cod2unbound.com.
    static func base(environment: [String: String] = ProcessInfo.processInfo.environment) -> URL {
        if let value = environment["COD2_SHARE_URL"], let url = URL(string: value), ["http", "https"].contains(url.scheme?.lowercased() ?? ""),
           url.host?.isEmpty == false { return url }
        return production
    }
    init(base: URL = ConfigShareClient.base()) { self.base = base }

    func request(path: String, body: Data) -> URLRequest {
        var request = URLRequest(url: base.appendingPathComponent(path), cachePolicy: .reloadIgnoringLocalAndRemoteCacheData, timeoutInterval: timeout)
        request.httpMethod = "POST"
        request.httpBody = body
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.httpShouldHandleCookies = false
        return request
    }
    static func encode<T: Encodable>(_ value: T) -> Data {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return (try? encoder.encode(value)) ?? Data()
    }
    private func post(_ request: URLRequest) async -> (Int, Data)? {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = timeout
        configuration.timeoutIntervalForResource = timeout
        configuration.httpCookieStorage = nil
        configuration.urlCache = nil
        let session = URLSession(configuration: configuration)
        defer { session.finishTasksAndInvalidate() }
        guard let (data, response) = try? await session.data(for: request), let http = response as? HTTPURLResponse else { return nil }
        return (http.statusCode, data)
    }

    func share(_ body: ConfigShare.RequestBody) async -> ConfigShareReply {
        let reply = await post(request(path: "api/configs/share", body: Self.encode(body)))
        if Task.isCancelled { return .cancelled }
        guard let (status, data) = reply else { return .unreachable }
        return Self.shareReply(status: status, body: data)
    }
    func withdraw(request number: Int, token: String) async -> ConfigWithdrawReply {
        struct Body: Encodable { var v = 1; var request: Int; var token: String }
        let reply = await post(request(path: "api/configs/withdraw", body: Self.encode(Body(request: number, token: token))))
        if Task.isCancelled { return .cancelled }
        guard let (status, data) = reply else { return .unreachable }
        return Self.withdrawReply(status: status, body: data)
    }

    /// Every reply in the contract; anything else is `unreachable`, never retried by itself.
    static func shareReply(status: Int, body: Data) -> ConfigShareReply {
        let object = (try? JSONSerialization.jsonObject(with: body)) as? [String: Any]
        switch status {
        case 201:
            guard let object, let request = object["request"] as? Int, request > 0, let address = object["url"] as? String,
                  let url = pullURL(address, number: request), let id = object["id"] as? String, !id.isEmpty,
                  let token = object["token"] as? String, token.range(of: "^[0-9a-f]{64}$", options: .regularExpression) != nil else { return .unreachable }
            return .sent(ConfigShareReceipt(request: request, url: url, id: id, token: token))
        case 409 where object?["error"] as? String == "already": return .already
        case 413: return .tooLarge
        case 429 where object?["error"] as? String == "limit": return .limit(everyone: object?["scope"] as? String == "day")
        default: return .unreachable
        }
    }
    static func withdrawReply(status: Int, body: Data) -> ConfigWithdrawReply {
        let object = (try? JSONSerialization.jsonObject(with: body)) as? [String: Any]
        switch status {
        case 200 where object?["state"] as? String == "withdrawn": return .withdrawn
        case 200 where object?["state"] as? String == "removal":
            guard let address = object?["url"] as? String, let url = pullURL(address, number: nil) else { return .unreachable }
            return .removal(url)
        case 403 where object?["error"] as? String == "forbidden": return .forbidden
        default: return .unreachable
        }
    }
    /// Only the catalogue's own pull requests are ever opened in the browser.
    static func pullURL(_ address: String, number: Int?) -> URL? {
        guard address.hasPrefix(pulls), let tail = Int(address.dropFirst(pulls.count)), tail > 0,
              number == nil || tail == number, let url = URL(string: address) else { return nil }
        return url
    }
}

// MARK: - The record

/// What the launcher remembers about a shared loadout: what turns Share into Stop Sharing and View
/// Request. Kept in the launcher's own folder (`shared-setups.json` beside `launcher-settings.json`),
/// never in the loadout's config. Reset Settings leaves it alone (it is not a setting, and without the
/// token a request can no longer be taken back); moving a loadout to the Trash keeps it, so putting
/// the loadout back keeps its Stop Sharing.
struct ConfigShareRecord: Codable, Equatable, Sendable {
    var request: Int
    var url: URL
    var token: String
    /// The day it was sent, `YYYY-MM-DD` in UTC.
    var sent: String
    /// SHA-256 of the bytes that were sent.
    var sha256: String
    /// The name it was sent under.
    var name: String
}
struct ConfigShareRecordFile: Codable, Equatable, Sendable {
    var version = 1
    /// The last name typed, offered next time.
    var name: String?
    /// By lower-cased loadout identifier.
    var loadouts: [String: ConfigShareRecord] = [:]

    static func url(home: URL) -> URL { home.appendingPathComponent("shared-setups.json") }
    static func load(home: URL) -> ConfigShareRecordFile {
        guard let data = try? Data(contentsOf: url(home: home)), let file = try? JSONDecoder().decode(Self.self, from: data), file.version == 1 else { return .init() }
        return file
    }
    /// Written whole to a temporary file, synced, then renamed over the old one.
    func save(home: URL) throws {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys, .prettyPrinted, .withoutEscapingSlashes]
        try PrivateFile.write(encoder.encode(self), to: Self.url(home: home))
    }
    func record(_ loadout: String) -> ConfigShareRecord? { loadouts[loadout.lowercased()] }
    mutating func remember(_ record: ConfigShareRecord, loadout: String) { loadouts[loadout.lowercased()] = record; name = record.name }
    mutating func forget(_ loadout: String) { loadouts[loadout.lowercased()] = nil }

    static func day(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX"); formatter.timeZone = TimeZone(identifier: "UTC"); formatter.dateFormat = "yyyy-MM-dd"
        return formatter.string(from: date)
    }
}

// MARK: - Credit in Player Setups

enum ConfigShareCredit {
    /// A setup a player sent from the app: self-published by “CoD2 Unbound”, from a catalogue pull request.
    static func isShared(_ entry: ConfigDirectoryEntry) -> Bool {
        entry.provenance.attribution == .selfPublished && entry.provenance.publishedBy == "CoD2 Unbound"
            && entry.sourceURL.absoluteString.hasPrefix(ConfigShareClient.pulls)
    }
    /// “Shared by its player from CoD2 Unbound on 21 November 2026”, with the entry's own date.
    static func line(_ entry: ConfigDirectoryEntry) -> String? {
        guard isShared(entry) else { return nil }
        return "Shared by its player from CoD2 Unbound on \(longDay(entry.provenance.retrievedAt))"
    }
    static func longDay(_ day: String) -> String {
        let input = DateFormatter()
        input.locale = Locale(identifier: "en_US_POSIX"); input.timeZone = TimeZone(identifier: "UTC"); input.dateFormat = "yyyy-MM-dd"
        guard let date = input.date(from: day) else { return day }
        let output = DateFormatter()
        output.locale = Locale(identifier: "en_GB"); output.timeZone = TimeZone(identifier: "UTC"); output.dateFormat = "d MMMM yyyy"
        return output.string(from: date)
    }
}
