import Foundation

@main struct ConfigImportTests {
    static func require(_ condition: @autoclosure () -> Bool, _ message: String) {
        if !condition() { FileHandle.standardError.write(Data("FAIL: \(message)\n".utf8)); exit(1) }
    }
    static func bytes(_ string: String) -> Data { Data(string.utf8) }
    static let catalog = ConfigImportCatalogSnapshot(settings: [
        "sensitivity": .init(archived: true), "com_maxfps": .init(archived: true),
        "cg_fov": .init(cheatProtected: true), "r_fullbright": .init(cheatProtected: true),
        "name": .init(archived: true), "cl_allowdownload": .init(archived: true),
        "foreign_fixture_setting": .init(inert: true)
    ], commands: ["+attack": .init(bindable: true), "-attack": .init(bindable: true),
                  "+forward": .init(bindable: true), "weapnext": .init(bindable: true)])
    static func review(_ text: String, dialect: ConfigDialect = .stock,
                       files: [String: Data] = [:]) -> ConfigImportReview {
        let entry = ConfigImportFile(path: "config.cfg", data: bytes(text), dialect: dialect)
        let others = files.map { ConfigImportFile(path: $0.key, data: $0.value, dialect: dialect) }
        return ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: entry.path, files: [entry] + others))
    }
    static func safety() {
        for dialect in [ConfigDialect.stock, .cod2x] {
            let comment = review("// harmless; quit\n", dialect: dialect)
            require(comment.candidates.count == 2, "comment semicolon yields two review commands")
            require(comment.candidates.last?.classification == .needsOK, "quit after apparent comment needs OK")
            let quotes = review("bind j \"say \\\"a;quit\\\"\"\n", dialect: dialect)
            require(quotes.candidates.count == 2, "escaped quotes count in command splitting")
            require(quotes.candidates[1].classification != .safe, "quote fragment is never safe")
        }
        for command in ["echo hi", "alias a b", "typo value", "/connect host"] {
            let item = review(command).candidates[0]
            require(item.classification == .needsOK && !item.importable, "unknown forwarded command never importable")
            require(item.reasons.contains(.unknownForwardedCommand), "unknown forwarding shown")
        }
        for text in ["seta password secret", "rconpassword secret", "seta cl_cdkey secret",
                     "seta match_login secret", "seta fs_homepath elsewhere", "seta foo C:\\private\\file",
                     "seta foo /Users/example/config", "rcon login secret", "openscriptmenu mod_login secret"] {
            let item = review(text).candidates[0]
            require(item.classification == .blocked && !item.importable, "secrets and filesystem paths blocked")
            require(!item.details.contains("secret"), "secret review details redacted")
        }
        require(review("unbindall\nunbindall").candidates.allSatisfy { $0.classification == .needsOK }, "both unbindall commands need OK")
        require(review("seta sensitivity 2.1").candidates[0].selectedByDefault, "ordinary known setting defaults on")
        require(review("bind w +forward").candidates[0].selectedByDefault, "known action bind defaults on")
        for name in ["cl_timeNudge", "r_vsync", "cl_run", "pmove_fixed", "cg_sprintMeterFullColor", "r_primitives"] {
            let item = review("seta \(name) 1").candidates[0]
            require(!item.selectedByDefault && item.reasons.contains(.inertName), "foreign setting labelled inert and defaults off")
        }
        for name in ["cg_fov", "r_fullbright"] {
            require(!review("seta \(name) 80").candidates[0].importable, "cheat protected values never imported")
        }
        let nested = review("seta a \"vstr b\"\nseta b \"connect example.invalid; quit\"\nbind j \"+attack; vstr a\"")
        let binding = nested.candidates.last!
        require(binding.classification == .needsOK, "transitive risky bind needs OK")
        require(binding.effects.contains { $0.reasons.contains(.connect) } && binding.effects.contains { $0.reasons.contains(.quit) }, "all transitive risks exposed")
        let secret = review("seta admin \"rcon login secret\"\nbind j \"vstr admin\"")
        require(secret.candidates.allSatisfy { $0.classification == .blocked }, "stored and reachable credential blocks entire chain")
        let exec = review("bind j \"exec sub/extra\"", files: ["sub/extra.cfg": bytes("quit\n")])
        require(exec.candidates[0].effects.contains { $0.reasons.contains(.quit) }, "nested exec reviewed in same root")
        require(!review("exec ../outside").candidates[0].importable, "exec cannot escape root")
        require(review("exec absent").candidates[0].reasons.contains(.unresolvedExec), "missing exec shown")
        for text in ["set a \"vstr a\"\nvstr a", "set a \"vstr b\"\nset b \"vstr a\"\nvstr a"] {
            require(review(text).candidates.last!.reasons.contains(.cycle), "vstr cycles detected without hanging")
        }
        require(review("exec loop", files: ["loop.cfg": bytes("exec loop")]).candidates[0].reasons.contains(.cycle), "exec cycle detected")
        require(!review("bind j \"echo hi\"").candidates[0].importable, "unknown nested commands never executable through bind")
        require(!review("seta x \"quit\"\nsetfromdvar y x\nbind j \"vstr y\"").candidates.last!.selectedByDefault, "copied variable dependency is reviewed")
        require(!review("seta x \"quit\"\nseta x \"+attack\"\nbind j \"vstr x\"").candidates.last!.selectedByDefault, "all possible definitions reviewed conservatively")
        require(!review("bind j \"wait -1\"").candidates[0].importable, "negative wait cannot stall game")
        require(!review(String(repeating: "a", count: 4096)).candidates[0].importable, "engine line truncation never safe")
        require(!review("// a real comment").candidates[0].selectedByDefault, "comments are review material rather than applicable changes")
        for text in ["rcon set rcon_password secret", "rcon \"login secret\"", "login secret", "seta foo relative/config.cfg"] {
            require(review(text).candidates[0].classification == .blocked, "credential wrappers and relative paths blocked")
        }
        let nulFile = ConfigImportFile(path: "nul.cfg", data: bytes("seta sensitivity 2") + Data([0]) + bytes("quit"), dialect: .stock)
        require(!ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: nulFile.path, files: [nulFile])).candidates[0].importable, "ignored NUL suffix cannot be imported as clean text")
        let reassigned = review("seta a +attack\na \"connect example.invalid\"\nbind j \"vstr a\"")
        require(reassigned.candidates.last!.effects.contains { $0.reasons.contains(.connect) }, "bare writes to script variables cannot hide effects")
        for text in ["toggle helper \"rcon login secret\"", "seta a \"rcon login secret\"\nsetfromdvar b a"] {
            require(review(text).candidates.last!.classification == .blocked, "toggle and copied script values cannot import credentials")
        }
        let commentScript = review("seta helper \"// apparent comment;rcon login secret\"").candidates[0]
        require(commentScript.reasons.contains(.credential) && !commentScript.details.contains("secret"), "apparent comments in stored scripts cannot hide credentials")
        require(review("bind j \"weapnext\(String(repeating: " ", count: 1015))\"").candidates[0].importable, "exact 1023-byte engine bind limit is accepted")
        let perPress = review("seta step1 \"+attack;set next vstr step2\"\nseta step2 \"weapnext;set next vstr step1\"\nseta next \"vstr step1\"\nbind j \"vstr next\"")
        require(perPress.candidates.allSatisfy { !$0.reasons.contains(.cycle) && $0.importable }, "per-press pointer changes do not execute their newly stored scripts immediately")
        require(review("seta foreign_fixture_setting 1").candidates[0].reasons.contains(.inertName), "catalog can identify proven foreign names without classifying all custom helpers as inert")
        require(review("seta destination \"https://user:secret@example.invalid/config\"").candidates[0].reasons.contains(.credential), "credentials inside setting URLs are blocked and redacted")
        print("PASS: synthetic safety and transitive dependency checks")
    }
    struct FixtureFetcher: ConfigImportTextFetcher {
        let responses: [URL: ConfigImportTextResponse]
        func fetch(_ request: ConfigImportFetchRequest) async throws -> ConfigImportTextResponse {
            guard let response = responses[request.url] else { throw ConfigImportError.missingEntry }
            return response
        }
    }
    static func rejects(_ operation: () async throws -> ConfigImportBundle, _ message: String) async {
        do { _ = try await operation(); require(false, message) } catch { }
    }
    static func sources() async throws {
        let temp = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: temp, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: temp) }
        let root = temp.appendingPathComponent("selected")
        try FileManager.default.createDirectory(at: root.appendingPathComponent("sub"), withIntermediateDirectories: true)
        try bytes("exec sub/extra\nbind j +attack").write(to: root.appendingPathComponent("main.cfg"))
        try bytes("seta sensitivity 2.1").write(to: root.appendingPathComponent("sub/extra.cfg"))
        let loader = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [:]))
        let file = try await loader.load(.file(root.appendingPathComponent("main.cfg")), dialect: .stock)
        require(file.files.count == 2 && file.source.kind == .file, "selected file follows same-root exec")
        let folder = try await loader.load(.folder(root, entryPath: "main.cfg"), dialect: .stock)
        require(folder.files.count == 2 && folder.source.kind == .folder, "selected folder uses explicit entry")
        try bytes("quit").write(to: temp.appendingPathComponent("outside.cfg"))
        try FileManager.default.createSymbolicLink(at: root.appendingPathComponent("escape.cfg"), withDestinationURL: temp.appendingPathComponent("outside.cfg"))
        try bytes("exec escape; exec ../outside; exec sub/extra").write(to: root.appendingPathComponent("main.cfg"))
        let bounded = try await loader.load(.file(root.appendingPathComponent("main.cfg")), dialect: .stock)
        require(bounded.files.count == 2, "traversal and symlink escapes are not read")
        require(bounded.unresolved.count == 2, "unsafe or missing execs retained as review failures")
        try bytes("exec loop").write(to: root.appendingPathComponent("loop.cfg"))
        let loop = try await loader.load(.file(root.appendingPathComponent("loop.cfg")), dialect: .stock)
        require(loop.files.count == 1, "loader deduplicates cyclic files")
        for input in [ConfigImportInput.paste(bytes("seta sensitivity 2")), .drop(bytes("seta sensitivity 2"))] {
            let bundle = try await loader.load(input, dialect: .stock)
            require(bundle.files.count == 1, "paste/drop text does not need files or network")
        }
        let tooLarge = Data(repeating: 97, count: 65536)
        await rejects({ try await loader.load(.paste(tooLarge), dialect: .stock) }, "file size over game buffer rejected")
        for data in [Data([0, 1, 2]), bytes("<!doctype html><html>bad</html>"), Data([239, 187, 191]) + bytes("<html>bad</html>"), Data([80, 75, 3, 4]), bytes("%PDF-1.7")] {
            await rejects({ try await loader.load(.paste(data), dialect: .stock) }, "non-text source rejected")
        }
        let entryURL = URL(string: "https://example.invalid/config/main.cfg")!
        let depURL = URL(string: "https://example.invalid/config/sub/extra.cfg")!
        let response = ConfigImportTextResponse(data: bytes("exec sub/extra"), finalURL: entryURL, contentType: "text/plain")
        let request = ConfigImportFetchRequest(url: entryURL, maximumBytes: 65535, root: entryURL.deletingLastPathComponent())
        require(!request.allows(URL(string: "https://example.invalid/config/%252e%252e/evil.cfg")!), "double-encoded traversal is rejected before redirects are followed")
        let network = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [
            entryURL: response, depURL: .init(data: bytes("quit"), finalURL: depURL, contentType: "text/plain")
        ]))
        let remote = try await network.load(.remote(entryURL), dialect: .stock)
        require(remote.files.count == 2 && remote.source.kind == .https, "injected HTTPS fetch and same-root dependencies")
        for url in ["http://example.invalid/a.cfg", "file:///private/config.cfg", "https://user:password@example.invalid/config.cfg"] {
            await rejects({ try await network.load(.remote(URL(string: url)!), dialect: .stock) }, "URL policy rejects insecure or credential-bearing URL")
        }
        for url in ["http://localhost:8080/main.cfg", "http://127.0.0.1:8080/main.cfg"] {
            let u = URL(string: url)!
            let local = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [u: .init(data: bytes("quit"), finalURL: u, contentType: "text/plain")]))
            let localBundle = try await local.load(.remote(u), dialect: .stock)
            require(localBundle.source.kind == .localhost, "localhost HTTP supported")
        }
        let outside = URL(string: "https://example.invalid/other/evil.cfg")!
        let redirected = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [entryURL: .init(data: bytes("quit"), finalURL: outside, contentType: "text/plain")]))
        await rejects({ try await redirected.load(.remote(entryURL), dialect: .stock) }, "redirect outside selected root rejected")
        let html = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [entryURL: .init(data: bytes("quit"), finalURL: entryURL, contentType: "text/html")]))
        await rejects({ try await html.load(.remote(entryURL), dialect: .stock) }, "HTML MIME rejected even when body looks like cfg")
        let github = URL(string: "https://github.com/test/configs/blob/main/main.cfg")!
        let raw = URL(string: "https://raw.githubusercontent.com/test/configs/main/main.cfg")!
        let git = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [raw: .init(data: bytes("quit"), finalURL: raw, contentType: "text/plain")]))
        let gitBundle = try await git.load(.remote(github), dialect: .stock)
        require(gitBundle.source.kind == .github, "GitHub blob normalized to raw text")
        let gist = URL(string: "https://gist.github.com/test/abc123")!
        let gistRaw = URL(string: "https://gist.githubusercontent.com/test/abc123/raw")!
        let gistFinal = URL(string: "https://gist.githubusercontent.com/test/abc123/raw/revision/main.cfg")!
        let gistLoader = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [gistRaw: .init(data: bytes("quit"), finalURL: gistFinal, contentType: "text/plain")]))
        let gistBundle = try await gistLoader.load(.remote(gist), dialect: .stock)
        require(gistBundle.source.kind == .gist, "gist normalized and revision provenance retained")
        let tight = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [:]), limits: .init(maximumFiles: 1))
        try bytes("exec sub/extra").write(to: root.appendingPathComponent("main.cfg"))
        await rejects({ try await tight.load(.file(root.appendingPathComponent("main.cfg")), dialect: .stock) }, "bundle file count bounded")
        let aggregate = ConfigImportLoader(catalog: catalog, fetcher: FixtureFetcher(responses: [:]), limits: .init(maximumTotalBytes: 20))
        await rejects({ try await aggregate.load(.file(root.appendingPathComponent("main.cfg")), dialect: .stock) }, "aggregate dependency bytes bounded")
        print("PASS: synthetic source, URL policy, filesystem containment and size checks; no network")
    }
    static func windows() throws {
        let volume = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: volume) }
        let players = volume.appendingPathComponent("Program Files (x86)/Activision/Call of Duty 2/main/players")
        for name in ["Alpha", "Zulu"] {
            try FileManager.default.createDirectory(at: players.appendingPathComponent(name), withIntermediateDirectories: true)
            try bytes("seta sensitivity 2").write(to: players.appendingPathComponent(name + "/config_mp.cfg"))
        }
        try bytes("Zulu\n").write(to: players.appendingPathComponent("active.txt"))
        try bytes("seta sensitivity 3").write(to: players.appendingPathComponent("Zulu/config_mp_2.cfg"))
        try bytes("seta sensitivity 4").write(to: players.appendingPathComponent("Zulu/config_mp_raptr.cfg"))
        try bytes("not a config").write(to: players.appendingPathComponent("Zulu/unrelated.txt"))
        let unrelated = volume.appendingPathComponent("Users/private/unknown-game/main/players/Hidden")
        try FileManager.default.createDirectory(at: unrelated, withIntermediateDirectories: true)
        try bytes("quit").write(to: unrelated.appendingPathComponent("config_mp.cfg"))
        let outside = volume.appendingPathComponent("outside")
        try FileManager.default.createDirectory(at: outside, withIntermediateDirectories: true)
        try bytes("quit").write(to: outside.appendingPathComponent("config_mp.cfg"))
        try FileManager.default.createSymbolicLink(at: players.appendingPathComponent("Escape"), withDestinationURL: outside)
        let discovery = WindowsProfiles().discover(mountedVolumes: [volume])
        require(discovery.profiles.count == 4, "known install offers primary and backup configs only")
        require(discovery.profiles.first?.profileName == "Zulu" && discovery.profiles.first?.isActive == true && discovery.profiles.first?.isBackup == false, "active profile primary offered first")
        require(discovery.profiles.filter(\.isBackup).count == 2, "numbered and Raptr backups explicitly labelled")
        require(!discovery.profiles.contains { $0.profileName == "Hidden" || $0.profileName == "Escape" }, "discovery avoids unrelated directories and profile symlinks")
        let selected = WindowsProfiles().discover(mountedVolumes: [], selectedFolders: [unrelated])
        require(selected.profiles.count == 1 && selected.profiles[0].profileName == "Hidden", "user-selected profile folder is discoverable")
        let install = players.deletingLastPathComponent().deletingLastPathComponent()
        require(WindowsProfiles().discover(mountedVolumes: [volume], selectedFolders: [install]).profiles.count == 4, "overlapping selected roots deduplicate configs")
        let limited = WindowsProfiles(maximumEntries: 1).discover(mountedVolumes: [volume])
        require(limited.truncated && limited.profiles.count <= 1, "bounded discovery reports truncation")
        print("PASS: synthetic Windows volume/profile/backup discovery and active-first ordering")
    }
    static func corpus(_ root: URL, research: URL) throws {
        let fixture = try ConfigImportCatalogSnapshot(
            researchDvars: Data(contentsOf: research.appendingPathComponent("dvars.json")),
            researchCommands: Data(contentsOf: research.appendingPathComponent("commands.json")))
        guard let enumerator = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else {
            require(false, "private corpus directory unavailable"); return
        }
        let urls = enumerator.compactMap { $0 as? URL }.filter { ["cfg", "txt"].contains($0.pathExtension.lowercased()) }
        var byteCount = 0
        for dialect in [ConfigDialect.stock, .cod2x] {
            var counts = [0, 0, 0], importable = 0, effects = 0, cycles = 0, limited = 0
            for url in urls {
                let data = try Data(contentsOf: url)
                if dialect == .stock { byteCount += data.count }
                let file = ConfigImportFile(path: "private.cfg", data: data, dialect: dialect)
                require(file.document.serialized() == data, "private corpus byte round trip")
                let result = ConfigImportSafety(catalog: fixture).review(.init(source: .init(kind: .file), entryPath: file.path, files: [file]))
                for item in result.candidates {
                    counts[item.classification.rawValue] += 1
                    if item.importable { importable += 1 }
                    effects += item.effects.count
                    if item.reasons.contains(.cycle) { cycles += 1 }
                    if item.reasons.contains(.limit) { limited += 1 }
                }
            }
            print("CORPUS \(dialect.rawValue): files=\(urls.count) safe=\(counts[0]) needsOK=\(counts[1]) blocked=\(counts[2]) importable=\(importable) effects=\(effects) cycles=\(cycles) limits=\(limited); content withheld")
        }
        print("PASS: private corpus round trip/classification, \(urls.count) files, \(byteCount) bytes; no execution or disk writes")
    }
    static func main() async {
        do {
            safety(); try await sources(); try windows()
            let arguments = Array(CommandLine.arguments.dropFirst())
            if arguments.count == 3 && arguments[0] == "--corpus" {
                try corpus(URL(fileURLWithPath: arguments[1]), research: URL(fileURLWithPath: arguments[2]))
            } else { require(arguments.isEmpty, "usage: config_import.sh [--corpus DIRECTORY RESEARCH_DVARS_DIRECTORY]") }
        } catch {
            FileHandle.standardError.write(Data("FAIL: import model check threw \(type(of: error)); content withheld\n".utf8))
            exit(1)
        }
    }
}
