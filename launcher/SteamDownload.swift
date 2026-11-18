import CryptoKit
import Darwin
import Foundation

// Owner: ship-shell. Setup › Game data › Download from Steam (the six “From Steam” boards): what Steam
// holds for Call of Duty 2, the pinned download tool, what that tool prints, and one run of it.
// Nothing here knows the launcher's model; ModelSteam.swift drives it.
//
// The tool is DepotDownloader (SteamRE, GPL-2.0). It is fetched from its own release page when the
// player asks for a download, checked against the checksum below, run as a separate process and
// removed afterwards. It is never part of this app.

/// The languages Steam has the game in. Each is one depot beside the three every copy shares.
enum SteamLanguage: String, CaseIterable, Identifiable, Sendable {
    case english, french, german, italian, spanish
    var id: String { rawValue }
    var title: String { rawValue.capitalized }
    var depot: Int {
        switch self { case .english: 2634; case .german: 2636; case .french: 2637; case .spanish: 2638; case .italian: 2639 }
    }
}

/// Call of Duty 2 on Steam: app 2630.
enum SteamGame {
    static let app = 2630
    struct Depot: Equatable, Sendable { var id: Int; var bytes: Int64 }
    /// The depots one language needs, in the order they are asked for. Sizes are Steam's own, rounded:
    /// they only weigh the progress bar, and the files decide whether the download is complete.
    static func depots(_ language: SteamLanguage) -> [Depot] {
        [Depot(id: 2631, bytes: 12_000_000), Depot(id: 2632, bytes: 1_360_000_000), Depot(id: 2633, bytes: 800_000_000),
         Depot(id: language.depot, bytes: 1_530_000_000)]
    }
    static func bytes(_ language: SteamLanguage) -> Int64 { depots(language).reduce(0) { $0 + $1.bytes } }
    /// Free room asked for before a download starts: the game, and what the tool keeps beside it while it works.
    static func roomNeeded(_ language: SteamLanguage) -> Int64 { bytes(language) + 300_000_000 }
    static let storePage = URL(string: "https://store.steampowered.com/app/2630/Call_of_Duty_2/")!
    /// “1.42” for 1,420,000,000 bytes: gigabytes as Steam and Finder count them.
    static func gigabytes(_ bytes: Int64) -> String { String(format: "%.2f", Double(max(0, bytes)) / 1_000_000_000) }
}

struct DepotToolError: LocalizedError, Equatable {
    var message: String
    var errorDescription: String? { message }
    static let fetch = DepotToolError(message: "The download tool couldn’t be fetched. Check your connection and try again.")
    static let checksum = DepotToolError(message: "The download tool didn’t match its checksum, so it was not run. Try again later.")
    static let unpack = DepotToolError(message: "The download tool couldn’t be unpacked.")
}

/// The download tool's release archive, pinned by checksum.
struct DepotTool: Equatable, Sendable {
    var archive: URL
    var sha256: String
    static let executableName = "DepotDownloader"
    /// DepotDownloader 3.4.0 for Apple silicon (32 MB), from the project's own release page.
    static let pinned = DepotTool(
        archive: URL(string: "https://github.com/SteamRE/DepotDownloader/releases/download/DepotDownloader_3.4.0/DepotDownloader-macos-arm64.zip")!,
        sha256: "60e80c7c496f3f9a079cd3c62036b35d088c27bc0149baf38f009eb57a52f6a5")

    /// SHA-256 of a file, read in pieces.
    static func digest(of file: URL) -> String? {
        guard let handle = try? FileHandle(forReadingFrom: file) else { return nil }
        defer { try? handle.close() }
        var hash = SHA256()
        while let chunk = try? handle.read(upToCount: 1 << 20), !chunk.isEmpty { hash.update(data: chunk) }
        return hash.finalize().map { String(format: "%02x", $0) }.joined()
    }

