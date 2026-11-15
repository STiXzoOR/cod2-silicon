import Foundation
import AppKit
import CryptoKit
import Darwin
import Network

// Servers › Hosting: the servers this Mac runs. Each has its own folder under the app's data
// (server.cfg, log, the game's writable files); the game folder is only read. Nothing is contacted
// until a server is started or zPAM is asked for.

/// Where zPAM stands on this Mac for one of its two lines.
enum ZPAMState: Equatable {
    /// Not looked up yet, or the look-up failed (`String` says why).
    case unknown, failed(String)
    case missing(ZPAMRelease)
    case getting(ZPAMRelease, received: Int)
    case ready(version: String)
}

@MainActor final class HostingModel: ObservableObject {
    enum Phase: Equatable {
        case stopped, starting
        case running(since: Date)
        case failed(String)
        /// The port is taken; `next` is the first free one after it.
        case portBusy(next: Int)
    }
    let folder: URL
    /// The game app that carries the dedicated servers, and the game data they read.
    var gameBundle: URL
    var dataPath: () -> String = { "" }
    var notify: (String) -> Void = { _ in }
    @Published private(set) var servers: [HostedServer] = []
    @Published var selection: String?
    /// A new server that isn't saved yet. Saved servers save as they are edited.
    @Published var draft: HostedServer?
    /// The running server whose settings are shown instead of its running page.
    @Published var editing: String?
    @Published private(set) var phases: [String: Phase] = [:]
    @Published private(set) var statuses: [String: HostedStatus] = [:]
    @Published private(set) var logs: [String: [String]] = [:]
    @Published private(set) var zpam: [ZPAMLine: ZPAMState] = [.four: .unknown, .three: .unknown]
    @Published var exporting: HostedServer?
    /// Whether this Mac reaches its network over Wi-Fi. There, AirDrop and Handoff take the radio away
    /// for a moment about twice a second, which players on a hosted server see as freezing and jumping.
    @Published private(set) var onWiFi = false
    private var pathMonitor: NWPathMonitor?
    /// The running server whose whole player list is open as a dialog: its page lists as many as fit.
    @Published var showingPlayers: HostedServer?
    private var processes: [String: Process] = [:]
    private var sessionLogs: [String: GameSessionLog] = [:]
    private var poll: Task<Void, Never>?
    private var commands: Task<Void, Never>?
    private var download: Task<Void, Never>?
    /// The release listing, for tests and the review harness to replace: no network there.
    var listReleases: @Sendable () async throws -> Data = {
        var request = URLRequest(url: ZPAMRelease.releasesURL, timeoutInterval: 20)
        request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        let (data, response) = try await URLSession.shared.data(for: request)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw URLError(.badServerResponse) }
        return data
    }

    init(folder: URL, gameBundle: URL) {
        self.folder = folder; self.gameBundle = gameBundle
        load()
    }

    // MARK: Saved servers

    private var listURL: URL { folder.appendingPathComponent("servers.json") }
    func home(_ id: String) -> URL { folder.appendingPathComponent("servers/\(id)") }
    private func logFolder(_ id: String) -> URL { home(id).appendingPathComponent("logs", isDirectory: true) }
    /// The server's log: the newest start's, or the single file 0.3.0 wrote when there is none yet.
    func logURL(_ id: String) -> URL { latestLog(id) ?? logFolder(id).appendingPathComponent("console.log") }
    /// The log of the server's last start. One per start: the newest ten are kept, 4 MB each, 20 MB a server.
    func latestLog(_ id: String) -> URL? { GameSessionLog.latest(folder: logFolder(id), prefix: "server") }
    /// The newest log among all the servers saved here.
    func latestLog() -> URL? {
        servers.compactMap { latestLog($0.id) }.max {
            let dates = [$0, $1].map { (try? $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast }
            return dates[0] < dates[1]
        }
    }
    private func load() {
        guard let data = try? Data(contentsOf: listURL), let saved = try? JSONDecoder().decode([HostedServer].self, from: data) else { return }
        servers = saved
        selection = saved.first?.id
        for line in [ZPAMLine.four, .three] { if let version = installedZPAM(line) { zpam[line] = .ready(version: version) } }
    }
    private func store() {
        do {
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
            let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            // The admin passwords are in here, as they are in each server.cfg: the owner alone reads it.
            try encoder.encode(servers).write(to: listURL, options: .atomic)
            try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: listURL.path)
        } catch { notify("Couldn’t save your servers. \(error.localizedDescription)") }
    }
    var selected: HostedServer? { servers.first { $0.id == selection } }
    /// The new server's form is in front: nothing saved is selected.
    var showingDraft: Bool { draft != nil && selection == nil }
    func phase(_ id: String) -> Phase { phases[id] ?? .stopped }
    func isRunning(_ id: String) -> Bool { if case .running = phase(id) { return true }; return phase(id) == .starting }
    var running: [HostedServer] { servers.filter { isRunning($0.id) } }

    func newServer(engine: GameEngine) {
        var server = HostedServer.new(engine: engine)
        // A port no saved server has, from the ones the game's local list asks while there is one.
        let taken = Set(servers.map(\.port))
        if taken.contains(server.port) { server.port = HostingConfig.otherPort(for: server) { !taken.contains($0) } }
        if servers.contains(where: { $0.name == server.name }) { server.name = "My Server \(servers.count + 1)" }
        draft = server
        selection = nil
    }
    func cancelDraft() { draft = nil; if selection == nil { selection = servers.first?.id } }
    /// Keeps the new server. What it holds is cleaned where it is used, in the config it becomes.
    @discardableResult func saveDraft() -> HostedServer? {
        guard var server = draft else { return nil }
        if HostingConfig.safe(server.name, limit: 64).isEmpty { server.name = "My Server" }
        if HostingConfig.password(server.adminPassword).isEmpty { server.adminPassword = HostedServer.password() }
        servers.append(server)
        store()
        draft = nil; selection = server.id
        return server
    }
    /// An edit to a saved server, saved at once. A running server uses it the next time it starts.
    func update(_ server: HostedServer) {
        guard let index = servers.firstIndex(where: { $0.id == server.id }), servers[index] != server else { return }
        servers[index] = server
        store()
    }
    func delete(_ server: HostedServer) {
        guard !isRunning(server.id) else { return }
        servers.removeAll { $0.id == server.id }
        store()
        // Its folder goes to the Trash, so a deleted server's log and config can still be recovered.
        if FileManager.default.fileExists(atPath: home(server.id).path) { try? FileManager.default.trashItem(at: home(server.id), resultingItemURL: nil) }
        phases[server.id] = nil; statuses[server.id] = nil; logs[server.id] = nil
        if selection == server.id { selection = draft == nil ? servers.first?.id : nil }
    }

    // MARK: Start and stop

    func executable(_ engine: GameEngine) -> URL {
        gameBundle.appendingPathComponent("Contents/MacOS/" + (engine == .cod2x ? "cod2_macos_ded" : "cod2_macos_ded_stock"))
    }
    /// Save and Start, or Start on a saved server. `port` replaces a busy one.
    func start(_ server: HostedServer, port: Int? = nil) {
        var server = server
        if let port, port != server.port {
            server.port = port
            if let index = servers.firstIndex(where: { $0.id == server.id }) { servers[index] = server; store() }
        }
        guard !isRunning(server.id) else { return }
        let binary = executable(server.engine), data = dataPath()
        guard FileManager.default.isExecutableFile(atPath: binary.path) else {
            phases[server.id] = .failed("This copy of CoD2 Silicon doesn’t include the \(server.engine == .cod2x ? "CoD2x" : "original 1.3") server. Reinstall CoD2 Silicon to restore it."); return
        }
        guard !data.isEmpty, FileManager.default.fileExists(atPath: data + "/main/iw_00.iwd") else {
            phases[server.id] = .failed("The server uses your game files, and they aren’t set up yet. Choose your Call of Duty 2 folder in Settings › Game Data."); return
        }
        if server.rules == .zpam, installedZPAM(server.zpamLine) == nil {
            phases[server.id] = .failed("This server needs zPAM, which isn’t on this Mac yet. Get it in the server’s settings, then start again."); return
        }
        if !Self.portFree(server.port) {
            phases[server.id] = .portBusy(next: HostingConfig.otherPort(for: server, free: Self.portFree)); return
        }
        do {
            try prepare(server)
            let process = Process()
            process.executableURL = binary
            process.arguments = try HostingConfig.arguments(server, dataPath: data, home: home(server.id).path)
            process.currentDirectoryURL = home(server.id)
            var environment = ProcessInfo.processInfo.environment
            environment["HOME"] = home(server.id).appendingPathComponent("user").path
            process.environment = environment
            // A log per start, with both passwords kept out of it. The server writes into the log's
            // pipe, which outlives this app without stopping the server.
            let log = try GameSessionLog(folder: logFolder(server.id), prefix: "server", secrets: [server.joinPassword, server.adminPassword])
            process.standardOutput = log.pipe; process.standardError = log.pipe; process.standardInput = FileHandle.nullDevice
            let id = server.id
            process.terminationHandler = { [weak self] finished in
                let status = finished.terminationStatus, signalled = finished.terminationReason == .uncaughtSignal
                log.finish()
                Task { @MainActor [weak self] in self?.ended(id, status: status, signalled: signalled) }
            }
            do { try process.run() } catch { log.finish(); throw error }
            log.closeParentWriter()
            sessionLogs[id] = log
            processes[id] = process
            phases[id] = .starting
            statuses[id] = nil
            startPolling()
        } catch {
            phases[server.id] = .failed((error as? LauncherError)?.message ?? "The server couldn’t start. \(error.localizedDescription)")
        }
    }
    /// The server's own folder: its config, and zPAM's files beside it when it uses them.
    private func prepare(_ server: HostedServer) throws {
        let main = home(server.id).appendingPathComponent("main")
        for directory in [main, home(server.id).appendingPathComponent("logs"), home(server.id).appendingPathComponent("user")] {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        }
        var config = HostingConfig.serverConfig(server)
        // Archives from an earlier zPAM set-up would still load: only this server's own are left.
        for old in (try? FileManager.default.contentsOfDirectory(at: main, includingPropertiesForKeys: nil)) ?? [] where old.pathExtension == "iwd" {
            try FileManager.default.removeItem(at: old)
        }
        if server.rules == .zpam, let version = installedZPAM(server.zpamLine) {
            let source = zpamFolder(server.zpamLine, version)
            for archive in try FileManager.default.contentsOfDirectory(at: source, includingPropertiesForKeys: nil) where archive.pathExtension == "iwd" {
                let target = main.appendingPathComponent(archive.lastPathComponent)
                // A hard link costs no space; a copy when the folders are on different volumes.
                do { try FileManager.default.linkItem(at: archive, to: target) } catch { try FileManager.default.copyItem(at: archive, to: target) }
            }
            if let template = try? String(contentsOf: source.appendingPathComponent("server.cfg"), encoding: .isoLatin1) {
                config = HostingConfig.serverConfig(server, zpamTemplate: template)
            }
        }
        let url = main.appendingPathComponent("server.cfg")
        try Data(config.utf8).write(to: url, options: .atomic)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path)
    }
    func stop(_ server: HostedServer) {
        // SIGTERM asks the engine for its normal shutdown, which tells the players and the masters.
        processes[server.id]?.terminate()
    }
    /// For quitting: asks every server to shut down, waits up to three seconds, then ends what is left.
    func stopAllAndWait() {
        let running = Array(processes.values)
        guard !running.isEmpty else { return }
        for process in running { process.terminate() }
        let deadline = Date().addingTimeInterval(3)
        while Date() < deadline, running.contains(where: \.isRunning) { usleep(50_000) }
        for process in running where process.isRunning { kill(process.processIdentifier, SIGKILL) }
    }
    private func ended(_ id: String, status: Int32, signalled: Bool) {
        processes[id] = nil
        sessionLogs[id] = nil
        statuses[id] = nil
        if editing == id { editing = nil }
        refreshLog(id)
        if case .starting = phase(id), status != 0 || signalled {
            phases[id] = .failed("The server stopped while starting. Its log, below, says why.")
        } else if status != 0 && !signalled {
            phases[id] = .failed("The server stopped with an error. Its log says why.")
        } else {
            phases[id] = .stopped
        }
        if processes.isEmpty { poll?.cancel(); poll = nil }
    }

    // MARK: What a running server says

    /// Follows the network path from the first server start on; nothing is asked of the network itself.
    private func watchNetwork() {
        guard pathMonitor == nil else { return }
        let monitor = NWPathMonitor()
        monitor.pathUpdateHandler = { [weak self] path in
            let wifi = HostingNetwork.isWiFi(satisfied: path.status == .satisfied, preferred: path.availableInterfaces.first?.type)
            Task { @MainActor [weak self] in self?.onWiFi = wifi }
        }
        monitor.start(queue: .global(qos: .utility))
        pathMonitor = monitor
    }
    private func startPolling() {
        watchNetwork()
        guard poll == nil else { return }
        poll = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(for: .seconds(3))
            }
        }
    }
    private func refresh() async {
        for server in servers where processes[server.id] != nil {
            let port = server.port, id = server.id
            let answer = await Task.detached { HostingNetwork.exchange(port: port, command: "getstatus") }.value
            guard processes[id] != nil else { continue }
            if let status = answer.flatMap(HostedStatus.parse) {
                statuses[id] = status
                if case .starting = phase(id) { phases[id] = .running(since: Date()) }
            }
            refreshLog(id)
        }
    }
    /// The last lines of the server's console, read from the end of its log.
    func refreshLog(_ id: String) {
        guard let handle = try? FileHandle(forReadingFrom: logURL(id)) else { return }
        defer { try? handle.close() }
        let size = (try? handle.seekToEnd()) ?? 0
        try? handle.seek(toOffset: size > 32768 ? size - 32768 : 0)
        guard let data = try? handle.readToEnd() else { return }
        let lines = String(decoding: data, as: UTF8.self).split(separator: "\n", omittingEmptySubsequences: true).suffix(200)
        logs[id] = lines.map { HostingModel.plain(String($0)) }
    }
    /// A console line without the game's colour codes (^1 … ^9).
    static func plain(_ line: String) -> String {
        line.replacingOccurrences(of: #"\^[0-9]"#, with: "", options: .regularExpression)
    }
    /// A console command for the server, sent over its own admin channel on this Mac.
    func send(_ command: String, to server: HostedServer) {
        let password = HostingConfig.password(server.adminPassword)
        guard isRunning(server.id), !password.isEmpty else { return }
        let port = server.port, previous = commands
        // The engine drops an admin command that comes within half a second of the last one, so they queue.
        commands = Task.detached {
            await previous?.value
            _ = HostingNetwork.exchange(port: port, command: "rcon \(password) \(command)")
            try? await Task.sleep(for: .milliseconds(550))
        }
    }
    func nextMap(_ server: HostedServer) { send("map_rotate", to: server) }
    func kick(_ player: HostedStatus.Player, from server: HostedServer) {
        // By name, as the engine's `kick` takes it; colour codes are not part of the name it compares.
        send("kick \"\(HostingConfig.safe(HostingModel.plain(player.name), limit: 32))\"", to: server)
    }
    /// The address people on the same network join with.
    func localAddress(_ server: HostedServer) -> String { "\(HostingNetwork.localIPv4() ?? "127.0.0.1"):\(server.port)" }

    static func portFree(_ port: Int) -> Bool { HostingNetwork.portFree(port) }

    // MARK: zPAM

    private func zpamFolder(_ line: ZPAMLine, _ version: String) -> URL { folder.appendingPathComponent("zpam/\(line.rawValue)/\(version)") }
    /// The newest version of this line that is fully on this Mac.
    func installedZPAM(_ line: ZPAMLine) -> String? {
        let base = folder.appendingPathComponent("zpam/\(line.rawValue)")
        let versions = (try? FileManager.default.contentsOfDirectory(atPath: base.path)) ?? []
        return versions.filter { version in
            FileManager.default.fileExists(atPath: base.appendingPathComponent("\(version)/server.cfg").path)
                && ((try? FileManager.default.contentsOfDirectory(atPath: base.appendingPathComponent(version).path)) ?? []).contains { $0.hasSuffix(".iwd") }
        }.max { $0.compare($1, options: .numeric) == .orderedAscending }
    }
    /// Asks GitHub which zPAM is current. Called when the form shows Competitive · zPAM, never before.
    func lookUpZPAM(_ line: ZPAMLine) async {
        if let version = installedZPAM(line) { zpam[line] = .ready(version: version); return }
        if case .getting = zpam[line] { return }
        do {
            let listing = try await listReleases()
            guard let release = ZPAMRelease.newest(line, in: listing) else { zpam[line] = .failed("The zPAM project’s releases don’t list a download with a checksum right now."); return }
            if case .getting = zpam[line] { return }
            zpam[line] = .missing(release)
        } catch {
            zpam[line] = .failed("The zPAM project’s releases on GitHub couldn’t be reached. Check your connection and try again.")
        }
    }
    func getZPAM(_ line: ZPAMLine) {
        guard case .missing(let release) = zpam[line] else { return }
        zpam[line] = .getting(release, received: 0)
        let destination = zpamFolder(line, release.version), scratch = folder.appendingPathComponent("zpam/.download-\(UUID().uuidString)")
        let progress: @Sendable (Int) -> Void = { [weak self] received in
            Task { @MainActor [weak self] in
                if case .getting(let current, _) = self?.zpam[line], current == release { self?.zpam[line] = .getting(release, received: received) }
            }
        }
        download = Task { [weak self] in
            do {
                try await ZPAMInstaller.install(release, into: destination, scratch: scratch, progress: progress)
                self?.zpam[line] = .ready(version: release.version)
            } catch is CancellationError {
                self?.zpam[line] = .missing(release)
            } catch {
                self?.zpam[line] = .failed((error as? LauncherError)?.message ?? "zPAM couldn’t be downloaded. \(error.localizedDescription)")
            }
            try? FileManager.default.removeItem(at: scratch)
        }
    }
    func cancelZPAM() { download?.cancel() }

    // MARK: Export

    /// Writes the export into a new folder inside `parent`; returns the folder.
    func export(_ server: HostedServer, to parent: URL, passwords: Bool, matchSettings: Bool) throws -> URL {
        var template: String?
        var release: ZPAMRelease?
        if server.rules == .zpam, matchSettings {
            if let version = installedZPAM(server.zpamLine) {
                template = try? String(contentsOf: zpamFolder(server.zpamLine, version).appendingPathComponent("server.cfg"), encoding: .isoLatin1)
            }
            if case .missing(let known) = zpam[server.zpamLine] { release = known }
        }
        let files = HostingConfig.export(server, zpamTemplate: template, zpam: release, passwords: passwords, matchSettings: matchSettings)
        var target = parent.appendingPathComponent(HostingConfig.fileName(server) + "-server")
        var number = 2
        while FileManager.default.fileExists(atPath: target.path) { target = parent.appendingPathComponent("\(HostingConfig.fileName(server))-server-\(number)"); number += 1 }
        try FileManager.default.createDirectory(at: target, withIntermediateDirectories: false)
        for (name, text) in files {
            let url = target.appendingPathComponent(name)
            try Data(text.utf8).write(to: url, options: .atomic)
            if name.hasSuffix(".sh") { try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: url.path) }
        }
        return target
    }

    // MARK: Review harness (preview mode only)

    func preview(servers: [HostedServer], selection: String?, draft: HostedServer? = nil, editing: String? = nil, phases: [String: Phase] = [:], statuses: [String: HostedStatus] = [:],
                 logs: [String: [String]] = [:], zpam: [ZPAMLine: ZPAMState] = [:], exporting: HostedServer? = nil,
                 showingPlayers: HostedServer? = nil, onWiFi: Bool = false) {
        self.servers = servers; self.selection = selection; self.draft = draft; self.editing = editing; self.phases = phases; self.statuses = statuses; self.logs = logs
        for (line, state) in zpam { self.zpam[line] = state }
        self.exporting = exporting; self.showingPlayers = showingPlayers; self.onWiFi = onWiFi
    }
}

