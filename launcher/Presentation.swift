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
    var access: String
    enum Occupancy: Equatable { case empty, open, full }

    init(_ server: GameServer) {
        let version = server.version
        isCoD2x = version.hasPrefix("1.4") || server.fields["protocol"] == "120"
        let shown = version == "1.4.x" ? "1.4" : String(version.prefix(16))
        versionLabel = isCoD2x ? "CoD2x \(shown)" : "Original \(shown)"
        pingLevel = Self.pingLevel(server.ping)
        occupancy = server.playerCount == 0 ? .empty : (server.maxPlayers > 0 && server.playerCount >= server.maxPlayers ? .full : .open)
        access = server.password ? "Password" : "Open"
    }
    static func pingLevel(_ ping: Int) -> Int { ping < 40 ? 4 : ping < 60 ? 3 : ping < 80 ? 2 : 1 }
}

/// The player's frame cap (com_maxfps, 0–1000; 0 is no cap), shown as plain data.
enum FrameCap {
    /// Quick picks in Settings, with neutral captions; any other value is entered by hand.
    static let presets: [(value: Int, caption: String)] = [(125, "Low"), (250, "Default"), (333, "High"), (1000, "Engine maximum")]
    static func isPreset(_ fps: Int) -> Bool { presets.contains { $0.value == fps } }
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
    /// When it was published, which orders What’s New. Entries an older build stored have none.
    var date: Date? = nil
    /// A release’s version ("0.3.0"); news has none.
    var version: String? = nil
    /// A place in the app that a news entry opens instead of a link (NewsFeed.places).
    var open: String? = nil
    static let bundled = [
        Dispatch(stamp: "PREVIEW", title: "CoD2 Silicon 0.3.4", summary: "A switch in Settings › Privacy sends crash reports to the project. It is off until you turn it on.",
                 link: "https://github.com/cod2unbound/cod2-unbound/releases"),
        Dispatch(stamp: "AHEAD", title: "What’s next", summary: "The Metal renderer comes next. It isn’t in this release, and the original look stays until it matches.",
                 link: "https://github.com/cod2unbound/cod2-unbound/blob/main/docs/ROADMAP.md"),
    ]
}

/// The project’s news file, `docs/news.json` on the main branch: short notes that are not releases, and
/// what comes next. It is fetched with the update check, so news needs no new build. Plain text only:
/// a title, a sentence, and either a github.com link or a place in the app to open.
///
///     { "items": [ { "date": "2026-11-21", "title": "…", "summary": "…", "open": "configs" } ],
///       "next":  { "title": "…", "summary": "…", "link": "https://github.com/…" } }
///
/// An item dated in the future stays hidden until that day.
enum NewsFeed {
    static let url = URL(string: "https://raw.githubusercontent.com/cod2unbound/cod2-unbound/main/docs/news.json")!
    static let places = ["configs", "servers", "library", "updates"]
    struct Feed: Equatable { var items: [Dispatch] = []; var next: Dispatch? = nil }
    private struct Item: Decodable { var date: String?; var title: String?; var summary: String?; var link: String?; var open: String? }
    private struct File: Decodable { var items: [Item]?; var next: Item? }