    /// The tool, ready to run in `folder`: fetched, checked and unpacked, or the copy a stopped try left there.
    func install(into folder: URL) async throws -> URL {
        let files = FileManager.default
        let executable = folder.appendingPathComponent(Self.executableName), stamp = folder.appendingPathComponent(".checked")
        if files.isExecutableFile(atPath: executable.path), (try? String(contentsOf: stamp, encoding: .utf8)) == sha256 { return executable }
        try? files.removeItem(at: folder)
        try files.createDirectory(at: folder, withIntermediateDirectories: true)
        let zip = folder.appendingPathComponent("tool.zip")
        do {
            let (fetched, response) = try await URLSession(configuration: .ephemeral).download(from: archive)
            if let http = response as? HTTPURLResponse, http.statusCode != 200 { throw DepotToolError.fetch }
            try files.moveItem(at: fetched, to: zip)
        } catch is CancellationError {
            throw CancellationError()
        } catch {
            throw DepotToolError.fetch
        }
        guard Self.digest(of: zip) == sha256 else {
            try? files.removeItem(at: folder)
            throw DepotToolError.checksum
        }
        let unzip = Process()
        unzip.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
        unzip.arguments = ["-x", "-k", zip.path, folder.path]
        unzip.standardOutput = FileHandle.nullDevice; unzip.standardError = FileHandle.nullDevice
        do { try unzip.run() } catch { throw DepotToolError.unpack }
        unzip.waitUntilExit()
        try? files.removeItem(at: zip)
        guard unzip.terminationStatus == 0, files.fileExists(atPath: executable.path) else {
            try? files.removeItem(at: folder)
            throw DepotToolError.unpack
        }
        try? files.setAttributes([.posixPermissions: 0o755], ofItemAtPath: executable.path)
        try Data(sha256.utf8).write(to: stamp)
        return executable
    }

    /// The tool's command line. The depots are named one by one: left to choose by system and language
    /// the tool also takes a 1.5 GB extra depot. `-qr` shows a code for the Steam app, so no name or
    /// password is ever typed here; the random login ID keeps this sign-in from ending another one.
    static func arguments(language: SteamLanguage, folder: URL, loginID: UInt32) -> [String] {
        ["-app", String(SteamGame.app), "-depot"] + SteamGame.depots(language).map { String($0.id) }
            + ["-os", "windows", "-language", language.rawValue, "-qr", "-dir", folder.path, "-loginid", String(loginID)]
    }
}

/// The squares of a QR code, as the tool draws it in text.
struct QRMatrix: Equatable, Sendable {
    let size: Int
    let dark: [Bool]
    subscript(row: Int, column: Int) -> Bool { dark[row * size + column] }

    /// The code inside lines of two-character modules (`██` dark, two spaces light), without the quiet
    /// zone around it. nil unless what is left is a square of a size a QR code can have.
    init?(lines: [String]) {
        let rows = lines.map { line -> [Bool] in
            let characters = Array(line)
            return stride(from: 0, to: characters.count - 1, by: 2).map { characters[$0] == "█" }
        }
        let filled = rows.indices.filter { rows[$0].contains(true) }
        guard let top = filled.first, let bottom = filled.last else { return nil }
        let columns = rows[top...bottom].flatMap { row in row.indices.filter { row[$0] } }
        guard let left = columns.min(), let right = columns.max() else { return nil }
        let side = bottom - top + 1
        guard side == right - left + 1, side >= 21, side <= 177, (side - 17) % 4 == 0 else { return nil }
        size = side
        dark = rows[top...bottom].flatMap { row in (left...right).map { $0 < row.count && row[$0] } }
    }
}

extension QRMatrix {
    /// A code-shaped pattern for the review harness: three finder squares and fixed noise. It scans to nothing.
    static let preview: QRMatrix = {
        let n = 29
        var seed: UInt32 = 2630
        var rows: [String] = []
        for r in 0..<n {
            var line = ""
            for c in 0..<n {
                seed = (seed &* 1_103_515_245 &+ 12345) & 0x7fff_ffff
                var dark = (seed >> 16) & 1 == 1
                for (or, oc) in [(0, 0), (0, n - 7), (n - 7, 0)] {
                    let a = r - or, b = c - oc
                    if a >= -1, a <= 7, b >= -1, b <= 7 {
                        dark = a >= 0 && a < 7 && b >= 0 && b < 7 && (a == 0 || a == 6 || b == 0 || b == 6 || (2...4).contains(a) && (2...4).contains(b))
                    }
                }
                line += dark ? "██" : "  "
            }
            rows.append(line)
        }
        return QRMatrix(lines: rows)!
    }()
}

/// What a line of the tool's output means to setup.
enum DepotEvent: Equatable, Sendable {
    case connecting
    /// A code to scan, or a renewed one.
    case qr(QRMatrix)
    /// The phone approved the sign-in.
    case signedIn
    /// Steam says the account has no licence for the game.
    case notOwned
    case signInFailed(String)
    /// The files of this depot follow.
    case depot(Int)
    /// A file is finished; the percentage counts the files of its depot.
    case file(percent: Double, path: String)
    /// A file is being made room for or checked against what is on disk.
    case checking(path: String)
    /// The tool's closing line.
    case finished
}

/// Reads the tool's output line by line. The wording is DepotDownloader 3.4.0's; a line it doesn't
/// know means nothing, so a newer wording degrades to less detail and never to a wrong state.
struct DepotOutputParser: Sendable {
    private var matrix: [String]?
    private var leading = 0
    private var awaitingApproval = false
    private var signedIn = false
    private var failed = false

