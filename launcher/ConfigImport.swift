import Foundation

struct ConfigImportSource: Sendable {
    enum Kind: String, Sendable { case file, folder, paste, drop, https, github, gist, localhost }
    let kind: Kind
    var originalURL: URL? = nil
    var rootURL: URL? = nil
    var fetchedURL: URL? = nil
}

struct ConfigImportFile: Sendable {
    let path: String
    let document: ConfigDocument
    init(path: String, data: Data, dialect: ConfigDialect) {
        self.path = path
        self.document = ConfigDocument(data: data, dialect: dialect)
    }
}

struct ConfigImportBundle: Sendable {
    let source: ConfigImportSource
    let entryPath: String
    let files: [ConfigImportFile]
    var unresolved: [ConfigImportUnresolved] = []
}

/// Joining/bind truncation comes from F1's effective lookup on this one command,
/// so imports never maintain a separate tokenizer or argument-joining parser.
enum ConfigImportScript {
    struct Assignment { let name: String; let value: Data }
    static func assignment(_ command: ConfigCommand, document: ConfigDocument, catalog: any ConfigImportCatalog,
                           knownNames: Set<String> = []) -> Assignment? {
        let tokens = command.tokens
        guard !tokens.isEmpty else { return nil }
        let explicit = command.kind == .setting
        guard tokens.count >= (explicit ? 3 : 2) else { return nil }
        let name = tokens[explicit ? 1 : 0].text.lowercased()
        guard explicit || (command.kind == .unknown && catalog.command(named: name, dialect: document.dialect) == nil
                           && !reservedCommands.contains(name)
                           && (catalog.setting(named: name, dialect: document.dialect) != nil || knownNames.contains(name))) else { return nil }
        let single = ConfigDocument(data: document.serialized().subdata(in: command.range), dialect: document.dialect)
        guard let value = single.effectiveSettingBytes(named: name) else { return nil }
        return .init(name: name, value: value)
    }
    static let reservedCommands: Set<String> = ["toggle", "togglep", "reset", "setfromdvar", "increase", "decrease", "wait",
        "connect", "reconnect", "disconnect", "quit", "rcon", "openscriptmenu", "match", "mr", "say", "say_team", "tell",
        "cmd", "callvote", "vote", "voicechat", "voiceteamchat", "mp_quickmessage", "writeconfig", "writedefaults",
        "condump", "record", "screenshot", "screenshotjpeg"]
    static func binding(_ command: ConfigCommand, document: ConfigDocument) -> Data? {
        guard command.tokens.count >= 3 else { return nil }
        return ConfigDocument(data: document.serialized().subdata(in: command.range), dialect: document.dialect)
            .effectiveBindBytes(key: command.tokens[1].text)
    }
    static func bindingWasTruncated(_ command: ConfigCommand) -> Bool {
        let arguments = command.tokens.dropFirst(2)
        if arguments.count == 1 { return arguments.first!.value.count > 1023 }
        // F1's multi-argument bind join adds spaces and quotes arguments that
        // contain a space. Only measure it here; F1 still owns the stored bytes.
        return arguments.reduce(0) { $0 + $1.value.count + ($1.value.contains(32) ? 2 : 0) }
            + max(0, arguments.count - 1) > 1023
    }
    static func looksExecutable(_ data: Data, dialect: ConfigDialect) -> Bool {
        let document = ConfigDocument(data: data, dialect: dialect)
        if document.commands.filter({ !$0.tokens.isEmpty }).count > 1 { return true }
        guard let command = document.commands.first(where: { !$0.tokens.isEmpty }),
              let name = command.tokens.first?.text.lowercased() else { return false }
        return name.hasPrefix("+") || name.hasPrefix("-") || [.setting, .bind, .exec, .vstr, .unbind, .unbindall].contains(command.kind)
            || ["connect", "reconnect", "quit", "rcon", "openscriptmenu", "match", "mr", "say", "say_team", "wait", "toggle", "togglep", "setfromdvar", "writeconfig"].contains(name)
    }
    static func execPath(_ value: Data) -> String? {
        // Refuse ambiguous/traversing names instead of following the engine's
        // broader search path. Its filename buffer keeps only 63 bytes.
        guard value.count <= 63 else { return nil }
        var name = ConfigDocument.text(value).replacingOccurrences(of: "\\", with: "/")
        guard !name.isEmpty, !name.hasPrefix("/"), !name.contains(".."), !name.contains(":"),
              !name.contains("%"), !name.contains("?"), !name.contains("#"),
              name.unicodeScalars.allSatisfy({ $0.value >= 32 && $0.value < 127 }) else { return nil }
        while name.contains("//") { name = name.replacingOccurrences(of: "//", with: "/") }
        guard !name.hasSuffix("/"), !name.split(separator: "/").contains(".") else { return nil }
        if !name.split(separator: "/").last!.contains(".") { name += ".cfg" }
        guard name.utf8.count <= 63 else { return nil }
        return name
    }
}