    static func parse(_ data: Data, now: Date = Date(), calendar: Calendar = .current) throws -> Feed {
        guard data.count <= 65_536 else { throw LauncherError(message: "The news file is too large.") }
        let file = try JSONDecoder().decode(File.self, from: data)
        var items: [Dispatch] = []
        for item in (file.items ?? []).prefix(50) {
            guard var entry = dispatch(item), let day = item.date.flatMap({ date($0, calendar: calendar) }), day <= now else { continue }
            entry.date = day; entry.stamp = StencilDate.day(day, calendar: calendar)
            items.append(entry)
        }
        items.sort { ($0.date ?? .distantPast) > ($1.date ?? .distantPast) }
        return Feed(items: Array(items.prefix(5)), next: file.next.flatMap(dispatch))
    }
    private static func dispatch(_ item: Item) -> Dispatch? {
        let title = text(item.title, limit: 80)
        guard !title.isEmpty else { return nil }
        let link = item.link.flatMap { $0.hasPrefix("https://github.com/") && $0.count <= 256 && !$0.contains(where: \.isWhitespace) ? $0 : nil }
        return Dispatch(stamp: "NEXT", title: title, summary: text(item.summary, limit: 160), link: link,
                        open: item.open.flatMap { places.contains($0) ? $0 : nil })
    }
    /// One line of plain text: control and formatting characters become spaces, runs of spaces close up.
    private static func text(_ raw: String?, limit: Int) -> String {
        let flat = String(String.UnicodeScalarView((raw ?? "").unicodeScalars.prefix(2_000).map { CharacterSet.controlCharacters.contains($0) || CharacterSet.newlines.contains($0) ? " " : $0 }))
        let line = flat.split(separator: " ", omittingEmptySubsequences: true).joined(separator: " ")
        return line.count > limit ? String(line.prefix(limit - 1)).trimmingCharacters(in: .whitespaces) + "…" : line
    }
    /// The start of a `YYYY-MM-DD` day in `calendar`; nil for anything else, including days that don’t exist.
    private static func date(_ raw: String, calendar: Calendar) -> Date? {
        let parts = raw.split(separator: "-", omittingEmptySubsequences: false)
        guard raw.count == 10, parts.count == 3, parts.allSatisfy({ $0.allSatisfy(\.isASCII) && $0.allSatisfy(\.isNumber) }),
              let year = Int(parts[0]), let month = Int(parts[1]), let day = Int(parts[2]), year >= 2026,
              let date = calendar.date(from: DateComponents(year: year, month: month, day: day)) else { return nil }
        let back = calendar.dateComponents([.year, .month, .day], from: date)
        return back.year == year && back.month == month && back.day == day ? date : nil
    }
}

/// Home’s What’s New card: releases from GitHub Releases and notes from the project’s news file, newest
/// first, with what comes next last. Both arrive with the update check; until one has answered the card
/// holds one line about this version, with no date.
enum HomeDispatches {
    static func builtIn(version: String) -> Dispatch {
        Dispatch(stamp: "NOW", title: "CoD2 Silicon \(version)",
                 summary: version == "0.3.4" ? "A switch in Settings › Privacy sends crash reports to the project. It is off until you turn it on."
                        : version == "0.3.3" ? "The project has a new home at github.com/cod2unbound, and the app updates from there."
                        : version == "0.3.2" ? "A console you can read on 4K, 5K and 6K screens, and a launcher that shows your loadout where it showed frame rate."
                        : version == "0.3.1" ? "Friends on Windows and Linux can join a CoD2x server you host, and a log for each server start."
                        : version == "0.3.0" ? "CoD2x 1.4.6.8 beside the original 1.3, loadouts, and one-click updates."
                                             : "What changed in this version is in the release notes.",
                 link: "https://github.com/cod2unbound/cod2-unbound/releases", version: version)
    }
    /// At most `rows` lines. The running version’s line is always among them: its release as fetched,
    /// else the built-in one.
    static func list(version: String, releases: [Dispatch], news: [Dispatch] = [], next: Dispatch? = nil, rows: Int = 3) -> [Dispatch] {
        let mine = releases.first { $0.version == version || $0.title == version || $0.title.hasSuffix(" " + version) } ?? builtIn(version: version)
        let dated = (releases + news).filter { $0.date != nil && $0 != mine }
        let all = (dated + [mine]).sorted { ($0.date ?? .distantPast) > ($1.date ?? .distantPast) }
        var top = Array(all.prefix(max(1, rows - (next == nil ? 0 : 1))))
        if !top.contains(mine) { top[top.count - 1] = mine }
        return top + (next.map { [$0] } ?? [])
    }
}

