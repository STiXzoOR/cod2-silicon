import Foundation

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
        print("PASS: summary reader/retention, partial-line handling, persisted/default privacy and launch opt-out")
    }
}