/// Downloads a zPAM release, checks it against the size and SHA-256 its release publishes, and keeps
/// only what a server needs from it: the archives and zPAM's own server.cfg.
enum ZPAMInstaller {
    static func install(_ release: ZPAMRelease, into destination: URL, scratch: URL, progress: @escaping @Sendable (Int) -> Void) async throws {
        let manager = FileManager.default
        try manager.createDirectory(at: scratch, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let archive = scratch.appendingPathComponent("zpam.zip")
        manager.createFile(atPath: archive.path, contents: nil)
        let handle = try FileHandle(forWritingTo: archive)
        defer { try? handle.close() }
        var request = URLRequest(url: release.url, timeoutInterval: 60)
        request.cachePolicy = .reloadIgnoringLocalCacheData
        let (bytes, response) = try await URLSession.shared.bytes(for: request)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw LauncherError(message: "The zPAM download was refused by GitHub. Try again later.") }
        var hash = SHA256(), buffer = Data(capacity: 1 << 18), received = 0
        for try await byte in bytes {
            buffer.append(byte)
            if buffer.count == 1 << 18 {
                received += buffer.count
                guard received <= release.bytes else { throw LauncherError(message: "The zPAM download is larger than its release says, so it wasn’t kept.") }
                hash.update(data: buffer); try handle.write(contentsOf: buffer); buffer.removeAll(keepingCapacity: true)
                progress(received)
                try Task.checkCancellation()
            }
        }
        received += buffer.count
        hash.update(data: buffer); try handle.write(contentsOf: buffer); try handle.close()
        let digest = hash.finalize().map { String(format: "%02x", $0) }.joined()
        guard received == release.bytes, digest == release.sha256 else {
            throw LauncherError(message: "The zPAM download doesn’t match the checksum its release publishes, so it wasn’t kept.")
        }
        progress(received)
        // Unpacked in its own empty folder; only plain files with the expected names leave it.
        let unpacked = scratch.appendingPathComponent("unpacked")
        try manager.createDirectory(at: unpacked, withIntermediateDirectories: true)
        try await run("/usr/bin/ditto", ["-x", "-k", archive.path, unpacked.path])
        let main = unpacked.appendingPathComponent("main")
        let names = try manager.contentsOfDirectory(atPath: main.path).filter { name in
            name == "server.cfg" || (name.hasSuffix(".iwd") && name.lowercased().hasPrefix("zpam") && name.allSatisfy { $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "_" || $0 == ".") })
        }
        guard names.contains("server.cfg"), names.contains(where: { $0.hasSuffix(".iwd") }) else {
            throw LauncherError(message: "The zPAM download doesn’t hold the files a server needs.")
        }
        let staging = destination.deletingLastPathComponent().appendingPathComponent(".\(destination.lastPathComponent).incoming")
        try? manager.removeItem(at: staging)
        try manager.createDirectory(at: staging, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        for name in names {
            let source = main.appendingPathComponent(name)
            guard (try source.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey])).isRegularFile == true else {
                throw LauncherError(message: "The zPAM download holds something that isn’t a plain file, so it wasn’t kept.")
            }
            try manager.moveItem(at: source, to: staging.appendingPathComponent(name))
        }
        try? manager.removeItem(at: destination)
        try manager.moveItem(at: staging, to: destination)
    }
    private static func run(_ tool: String, _ arguments: [String]) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            let process = Process()
            process.executableURL = URL(fileURLWithPath: tool); process.arguments = arguments
            process.standardOutput = FileHandle.nullDevice; process.standardError = FileHandle.nullDevice
            process.terminationHandler = { finished in
                if finished.terminationStatus == 0 { continuation.resume() }
                else { continuation.resume(throwing: LauncherError(message: "The zPAM download couldn’t be unpacked.")) }
            }
            do { try process.run() } catch { continuation.resume(throwing: error) }
        }
    }
}