    private static func isMatrix(_ line: String) -> Bool {
        !line.isEmpty && line.count % 2 == 0 && line.allSatisfy { $0 == " " || $0 == "█" }
    }
    private mutating func closeMatrix() -> [DepotEvent] {
        guard let lines = matrix else { return [] }
        matrix = nil; leading = 0
        return QRMatrix(lines: lines).map { [.qr($0)] } ?? []
    }

    /// Nothing more has come for a moment: a code whose last rows have been printed is complete.
    mutating func idle() -> [DepotEvent] {
        guard let lines = matrix, lines.contains(where: { $0.contains("█") }), lines.last?.contains("█") == false else { return [] }
        return closeMatrix()
    }

    mutating func feed(_ line: String) -> [DepotEvent] {
        if let lines = matrix {
            if Self.isMatrix(line) {
                matrix = lines + [line]
                let blank = !line.contains("█"), started = lines.contains { $0.contains("█") }
                if !started {
                    if blank { leading += 1 }
                    return []
                }
                // The quiet zone under the code is as tall as the one above it: its last row ends the code.
                let trailing = (lines + [line]).reversed().prefix { !$0.contains("█") }.count
                return blank && leading > 0 && trailing >= leading ? closeMatrix() : []
            }
            let events = closeMatrix()
            return events + feed(line)
        }
        let text = line.trimmingCharacters(in: .whitespaces)
        if text.isEmpty { return [] }
        if text.contains("Use the Steam Mobile App to sign in with this QR code") {
            matrix = []; leading = 0; awaitingApproval = true
            return []
        }
        if text.hasPrefix("Connecting to Steam3") { return [.connecting] }
        if text.contains("is not available from this account") { return [.notOwned] }
        if !failed, text.contains("Failed to authenticate with Steam") || text.contains("Unable to get steam3 credentials")
            || text.contains("InitializeSteam failed") || text.contains("Timeout connecting to Steam3") {
            failed = true
            return [.signInFailed(text)]
        }
        var events: [DepotEvent] = []
        let proof = text.contains("licenses for account") || text.hasPrefix("Using Steam3 suggested CellID")
            || text.hasPrefix("Processing depot") || text.hasPrefix("Downloading depot")
        if !signedIn, awaitingApproval, proof || text == "Done!" {
            signedIn = true
            events.append(.signedIn)
        }
        if text.hasPrefix("Downloading depot "), let id = Int(text.dropFirst("Downloading depot ".count).prefix { $0.isNumber }) {
            events.append(.depot(id))
        } else if text.hasPrefix("Pre-allocating ") {
            events.append(.checking(path: String(text.dropFirst("Pre-allocating ".count))))
        } else if text.hasPrefix("Validating ") {
            events.append(.checking(path: String(text.dropFirst("Validating ".count))))
        } else if text.hasPrefix("Total downloaded:") {
            events.append(.finished)
        } else if let mark = text.firstIndex(of: "%"), let percent = Double(text[..<mark]), (0...100).contains(percent),
                  text[mark...].hasPrefix("% ") {
            events.append(.file(percent: percent, path: String(text[text.index(mark, offsetBy: 2)...])))
        }
        return events
    }
}

/// How far a download is, in bytes. The tool only says which files of a depot are finished, so the
/// bar moves between those lines by what the tool has written to disk since the last one.
struct SteamProgress: Equatable, Sendable {
    private let depots: [SteamGame.Depot]
    private var current = 0
    private var percent = 0.0
    private var written: Int64 = 0
    private var writtenAtLastFile: Int64 = 0
    private var complete = false
    private(set) var done: Int64 = 0
    let total: Int64

    init(depots: [SteamGame.Depot]) {
        self.depots = depots
        total = depots.reduce(0) { $0 + $1.bytes }
    }
    mutating func depot(_ id: Int) {
        guard let index = depots.firstIndex(where: { $0.id == id }), index != current else { return }
        current = index; percent = 0
        settle()
    }
    mutating func file(percent value: Double) {
        // A depot the tool didn't announce: its count of finished files starts over.
        if value + 20 < percent, current + 1 < depots.count { current += 1 }
        percent = min(100, max(0, value))
        writtenAtLastFile = written
        settle()
    }
    /// The tool's bytes written so far, as the system counts them for its process.
    mutating func wrote(_ bytes: Int64) {
        written = max(written, bytes)
        settle()
    }
    mutating func finish() {
        complete = true
        done = total
    }
    private mutating func settle() {
        guard !complete, !depots.isEmpty else { return }
        let before = depots[..<current].reduce(Int64(0)) { $0 + $1.bytes }
        let byFiles = before + Int64(Double(depots[current].bytes) * percent / 100)
        let ceiling = total - max(1, total / 500)
        done = max(done, min(ceiling, byFiles + max(0, written - writtenAtLastFile)))
    }
}