struct ConfigImportUnresolved: Sendable {
    let fromFile: String
    let requestedName: String
    let reason: ConfigImportError
}
enum ConfigImportInput: Sendable {
    case file(URL), folder(URL, entryPath: String), paste(Data), drop(Data), remote(URL)
}

enum ConfigImportError: Error, LocalizedError, Sendable {
    case missingEntry, tooLarge, nonText, invalidURL, outsideRoot, limits, httpStatus(Int)
    var errorDescription: String? {
        switch self {
        case .missingEntry: return "The selected config could not be read."
        case .tooLarge: return "Config text exceeds the game's 65,535-byte buffer limit."
        case .nonText: return "Choose plain config text, rather than a web page or binary file."
        case .invalidURL: return "Use HTTPS, or HTTP on localhost or 127.0.0.1, without URL credentials."
        case .outsideRoot: return "A dependency or redirect leaves the selected folder or repository."
        case .limits: return "This import exceeds the bounded dependency review limits."
        case .httpStatus(let status): return "The config server returned HTTP \(status)."
        }
    }
}
struct ConfigImportLimits: Sendable {
    var maximumFiles = 64
    var maximumFileBytes = 65535
    var maximumTotalBytes = 262140
}
struct ConfigImportFetchRequest: Sendable {
    let url: URL
    let maximumBytes: Int
    let root: URL
    func allows(_ candidate: URL) -> Bool {
        guard ConfigImportLoader.permittedURL(candidate),
              candidate.scheme?.lowercased() == root.scheme?.lowercased(),
              candidate.host?.lowercased() == root.host?.lowercased(),
              candidate.port == root.port else { return false }
        let prefix = root.path.hasSuffix("/") ? root.path : root.path + "/"
        return candidate.path == root.path || candidate.path.hasPrefix(prefix)
    }
}
struct ConfigImportTextResponse: Sendable {
    let data: Data
    let finalURL: URL
    let contentType: String?
}
protocol ConfigImportTextFetcher: Sendable {
    func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse
}

