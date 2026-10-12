import Foundation
import Darwin

@main struct MatchSummariesTests {
    static func main() throws {
        let scratch = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        try FileManager.default.createDirectory(at: scratch.appendingPathComponent("main/stats"), withIntermediateDirectories: true)
        var settings = GameSettings()
        precondition(settings.matchSummariesEnabled)
        let legacy = try JSONDecoder().decode(GameSettings.self, from: Data("{}".utf8))
        precondition(legacy.matchSummariesEnabled)
        settings.matchSummariesEnabled = false
        let persisted = try JSONDecoder().decode(GameSettings.self, from: JSONEncoder().encode(settings))
        precondition(!persisted.matchSummariesEnabled)
        for migrated in [false, true] {
            let dvars = try settings.launchDvars(migrated: migrated)
            precondition(dvars.contains { $0.0 == "cl_matchSummaries" && $0.1 == "0" })
        }
        settings.advanced = "CL_MATCHSUMMARIES 1"
        precondition((try? settings.dvars()) == nil, "Advanced options cannot override privacy")
        let row = #"{"schemaVersion":1,"endedAt":"2026-10-12T00:00:00Z","endReason":"disconnect","map":"mp_toujane","mode":"sd","durationSeconds":30,"serverAddress":"192.0.2.1:28960","ruleHints":{"g_competitive":"1"},"frameCount":100,"frameTimeP50MS":3,"frameTimeP99MS":7,"frameTimeMaxMS":20,"framesUnderCap":98,"capSampleFrames":100,"capFPS":333,"pingAverageMS":40,"pingMaximumMS":50,"pingSamples":3,"packetsReceived":3,"packetsLost":2,"packetLossPercent":40,"snapshotsReceived":4}"#
        let path = scratch.appendingPathComponent("main/stats/matches.jsonl")
        try (Array(repeating: row, count: 205).joined(separator: "\n") + "\ninvalid\n{\"partial\":").write(to: path, atomically: true, encoding: .utf8)
        let summaries = try MatchSummaries.read(homePath: scratch)
        precondition(summaries.count == 200)
        let match = summaries.last!
        precondition(match.frameTimeP99MS == 7 && match.pingAverageMS == 40 && match.packetLossPercent == 40)
        precondition(match.map == "mp_toujane" && match.capFPS == 333 && match.ruleHints["g_competitive"] == "1")
        try row.write(to: path, atomically: true, encoding: .utf8)
        let partial = try MatchSummaries.read(homePath: scratch)
        precondition(partial.isEmpty, "A final record without newline is not committed")
        try (row.replacingOccurrences(of: "\"capSampleFrames\":100", with: "\"capSampleFrames\":200") + "\n").write(to: path, atomically: true, encoding: .utf8)
        let inconsistent = try MatchSummaries.read(homePath: scratch)
        precondition(inconsistent.isEmpty, "Impossible totals cannot become measurements")
        let oversized = String(repeating: "x", count: MatchSummaries.maximumBytes + 100) + "\n" + row + "\n"
        try oversized.write(to: path, atomically: true, encoding: .utf8)
        let tail = try MatchSummaries.read(homePath: scratch)
        precondition(tail.count == 1, "Reader considers a bounded complete tail of an oversized file")
        try numericContract(row: row, path: path, home: scratch)
        try accessFailure(row: row, path: path, home: scratch)
        try FileManager.default.removeItem(at: path)
        let outside = scratch.appendingPathComponent("outside.jsonl")
        try (row + "\n").write(to: outside, atomically: true, encoding: .utf8)
        try FileManager.default.createSymbolicLink(at: path, withDestinationURL: outside)
        let linked = try MatchSummaries.read(homePath: scratch)
        precondition(linked.isEmpty, "Reader stays within ordinary stats files")
        try FileManager.default.removeItem(at: path)
        let missing = scratch.appendingPathComponent("missing")
        let absent = try MatchSummaries.read(homePath: missing)
        precondition(absent.isEmpty)
        try missingAndRejectedPaths(path: path, home: scratch)
        print("PASS: summary reader/retention, numeric ranges and packet totals, access-denial errors, missing/rejected paths, partial-line handling, persisted/default privacy and launch opt-out")
    }

