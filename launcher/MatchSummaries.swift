import Foundation
import Darwin

/// Local engine observations. Nil means the session had no sample for that metric.
struct MatchSummary: Codable, Equatable, Sendable {
    let schemaVersion: Int
    let endedAt: String
    let endReason: String
    let map: String
    let mode: String
    let durationSeconds: Double
    let serverAddress: String
    let ruleHints: [String: String]
    let frameCount: UInt64
    let frameTimeP50MS: Double?
    let frameTimeP99MS: Double?
    let frameTimeMaxMS: Double?
    let frameTimeHistogramLimitMS: Double?
    let framesUnderCap: UInt64?
    let capSampleFrames: UInt64
    let capFPS: Int
    let pingAverageMS: Double?
    let pingMaximumMS: Double?
    let pingSamples: UInt64
    let packetsReceived: UInt64
    let packetsLost: UInt64
    let packetLossPercent: Double?
    let packetLossSource: String?
    let snapshotsReceived: UInt64

    /// Millisecond samples up to this bound are exact; longer quantiles use the match maximum.
    var frameQuantilesAreExact: Bool {
        guard let limit = frameTimeHistogramLimitMS, let p99 = frameTimeP99MS else { return false }
        return p99 < limit
    }

    fileprivate var valid: Bool {
        let measured = [durationSeconds, frameTimeP50MS, frameTimeP99MS, frameTimeMaxMS,
                        frameTimeHistogramLimitMS, pingAverageMS, pingMaximumMS, packetLossPercent].compactMap { $0 }
        // The producer converts UInt64 elapsed milliseconds to seconds and stores
        // frame/ping samples in signed 32-bit integers. Keep their safe ranges
        // while preserving fractional measurements accepted by this schema.
        let maximumSampleMS = Double(Int32.max)
        guard schemaVersion == 1, measured.allSatisfy({ $0.isFinite && $0 >= 0 }),
              durationSeconds <= Double(UInt64.max) / 1000,
              (0...1000).contains(capFPS), capSampleFrames <= frameCount,
              pingSamples <= snapshotsReceived else { return false }
        if let limit = frameTimeHistogramLimitMS {
            guard limit > 0, limit <= maximumSampleMS else { return false }
        }
        if frameCount == 0 {
            guard frameTimeP50MS == nil, frameTimeP99MS == nil, frameTimeMaxMS == nil else { return false }
        } else {
            guard let p50 = frameTimeP50MS, let p99 = frameTimeP99MS, let max = frameTimeMaxMS,
                  p50 <= p99, p99 <= max, max <= maximumSampleMS else { return false }
        }
        if capSampleFrames == 0 {
            guard framesUnderCap == nil else { return false }
        } else {
            guard let framesUnderCap, framesUnderCap <= capSampleFrames else { return false }
        }
        if pingSamples == 0 {
            guard pingAverageMS == nil, pingMaximumMS == nil else { return false }
        } else {
            guard let average = pingAverageMS, let max = pingMaximumMS,
                  average <= max, max <= maximumSampleMS else { return false }
        }
        if packetsReceived == 0 {
            return packetsLost == 0 && packetLossPercent == nil
        }
        let (packetTotal, overflow) = packetsReceived.addingReportingOverflow(packetsLost)
        guard !overflow, let loss = packetLossPercent, loss <= 100 else { return false }
        let expected = 100 * Double(packetsLost) / Double(packetTotal)
        return abs(loss - expected) <= 0.001
    }
}

enum MatchSummaries {
    static let recordLimit = 200
    static let maximumBytes = recordLimit * 8192

    /// Oldest first. A partial final line or malformed record cannot become measured advice.
    static func read(homePath: URL) throws -> [MatchSummary] {
        var directory = open(homePath.path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW)
        guard directory >= 0 else { return try openFailure(errno) }
        for component in ["main", "stats"] {
            let child = openat(directory, component, O_RDONLY | O_DIRECTORY | O_NOFOLLOW)
            let code = errno
            close(directory)
            guard child >= 0 else { return try openFailure(code) }
            directory = child
        }
        let fd = openat(directory, "matches.jsonl", O_RDONLY | O_NOFOLLOW | O_NONBLOCK)
        let code = errno
        close(directory)
        guard fd >= 0 else { return try openFailure(code) }
        var status = stat()
        guard fstat(fd, &status) == 0 else {
            let code = errno
            close(fd)
            throw NSError(domain: NSPOSIXErrorDomain, code: Int(code))
        }
        guard (status.st_mode & S_IFMT) == S_IFREG, status.st_size >= 0 else {
            close(fd)
            return []
        }
        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        defer { try? handle.close() }
        let offset = max(0, status.st_size - off_t(maximumBytes))
        try handle.seek(toOffset: UInt64(offset))
        var data = try handle.read(upToCount: maximumBytes) ?? Data()
        if offset > 0 {
            guard let newline = data.firstIndex(of: 10) else { return [] }
            data.removeSubrange(...newline)
        }
        guard let finalNewline = data.lastIndex(of: 10) else { return [] }
        data.removeSubrange(data.index(after: finalNewline)..<data.endIndex)
        let decoder = JSONDecoder()
        var summaries: [MatchSummary] = []
        for line in data.split(separator: 10) {
            guard let summary = try? decoder.decode(MatchSummary.self, from: Data(line)),
                  summary.valid else { continue }
            summaries.append(summary)
            if summaries.count > recordLimit { summaries.removeFirst() }
        }
        return summaries
    }

    /// Missing paths and deliberately rejected links/non-directories have no readable history.
    private static func openFailure(_ code: Int32) throws -> [MatchSummary] {
        switch code {
        case ENOENT, ENOTDIR, ELOOP: return []
        default: throw NSError(domain: NSPOSIXErrorDomain, code: Int(code))
        }
    }
}
