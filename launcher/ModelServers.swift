import AppKit
import SwiftUI

// Owner: ship-servers. Servers: the list, filters and sorting, refresh, favourites, the ready check, joining a server and the version switch.
// Stored state lives in Model.swift (in this unit's MARK region); methods and derived values live here.
@MainActor extension LauncherModel {
    var selected: GameServer? { visibleServers.first { $0.address == selectedServer } }
    var lastServer: GameServer? {
        guard let address = library.recent.first else { return nil }
        return servers.first { $0.address == address } ?? library.cache.first { $0.address == address }
    }
    var recentServers: [GameServer] {
        library.recent.prefix(12).compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    var favoriteServers: [GameServer] {
        library.favorites.sorted().compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    var visibleServers: [GameServer] {
        let result = servers.filter {
            (search.isEmpty || [QuakeColors.plain($0.name), $0.map, MapCatalog.displayName($0.map), $0.mod, $0.address].joined(separator: " ").localizedCaseInsensitiveContains(search)) &&
            (filter != .stock || !ServerFacts($0).isCoD2x) && (filter != .codx || ServerFacts($0).isCoD2x) &&
            (!hideEmpty || $0.playerCount > 0) && (!hideFull || $0.maxPlayers == 0 || $0.playerCount < $0.maxPlayers)
        }
        return result.sorted { lhs, rhs in
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
    /// The answer to `engineRequest`: save CoD2x as the engine, then join.
    func turnOnCoD2x(andJoin request: EngineRequest) {
        dismissSheet()
        savedSettings.engine = .cod2x
        settings.engine = .cod2x
        if !snapshot {
            do { try PrivateFile.write(JSONEncoder().encode(savedSettings), to: home.appendingPathComponent("launcher-settings.json")) }
            catch { notice = error.localizedDescription }
        }
        do { try serverConnection.select(address: request.link.address) } catch { notice = error.localizedDescription; return }
        pendingLink = request.link
        play()
    }
    /// Original 1.3 joins only stock 1.3 servers; a CoD2x server asks to turn CoD2x on, and a server
    /// not seen yet is asked for its version first, once.
    func admit(_ link: LaunchLink) -> Bool {
        let address = (try? LaunchLink.canonicalAddress(link.address)) ?? link.address
        switch chosenEngine.check(server: knownServer(address), lookedUp: checkedAddresses.contains(address)) {
        case .go: return true
        case .needsCoD2x:
            pendingLink = nil; fastPlay = false
            present(.switchVersion(EngineRequest(link: link)))
        case .lookUp:
            guard lookingUp.insert(address).inserted else { break }
            Task {
                if let server = await ServerQueries.server(address) { remember(server) }
                lookingUp.remove(address); checkedAddresses.insert(address)
                if pendingLink?.url == link.url, !gameRunning { play() }
            }
        }
        return false
    }
    func knownServer(_ address: String) -> GameServer? {
        func same(_ server: GameServer) -> Bool { (try? LaunchLink.canonicalAddress(server.address)) == address }
        return servers.first(where: same) ?? library.cache.first(where: same)
    }
    func remember(_ server: GameServer) {
        let normalized = try? LaunchLink.canonicalAddress(server.address)
        if let index = servers.firstIndex(where: { (try? LaunchLink.canonicalAddress($0.address)) == normalized }) { servers[index] = server }
        else { servers.append(server) }
    }
    func favorite(_ address: String) {
        if library.favorites.contains(address) { library.favorites.remove(address) } else { library.favorites.insert(address) }
        saveLibrary()
    }
    func showServer(_ address: String) {
        navigate(to: .servers)
        clearServerFilters()
        selectedServer = address
    }
    func clearServerFilters() { search = ""; filter = .all; hideEmpty = false; hideFull = false }
    func sortServers(by key: ServerSort) {
        if sort == key { sortAscending.toggle() }
        else { sort = key; sortAscending = key != .players }
    }
    var serverCacheAge: String {
        guard let date = library.cachedAt else { return "Not refreshed yet" }
        if Date().timeIntervalSince(date) < 60 { return "Refreshed just now" }
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .full
        return "Refreshed \(formatter.localizedString(for: date, relativeTo: Date()))"
    }
    var directAddressError: String? {
        let address = directAddress.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !address.isEmpty else { return nil }
        guard (try? LaunchLink.canonicalAddress(address)) != nil else {
            return "Use a hostname or IP address, with an optional port from 1 to 65535. For example, 203.0.113.24:28960."
        }
        return nil
    }
    func copyServer(_ server: GameServer, link: Bool = false) {
        let value = link ? (try? LaunchLink.direct(server.address).url) : server.address
        guard let value else { return }
        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(value, forType: .string)
        notice = link ? "Server link copied." : "Server address copied."
        studio.announce(notice)
    }
    func refresh() {
        guard !snapshot, !refreshing else { return }
        refreshing = true; refreshResult = ""; queryProgress = "Contacting both master servers…"
        browserTask = Task {
            let discovered = await ServerQueries.masterAddresses()
            let addresses = Array(Set(discovered).union(library.favorites).union(library.recent)).sorted()
            if discovered.isEmpty && !Task.isCancelled { notice = "Master servers did not respond. Cached servers and direct connect remain available." }
            var fresh: [GameServer] = []
            for start in stride(from: 0, to: addresses.count, by: 6) {
                if Task.isCancelled { break }
                let batch = Array(addresses[start..<min(start + 6, addresses.count)])
                await withTaskGroup(of: GameServer?.self) { group in
                    for address in batch {
                        group.addTask { await ServerQueries.server(address) }
                        try? await Task.sleep(for: .milliseconds(40))
                    }
                    for await value in group { if let value { fresh.append(value) } }
                }
                servers = fresh + library.cache.filter { cached in !fresh.contains { $0.address == cached.address } }
                queryProgress = "Asking \(min(start + 6, addresses.count)) of \(addresses.count) servers…"
            }
            if fresh.isEmpty && !Task.isCancelled { notice = "No servers responded. Your cached results remain available; try direct connect or refresh later." }
            if !fresh.isEmpty { servers = fresh; library.cache = fresh; library.cachedAt = Date(); saveLibrary() }
            refreshResult = Task.isCancelled ? "Refresh stopped. \(fresh.count) servers answered." : "\(fresh.count) servers answered."
            if fresh.isEmpty && !Task.isCancelled { refreshResult += " Try refreshing again." }
            refreshing = false
            studio.announce(refreshResult)
            loadArtwork()
        }
    }
    func cancelRefresh() { browserTask?.cancel() }
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
        return readyChecker.evaluate(server: server, profile: profile, mode: chosenEngine.catalogMode,
            advancedChain: try configStudio.readAdvancedChain(profileID: profile.profile.id))
    }
    func fixForServerOnly(_ address: String) throws {
        guard let configStudio = readySession, !configStudio.isReadOnly else { throw ConfigProfileError.gameRunning }
        try serverConnection.select(address: address)
        guard let report = try readyCheck(for: address) else { throw ConfigReadyCheckError.noVerifiedFix }
        try serverConnection.fix(report: report)
        objectWillChange.send()
    }
    /// Query one endpoint, including direct-connect addresses absent from the
    /// master list. Selection and staged choices remain the caller's decision.
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
    func connect(_ server: GameServer? = nil) {
        if server == nil && directAddress.trimmingCharacters(in: .whitespaces).isEmpty {
            notice = "Enter a server address, such as 203.0.113.24:28960."; return
        }
        do {
            if server == nil, let error = directAddressError { directConnectError = error; return }
            let link = try LaunchLink.direct(server?.address ?? directAddress.trimmingCharacters(in: .whitespacesAndNewlines), password: server == nil ? directPassword : serverPassword)
            try serverConnection.select(address: link.address)
            if server == nil { directPassword = ""; directConnectError = nil } else { serverPassword = "" }
            if activeGamePID != nil { openLink(link.url) }
            else { pendingLink = link; play() }
        }
        catch {
            if server == nil { directConnectError = "This connection couldn’t be opened. Check the address and password and try again." }
            else { notice = error.localizedDescription }
        }
    }

    // MARK: Menu commands and sheets this unit owns
    /// File › Connect to Address… (⌘L), the Servers toolbar, and `cod2://` links with the address filled in.
    func connectToAddress(_ address: String? = nil) { present(.connect(address: address)) }
    /// Server › Launch on Selected Server.
    func launchSelectedServer() { if let selected { connect(selected) } }
    /// Server › Add to Favorites (⌘D) for the selected server.
    func toggleFavoriteSelected() { if let selected { favorite(selected.address) } }
    /// Server › Copy Address (⇧⌘C) for the selected server.
    func copySelectedAddress() { if let selected { copyServer(selected) } }
    /// Server row › Copy Join Link.
    func copyJoinLink(_ server: GameServer) { copyServer(server, link: true) }
    /// Server row › Refresh This Server: asks that one server again.
    func refreshServer(_ server: GameServer) { Task { _ = try? await refreshReadyCheck(for: server.address) } }
    /// Server › Clear Recent Servers.
    func clearRecentServers() { library.recent = []; saveLibrary() }
    /// Server › Filters… (⌥⌘F): ship-servers builds the popover.
    func showServerFilters() {}
    /// Removing a favourite from the Favorites scope or the sidebar asks first.
    func requestRemoveFavorite(_ server: GameServer) {
        confirm(ConfirmationRequest(title: "Remove “\(QuakeColors.plain(server.name))” from Favorites?", message: "It stays in the server list and in Recent.",
                                    actionTitle: "Remove", destructive: false) { [weak self] in
            if self?.library.favorites.contains(server.address) == true { self?.favorite(server.address) }
        })
    }

    // MARK: Review harness
    /// Review-harness states for this unit's screens; true when `screen` is one of them.
    func previewServers(_ screen: String) -> Bool {
        switch screen {
        case "servers": page = .servers
        case "servers-empty": page = .servers; search = "zzz"
        case "servers-no-cache": page = .servers; servers = []; library.cache = []; library.cachedAt = nil; selectedServer = nil
        case "servers-inspector-empty": page = .servers; selectedServer = nil
        case "servers-direct-error": page = .servers; directAddress = "invalid address;quit"
        case "servers-refreshing": page = .servers; refreshing = true; queryProgress = "Asking 24 of 120 servers…"
        case "servers-refreshing-empty": page = .servers; servers = []; library.cache = []; library.cachedAt = nil; selectedServer = nil; refreshing = true; queryProgress = "Contacting both master servers…"
        default: return false
        }
        return true
    }
}
