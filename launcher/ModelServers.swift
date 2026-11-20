import AppKit
import Combine
import Security
import SwiftUI

// Owner: ship-servers. Servers: the list, its scope, search, filters and sorting, refresh, favourites,
// the ready check, joining a server (passwords, the game-version switch) and Connect to Address.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.

/// All · Favorites · Recent, the switch at the left of the Servers toolbar.
enum ServerScope: String, CaseIterable, Identifiable {
    /// `hosting` is the servers this Mac runs (HostingView.swift), not a slice of the list.
    case all, favorites, recent, hosting
    var id: String { rawValue }
    var title: String {
        switch self { case .all: "All"; case .favorites: "Favorites"; case .recent: "Recent"; case .hosting: "Hosting" }
    }
}

extension ServerFilter: Codable {}

/// The Filters popover. Every field's default shows every server.
struct ServerFilters: Codable, Equatable {
    /// Game version: `.all` (Any), `.codx` (CoD2x) or `.stock` (Original 1.3).
    var version: ServerFilter = .all
    /// A game-mode code (`tdm`), or nil for any mode.
    var mode: String?
    /// Only servers that answered within this many milliseconds, or nil for any ping.
    var maxPing: Int?
    var hideEmpty = false
    var hideFull = false
    var hidePassworded = false
    static let pingLimits = [50, 100, 150, 250]
    init() {}
    // A file written by another version keeps what it can and defaults the rest.
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = (try? c.decodeIfPresent(ServerFilter.self, forKey: .version)) ?? .all
        mode = try? c.decodeIfPresent(String.self, forKey: .mode)
        maxPing = (try? c.decodeIfPresent(Int.self, forKey: .maxPing)).flatMap { Self.pingLimits.contains($0) ? $0 : nil }
        hideEmpty = (try? c.decodeIfPresent(Bool.self, forKey: .hideEmpty)) ?? false
        hideFull = (try? c.decodeIfPresent(Bool.self, forKey: .hideFull)) ?? false
        hidePassworded = (try? c.decodeIfPresent(Bool.self, forKey: .hidePassworded)) ?? false
    }
    /// How many filters are on, for the toolbar's badge and the popover's summary.
    var count: Int { [version != .all, mode != nil, maxPing != nil, hideEmpty, hideFull, hidePassworded].filter { $0 }.count }
    func admits(_ server: GameServer) -> Bool {
        let facts = ServerFacts(server)
        if version == .codx && !facts.isCoD2x { return false }
        if version == .stock && facts.isCoD2x { return false }
        if let mode, server.gametype.lowercased() != mode { return false }
        if let maxPing, server.ping >= maxPing { return false }
        if hideEmpty && server.playerCount == 0 { return false }
        if hideFull && facts.occupancy == .full { return false }
        if hidePassworded && server.password { return false }
        return true
    }
}

/// Servers' own file, launcher-servers.json in the app home.
struct ServerBrowserPrefs: Codable {
    var filters = ServerFilters()
    /// When you last launched on each server (canonical address), for Connect to Address's recent list.
    var launchedAt: [String: Date] = [:]
    init() {}
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        filters = (try? c.decodeIfPresent(ServerFilters.self, forKey: .filters)) ?? ServerFilters()
        launchedAt = (try? c.decodeIfPresent([String: Date].self, forKey: .launchedAt)) ?? [:]
    }
}

/// The game version a server runs, in the words the boards use.
enum ServerVersion {
    static func engine(_ server: GameServer) -> GameEngine { ServerFacts(server).isCoD2x ? .cod2x : .original }
    /// “CoD2x 1.4.6.8” or “Original 1.3”.
    static func label(_ server: GameServer) -> String {
        let facts = ServerFacts(server)
        guard !facts.isCoD2x else { return facts.versionLabel }
        let version = server.version.trimmingCharacters(in: .whitespaces)
        return "Original " + (version.hasPrefix("1.") ? String(version.prefix(8)) : "1.3")
    }
}

extension GameEngine {
    /// The version a player picks: “CoD2x” or “Original 1.3”.
    var versionName: String { self == .cod2x ? "CoD2x" : "Original 1.3" }
}