/// One run of the download tool. It gets a home folder of its own, so the sign-in it stores lands
/// there and goes when the folder does; what it prints comes back as events on the main actor.
@MainActor final class DepotRun {
    enum Event: Sendable { case output(DepotEvent), exited(status: Int32) }
    private let process = Process()
    private let onEvent: @MainActor (Event) -> Void
    private var parser = DepotOutputParser()
    private var pending = Data()
    private var reader: Task<Void, Never>?
    private var quiet: Task<Void, Never>?
    /// What the tool printed, newest last, without the lines of a sign-in code.
    private(set) var log: [String] = []

    init(tool: URL, arguments: [String], home: URL, onEvent: @escaping @MainActor (Event) -> Void) {
        self.onEvent = onEvent
        process.executableURL = tool
        process.arguments = arguments
        process.currentDirectoryURL = home
        process.environment = ["HOME": home.path, "CFFIXED_USER_HOME": home.path, "TMPDIR": home.appendingPathComponent("tmp").path + "/",
                               "LC_ALL": "en_US.UTF-8", "PATH": "/usr/bin:/bin:/usr/sbin:/sbin"]
        process.standardInput = FileHandle.nullDevice
    }

    func start() throws {
        let home = process.currentDirectoryURL ?? FileManager.default.temporaryDirectory
        try FileManager.default.createDirectory(at: home.appendingPathComponent("tmp"), withIntermediateDirectories: true)
        let pipe = Pipe()
        process.standardOutput = pipe; process.standardError = pipe
        let (stream, continuation) = AsyncStream<Data>.makeStream()
        let reading = pipe.fileHandleForReading
        reading.readabilityHandler = { handle in
            let data = handle.availableData
            if data.isEmpty {
                handle.readabilityHandler = nil
                continuation.finish()
            } else {
                continuation.yield(data)
            }
        }
        // Should the pipe stay open past the tool's end, its exit still ends the run.
        process.terminationHandler = { _ in
            DispatchQueue.global().asyncAfter(deadline: .now() + 0.5) { continuation.finish() }
        }
        do { try process.run() } catch {
            reading.readabilityHandler = nil
            continuation.finish()
            throw error
        }
        reader = Task { [weak self] in
            for await data in stream { self?.consume(data) }
            guard let self else { return }
            reading.readabilityHandler = nil
            while self.process.isRunning { try? await Task.sleep(for: .milliseconds(40)) }
            self.quiet?.cancel()
            if !self.pending.isEmpty { self.line(String(decoding: self.pending, as: UTF8.self)); self.pending = Data() }
            self.deliver(self.parser.idle())
            self.onEvent(.exited(status: self.process.terminationReason == .exit ? self.process.terminationStatus : -1))
        }
    }

    /// Ends the tool. It has no clean-up of its own to wait for: what it downloaded so far is on disk.
    func stop() { if process.isRunning { process.terminate() } }
    var isRunning: Bool { process.isRunning }
    /// Returns once the tool has gone and its last line has been read.
    func finished() async { await reader?.value }
    /// For quitting: ends the tool and waits for it, without going back to the run loop.
    func stopNow() {
        guard process.isRunning else { return }
        process.terminate()
        process.waitUntilExit()
    }

    /// Bytes the tool has written to disk since it started: the system's own count for the process.
    var bytesWritten: Int64 {
        guard process.isRunning else { return 0 }
        var info = rusage_info_current()
        let result = withUnsafeMutablePointer(to: &info) {
            $0.withMemoryRebound(to: rusage_info_t?.self, capacity: 1) { proc_pid_rusage(process.processIdentifier, RUSAGE_INFO_CURRENT, $0) }
        }
        return result == 0 ? Int64(clamping: info.ri_diskio_byteswritten) : 0
    }

    private func consume(_ data: Data) {
        pending.append(data)
        while let newline = pending.firstIndex(of: 0x0A) {
            line(String(decoding: pending[pending.startIndex..<newline], as: UTF8.self))
            pending.removeSubrange(pending.startIndex...newline)
        }
        quiet?.cancel()
        quiet = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(250))
            guard let self, !Task.isCancelled else { return }
            self.deliver(self.parser.idle())
        }
    }
    private func line(_ raw: String) {
        let text = raw.hasSuffix("\r") ? String(raw.dropLast()) : raw
        // A sign-in code is for the screen only: it is never kept.
        if !text.contains("█"), !text.trimmingCharacters(in: .whitespaces).isEmpty {
            log.append(text)
            if log.count > 400 { log.removeFirst(log.count - 400) }
        }
        deliver(parser.feed(text))
    }
    private func deliver(_ events: [DepotEvent]) { for event in events { onEvent(.output(event)) } }
}
