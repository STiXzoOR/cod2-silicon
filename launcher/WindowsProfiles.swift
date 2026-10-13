import Foundation
import Darwin

struct WindowsProfileCandidate: Sendable, Identifiable {
    var id: String { configURL.path }
    let profileName: String
    let configURL: URL
    let playersURL: URL
    let isActive: Bool
    /// I2 offers these as optional extra loadouts, never as the primary config.
    let isBackup: Bool
}
struct WindowsProfileDiscovery: Sendable {
    let profiles: [WindowsProfileCandidate]
    let truncated: Bool
    let unreadableDirectories: Int
}

/// No recursive volume search. Only these install paths and explicit selected
/// folders are inspected. No AppData, home directories, or game data are read.
struct WindowsProfiles: Sendable {
    var maximumEntries = 256
    var maximumVolumes = 32
    private static let installPaths = [
        "Program Files (x86)/Activision/Call of Duty 2", "Program Files/Activision/Call of Duty 2",
        "Activision/Call of Duty 2", "Games/Call of Duty 2",
        "SteamLibrary/steamapps/common/Call of Duty 2",
        "Program Files (x86)/Steam/steamapps/common/Call of Duty 2",
        "Program Files/Steam/steamapps/common/Call of Duty 2"
    ]

    func discoverMountedVolumes(selectedFolders: [URL] = []) -> WindowsProfileDiscovery {
        discover(mountedVolumes: FileManager.default.mountedVolumeURLs(includingResourceValuesForKeys: nil, options: []) ?? [], selectedFolders: selectedFolders)
    }
    func discover(mountedVolumes: [URL], selectedFolders: [URL] = []) -> WindowsProfileDiscovery {
        var scanner = Scanner(maximumEntries: max(0, maximumEntries))
        let volumeLimit = max(0, maximumVolumes)
        if mountedVolumes.count > volumeLimit || selectedFolders.count > volumeLimit { scanner.truncated = true }
        for volume in mountedVolumes.prefix(volumeLimit) where volume.isFileURL {
            let root = volume.resolvingSymlinksInPath().standardizedFileURL
            for install in Self.installPaths {
                scanner.players(root.appendingPathComponent(install + "/main/players"), allowedRoot: root)
            }
        }
        for selected in selectedFolders.prefix(volumeLimit) where selected.isFileURL {
            let root = selected.resolvingSymlinksInPath().standardizedFileURL
            scanner.players(root.appendingPathComponent("main/players"), allowedRoot: root)
            scanner.players(root.appendingPathComponent("players"), allowedRoot: root)
            scanner.players(root, allowedRoot: root)
            scanner.profile(root, players: root.deletingLastPathComponent(), name: root.lastPathComponent, active: false, allowedRoot: root)
        }
        let profiles = scanner.found.values.sorted {
            if $0.isActive != $1.isActive { return $0.isActive }
            if $0.isBackup != $1.isBackup { return !$0.isBackup }
            let left = $0.profileName.lowercased(), right = $1.profileName.lowercased()
            return left == right ? $0.configURL.path < $1.configURL.path : left < right
        }
        return .init(profiles: profiles, truncated: scanner.truncated, unreadableDirectories: scanner.unreadable)
    }

    private struct Scanner {
        let maximumEntries: Int
        var scannedEntries = 0
        var truncated = false
        var unreadable = 0
        var visitedPlayers = Set<URL>()
        var visitedProfiles = Set<URL>()
        var found: [URL: WindowsProfileCandidate] = [:]

        func contained(_ url: URL, in root: URL) -> Bool {
            let path = url.resolvingSymlinksInPath().standardizedFileURL.path
            let prefix = root.path.hasSuffix("/") ? root.path : root.path + "/"
            return path == root.path || path.hasPrefix(prefix)
        }
        func regular(_ url: URL) -> Bool {
            guard let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey]) else { return false }
            return values.isRegularFile == true && values.isSymbolicLink != true
        }
        func directory(_ url: URL) -> Bool {
            guard let values = try? url.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey]) else { return false }
            return values.isDirectory == true && values.isSymbolicLink != true
        }
        /// Stream directory entries so a large unrelated directory cannot cause
        /// unbounded allocation. Count inspected names, including ignored files.
        mutating func entries(_ url: URL) -> [URL] {
            guard directory(url) else { return [] }
            guard let handle = opendir(url.path) else { unreadable += 1; return [] }
            defer { closedir(handle) }
            var result: [URL] = []
            while let row = readdir(handle) {
                let name = withUnsafePointer(to: &row.pointee.d_name) {
                    $0.withMemoryRebound(to: CChar.self, capacity: Int(MAXNAMLEN) + 1) { String(cString: $0) }
                }
                if name == "." || name == ".." { continue }
                guard scannedEntries < maximumEntries else { truncated = true; break }
                scannedEntries += 1
                result.append(url.appendingPathComponent(name))
            }
            return result.sorted { $0.lastPathComponent.lowercased() < $1.lastPathComponent.lowercased() }
        }
        mutating func players(_ url: URL, allowedRoot: URL) {
            guard directory(url), contained(url, in: allowedRoot), visitedPlayers.insert(url).inserted else { return }
            var active: String?
            let selection = url.appendingPathComponent("active.txt")
            if regular(selection), let handle = try? FileHandle(forReadingFrom: selection) {
                defer { try? handle.close() }
                if let data = try? handle.read(upToCount: 257), data.count <= 256 {
                    let name = ConfigDocument.text(data).trimmingCharacters(in: .whitespacesAndNewlines)
                    if !name.isEmpty, !name.contains("/"), !name.contains("\\"), !name.contains("..") { active = name }
                }
            }
            for child in entries(url) where directory(child) && contained(child, in: allowedRoot) {
                profile(child, players: url, name: child.lastPathComponent, active: child.lastPathComponent == active, allowedRoot: allowedRoot)
            }
        }
        mutating func profile(_ url: URL, players: URL, name: String, active: Bool, allowedRoot: URL) {
            guard directory(url), contained(url, in: allowedRoot), visitedProfiles.insert(url).inserted else { return }
            configs(in: url, players: players, name: name, active: active, allowedRoot: allowedRoot, forceBackup: false)
            for folder in ["backup", "backups", "Raptr"] {
                configs(in: url.appendingPathComponent(folder), players: players, name: name, active: active, allowedRoot: allowedRoot, forceBackup: true)
            }
        }
        mutating func configs(in url: URL, players: URL, name: String, active: Bool, allowedRoot: URL, forceBackup: Bool) {
            guard contained(url, in: allowedRoot) else { return }
            for file in entries(url) {
                let filename = file.lastPathComponent.lowercased()
                guard filename.hasPrefix("config_mp"), filename.hasSuffix(".cfg") || filename.hasSuffix(".cfg.bak"),
                      regular(file), contained(file, in: allowedRoot) else { continue }
                let canonical = file.standardizedFileURL
                found[canonical] = .init(profileName: name, configURL: canonical, playersURL: players,
                                         isActive: active, isBackup: forceBackup || filename != "config_mp.cfg")
            }
        }
    }
}
