import Foundation

// Pure formatting and parsing shared by the views and the unit tests (Foundation only).

enum Scenery: Int, CaseIterable, Sendable { case desert, europe, winter }

enum MapCatalog {
    private struct Entry { var name: String; var region: String; var scenery: Scenery }
    // Stock multiplayer maps by BSP name. Names follow the server browser's
    // convention; regions describe where each battle is set.
    private static let stock: [String: Entry] = [
        "toujane": Entry(name: "Toujane", region: "NORTH AFRICA", scenery: .desert),
        "matmata": Entry(name: "Matmata", region: "NORTH AFRICA", scenery: .desert),
        "decoy": Entry(name: "Decoy", region: "NORTH AFRICA", scenery: .desert),
        "carentan": Entry(name: "Carentan", region: "NORMANDY", scenery: .europe),
        "brecourt": Entry(name: "Brécourt", region: "NORMANDY", scenery: .europe),
        "dawnville": Entry(name: "Dawnville", region: "NORMANDY", scenery: .europe),
        "breakout": Entry(name: "Breakout", region: "NORMANDY", scenery: .europe),
        "farmhouse": Entry(name: "Farmhouse", region: "NORMANDY", scenery: .europe),
        "trainstation": Entry(name: "Trainstation", region: "NORMANDY", scenery: .europe),
        "burgundy": Entry(name: "Burgundy", region: "FRANCE", scenery: .europe),
        "rhine": Entry(name: "Rhine", region: "GERMANY", scenery: .europe),
        "leningrad": Entry(name: "Leningrad", region: "EASTERN FRONT", scenery: .winter),
        "railyard": Entry(name: "Railyard", region: "EASTERN FRONT", scenery: .winter),
        "harbor": Entry(name: "Harbor", region: "EASTERN FRONT", scenery: .winter),
        "downtown": Entry(name: "Downtown", region: "EASTERN FRONT", scenery: .winter),
        "stalingrad": Entry(name: "Stalingrad", region: "EASTERN FRONT", scenery: .winter),
    ]
    static var stockKeys: [String] { stock.keys.sorted() }

    /// Lowercase BSP name without the `mp_` prefix, or nil for unsafe metadata.
    static func key(_ map: String) -> String? {
        guard !map.isEmpty, map.utf8.count <= 64,
              map.utf8.allSatisfy({ (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0) || $0 == 95 }) else { return nil }
        let lower = map.lowercased()
        let key = lower.hasPrefix("mp_") ? String(lower.dropFirst(3)) : lower
        return key.isEmpty ? nil : key
    }
    static func displayName(_ map: String) -> String {
        guard let key = key(map) else { return "Unknown map" }
        if let entry = stock[key] { return entry.name }
        return key.split(separator: "_").map { $0.prefix(1).uppercased() + $0.dropFirst() }.joined(separator: " ")
    }
    static func region(_ map: String) -> String? { key(map).flatMap { stock[$0]?.region } }
    static func scenery(_ map: String) -> Scenery {
        guard let key = key(map) else { return .desert }
        if let entry = stock[key] { return entry.scenery }
        // Custom maps get a stable scene; server metadata is untrusted, so stay bounded.
        let seed = key.utf8.reduce(0) { ($0 &* 31 &+ Int($1)) & 0xffff }
        return Scenery(rawValue: seed % 3) ?? .desert
    }
}

enum GameModes {
    private static let names = ["tdm": "Team Deathmatch", "dm": "Deathmatch", "sd": "Search & Destroy",
                                "hq": "Headquarters", "ctf": "Capture the Flag"]
    static func short(_ code: String) -> String {
        let trimmed = code.trimmingCharacters(in: .whitespaces)
        return trimmed.isEmpty || trimmed == "—" ? "—" : String(trimmed.prefix(12)).uppercased()
    }
    static func long(_ code: String) -> String { names[code.lowercased()] ?? short(code) }
}

struct ServerFacts: Equatable {
    var isCoD2x: Bool
    var versionLabel: String
    var pingLevel: Int
    var occupancy: Occupancy
    var frameCap: String
    var access: String
    enum Occupancy: Equatable { case empty, open, full }

    init(_ server: GameServer) {
        let version = server.version
        isCoD2x = version.hasPrefix("1.4") || server.fields["protocol"] == "120"
        let shown = version == "1.4.x" ? "1.4" : String(version.prefix(16))
        versionLabel = isCoD2x ? "CoD2x \(shown)" : "Stock \(shown)"
        pingLevel = Self.pingLevel(server.ping)
        occupancy = server.playerCount == 0 ? .empty : (server.maxPlayers > 0 && server.playerCount >= server.maxPlayers ? .full : .open)
        if let fps = Int(server.fps), fps > 0 { frameCap = "\(fps) fps" } else { frameCap = "Not set" }
        access = server.password ? "Password" : "Open"
    }
    static func pingLevel(_ ping: Int) -> Int { ping < 40 ? 4 : ping < 60 ? 3 : ping < 80 ? 2 : 1 }
}