enum ReleaseNotes {
    struct Feed: Equatable {
        var dispatches: [Dispatch]; var latestVersion: String?; var latestAssets: [ReleaseAsset] = []
        /// What's New for the newest release (ship-settings).
        var latestNotes: [ReleaseNoteItem] = []
    }
    private struct Asset: Decodable { var name: String?; var size: Int?; var browser_download_url: String? }
    private struct Release: Decodable {
        var assets: [Asset]?
        var tag_name: String?
        var name: String?
        var body: String?
        var published_at: String?
        var html_url: String?
        var draft: Bool?
        var prerelease: Bool?
    }
    /// Parses GitHub's `GET /repos/{owner}/{repo}/releases` response. Nothing is downloaded here; the
    /// latest release's files are only listed, for Install Update (AppUpdate).
    static func parse(_ data: Data, limit: Int = 3, calendar: Calendar = .current) throws -> Feed {
        guard data.count <= 1_000_000 else { throw LauncherError(message: "The release feed is too large.") }
        let releases = try JSONDecoder().decode([Release].self, from: data).prefix(50).filter { $0.draft != true }
        var dispatches: [Dispatch] = []
        for release in releases.prefix(limit) {
            let tag = String((release.tag_name ?? "").prefix(32))
            let title = String((release.name ?? "").trimmingCharacters(in: .whitespacesAndNewlines).prefix(80))
            var stamp = "NEW"
            let date = release.published_at.flatMap { ISO8601DateFormatter().date(from: $0) }
            if let date { stamp = StencilDate.day(date, calendar: calendar) }
            let link = release.html_url.flatMap { $0.hasPrefix("https://github.com/") ? String($0.prefix(256)) : nil }
            dispatches.append(Dispatch(stamp: stamp, title: title.isEmpty ? "CoD2 Silicon \(version(tag))" : title,
                                       summary: summary(release.body ?? ""), link: link, date: date, version: tag.isEmpty ? nil : version(tag)))
        }
        let newest = releases.first { $0.prerelease != true }
        let assets = (newest?.assets ?? []).prefix(64).compactMap { asset -> ReleaseAsset? in
            guard let name = asset.name, let size = asset.size, let link = asset.browser_download_url, let url = URL(string: link) else { return nil }
            return ReleaseAsset(name: name, size: size, url: url)
        }
        return Feed(dispatches: dispatches, latestVersion: newest?.tag_name.map(version), latestAssets: assets, latestNotes: items(newest?.body ?? ""))
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

struct ReleaseAsset: Equatable, Sendable { var name: String; var size: Int; var url: URL }

// MARK: - ship-settings

/// Settings › General › Appearance. `system` follows macOS.
enum LauncherAppearance: String, Codable, Sendable, CaseIterable {
    case system, light, dark
    var title: String {
        switch self { case .system: "Match System"; case .light: "Light"; case .dark: "Dark" }
    }
}

/// Keys of the “Don’t ask again” choices kept in `GameSettings.dismissedWarnings`. A unit that adds a
/// prompt adds its key here; Settings › Advanced › Reset Warnings brings every one back.
enum DismissibleWarning {
    /// Servers: launching on a server that runs the other game version (ship-servers).
    static let switchVersion = "switch-version"
}

/// Settings wording that the unit tests check: times, sizes, shapes, the masked CD key.
enum SettingsText {
    /// “today at 14:02”, “yesterday at 21:30”, “on 8 Nov at 21:30”, in the Mac's own time format.
    static func when(_ date: Date, now: Date = Date(), calendar: Calendar = .current, locale: Locale = .current) -> String {
        let time = DateFormatter()
        time.locale = locale; time.calendar = calendar; time.timeZone = calendar.timeZone
        time.dateStyle = .none; time.timeStyle = .short
        let clock = time.string(from: date)
        if calendar.isDate(date, inSameDayAs: now) { return "today at \(clock)" }
        if let yesterday = calendar.date(byAdding: .day, value: -1, to: now), calendar.isDate(date, inSameDayAs: yesterday) { return "yesterday at \(clock)" }
        let day = DateFormatter()
        day.locale = locale; day.calendar = calendar; day.timeZone = calendar.timeZone
        day.setLocalizedDateFormatFromTemplate(calendar.isDate(date, equalTo: now, toGranularity: .year) ? "dMMM" : "dMMMyyyy")
        return "on \(day.string(from: date)) at \(clock)"
    }
    /// A file size with its unit, as Finder shows it (“48 MB”).
    static func bytes(_ count: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: max(0, count), countStyle: .file)
    }
    /// “2560 × 1440” from the game's `2560x1440`.
    static func resolution(_ value: String) -> String { value.replacingOccurrences(of: "x", with: " × ") }
    static func size(_ value: String) -> (width: Int, height: Int)? {
        let parts = value.split(separator: "x").compactMap { Int($0) }
        return parts.count == 2 && parts[0] > 0 && parts[1] > 0 ? (parts[0], parts[1]) : nil
    }
    /// The shape of a picture in the words people use: 16:9, 16:10, 4:3, 21:9, or “1.55:1” when it's none of them.
    static func aspect(width: Int, height: Int) -> String {
        guard width > 0, height > 0 else { return "" }
        let ratio = Double(width) / Double(height)
        let known: [(String, Double)] = [("16:9", 16.0 / 9), ("16:10", 1.6), ("4:3", 4.0 / 3), ("5:4", 1.25), ("3:2", 1.5), ("21:9", 64.0 / 27), ("32:9", 32.0 / 9)]
        if let match = known.first(where: { abs($0.1 - ratio) / $0.1 < 0.012 }) { return match.0 }
        return String(format: "%.2f:1", ratio)
    }
    /// The CD key as Settings shows it: only its last four characters.
    static func maskedKey(lastFour: String) -> String { "•••• •••• •••• •••• " + lastFour.uppercased() }
}

/// One line of What's New, from a release's notes: its section as a tag, the first sentence, the rest.
struct ReleaseNoteItem: Equatable, Sendable {
    var tag: String
    var title: String
    var detail: String
}

extension ReleaseNotes {
    /// The bullets of a release's notes (CHANGELOG.md's sections under “### Added”, “### Changed” and
    /// “### Fixed”), each with its section as a tag (New, Changed, Fixed), its first sentence as the title
    /// and the rest shortened to `length`. Other sections are left out.
    static func items(_ body: String, limit: Int = 12, length: Int = 180) -> [ReleaseNoteItem] {
        let tags = ["added": "New", "changed": "Changed", "fixed": "Fixed", "new": "New", "improved": "Changed"]
        var items: [ReleaseNoteItem] = []
        var tag: String?
        var bullet: [String] = []
        func flush() {
            guard let tag, !bullet.isEmpty, items.count < limit else { bullet = []; return }
            var text = bullet.joined(separator: " ")
            text = text.replacingOccurrences(of: #"!?\[([^\]]*)\]\([^)]*\)"#, with: "$1", options: .regularExpression)
            text = text.replacingOccurrences(of: #"[*`]+"#, with: "", options: .regularExpression)
            text = text.replacingOccurrences(of: #"\s+"#, with: " ", options: .regularExpression).trimmingCharacters(in: .whitespaces)
            bullet = []
            guard !text.isEmpty else { return }
            let end = text.range(of: #"[.:!?](\s|$)"#, options: .regularExpression)
            let title = end.map { String(text[..<$0.lowerBound]) } ?? text
            var detail = end.map { String(text[$0.upperBound...]).trimmingCharacters(in: .whitespaces) } ?? ""
            if detail.count > length {
                let cut = detail.prefix(length)
                if let stop = cut.lastIndex(of: "."), cut.distance(from: cut.startIndex, to: stop) > length / 2 { detail = String(cut[...stop]) }
                else { detail = cut.split(separator: " ").dropLast().joined(separator: " ") + "…" }
            }
            items.append(ReleaseNoteItem(tag: tag, title: String(title.prefix(120)), detail: detail))
        }
        for raw in body.prefix(200_000).replacingOccurrences(of: "\r\n", with: "\n").components(separatedBy: "\n") {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("#") {
                flush()
                let heading = line.drop { $0 == "#" }.trimmingCharacters(in: .whitespaces).lowercased()
                tag = line.hasPrefix("###") ? tags[heading] : nil
                continue
            }
            if raw.hasPrefix("- ") || raw.hasPrefix("* ") { flush(); bullet = [String(line.dropFirst(2))] }
            else if line.isEmpty { flush() }
            else if !bullet.isEmpty { bullet.append(line) }
        }
        flush()
        return items
    }
}

/// What Report a Problem and Copy Version Info show: the same lines, never the CD key or the player's name.
struct ReportDetails: Equatable, Sendable {
    var version: String
    var build: String
    var engines: [String]
    var macOS: String
    var model: String
    var shaders: String
    var display: String
    var lines: [String] {
        ["CoD2 Silicon \(version)" + (build.isEmpty || build == version ? "" : " (\(build))"),
         engines.joined(separator: " · "), "macOS \(macOS) · \(model)", "Shaders: \(shaders) · Display: \(display)"]
    }
    var text: String { lines.joined(separator: "\n") }
    /// A new issue on the project's bug form, its fields filled from these lines (GitHub's issue-form
    /// query parameters, by field id). Nothing is sent until the person posts it in the browser.
    func issueURL(repository: String = "cod2unbound/cod2-unbound") -> URL? {
        var parts = URLComponents(string: "https://github.com/\(repository)/issues/new")
        parts?.queryItems = [
            URLQueryItem(name: "template", value: "bug_report.yml"),
            URLQueryItem(name: "version", value: lines[0].replacingOccurrences(of: "CoD2 Silicon ", with: "")),
            URLQueryItem(name: "macos", value: macOS),
            URLQueryItem(name: "model", value: model),
            URLQueryItem(name: "steps", value: "Game versions: \(lines[1])\nShaders: \(shaders)\nDisplay: \(display)\n\n1. "),
        ]
        return parts?.url
    }
}

/// Settings › Privacy › What CoD2 Silicon contacts. Each line was checked against the code that does it
/// (Network.swift, ModelSettings.swift, Core.swift's AppUpdate and CoD2xFiles, ConfigImport, the engine's
/// cod2x_features.c update check); README.md's “What the app sends” says the same.
enum PrivacyContacts {
    static let lines: [(who: String, when: String)] = [
        ("Master servers", "Activision’s and the CoD2x project’s, when the server list refreshes. The game also asks them and the key server, as the original does."),
        ("Game servers", "Those in the list when it refreshes, and the one you join. CoD2x servers also get a hash of CoD2x’s ID for this Mac."),
        ("The CoD2x project", "With CoD2x, its update check each time the game’s menu opens: your CoD2x version, a hash of your CD key, CoD2x’s IDs for this Mac and a short description of it. Mod files a server needs come from its site."),
        ("GitHub", "When you check for or install updates, which also fetches the news on Home; when Configs loads player setups; when you download CoD2x’s menus and icons, or zPAM for a server you host. Nothing about you."),
        ("Servers you host", "Only while one runs. Set to The Internet, it lists itself on the master servers. A CoD2x server asks the CoD2x project about new versions."),
        ("Crash reports", "Only with a switch above on, once for each crash or freeze."),
        ("Config links", "Only a link you paste into Configs to import from it."),
    ]
}

struct MediaFacts: Equatable {
    var modifiedAt: Date?
    var map: String?
    var stamp: String
    var size: String
    init(name: String, date: Date?, bytes: Int?, calendar: Calendar = .current) {
        modifiedAt = date
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