    /// Access denial must reach Check-up's failed state, rather than look like no matches.
    static func accessFailure(row: String, path: URL, home: URL) throws {
        precondition(geteuid() != 0, "Access-denial fixtures require an unprivileged process")
        try (row + "\n").write(to: path, atomically: true, encoding: .utf8)
        let targets = [("summary file", path), ("stats directory", path.deletingLastPathComponent()),
                       ("main directory", home.appendingPathComponent("main")), ("home directory", home)]
        var failures: [String] = []
        for (name, target) in targets {
            let error = try deniedRead(target: target, home: home)
            if error?.domain != NSPOSIXErrorDomain || error?.code != Int(EACCES) { failures.append(name) }
        }
        if !failures.isEmpty {
            FileHandle.standardError.write(Data(("FAIL: access denial was hidden for " + failures.joined(separator: ", ") + "\n").utf8))
            throw NSError(domain: "MatchSummariesTests", code: failures.count,
                          userInfo: [NSLocalizedDescriptionKey: "Access-denial errors were hidden"])
        }
        let restored = try MatchSummaries.read(homePath: home)
        precondition(restored.count == 1, "The restored fixture remains readable")
    }

    /// Restore permissions before the caller asserts, including when the production read throws.
    static func deniedRead(target: URL, home: URL) throws -> NSError? {
        var status = stat()
        guard lstat(target.path, &status) == 0, chmod(target.path, 0) == 0 else {
            throw NSError(domain: "MatchSummariesTests", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "Cannot prepare access-denial fixture"])
        }
        defer { precondition(chmod(target.path, status.st_mode & 0o7777) == 0, "Fixture permissions are restored") }
        do { _ = try MatchSummaries.read(homePath: home); return nil }
        catch { return error as NSError }
    }

    static func missingAndRejectedPaths(path: URL, home: URL) throws {
        let absentFile = try MatchSummaries.read(homePath: home)
        precondition(absentFile.isEmpty, "An absent summary file is an empty history")
        try FileManager.default.createDirectory(at: path, withIntermediateDirectories: false)
        let directory = try MatchSummaries.read(homePath: home)
        precondition(directory.isEmpty, "A nonregular summary file is rejected")
        try FileManager.default.removeItem(at: path)
        precondition(mkfifo(path.path, 0o600) == 0)
        let fifo = try MatchSummaries.read(homePath: home)
        precondition(fifo.isEmpty, "A FIFO is rejected without waiting for a writer")
        try FileManager.default.removeItem(at: path)

        let empty = home.appendingPathComponent("empty-home")
        try FileManager.default.createDirectory(at: empty, withIntermediateDirectories: false)
        let absentMain = try MatchSummaries.read(homePath: empty)
        precondition(absentMain.isEmpty, "An absent main directory is an empty history")
        try FileManager.default.createDirectory(at: empty.appendingPathComponent("main"), withIntermediateDirectories: false)
        let absentStats = try MatchSummaries.read(homePath: empty)
        precondition(absentStats.isEmpty, "An absent stats directory is an empty history")

        let linkedHome = home.appendingPathComponent("linked-home")
        try FileManager.default.createSymbolicLink(at: linkedHome, withDestinationURL: home)
        let rejectedHome = try MatchSummaries.read(homePath: linkedHome)
        precondition(rejectedHome.isEmpty, "A symlink home is rejected")
        let linkedMain = home.appendingPathComponent("linked-main")
        try FileManager.default.createDirectory(at: linkedMain, withIntermediateDirectories: false)
        try FileManager.default.createSymbolicLink(at: linkedMain.appendingPathComponent("main"), withDestinationURL: home.appendingPathComponent("main"))
        let rejectedMain = try MatchSummaries.read(homePath: linkedMain)
        precondition(rejectedMain.isEmpty, "A symlink main directory is rejected")
        let linkedStats = home.appendingPathComponent("linked-stats")
        try FileManager.default.createDirectory(at: linkedStats.appendingPathComponent("main"), withIntermediateDirectories: true)
        try FileManager.default.createSymbolicLink(at: linkedStats.appendingPathComponent("main/stats"), withDestinationURL: home.appendingPathComponent("main/stats"))
        let rejectedStats = try MatchSummaries.read(homePath: linkedStats)
        precondition(rejectedStats.isEmpty, "A symlink stats directory is rejected")
    }

    static func numericContract(row: String, path: URL, home: URL) throws {
        func variant(_ values: [(String, String)]) -> String {
            values.reduce(row) { text, field in
                let start = text.range(of: "\"\(field.0)\":")!.upperBound
                let end = text[start...].firstIndex(of: ",")!
                return text.replacingCharacters(in: start..<end, with: field.1)
            }
        }
        let malformed: [(String, String)] = [
            ("duration exceeds UInt64 milliseconds", variant([("durationSeconds", "18446744073709556")])),
            ("duration would trap Int rendering", variant([("durationSeconds", "1e300")])),
            ("frame values exceed safe formatting range", variant([("frameTimeP50MS", "1e300"), ("frameTimeP99MS", "1e300"), ("frameTimeMaxMS", "1e300")])),
            ("frame maximum exceeds producer range", variant([("frameTimeMaxMS", "2147483648")])),
            ("ping values exceed safe formatting range", variant([("pingAverageMS", "1e300"), ("pingMaximumMS", "1e300")])),
            ("ping maximum exceeds producer range", variant([("pingMaximumMS", "2147483648")])),
            ("histogram limit exceeds safe formatting range", row.replacingOccurrences(of: "\"framesUnderCap\":", with: "\"frameTimeHistogramLimitMS\":1e300,\"framesUnderCap\":")),
            ("histogram limit must be positive", row.replacingOccurrences(of: "\"framesUnderCap\":", with: "\"frameTimeHistogramLimitMS\":0,\"framesUnderCap\":")),
            ("two maximum packet counters overflow", variant([("packetsReceived", "18446744073709551615"), ("packetsLost", "18446744073709551615"), ("packetLossPercent", "50")])),
            ("one additional packet overflows", variant([("packetsReceived", "18446744073709551615"), ("packetsLost", "1"), ("packetLossPercent", "0")]))
        ]
        var failures: [String] = []
        for (name, candidate) in malformed {
            try (row + "\n" + candidate + "\n" + row + "\n").write(to: path, atomically: true, encoding: .utf8)
            let summaries = try MatchSummaries.read(homePath: home)
            if summaries.count != 2 || summaries.contains(where: { $0.durationSeconds != 30 || $0.frameTimeMaxMS != 20 || $0.pingMaximumMS != 50 || $0.frameTimeHistogramLimitMS != nil || $0.packetsReceived != 3 }) {
                failures.append(name)
            }
        }
        let boundary = variant([("durationSeconds", "18446744073709552"), ("frameTimeP50MS", "2147483647"), ("frameTimeP99MS", "2147483647"), ("frameTimeMaxMS", "2147483647"), ("pingAverageMS", "2147483647"), ("pingMaximumMS", "2147483647"), ("packetsReceived", "1"), ("packetsLost", "18446744073709551614"), ("packetLossPercent", "100")])
        let fractional = variant([("durationSeconds", "0.125"), ("frameTimeP50MS", "3.25"), ("frameTimeP99MS", "5.2"), ("frameTimeMaxMS", "20.5"), ("pingAverageMS", "40.333"), ("pingMaximumMS", "50.5")]).replacingOccurrences(of: "\"framesUnderCap\":", with: "\"frameTimeHistogramLimitMS\":250,\"framesUnderCap\":")
        try (boundary + "\n" + fractional + "\n").write(to: path, atomically: true, encoding: .utf8)
        let accepted = try MatchSummaries.read(homePath: home)
        precondition(accepted.count == 2, "Representable boundaries and fractional measurements remain valid")
        precondition(Int(accepted[0].durationSeconds.rounded()) == 18446744073709552)
        precondition(accepted[0].packetsReceived + accepted[0].packetsLost == UInt64.max)
        precondition(accepted[0].frameTimeMaxMS == 2147483647 && accepted[0].pingMaximumMS == 2147483647)
        precondition((accepted[0].frameTimeMaxMS! * 10).rounded().isFinite)
        precondition(accepted[1].frameTimeP99MS == 5.2 && accepted[1].pingAverageMS == 40.333 && accepted[1].pingMaximumMS == 50.5)
        precondition(accepted[1].frameTimeHistogramLimitMS == 250 && accepted[1].frameQuantilesAreExact)
        if !failures.isEmpty {
            FileHandle.standardError.write(Data((failures.map { "FAIL: \($0)" }.joined(separator: "\n") + "\n").utf8))
            throw NSError(domain: "MatchSummariesTests", code: failures.count, userInfo: [NSLocalizedDescriptionKey: "Malformed numeric records were accepted"])
        }
    }
}