/// The player's frame cap (com_maxfps, 0–1000; 0 is no cap), shown as plain data.
enum FrameCap {
    /// Quick picks in Settings, with neutral captions; any other value is entered by hand.
    static let presets: [(value: Int, caption: String)] = [(125, "Low"), (250, "Default"), (333, "High"), (1000, "Engine maximum")]
    static func isPreset(_ fps: Int) -> Bool { presets.contains { $0.value == fps } }
    /// Home's status chip: the numeral (nil for no cap) and the words after it.
    static func chip(_ fps: Int) -> (numeral: String?, text: String) { fps > 0 ? ("\(fps)", "fps cap") : (nil, "No fps cap") }
    static func spoken(_ fps: Int) -> String { fps > 0 ? "Frame cap \(fps) frames per second" : "No frame cap" }
    static func customCaption(_ fps: Int) -> String { fps == 0 ? "Unlimited" : "Custom value" }
}

enum MouseMath {
    /// Distance for a full turn: counts per 360° (360 / (yaw × sensitivity)) divided by DPI, in centimetres.
    static func centimetresPer360(dpi: Double, sensitivity: Double, yaw: Double = 0.022) -> Double? {
        guard dpi.isFinite, sensitivity.isFinite, yaw.isFinite, dpi > 0, sensitivity > 0, yaw > 0 else { return nil }
        return 360 / (yaw * sensitivity) / dpi * 2.54
    }
    static func label(_ value: Double?) -> String { value.map { String(format: "%.1f", $0) } ?? "—" }
}

enum StencilDate {
    private static let months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]
    static func day(_ date: Date, calendar: Calendar = .current) -> String {
        let parts = calendar.dateComponents([.day, .month], from: date)
        return String(format: "%02d ", parts.day ?? 1) + months[((parts.month ?? 1) - 1 + 12) % 12]
    }
    static func dayAndTime(_ date: Date, calendar: Calendar = .current) -> String {
        let parts = calendar.dateComponents([.hour, .minute], from: date)
        return day(date, calendar: calendar) + String(format: " %02d:%02d", parts.hour ?? 0, parts.minute ?? 0)
    }
}

struct Dispatch: Codable, Equatable, Sendable {
    var stamp: String
    var title: String
    var summary: String
    var link: String?
    static let bundled = [
        Dispatch(stamp: "04 OCT", title: "CoD2 Silicon 0.1.0", summary: "First public release: native Apple silicon client with CoD2x 1.4 compatibility.",
                 link: "https://github.com/STiXzoOR/cod2-silicon/releases"),
        Dispatch(stamp: "IN FIELD", title: "Native Metal renderer", summary: "Metal 4 on macOS 26 and later, with MetalFX and HDR options. Planned for 0.3.", link: nil),
    ]
}