/// The few UDP exchanges hosting needs with a server on this Mac.
enum HostingNetwork {
    /// Sends one connectionless command to 127.0.0.1 and returns the answer's text, or nil in a second.
    static func exchange(port: Int, command: String, timeout: Int = 1) -> String? {
        let descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard descriptor >= 0 else { return nil }
        defer { close(descriptor) }
        var wait = timeval(tv_sec: timeout, tv_usec: 0)
        setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &wait, socklen_t(MemoryLayout<timeval>.size))
        var address = sockaddr_in()
        address.sin_family = sa_family_t(AF_INET); address.sin_port = in_port_t(port).bigEndian
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        let packet = [UInt8](repeating: 0xff, count: 4) + Array(command.utf8)
        let sent = withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { sendto(descriptor, packet, packet.count, 0, $0, socklen_t(MemoryLayout<sockaddr_in>.size)) }
        }
        guard sent == packet.count else { return nil }
        var buffer = [UInt8](repeating: 0, count: 16384)
        let count = recv(descriptor, &buffer, buffer.count, 0)
        guard count > 4 else { return nil }
        return String(decoding: buffer[4..<count], as: UTF8.self)
    }
    /// Wi-Fi only when the path works and the interface the system prefers is Wi-Fi: wired, a VPN in
    /// front, no network or any doubt is not Wi-Fi.
    static func isWiFi(satisfied: Bool, preferred: NWInterface.InterfaceType?) -> Bool { satisfied && preferred == .wifi }
    /// Whether nothing on this Mac holds the UDP port.
    static func portFree(_ port: Int) -> Bool {
        guard (1025...65535).contains(port) else { return false }
        let descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard descriptor >= 0 else { return false }
        defer { close(descriptor) }
        var address = sockaddr_in()
        address.sin_family = sa_family_t(AF_INET); address.sin_port = in_port_t(port).bigEndian
        address.sin_addr.s_addr = INADDR_ANY
        return withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { bind(descriptor, $0, socklen_t(MemoryLayout<sockaddr_in>.size)) }
        } == 0
    }
    /// This Mac's address on the local network: the first IPv4 address of an active Ethernet or Wi-Fi interface.
    static func localIPv4() -> String? {
        var list: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&list) == 0, let first = list else { return nil }
        defer { freeifaddrs(list) }
        for interface in sequence(first: first, next: { $0.pointee.ifa_next }) {
            let flags = Int32(interface.pointee.ifa_flags)
            guard let address = interface.pointee.ifa_addr, address.pointee.sa_family == UInt8(AF_INET),
                  flags & IFF_UP != 0, flags & IFF_LOOPBACK == 0, String(cString: interface.pointee.ifa_name).hasPrefix("en") else { continue }
            var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            if getnameinfo(address, socklen_t(address.pointee.sa_len), &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 {
                return String(decoding: host.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            }
        }
        return nil
    }
}