/// Streaming, ephemeral requests bound memory before downloading a full body.
/// Redirects are checked before following them, including HTTPS downgrades.
struct ConfigImportURLSessionFetcher: ConfigImportTextFetcher {
    private final class RedirectPolicy: NSObject, URLSessionTaskDelegate, Sendable {
        let policy: ConfigImportFetchRequest
        init(_ policy: ConfigImportFetchRequest) { self.policy = policy }
        func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                        newRequest request: URLRequest, completionHandler: @escaping @Sendable (URLRequest?) -> Void) {
            completionHandler(request.url.map { policy.allows($0) } == true ? request : nil)
        }
    }
    func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse {
        guard request.allows(request.url) else { throw ConfigImportError.outsideRoot }
        let configuration = URLSessionConfiguration.ephemeral
        configuration.httpCookieStorage = nil
        configuration.httpShouldSetCookies = false
        configuration.urlCredentialStorage = nil
        configuration.urlCache = nil
        configuration.timeoutIntervalForRequest = 15
        configuration.timeoutIntervalForResource = 30
        let session = URLSession(configuration: configuration, delegate: RedirectPolicy(request), delegateQueue: nil)
        defer { session.invalidateAndCancel() }
        var urlRequest = URLRequest(url: request.url)
        urlRequest.setValue("text/plain, application/octet-stream;q=0.5", forHTTPHeaderField: "Accept")
        let (stream, response) = try await session.bytes(for: urlRequest)
        guard let http = response as? HTTPURLResponse, let finalURL = response.url else { throw ConfigImportError.invalidURL }
        guard (200..<300).contains(http.statusCode) else { throw ConfigImportError.httpStatus(http.statusCode) }
        guard request.allows(finalURL) else { throw ConfigImportError.outsideRoot }
        if response.expectedContentLength > Int64(request.maximumBytes) { throw ConfigImportError.tooLarge }
        var data = Data()
        for try await byte in stream {
            try Task.checkCancellation()
            guard data.count < request.maximumBytes else { throw ConfigImportError.tooLarge }
            data.append(byte)
        }
        return .init(data: data, finalURL: finalURL, contentType: response.mimeType)
    }
}

struct ConfigImportLoader: Sendable {
    let catalog: any ConfigImportCatalog
    var fetcher: any ConfigImportTextFetcher = ConfigImportURLSessionFetcher()
    var limits = ConfigImportLimits()

    func load(_ input: ConfigImportInput, dialect: ConfigDialect) async throws -> ConfigImportBundle {
        guard limits.maximumFiles > 0, (1...65535).contains(limits.maximumFileBytes), limits.maximumTotalBytes > 0 else { throw ConfigImportError.limits }
        var source: ConfigImportSource
        var root: URL?
        var entryPath: String
        let entryData: Data
        switch input {
        case .paste(let data), .drop(let data):
            if case .paste = input { source = .init(kind: .paste) } else { source = .init(kind: .drop) }
            root = nil; entryPath = "import.cfg"; entryData = data
        case .file(let url):
            guard url.isFileURL else { throw ConfigImportError.missingEntry }
            let canonical = url.resolvingSymlinksInPath().standardizedFileURL
            root = canonical.deletingLastPathComponent()
            entryPath = canonical.lastPathComponent
            source = .init(kind: .file, originalURL: url, rootURL: root)
            entryData = try read(canonical, root: root!)
        case .folder(let url, let path):
            guard url.isFileURL, ConfigImportScript.execPath(Data(path.utf8)) == path else { throw ConfigImportError.outsideRoot }
            root = url.resolvingSymlinksInPath().standardizedFileURL
            entryPath = path
            source = .init(kind: .folder, originalURL: url, rootURL: root)
            entryData = try read(root!.appendingPathComponent(path), root: root!)
        case .remote(let original):
            let remote = try Self.remoteSource(original)
            let request = ConfigImportFetchRequest(url: remote.url, maximumBytes: limits.maximumFileBytes, root: remote.root)
            let response = try await fetcher.fetch(request)
            guard request.allows(response.finalURL) else { throw ConfigImportError.outsideRoot }
            try validate(response.data, contentType: response.contentType)
            root = remote.root
            entryPath = remote.path
            if remote.kind == .gist && response.finalURL.path.split(separator: "/").count >= 4 {
                // Pin siblings to the revision selected by the raw gist redirect.
                root = response.finalURL.deletingLastPathComponent()
                entryPath = response.finalURL.lastPathComponent
            }
            source = .init(kind: remote.kind, originalURL: original, rootURL: root, fetchedURL: response.finalURL)
            entryData = response.data
        }
        try validate(entryData)
        var files = [ConfigImportFile(path: entryPath, data: entryData, dialect: dialect)]
        var attempted = Set([entryPath]), unresolved: [ConfigImportUnresolved] = []
        var total = entryData.count, index = 0
        guard total <= limits.maximumTotalBytes else { throw ConfigImportError.limits }
        while index < files.count {
            try Task.checkCancellation()
            let file = files[index]; index += 1
            for value in try execRequests(file.document) {
                let requestedName = ConfigDocument.text(value)
                guard let path = ConfigImportScript.execPath(value) else {
                    unresolved.append(.init(fromFile: file.path, requestedName: requestedName, reason: .outsideRoot)); continue
                }
                guard let root else {
                    unresolved.append(.init(fromFile: file.path, requestedName: requestedName, reason: .missingEntry)); continue
                }
                guard attempted.insert(path).inserted else { continue }
                guard attempted.count <= limits.maximumFiles else { throw ConfigImportError.limits }
                do {
                    let url = root.appendingPathComponent(path)
                    let data: Data
                    if root.isFileURL { data = try read(url, root: root) }
                    else {
                        let request = ConfigImportFetchRequest(url: url, maximumBytes: limits.maximumFileBytes, root: root)
                        let response = try await fetcher.fetch(request)
                        guard request.allows(response.finalURL) else { throw ConfigImportError.outsideRoot }
                        data = response.data
                        try validate(data, contentType: response.contentType)
                    }
                    try validate(data)
                    guard data.count <= limits.maximumTotalBytes - total else { throw ConfigImportError.limits }
                    total += data.count
                    files.append(.init(path: path, data: data, dialect: dialect))
                } catch let error as ConfigImportError {
                    if case .limits = error { throw error }
                    unresolved.append(.init(fromFile: file.path, requestedName: requestedName, reason: error))
                } catch is CancellationError { throw CancellationError() }
                catch { unresolved.append(.init(fromFile: file.path, requestedName: requestedName, reason: .missingEntry)) }
            }
        }
        return .init(source: source, entryPath: entryPath, files: files, unresolved: unresolved)
    }