/// What Connect to Address accepts, and the join link Copy Join Link writes.
enum ServerAddressInput {
    /// `host:port`, a host name with or without a port, or a `cod2x://` or `cod2://` link, pasted or typed.
    /// Returns the canonical address (with the default port) and a link's password, or nil.
    static func parse(_ text: String) -> (address: String, password: String?)? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        let lower = trimmed.lowercased()
        var rest = trimmed
        for scheme in ["cod2x://", "cod2://"] where lower.hasPrefix(scheme) {
            rest = String(trimmed.dropFirst(scheme.count))
            // CoD2x's own command links: `cod2x://%2Bconnect+host:port/`, with an optional password.
            if let link = try? LaunchLink("cod2x://" + rest), let address = try? LaunchLink.canonicalAddress(link.address) {
                return (address, link.password.isEmpty ? nil : link.password)
            }
            break
        }
        rest = (rest.removingPercentEncoding ?? rest).replacingOccurrences(of: "+", with: " ")
        while rest.hasSuffix("/") { rest.removeLast() }
        rest = rest.trimmingCharacters(in: .whitespaces)
        for prefix in ["connect/", "connect "] where rest.lowercased().hasPrefix(prefix) {
            rest = String(rest.dropFirst(prefix.count)).trimmingCharacters(in: .whitespaces)
        }
        guard !rest.isEmpty, !rest.contains(" "), let address = try? LaunchLink.canonicalAddress(rest) else { return nil }
        return (address, nil)
    }
    /// The link CoD2x on Windows and this app both open: `cod2x://%2Bconnect+host:port/`.
    static func joinLink(_ address: String) -> String? {
        guard let canonical = try? LaunchLink.canonicalAddress(address) else { return nil }
        return "cod2x://%2Bconnect+" + canonical + "/"
    }
    /// The game rejects these in a password: quotation marks, semicolons, backslashes and anything outside printable ASCII.
    static func passwordProblem(_ password: String) -> String? {
        let bad = password.unicodeScalars.contains { $0.value < 0x20 || $0.value >= 0x7f || "\";\\".unicodeScalars.contains($0) }
        return bad ? "Passwords can’t contain quotation marks, semicolons, backslashes or letters outside plain English." : nil
    }
}

/// Server passwords the player chose to remember: the login keychain, one item per server, never synced.
enum ServerPasswords {
    private static var service: String { (Bundle.main.bundleIdentifier ?? "com.cod2unbound.app") + ".server-password" }
    private static func query(_ address: String) -> [String: Any] {
        [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service, kSecAttrAccount as String: address]
    }
    static func read(_ address: String) -> String? {
        var request = query(address)
        request[kSecReturnData as String] = true
        request[kSecMatchLimit as String] = kSecMatchLimitOne
        var item: CFTypeRef?
        guard SecItemCopyMatching(request as CFDictionary, &item) == errSecSuccess, let data = item as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }
    static func save(_ password: String, for address: String) -> Bool {
        let data = Data(password.utf8)
        let updated = SecItemUpdate(query(address) as CFDictionary, [kSecValueData as String: data] as CFDictionary)
        if updated == errSecSuccess { return true }
        guard updated == errSecItemNotFound else { return false }
        var item = query(address)
        item[kSecValueData as String] = data
        item[kSecAttrLabel as String] = "CoD2 Unbound server password (\(address))"
        item[kSecAttrSynchronizable as String] = false
        return SecItemAdd(item as CFDictionary, nil) == errSecSuccess
    }
    static func remove(_ address: String) { SecItemDelete(query(address) as CFDictionary) }
}