enum ReleaseNotes {
    struct Feed: Equatable { var dispatches: [Dispatch]; var latestVersion: String? }
    private struct Release: Decodable {
        var tag_name: String?
        var name: String?
        var body: String?
        var published_at: String?
        var html_url: String?
        var draft: Bool?
        var prerelease: Bool?
    }
    /// Parses GitHub's `GET /repos/{owner}/{repo}/releases` response. Notify only: nothing is downloaded.
    static func parse(_ data: Data, limit: Int = 3, calendar: Calendar = .current) throws -> Feed {
        guard data.count <= 1_000_000 else { throw LauncherError(message: "The release feed is too large.") }
        let releases = try JSONDecoder().decode([Release].self, from: data).prefix(50).filter { $0.draft != true }
        var dispatches: [Dispatch] = []
        for release in releases.prefix(limit) {
            let tag = String((release.tag_name ?? "").prefix(32))
            let title = String((release.name ?? "").trimmingCharacters(in: .whitespacesAndNewlines).prefix(80))
            var stamp = "NEW"
            if let published = release.published_at, let date = ISO8601DateFormatter().date(from: published) { stamp = StencilDate.day(date, calendar: calendar) }
            let link = release.html_url.flatMap { $0.hasPrefix("https://github.com/") ? String($0.prefix(256)) : nil }
            dispatches.append(Dispatch(stamp: stamp, title: title.isEmpty ? "CoD2 Silicon \(version(tag))" : title,
                                       summary: summary(release.body ?? ""), link: link))
        }
        let latest = releases.first { $0.prerelease != true }.flatMap { $0.tag_name }.map(version)
        return Feed(dispatches: dispatches, latestVersion: latest)
    }
    static func version(_ tag: String) -> String {
        String(tag.trimmingCharacters(in: .whitespaces).drop { $0 == "v" || $0 == "V" }.prefix(32))
    }
    static func isNewer(_ candidate: String, than current: String) -> Bool {
        candidate.compare(current, options: .numeric) == .orderedDescending
    }
    /// First prose paragraph of a Markdown body, without markup, at most `length` characters.
    static func summary(_ body: String, length: Int = 160) -> String {
        let lines = body.prefix(65_536).replacingOccurrences(of: "\r\n", with: "\n").components(separatedBy: "\n")
        var paragraph: [String] = []
        for raw in lines {
            var line = raw.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("#") || line.hasPrefix("```") || line.hasPrefix("|") || line.hasPrefix(">") || line.hasPrefix("<") {
                if !paragraph.isEmpty { break } else { continue }
            }
            if line.isEmpty { if !paragraph.isEmpty { break } else { continue } }
            for marker in ["- ", "* ", "+ "] where line.hasPrefix(marker) { line.removeFirst(2) }
            paragraph.append(line)
        }
        var text = paragraph.joined(separator: " ")
        text = text.replacingOccurrences(of: #"!?\[([^\]]*)\]\([^)]*\)"#, with: "$1", options: .regularExpression)
        text = text.replacingOccurrences(of: #"[*`]+"#, with: "", options: .regularExpression)
        text = text.replacingOccurrences(of: #"\s+"#, with: " ", options: .regularExpression).trimmingCharacters(in: .whitespaces)
        guard text.count > length else { return text }
        let cut = text.prefix(length)
        if let end = cut.lastIndex(where: { $0 == "." }), cut.distance(from: cut.startIndex, to: end) > length / 2 { return String(cut[...end]) }
        let words = cut.split(separator: " ").dropLast()
        return words.joined(separator: " ") + "…"
    }
}

struct MediaFacts: Equatable {
    var map: String?
    var stamp: String
    var size: String
    init(name: String, date: Date?, bytes: Int?, calendar: Calendar = .current) {
        map = Self.mapHint(name)
        stamp = date.map { StencilDate.dayAndTime($0, calendar: calendar) } ?? ""
        size = bytes.map(Self.sizeLabel) ?? ""
    }
    /// Recognizes a stock map name in a file name such as `toujane_tdm_1004_2114.dm_1`.
    static func mapHint(_ name: String) -> String? {
        let tokens = name.lowercased().split { !$0.isLetter }.map(String.init)
        return tokens.first { MapCatalog.stockKeys.contains($0) }
    }
    static func sizeLabel(_ bytes: Int) -> String {
        let value = Double(max(bytes, 0))
        if value < 1_000 { return "\(Int(value)) B" }
        if value < 1_000_000 { return String(format: "%.0f KB", value / 1_000) }
        if value < 1_000_000_000 { return String(format: "%.1f MB", value / 1_000_000) }
        return String(format: "%.1f GB", value / 1_000_000_000)
    }
}

enum KeyFormat {
    /// Up to 20 key characters, uppercased, in groups of four for display.
    static func groups(_ input: String) -> [String] {
        let characters = Array(input.uppercased().filter { $0.isASCII && ($0.isLetter || $0.isNumber) }.prefix(20))
        return stride(from: 0, to: characters.count, by: 4).map { String(characters[$0..<min($0 + 4, characters.count)]) }
    }
    static func display(_ input: String) -> String { groups(input).joined(separator: " ") }
    /// The four stamped lines of the setup dog tag.
    static func tag(_ input: String) -> [String] {
        let g = groups(input)
        func group(_ index: Int) -> String { index < g.count ? g[index].padding(toLength: 4, withPad: "·", startingAt: 0) : "····" }
        return ["\(group(0)) \(group(1))", "\(group(2)) \(group(3))", group(4), "COD2 · 1.3"]
    }
}

enum PlayerProfile {
    /// Shown when the engine has not saved a name yet; the launcher never invents one.
    static let unnamed = "Player"
    /// The engine's active profile from `main/players/active.txt` (its first token), if it is a safe folder name.
    static func activeProfile(_ text: String) -> String? {
        let trimmed = text.prefix(256).trimmingCharacters(in: .whitespacesAndNewlines)
        let token: Substring
        if trimmed.hasPrefix("\"") { token = trimmed.dropFirst().prefix { $0 != "\"" } }
        else { token = trimmed.prefix { !$0.isWhitespace } }
        guard !token.isEmpty, token.count <= 64, token != ".", token != "..",
              !token.contains(where: { "/\\:".contains($0) || $0.isNewline || ($0.asciiValue ?? 32) < 32 }) else { return nil }
        return String(token)
    }
    /// The `name` dvar from the engine's own config. Read-only: the launcher never rewrites config_mp.cfg.
    static func name(config: String) -> String? {
        for line in config.prefix(262_144).components(separatedBy: .newlines).reversed() {
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            guard trimmed.lowercased().hasPrefix("seta name ") || trimmed.lowercased().hasPrefix("set name ") else { continue }
            var value = trimmed.drop { $0 != " " }.dropFirst().drop { $0 != " " }.trimmingCharacters(in: .whitespaces)
            if value.hasPrefix("\""), value.hasSuffix("\""), value.count >= 2 { value = String(value.dropFirst().dropLast()) }
            let bounded = String(value.prefix(64))
            return QuakeColors.plain(bounded).trimmingCharacters(in: .whitespaces).isEmpty ? nil : bounded
        }
        return nil
    }
}

/// Scrims that keep hero text at 4.5:1 over the player's own loading screens.
enum ScrimMath {
    static func linear(_ channel: UInt8) -> Double {
        let c = Double(channel) / 255
        return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)
    }
    static func luminance(_ r: UInt8, _ g: UInt8, _ b: UInt8) -> Double { 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b) }
    static func contrast(_ a: Double, _ b: Double) -> Double { (max(a, b) + 0.05) / (min(a, b) + 0.05) }
    static func encode(_ linear: Double) -> Double { linear <= 0.0031308 ? 12.92 * linear : 1.055 * pow(linear, 1 / 2.4) - 0.055 }
    static func decode(_ encoded: Double) -> Double { encoded <= 0.04045 ? encoded / 12.92 : pow((encoded + 0.055) / 1.055, 2.4) }
    /// Low and high percentile luminance (2nd and 98th by default) of straight RGBA pixels inside a normalized region.
    static func percentiles(rgba: Data, width: Int, height: Int, region: (x: Double, y: Double, width: Double, height: Double),
                            lower: Double = 0.02, upper: Double = 0.98) -> (low: Double, high: Double)? {
        guard width > 0, height > 0, rgba.count == width * height * 4 else { return nil }
        let x0 = max(0, Int(Double(width) * region.x)), x1 = min(width, Int(Double(width) * (region.x + region.width)))
        let y0 = max(0, Int(Double(height) * region.y)), y1 = min(height, Int(Double(height) * (region.y + region.height)))
        guard x1 > x0, y1 > y0 else { return nil }
        let step = max(1, Int(sqrt(Double((x1 - x0) * (y1 - y0)) / 4096)))
        var values: [Double] = []
        rgba.withUnsafeBytes { raw in
            let p = raw.bindMemory(to: UInt8.self)
            for y in stride(from: y0, to: y1, by: step) {
                for x in stride(from: x0, to: x1, by: step) {
                    let o = (y * width + x) * 4
                    values.append(luminance(p[o], p[o + 1], p[o + 2]))
                }
            }
        }
        values.sort()
        let index = { (fraction: Double) in min(values.count - 1, max(0, Int(Double(values.count) * fraction))) }
        return (values[index(lower)], values[index(upper)])
    }
    // SwiftUI composites in gamma-encoded sRGB, so these blend there and judge the result in
    // linear light. A slight overshoot covers the gap between a grey estimate and real colour.
    /// Opacity of a black scrim so the brightest likely pixel stays behind light text at `target`.
    static func darkScrim(high: Double, text: Double, target: Double = 4.5) -> Double {
        let allowed = (text + 0.05) / (target * 1.04) - 0.05
        guard high > allowed else { return 0.35 }
        guard allowed > 0 else { return 0.92 }
        return min(0.92, max(0.35, 1 - encode(allowed) / encode(high)))
    }
    /// Opacity of a paper wash so the darkest likely pixel stays behind dark text at `target`.
    static func lightWash(low: Double, text: Double, paper: Double, target: Double = 4.5) -> Double {
        let needed = (text + 0.05) * target * 1.04 - 0.05
        guard low < needed else { return 0.3 }
        guard paper > needed else { return 0.95 }
        return min(0.95, max(0.3, (encode(needed) - encode(low)) / (encode(paper) - encode(low))))
    }
}