    private func read(_ url: URL, root: URL) throws -> Data {
        let canonical = url.resolvingSymlinksInPath().standardizedFileURL
        let prefix = root.path.hasSuffix("/") ? root.path : root.path + "/"
        guard canonical.path.hasPrefix(prefix) else { throw ConfigImportError.outsideRoot }
        let values = try canonical.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
        guard values.isRegularFile == true else { throw ConfigImportError.nonText }
        if (values.fileSize ?? 0) > limits.maximumFileBytes { throw ConfigImportError.tooLarge }
        let handle = try FileHandle(forReadingFrom: canonical)
        defer { try? handle.close() }
        let data = try handle.read(upToCount: limits.maximumFileBytes + 1) ?? Data()
        try validate(data)
        return data
    }
    private func validate(_ data: Data, contentType: String? = nil) throws {
        guard data.count <= limits.maximumFileBytes else { throw ConfigImportError.tooLarge }
        if let contentType {
            let mime = contentType.lowercased().split(separator: ";").first.map(String.init) ?? ""
            guard ["text/plain", "application/octet-stream", "application/x-config", "text/x-config"].contains(mime) else { throw ConfigImportError.nonText }
        }
        guard !data.contains(where: { $0 < 32 && ![9, 10, 13].contains($0) }) else { throw ConfigImportError.nonText }
        let sample = data.starts(with: [239, 187, 191]) ? data.dropFirst(3) : data
        let prefix = String(decoding: sample.prefix(256), as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if ["<!doctype", "<html", "<?xml", "%pdf-", "pk\u{3}\u{4}", "mz"].contains(where: { prefix.hasPrefix($0) }) { throw ConfigImportError.nonText }
    }
    private func execRequests(_ initial: ConfigDocument) throws -> [Data] {
        var queue = [(initial, 0)], visited = Set<Data>(), result: [Data] = [], index = 0, count = 0
        while index < queue.count {
            let (document, depth) = queue[index]; index += 1
            guard depth <= 16 else { throw ConfigImportError.limits }
            guard visited.insert(document.serialized()).inserted else { continue }
            for command in document.commands {
                count += 1
                guard count <= 8192 else { throw ConfigImportError.limits }
                if command.kind == .exec && command.tokens.count == 2 { result.append(command.tokens[1].value) }
                if let binding = command.kind == .bind ? ConfigImportScript.binding(command, document: document) : nil {
                    queue.append((.init(data: binding, dialect: document.dialect), depth + 1))
                }
                if let assignment = ConfigImportScript.assignment(command, document: document, catalog: catalog,
                                                                 knownNames: Set(command.tokens.prefix(1).map { $0.text.lowercased() })),
                   ConfigImportScript.looksExecutable(assignment.value, dialect: document.dialect) {
                    queue.append((.init(data: assignment.value, dialect: document.dialect), depth + 1))
                }
                if command.tokens.count > 2 && ["toggle", "togglep"].contains(command.tokens[0].text.lowercased()) {
                    for token in command.tokens.dropFirst(2) where ConfigImportScript.looksExecutable(token.value, dialect: document.dialect) {
                        queue.append((.init(data: token.value, dialect: document.dialect), depth + 1))
                    }
                }
            }
        }
        return result
    }

    static func permittedURL(_ url: URL) -> Bool {
        guard let parts = URLComponents(url: url, resolvingAgainstBaseURL: false), parts.user == nil, parts.password == nil,
              let host = parts.host?.lowercased(), !host.isEmpty,
              parts.queryItems?.contains(where: { ConfigImportSafety.credentialURLParameter($0.name) }) != true,
              !parts.path.contains("\\"), !parts.path.split(separator: "/").contains(where: { $0 == "." || $0 == ".." }),
              !["%2e", "%2f", "%5c", "%25"].contains(where: { parts.percentEncodedPath.lowercased().contains($0) }) else { return false }
        return parts.scheme?.lowercased() == "https" || (parts.scheme?.lowercased() == "http" && ["localhost", "127.0.0.1"].contains(host))
    }
    private struct Remote { let url: URL; let root: URL; let path: String; let kind: ConfigImportSource.Kind }
    private static func remoteSource(_ original: URL) throws -> Remote {
        guard permittedURL(original) else { throw ConfigImportError.invalidURL }
        var url = original
        let host = original.host!.lowercased(), segments = original.path.split(separator: "/").map(String.init)
        var kind: ConfigImportSource.Kind = original.scheme?.lowercased() == "http" ? .localhost : .https
        if host == "github.com" {
            guard segments.count >= 5, segments[2] == "blob" else { throw ConfigImportError.invalidURL }
            url = URL(string: "https://raw.githubusercontent.com/" + ([segments[0], segments[1]] + Array(segments.dropFirst(3))).joined(separator: "/"))!
            kind = .github
        } else if host == "gist.github.com" {
            guard segments.count == 2 else { throw ConfigImportError.invalidURL }
            url = URL(string: "https://gist.githubusercontent.com/" + segments.joined(separator: "/") + "/raw")!
            kind = .gist
        } else if host == "raw.githubusercontent.com" { kind = .github }
        else if host == "gist.githubusercontent.com" { kind = .gist }
        if kind == .github {
            let parts = url.path.split(separator: "/").map(String.init)
            guard parts.count >= 4 else { throw ConfigImportError.invalidURL }
            let root = URL(string: "https://raw.githubusercontent.com/" + parts.prefix(3).joined(separator: "/") + "/")!
            return .init(url: url, root: root, path: parts.dropFirst(3).joined(separator: "/"), kind: kind)
        }
        if kind == .gist {
            let parts = url.path.split(separator: "/").map(String.init)
            guard parts.count >= 3, parts[2] == "raw" else { throw ConfigImportError.invalidURL }
            let root = URL(string: "https://gist.githubusercontent.com/" + parts.prefix(3).joined(separator: "/"))!
            return .init(url: url, root: root, path: parts.count >= 5 ? parts.dropFirst(4).joined(separator: "/") : "import.cfg", kind: kind)
        }
        var root = URLComponents(url: url.deletingLastPathComponent(), resolvingAgainstBaseURL: false)!
        root.query = nil; root.fragment = nil
        return .init(url: url, root: root.url!, path: url.lastPathComponent, kind: kind)
    }
}