@MainActor extension LauncherModel {
    // MARK: Lists
    /// The selected server, only while it's visible: the selection never points at a hidden server.
    var selected: GameServer? { visibleServers.first { $0.address == selectedServer } }
    var lastServer: GameServer? {
        guard let address = library.recent.first else { return nil }
        return servers.first { $0.address == address } ?? library.cache.first { $0.address == address }
    }
    var recentServers: [GameServer] { recent(limit: 12) }
    var favoriteServers: [GameServer] {
        library.favorites.sorted().compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    private func recent(limit: Int) -> [GameServer] {
        library.recent.prefix(limit).compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    /// The servers in the current scope, before search and filters.
    var scopedServers: [GameServer] {
        switch serverScope {
        case .all: servers
        case .favorites: favoriteServers
        case .recent: recent(limit: 30)
        case .hosting: []
        }
    }
    var visibleServers: [GameServer] {
        let query = search.trimmingCharacters(in: .whitespacesAndNewlines)
        let filters = serverFilters
        return scopedServers.filter { server in
            (query.isEmpty || [QuakeColors.plain(server.name), server.map, MapCatalog.displayName(server.map), server.mod, server.address]
                .joined(separator: " ").localizedCaseInsensitiveContains(query)) && filters.admits(server)
        }
        .sorted { lhs, rhs in
            let a = sortAscending ? lhs : rhs, b = sortAscending ? rhs : lhs
            switch sort {
            case .players: if a.playerCount != b.playerCount { return a.playerCount < b.playerCount }
            case .name:
                let order = QuakeColors.plain(a.name).localizedStandardCompare(QuakeColors.plain(b.name))
                if order != .orderedSame { return order == .orderedAscending }
            case .ping: if a.ping != b.ping { return a.ping < b.ping }
            }
            return a.address < b.address
        }
    }
    /// Game modes the Filters popover offers: those the list holds, plus the one chosen.
    var serverModes: [String] {
        var codes = Set((servers + library.cache).map { $0.gametype.lowercased() }.filter { $0 != "—" && !$0.isEmpty })
        if let mode = serverFilters.mode { codes.insert(mode) }
        return codes.sorted { GameModes.long($0).localizedStandardCompare(GameModes.long($1)) == .orderedAscending }
    }
    var searchOrFiltersOn: Bool { !search.trimmingCharacters(in: .whitespaces).isEmpty || serverFilters.count > 0 }
    func knownServer(_ address: String) -> GameServer? {
        func same(_ server: GameServer) -> Bool { (try? LaunchLink.canonicalAddress(server.address)) == address }
        return servers.first(where: same) ?? library.cache.first(where: same)
    }
    func remember(_ server: GameServer) {
        let normalized = try? LaunchLink.canonicalAddress(server.address)
        if let index = servers.firstIndex(where: { (try? LaunchLink.canonicalAddress($0.address)) == normalized }) { servers[index] = server }
        else { servers.append(server) }
    }
    func isFavorite(_ address: String) -> Bool { library.favorites.contains(address) }
    func favorite(_ address: String) {
        if library.favorites.contains(address) { library.favorites.remove(address) } else { library.favorites.insert(address) }
        saveLibrary()
    }
    /// The star: removing from the Favorites scope asks first; elsewhere it just toggles.
    func toggleFavorite(_ server: GameServer) {
        if isFavorite(server.address) && serverScope == .favorites { requestRemoveFavorite(server) } else { favorite(server.address) }
    }
    /// Sidebar › a favourite, and Recent › a server: the server in the list, selected.
    func showServer(_ address: String) {
        navigate(to: .servers)
        if !scopedServers.contains(where: { $0.address == address }) { serverScope = .all }
        selectedServer = address
        if selected == nil { clearServerFilters(); selectedServer = address }
    }
    /// Home › All Recent: Servers on the Recent scope, with nothing hiding a recent server.
    func showRecentServers() {
        navigate(to: .servers)
        guard page == .servers else { return }
        serverScope = .recent
        search = ""
        if visibleServers.count < scopedServers.count { serverFilters = ServerFilters() }
    }
    /// Clear Search and Filters.
    func clearServerFilters() { search = ""; serverFilters = ServerFilters() }
    func sortServers(by key: ServerSort) {
        if sort == key { sortAscending.toggle() }
        else { sort = key; sortAscending = key != .players }
    }

    // MARK: Status
    /// The list's age for other pages (“Refreshed 2 minutes ago”).
    var serverCacheAge: String {
        guard let date = library.cachedAt else { return "Not refreshed yet" }
        if Date().timeIntervalSince(date) < 60 { return "Refreshed just now" }
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .full
        return "Refreshed \(formatter.localizedString(for: date, relativeTo: Date()))"
    }
    /// Never refreshed and nothing cached: nothing has been contacted yet.
    var serversNeverRefreshed: Bool { servers.isEmpty && library.cache.isEmpty && library.cachedAt == nil }
    /// The toolbar's status pill.
    func serverStatusLine(now: Date = Date()) -> String {
        if refreshing { return refreshAsked == 0 ? "Asking the master servers…" : "\(refreshAnswered) of \(refreshAsked) answered" }
        if serversNeverRefreshed && servers.isEmpty { return "Not refreshed yet" }
        let total = scopedServers.count, shown = visibleServers.count
        let count = shown == total ? "\(total) \(total == 1 ? "server" : "servers")" : "\(shown) of \(total) servers"
        if shown != total { return count }
        guard let date = library.cachedAt else { return count }
        if mastersDidNotAnswer { return "\(count) · from \(Self.clock(date, now: now))" }
        return "\(count) · \(Self.age(date, now: now))"
    }
    /// “just now”, “2 min ago”, “3 h ago”, then a date.
    static func age(_ date: Date, now: Date = Date()) -> String {
        let seconds = max(0, now.timeIntervalSince(date))
        if seconds < 60 { return "just now" }
        if seconds < 3600 { return "\(Int(seconds / 60)) min ago" }
        if seconds < 86_400 { return "\(Int(seconds / 3600)) h ago" }
        return date.formatted(.dateTime.day().month(.abbreviated))
    }
    /// “14:02” today, otherwise “8 Nov, 14:02”.
    static func clock(_ date: Date, now: Date = Date()) -> String {
        let time = date.formatted(date: .omitted, time: .shortened)
        return Calendar.current.isDate(date, inSameDayAs: now) ? time : date.formatted(.dateTime.day().month(.abbreviated)) + ", " + time
    }

    // MARK: Refresh
    /// Servers opened: read the saved filters once, and refresh when Settings › General asks for it.
    func serversAppeared() {
        loadServerPrefsOnce()
        guard refreshServersOnOpen, !snapshot, !refreshing else { return }
        if let date = library.cachedAt, Date().timeIntervalSince(date) < 120 { return }
        refresh()
    }
    /// Asks both master servers for the list, then every server (and your favourites and recent servers)
    /// for its status. Answers appear as they arrive; Stop keeps what has arrived.
    func refresh() {
        guard !snapshot, !refreshing else { return }
        loadServerPrefsOnce()
        refreshing = true; refreshAsked = 0; refreshFinished = 0; refreshAnswered = 0
        browserTask = Task {
            let discovered = await ServerQueries.masterAddresses()
            guard !Task.isCancelled else { refreshing = false; studio.announce("Refresh stopped."); return }
            let answered = !discovered.isEmpty
            mastersDidNotAnswer = !answered
            let addresses = Array(Set(discovered).union(library.favorites).union(library.recent)).sorted()
            refreshAsked = addresses.count
            let earlier = servers.isEmpty ? library.cache : servers
            var fresh: [GameServer] = []
            // With a new list, the page fills as servers answer; without one, answers update the old list.
            if answered { servers = [] }
            for start in stride(from: 0, to: addresses.count, by: 6) {
                if Task.isCancelled { break }
                let batch = Array(addresses[start..<min(start + 6, addresses.count)])
                await withTaskGroup(of: GameServer?.self) { group in
                    for address in batch {
                        group.addTask { await ServerQueries.server(address) }
                        try? await Task.sleep(for: .milliseconds(40))
                    }
                    for await value in group {
                        refreshFinished += 1
                        if let value { fresh.append(value); refreshAnswered = fresh.count }
                    }
                }
                servers = answered ? fresh : Self.merge(fresh, over: earlier)
            }
            let stopped = Task.isCancelled
            if answered {
                servers = fresh
                if !fresh.isEmpty || !stopped { library.cache = fresh; library.cachedAt = Date() }
            } else {
                servers = Self.merge(fresh, over: earlier)
                library.cache = servers
            }
            saveLibrary()
            refreshing = false
            let count = "\(fresh.count) \(fresh.count == 1 ? "server" : "servers")"
            studio.announce(stopped ? "Refresh stopped. \(count) answered." : answered ? "\(count) answered." : "The master servers didn’t answer.")
            loadArtwork()
        }
    }
    func cancelRefresh() { browserTask?.cancel() }
    private static func merge(_ fresh: [GameServer], over earlier: [GameServer]) -> [GameServer] {
        let answered = Set(fresh.map(\.address))
        return fresh + earlier.filter { !answered.contains($0.address) }
    }
    /// Server row › Refresh This Server: asks that one server again.
    func refreshServer(_ server: GameServer) {
        guard !snapshot else { return }
        let name = QuakeColors.plain(server.name)
        Task {
            guard let fresh = await ServerQueries.server(server.address) else {
                flash("\(name) didn’t answer. It may be offline or busy; try again in a moment."); return
            }
            remember(fresh)
            if let index = library.cache.firstIndex(where: { $0.address == fresh.address }) { library.cache[index] = fresh; saveLibrary() }
            flash("\(QuakeColors.plain(fresh.name)) answered: \(fresh.playerCount) of \(fresh.maxPlayers) players, \(fresh.ping) ms.")
        }
    }

    // MARK: Saved filters and choices
    private var serverPrefsURL: URL { home.appendingPathComponent("launcher-servers.json") }
    func loadServerPrefsOnce() {
        guard !serverPrefsLoaded else { return }
        serverPrefsLoaded = true
        guard !snapshot, let data = try? Data(contentsOf: serverPrefsURL), let prefs = try? JSONDecoder().decode(ServerBrowserPrefs.self, from: data) else { return }
        serverPrefs = prefs
        if serverFilters != prefs.filters { serverFilters = prefs.filters }
    }
    func saveServerPrefs() {
        serverPrefs.filters = serverFilters
        guard !snapshot, serverPrefsLoaded else { return }
        do { try PrivateFile.write(JSONEncoder().encode(serverPrefs), to: serverPrefsURL) }
        catch { notice = "Your server filters couldn’t be saved." }
    }
    /// Settings › General › Refresh the server list when Servers opens.
    var refreshServersOnOpen: Bool { settings.refreshServersOnOpen }
    /// Shared with Settings › Advanced › Reset Warnings.
    var alwaysUseServerVersion: Bool {
        get { isWarningDismissed(DismissibleWarning.switchVersion) }
        set {
            if newValue { dismissWarning(DismissibleWarning.switchVersion) }
            else { settings.dismissedWarnings.remove(DismissibleWarning.switchVersion) }
        }
    }
    /// When you last launched on a server, for Connect to Address's recent addresses.
    func lastLaunched(_ address: String) -> Date? {
        (try? LaunchLink.canonicalAddress(address)).flatMap { serverPrefs.launchedAt[$0] }
    }
    private func stampLaunch(_ address: String) {
        loadServerPrefsOnce()
        serverPrefs.launchedAt[address] = Date()
        // Only what Recent can show is kept.
        let kept = Set(library.recent.prefix(30).compactMap { try? LaunchLink.canonicalAddress($0) } + [address])
        serverPrefs.launchedAt = serverPrefs.launchedAt.filter { kept.contains($0.key) }
        saveServerPrefs()
    }

    // MARK: Joining a server
    /// The engine the next game would normally start with (Settings › General), ignoring a one-match switch.
    var normalEngine: GameEngine { installedEngines.contains(settings.engine) ? settings.engine : .cod2x }
    /// A switch to the server's game version for one match (read by `chosenEngine`).
    var matchEngineOverride: GameEngine? {
        guard let match = matchEngine, installedEngines.contains(match.engine) else { return nil }
        return match.engine
    }
    /// The engine a launch on this server uses: the version it runs when this app carries it.
    func launchEngine(for server: GameServer?) -> GameEngine {
        guard let server else { return chosenEngine }
        let needed = ServerVersion.engine(server)
        return installedEngines.contains(needed) ? needed : normalEngine
    }
    /// Launch on a server from the list, its details, a menu or Home. nil launches on the selected server.
    func connect(_ server: GameServer? = nil) {
        guard let server = server ?? selected else { return }
        join(server.address)
    }
    /// Every launch on a server comes here: a locked server asks for its password (filled in when it was
    /// remembered), then the game starts; the version check happens in `admit(_:)` on the way.
    func join(_ address: String, password: String? = nil, loadout: String? = nil) {
        guard let canonical = try? LaunchLink.canonicalAddress(address) else { notice = "This server address isn’t valid."; return }
        let server = knownServer(canonical)
        if let server, server.password, password == nil {
            if let loadout { try? useLoadoutForServer(loadout, address: server.address) }
            present(.password(server: server.address)); return
        }
        do {
            let link = try LaunchLink.direct(server?.address ?? canonical, password: password ?? "")
            try serverConnection.select(address: link.address)
            if let loadout { try useLoadoutForServer(loadout, address: link.address) }
            if activeGamePID != nil { openLink(link.url) }
            else { pendingLink = link; play() }
        } catch { notice = ConfigImportReviewModel.message(for: error) }
    }
    /// Before a game starts on a server: when it runs the other game version, launch with that version
    /// for this match (asking first, unless “Don’t ask again” is on). Original 1.3 can't join a CoD2x
    /// server, so a server it hasn't seen yet is asked for its version first, once.
    func admit(_ link: LaunchLink) -> Bool {
        loadServerPrefsOnce()
        let address = (try? LaunchLink.canonicalAddress(link.address)) ?? link.address
        if let match = matchEngine {
            if match.address == address { stampLaunch(address); return true }
            matchEngine = nil; followEngine()
        }
        let normal = normalEngine
        guard let server = knownServer(address) else {
            if normal == .original, !checkedAddresses.contains(address) {
                guard lookingUp.insert(address).inserted else { return false }
                Task {
                    if let server = await ServerQueries.server(address) { remember(server) }
                    lookingUp.remove(address); checkedAddresses.insert(address)
                    if pendingLink?.url == link.url, !gameRunning { play() }
                }
                return false
            }
            stampLaunch(address); return true
        }
        let needed = ServerVersion.engine(server)
        guard needed != normal, installedEngines.contains(needed) else {
            if needed == .cod2x, normal == .original, !installedEngines.contains(.cod2x) {
                pendingLink = nil; fastPlay = false
                notice = "This server runs CoD2x, which this copy of CoD2 Unbound doesn’t include. Reinstall the app to get it."
                return false
            }
            stampLaunch(address); return true
        }
        if alwaysUseServerVersion { useEngineForMatch(needed, address: address); stampLaunch(address); return true }
        pendingLink = nil; fastPlay = false
        present(.switchVersion(EngineRequest(link: link)))
        return false
    }
    /// The Switching game version sheet's Launch: start the server's version for this match.
    func launchWithServerVersion(_ request: EngineRequest, dontAskAgain: Bool) {
        dismissSheet()
        if dontAskAgain { alwaysUseServerVersion = true }
        guard let address = try? LaunchLink.canonicalAddress(request.link.address) else { return }
        if let server = knownServer(address) { useEngineForMatch(ServerVersion.engine(server), address: address) }
        do { try serverConnection.select(address: request.link.address) } catch { notice = error.localizedDescription; return }
        pendingLink = request.link
        play()
    }
    /// The engine for one match; the normal one comes back (with Config Studio's dialect) when the game ends.
    private func useEngineForMatch(_ engine: GameEngine, address: String) {
        matchEngine = (address, engine)
        if matchEngineReset == nil {
            matchEngineReset = $gameRunning.removeDuplicates().dropFirst().sink { [weak self] running in
                MainActor.assumeIsolated {
                    guard !running, let self, self.matchEngine != nil else { return }
                    // Published values change after this runs; finish once the game has ended.
                    Task { @MainActor in
                        guard !self.gameRunning else { return }
                        self.matchEngine = nil; self.followEngine()
                    }
                }
            }
        }
        followEngine()
    }

    // MARK: Passwords
    /// A password remembered for this server on this Mac (the review harness keeps them in memory).
    func savedPassword(for address: String) -> String? {
        guard let canonical = try? LaunchLink.canonicalAddress(address) else { return nil }
        return snapshot ? previewPasswords[canonical] : ServerPasswords.read(canonical)
    }
    /// Remember or forget a server's password; false when the keychain refused it.
    @discardableResult func rememberPassword(_ password: String?, for address: String) -> Bool {
        guard let canonical = try? LaunchLink.canonicalAddress(address) else { return false }
        if snapshot { previewPasswords[canonical] = password; return true }
        guard let password else { ServerPasswords.remove(canonical); return true }
        return ServerPasswords.save(password, for: canonical)
    }

    // MARK: Ready check
    /// The session the ready check reads: Configs' own (the review harness attaches one to a preview).
    var readySession: ConfigStudioSession? { studio.session ?? configStudio }
    /// A connection alternative does not change the active loadout in Studio.
    func useLoadoutForServer(_ id: String, address: String) throws {
        guard let configStudio = readySession, !configStudio.isReadOnly else { throw ConfigProfileError.gameRunning }
        _ = try configStudio.readProfile(id)
        try serverConnection.select(address: address)
        try serverConnection.useProfile(id)
        objectWillChange.send()
    }
    func readyCheck(for address: String) throws -> ConfigReadyReport? {
        guard let readyChecker, let configStudio = readySession else { return nil }
        let normalized = try LaunchLink.canonicalAddress(address)
        let server = servers.first { (try? LaunchLink.canonicalAddress($0.address)) == normalized }
        let chosenID = serverConnection.address == normalized ? serverConnection.profileID : nil
        guard let profile = try chosenID.map(configStudio.readProfile) ?? configStudio.readActive() else { return nil }
        return readyChecker.evaluate(server: server, profile: profile, mode: launchEngine(for: server).catalogMode,
            advancedChain: try configStudio.readAdvancedChain(profileID: profile.profile.id))
    }
    /// Another saved loadout that already suits this server (“Use “Scrim””), or nil: one the ready check
    /// would pass, needing no change of its own (values the server sets while you play don't count).
    func fittingLoadout(for address: String, except current: String?) -> ConfigProfile? {
        guard let readyChecker, let configStudio = readySession, let normalized = try? LaunchLink.canonicalAddress(address) else { return nil }
        let server = servers.first { (try? LaunchLink.canonicalAddress($0.address)) == normalized }
        let mode = launchEngine(for: server).catalogMode
        for summary in studio.profiles where summary.id != current {
            guard let profile = try? configStudio.readProfile(summary.id) else { continue }
            let chain = try? configStudio.readAdvancedChain(profileID: summary.id)
            let report = readyChecker.evaluate(server: server, profile: profile, mode: mode, advancedChain: chain)
            let kind = ReadyCheckPresentation(report: report, loadout: summary.displayName, studio: studio, rules: ServerReadyCheckModel.rules).kind
            if kind == .matches || kind == .serverSets { return summary }
        }
        return nil
    }
    func fixForServerOnly(_ address: String) throws {
        guard let configStudio = readySession, !configStudio.isReadOnly else { throw ConfigProfileError.gameRunning }
        try serverConnection.select(address: address)
        guard let report = try readyCheck(for: address) else { throw ConfigReadyCheckError.noVerifiedFix }
        try serverConnection.fix(report: report)
        objectWillChange.send()
    }
    /// Query one endpoint, including addresses absent from the master list. Selection and staged
    /// choices remain the caller's decision.
    func refreshReadyCheck(for address: String) async throws -> ConfigReadyReport? {
        let normalized = try LaunchLink.canonicalAddress(address)
        if let server = await ServerQueries.server(normalized) {
            if let index = servers.firstIndex(where: { (try? LaunchLink.canonicalAddress($0.address)) == normalized }) {
                servers[index] = server
            } else {
                servers.append(server)
            }
        }
        return try readyCheck(for: normalized)
    }

    // MARK: Copying
    func copyServer(_ server: GameServer, link: Bool = false) {
        guard let value = link ? ServerAddressInput.joinLink(server.address) : server.address else { return }
        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(value, forType: .string)
        flash(link ? "Join link copied: \(value)" : "Address copied: \(value)")
    }
    /// A short confirmation in the status toast that clears itself.
    func flash(_ message: String) {
        notice = message
        studio.announce(message)
        Task { try? await Task.sleep(for: .seconds(3)); if notice == message { notice = "" } }
    }

    // MARK: Menu commands and sheets this unit owns
    /// File › Connect to Address… (⌘L), the Servers toolbar, and `cod2x://` links with the address filled in.
    func connectToAddress(_ address: String? = nil) { loadServerPrefsOnce(); present(.connect(address: address)) }
    /// Server › Launch on Selected Server.
    func launchSelectedServer() { if let selected { connect(selected) } }
    /// Server › Add to Favorites (⌘D) for the selected server.
    func toggleFavoriteSelected() { if let selected { toggleFavorite(selected) } }
    /// Server › Copy Address (⇧⌘C) for the selected server.
    func copySelectedAddress() { if let selected { copyServer(selected) } }
    /// Server row › Copy Join Link.
    func copyJoinLink(_ server: GameServer) { copyServer(server, link: true) }
    /// Server › Clear Recent Servers.
    func clearRecentServers() {
        library.recent = []; saveLibrary()
        serverPrefs.launchedAt = [:]; saveServerPrefs()
    }
    /// Server › Filters… (⌥⌘F): Servers with its Filters popover open.
    func showServerFilters() {
        navigate(to: .servers)
        guard page == .servers else { return }
        serverFiltersPresented = true
    }
    /// Removing a favourite from the Favorites scope or the sidebar asks first.
    func requestRemoveFavorite(_ server: GameServer) {
        let inRecent = library.recent.contains(server.address), inList = servers.contains { $0.address == server.address }
        let message = inList && inRecent ? "It stays in the server list and in Recent." : inList ? "It stays in the server list."
            : inRecent ? "It stays in Recent." : "You can add it again with Connect to Address."
        confirm(ConfirmationRequest(title: "Remove “\(QuakeColors.plain(server.name))” from Favorites?", message: message,
                                    actionTitle: "Remove", destructive: false) { [weak self] in
            if self?.library.favorites.contains(server.address) == true { self?.favorite(server.address) }
        })
    }

    /// Deleting a hosted server asks first; its folder goes to the Trash.
    func requestDeleteHostedServer(_ server: HostedServer) {
        confirm(ConfirmationRequest(title: "Delete “\(server.name)”?", message: "Its settings are removed. Its folder, with the log, moves to the Trash.",
                                    actionTitle: "Delete") { [weak self] in self?.hosting.delete(server) })
    }
    /// Quitting with servers running asks first: they can't keep running without the app.
    func confirmQuitWithServers() {
        let count = hosting.running.count
        confirm(ConfirmationRequest(title: count == 1 ? "“\(hosting.running[0].name)” is still running" : "\(count) servers are still running",
                                    message: "Quitting CoD2 Unbound stops \(count == 1 ? "it" : "them"). Players on \(count == 1 ? "it" : "them") are disconnected.",
                                    actionTitle: count == 1 ? "Stop Server and Quit" : "Stop Servers and Quit") { [weak self] in
            self?.hostingQuitConfirmed = true
            NSApp.terminate(nil)
        })
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewServers(_ screen: String) -> Bool {
        if screen.hasPrefix("servers-hosting") { page = .servers; serverScope = .hosting; previewHosting(screen); return true }
        let base = servers
        switch screen {
        case "servers": page = .servers
        case "servers-filters":
            page = .servers; serverFilters.hideEmpty = true; serverFilters.maxPing = 100; serverFiltersPresented = true
        case "servers-refreshing":
            // The first four answers, by ping, while the rest are still being asked.
            page = .servers; refreshing = true; refreshAsked = 212; refreshFinished = 92; refreshAnswered = 86
            servers = [base[0], base[1], base[6], base[2]]
        case "servers-refreshing-empty":
            page = .servers; servers = []; library.cache = []; library.cachedAt = nil; selectedServer = nil; refreshing = true
        case "servers-never", "servers-no-cache":
            page = .servers; servers = []; library.cache = []; library.cachedAt = nil; selectedServer = nil
        case "servers-filtered", "servers-empty":
            page = .servers; search = "zzz"; serverFilters.hideEmpty = true; serverFilters.maxPing = 100
        case "servers-no-reply":
            page = .servers; mastersDidNotAnswer = true
            library.cachedAt = Calendar.current.date(bySettingHour: 14, minute: 2, second: 0, of: Date())
        case "servers-favorites": page = .servers; serverScope = .favorites
        case "servers-favorites-empty": page = .servers; serverScope = .favorites; library.favorites = []; selectedServer = nil
        case "servers-remove-favorite":
            // The star in the Favorites scope asks first (AllSheets › Remove favorite).
            page = .servers; serverScope = .favorites
            if let server = base.first(where: { QuakeColors.plain($0.name) == "Desert Fox SD" }) { selectedServer = server.address; toggleFavorite(server) }
        case "servers-recent": page = .servers; serverScope = .recent
        case "servers-inspector-empty": page = .servers; selectedServer = nil
        case "servers-connect", "servers-connect-error":
            page = .servers
            // The recent addresses show when each was last played.
            for (index, address) in library.recent.prefix(4).enumerated() {
                serverPrefs.launchedAt[(try? LaunchLink.canonicalAddress(address)) ?? address] = Calendar.current.date(byAdding: .day, value: -(3 + index * 6), to: Date())
            }
            sheet = .connect(address: screen == "servers-connect" ? "203.0.113.24:28960" : "invalid address;quit")
        case "servers-password":
            page = .servers
            let locked = base[4].address
            previewPasswords[(try? LaunchLink.canonicalAddress(locked)) ?? locked] = "scrimnight"
            sheet = .password(server: locked)
        case "servers-version":
            page = .servers
            if let link = try? LaunchLink.direct(base[1].address) { sheet = .switchVersion(EngineRequest(link: link)) }
        default: return false
        }
        return true
    }
}
