import AppKit
import SwiftUI

/// Setup › Game data › From Steam: where the flow is and what its screens show.
struct SteamSetup: Equatable {
    enum Phase: Equatable {
        /// The game data step as it always was.
        case off
        /// What is downloaded, in which language and where.
        case offer
        /// The tool is being fetched, or Steam has not shown a code yet.
        case starting
        case qr, downloading, checking, notOwned, stopped, done
    }
    var phase = Phase.off
    var language = SteamLanguage.english
    var folder = ""
    var freeBytes: Int64?
    /// While starting: true once the tool is on this Mac and Steam is being reached.
    var reachingSteam = false
    var qr: QRMatrix?
    var doneBytes: Int64 = 0
    var totalBytes: Int64 = 0
    var currentFile = ""
    var secondsLeft: Int?
    var stoppedTitle = ""
    var stoppedDetail = ""
    /// “Resume” once files are on disk, “Try Again” when the sign-in never finished.
    var retryTitle = "Resume"
    var detailsShown = false
    var log: [String] = []
    /// Done: how much is in the folder.
    var downloadedBytes: Int64 = 0
}

/// Where a Steam download was started and in which language: kept in the launcher's folder until the
/// game is complete, so a launcher that was closed mid-download can offer to finish it.
struct SteamUnfinished: Codable, Equatable {
    var folder: String
    var language: String
}

/// How long the flow waits before it gives up, in seconds. Tests shorten these.
struct SteamTimings: Sendable {
    /// From the first code to the phone's approval.
    var signIn = 300.0
    /// For Steam to answer at all.
    var connect = 60.0
    /// Without a line from the tool or a byte written, while downloading.
    var stall = 120.0
    /// The same, for a run whose disk writes the system never reported: only the tool's lines show
    /// it is alive then, and one large archive can take many minutes between two of them.
    var stallUnseen = 900.0
    var tick = 1.0
}

/// What one try keeps while it runs. None of it is shown.
struct SteamWork {
    var generation = 0
    var run: DepotRun?
    var task: Task<Void, Never>?
    var ticker: Task<Void, Never>?
    var session: URL?
    var progress = SteamProgress(depots: [])
    var signedIn = false
    var notOwned = false
    var stopRequested = false
    var stalled = false
    var timedOut = false
    var failure: String?
    /// The tool printed its closing line.
    var finished = false
    var startedAt = Date()
    var codeDeadline: Date?
    var lastActivity = Date()
    var samples: [(at: Date, bytes: Int64)] = []
    /// The system's count of the tool's disk writes has moved at least once, so it can be trusted to
    /// show a download that is alive between two finished files.
    var sawWrites = false
    /// Keeps the Mac awake while the tool runs.
    var awake: NSObjectProtocol?
}

// Owner: ship-shell. The Steam download inside Setup › Game data (the six “From Steam” boards).
// State lives in Model.swift (`steam`, `steamWork`); SteamDownload.swift holds the tool and its output.
@MainActor extension LauncherModel {
    var steamActive: Bool { steam.phase != .off }
    /// Everything the download needs on this Mac while it runs: the tool, and each try's sign-in.
    var steamScratch: URL { home.appendingPathComponent("steam", isDirectory: true) }
    var steamFolderDisplay: String { PathDisplay.short(steam.folder) }
    var steamRoomShort: Bool {
        guard let free = steam.freeBytes else { return false }
        return free < SteamGame.roomNeeded(steam.language)
    }
    var steamSizeWords: String { SteamGame.gigabytes(SteamGame.bytes(steam.language)) + " GB" }
    /// “1.42 of 3.70 GB”.
    var steamAmountWords: String { "\(SteamGame.gigabytes(steam.doneBytes)) of \(SteamGame.gigabytes(steam.totalBytes)) GB" }
    var steamTimeWords: String? {
        guard let seconds = steam.secondsLeft else { return nil }
        if seconds < 50 { return "less than a minute left" }
        let minutes = Int((Double(seconds) / 60).rounded())
        if minutes < 90 { return "about \(minutes) minute\(minutes == 1 ? "" : "s") left" }
        let hours = Int((Double(minutes) / 60).rounded())
        return "about \(hours) hour\(hours == 1 ? "" : "s") left"
    }
    var steamHeading: (title: String, detail: String) {
        switch steam.phase {
        case .off, .offer: ("Download your copy from Steam", "For players who own Call of Duty 2 on Steam. You sign in on your phone, and the game’s files come straight from Steam to this Mac.")
        case .starting, .qr: ("Sign in with the Steam app", "Scan the code with your phone. Nothing about your account is typed here.")
        case .notOwned: ("This account doesn’t have the game", "Steam was asked for Call of Duty 2 and said no.")
        case .downloading, .checking: ("Downloading Call of Duty 2", "From Steam’s servers to this Mac.")
        case .stopped: (steam.retryTitle == "Resume" ? "The download stopped" : "The sign-in didn’t finish",
                        steam.retryTitle == "Resume" ? "Nothing is lost. Start again when you are back online." : "Nothing was downloaded. You can try again.")
        case .done: ("Game data downloaded", "CoD2 Silicon uses the files where they are; nothing is copied.")
        }
    }

    // MARK: A download left unfinished
    private var steamUnfinishedFile: URL { home.appendingPathComponent("steam-download.json") }
    /// Reads the note a started download left. It only counts while its folder still holds the
    /// download tool's own notes: a finished, moved or deleted download is forgotten.
    func refreshSteamUnfinished() {
        guard !snapshot else { return }
        guard let data = try? Data(contentsOf: steamUnfinishedFile), let note = try? JSONDecoder().decode(SteamUnfinished.self, from: data) else {
            steamUnfinished = nil
            return
        }
        if Self.steamDownloadUnfinished(note.folder) {
            steamUnfinished = note
        } else {
            steamUnfinished = nil
            // Before the sign-in nothing is in the folder yet: the note stays while a try is under way.
            if steamWork.run == nil, steamWork.task == nil { try? FileManager.default.removeItem(at: steamUnfinishedFile) }
        }
    }
    private func rememberSteamUnfinished() {
        guard !snapshot, let data = try? JSONEncoder().encode(SteamUnfinished(folder: steam.folder, language: steam.language.rawValue)) else { return }
        try? data.write(to: steamUnfinishedFile, options: .atomic)
    }
    private func forgetSteamUnfinished() {
        steamUnfinished = nil
        if !snapshot { try? FileManager.default.removeItem(at: steamUnfinishedFile) }
    }
    /// Game data › Finish the Download…, for a download a closed launcher left behind: the offer,
    /// with that folder and language already chosen.
    func resumeSteamUnfinished() {
        guard let note = steamUnfinished else { return }
        steam.folder = note.folder
        steam.language = SteamLanguage(rawValue: note.language) ?? steam.language
        offerSteam()
    }
    static let steamUnfinishedMessage = "This folder holds a Steam download that didn’t finish. Finish the download, or pick another folder."
    var dataIsUnfinishedSteam: Bool { dataError == Self.steamUnfinishedMessage }
    /// Game data › Finish the Download…: the offer, pointed at the folder that was refused.
    func finishSteamDownload() {
        guard dataIsUnfinishedSteam, !attemptedDataPath.isEmpty else { return }
        steam.folder = attemptedDataPath
        dataError = ""; attemptedDataPath = ""
        offerSteam()
    }

    // MARK: The offer
    /// Game data › Download from Steam…
    func offerSteam() {
        guard setupStep == .data, !shaderBusy else { return }
        setupError = ""
        if steam.folder.isEmpty { steam.folder = Self.steamDefaultFolder().path }
        steam.phase = .offer
        refreshSteamRoom()
    }
    /// ~/Games/CoD2, unless something else is already there.
    static func steamDefaultFolder() -> URL {
        let games = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Games")
        let first = games.appendingPathComponent("CoD2")
        return steamFolderUsable(first) ? first : games.appendingPathComponent("CoD2 from Steam")
    }
    /// Empty, not there yet, or holding a download this flow left unfinished.
    static func steamFolderUsable(_ folder: URL) -> Bool {
        let names = ((try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []).filter { $0 != ".DS_Store" }
        return names.isEmpty || names.contains(".DepotDownloader")
    }
    func refreshSteamRoom() {
        var probe = URL(fileURLWithPath: steam.folder)
        while !FileManager.default.fileExists(atPath: probe.path), probe.path != "/" { probe.deleteLastPathComponent() }
        let values = try? probe.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
        steam.freeBytes = snapshot ? 212_000_000_000 : values?.volumeAvailableCapacityForImportantUsage
    }
    func pickSteamFolder() {
        guard steam.phase == .offer, let window = launcherWindow ?? NSApp.keyWindow, window.attachedSheet == nil else { return }
        let panel = makeSetupPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.canCreateDirectories = true
        panel.prompt = "Download Here"
        panel.message = "Choose where Call of Duty 2 goes. It needs about \(steamSizeWords)."
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.chooseSteamFolder(url)
        }
    }
    /// A folder that already holds other things gets a “Call of Duty 2” folder inside it.
    func chooseSteamFolder(_ url: URL) {
        let folder = Self.steamFolderUsable(url) ? url : url.appendingPathComponent("Call of Duty 2")
        guard GameSettings.safeValue(folder.path) else {
            setupError = "Choose a path without quotes, plus signs, semicolons or control characters."
            return
        }
        setupError = ""
        steam.folder = folder.path
        refreshSteamRoom()
    }

    // MARK: A try
    /// Continue on the offer, Resume, Try Again and Use Another Account all start here.
    func startSteam() {
        guard [.offer, .stopped, .notOwned].contains(steam.phase), steamWork.run == nil, steamWork.task == nil else { return }
        refreshSteamRoom()
        guard !steamRoomShort else { steam.phase = .offer; return }
        setupError = ""
        steam.qr = nil; steam.reachingSteam = false; steam.detailsShown = false; steam.log = []
        steam.currentFile = ""; steam.secondsLeft = nil; steam.doneBytes = 0
        steam.totalBytes = SteamGame.bytes(steam.language)
        steam.phase = .starting
        if snapshot { return }
        rememberSteamUnfinished()
        let generation = steamWork.generation + 1
        steamWork = SteamWork(generation: generation, progress: SteamProgress(depots: SteamGame.depots(steam.language)))
        let folder = URL(fileURLWithPath: steam.folder), language = steam.language, tool = steamTool, scratch = steamScratch
        steamWork.task = Task { [weak self] in
            do {
                try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                let executable = try await tool.install(into: scratch.appendingPathComponent("tool"))
                guard let self, self.steamWork.generation == generation, !Task.isCancelled else { return }
                self.steam.reachingSteam = true
                let session = scratch.appendingPathComponent("session-" + UUID().uuidString)
                let run = DepotRun(tool: executable,
                                   arguments: DepotTool.arguments(language: language, folder: folder, loginID: UInt32.random(in: 100_000_000...999_999_999)),
                                   home: session) { [weak self] event in self?.steamEvent(event, generation: generation) }
                self.steamWork.session = session
                self.steamWork.startedAt = Date(); self.steamWork.lastActivity = Date()
                try run.start()
                self.steamWork.run = run
                self.steamWork.awake = ProcessInfo.processInfo.beginActivity(options: [.userInitiated, .idleSystemSleepDisabled],
                                                                             reason: "Downloading Call of Duty 2 from Steam")
                self.steamWork.task = nil
                self.startSteamTicker(generation)
            } catch {
                guard let self, self.steamWork.generation == generation, !(error is CancellationError) else { return }
                self.steamWork.task = nil
                self.removeSteamSession()
                self.refreshSteamUnfinished()
                let message = (error as? DepotToolError)?.message ?? "The download couldn’t start: \(error.localizedDescription)"
                self.steamStopped(title: "The download couldn’t start", detail: message, retry: "Try Again")
            }
        }
    }
    private func steamEvent(_ event: DepotRun.Event, generation: Int) {
        guard steamWork.generation == generation else { return }
        switch event {
        case .exited(let status): steamRunEnded(status: status)
        case .output(let output):
            steamWork.lastActivity = Date()
            switch output {
            case .connecting: steam.reachingSteam = true
            case .qr(let matrix):
                steam.qr = matrix
                if steam.phase == .starting { steam.phase = .qr }
                if steamWork.codeDeadline == nil { steamWork.codeDeadline = Date().addingTimeInterval(steamTimings.signIn) }
            case .signedIn:
                steamWork.signedIn = true; steamWork.codeDeadline = nil
                steam.qr = nil
                steam.phase = .downloading
            case .notOwned: steamWork.notOwned = true
            case .signInFailed(let text): steamWork.failure = text
            case .depot(let id): steamWork.progress.depot(id)
            case .file(let percent, let path):
                steamWork.progress.file(percent: percent)
                steam.currentFile = steamRelative(path)
            case .checking(let path): steam.currentFile = steamRelative(path)
            case .finished: steamWork.finished = true
            }
            if steam.phase == .downloading { steam.doneBytes = steamWork.progress.done }
        }
    }
    /// A path the tool printed, as the game folder's own: “main/iw_07.iwd”.
    private func steamRelative(_ path: String) -> String {
        let folder = steam.folder.hasSuffix("/") ? steam.folder : steam.folder + "/"
        let trimmed = path.hasPrefix(folder) ? String(path.dropFirst(folder.count)) : path
        return trimmed.replacingOccurrences(of: "\\", with: "/")
    }
    private func startSteamTicker(_ generation: Int) {
        let tick = steamTimings.tick
        steamWork.ticker = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(tick))
                guard let self, self.steamWork.generation == generation, !Task.isCancelled else { return }
                self.steamTick()
            }
        }
    }
    private func steamTick() {
        guard let run = steamWork.run, run.isRunning else { return }
        let now = Date()
        let written = run.bytesWritten, before = steamWork.progress.done
        if written > 0 { steamWork.sawWrites = true }
        steamWork.progress.wrote(written)
        if steamWork.progress.done != before { steamWork.lastActivity = now }
        switch steam.phase {
        case .downloading:
            steam.doneBytes = steamWork.progress.done
            steamWork.samples.append((now, steam.doneBytes))
            steamWork.samples.removeAll { now.timeIntervalSince($0.at) > 30 }
            if let first = steamWork.samples.first, now.timeIntervalSince(first.at) >= 5, steam.doneBytes > first.bytes {
                let rate = Double(steam.doneBytes - first.bytes) / now.timeIntervalSince(first.at)
                steam.secondsLeft = Int(Double(steam.totalBytes - steam.doneBytes) / rate)
            }
            // One archive can take minutes and the tool prints nothing meanwhile: its disk writes show
            // it is alive. Should that count never move at all, only a much longer silence is a stall.
            let patience = steamWork.sawWrites ? steamTimings.stall : steamTimings.stallUnseen
            if now.timeIntervalSince(steamWork.lastActivity) > patience {
                steamWork.stalled = true
                run.stop()
            }
        case .starting, .qr:
            if let deadline = steamWork.codeDeadline, now > deadline {
                steamWork.timedOut = true
                run.stop()
            } else if steamWork.codeDeadline == nil, now.timeIntervalSince(steamWork.startedAt) > steamTimings.connect {
                steamWork.failure = steamWork.failure ?? "Steam didn’t answer."
                run.stop()
            }
        default: break
        }
    }
    /// The sign-in the tool stored is in its own home folder: removing that folder signs out.
    private func removeSteamSession() {
        if let session = steamWork.session { try? FileManager.default.removeItem(at: session) }
        steamWork.session = nil
        letSteamSleep()
    }
    private func letSteamSleep() {
        if let awake = steamWork.awake { ProcessInfo.processInfo.endActivity(awake) }
        steamWork.awake = nil
    }
    private func steamStopped(title: String, detail: String, retry: String) {
        steam.stoppedTitle = title; steam.stoppedDetail = detail; steam.retryTitle = retry
        steam.qr = nil; steam.secondsLeft = nil
        steam.phase = .stopped
    }
    private func steamRunEnded(status: Int32) {
        let work = steamWork
        steam.log = work.run?.log ?? []
        keepSteamLog()
        steamWork.ticker?.cancel(); steamWork.ticker = nil
        steamWork.run = nil
        removeSteamSession()
        refreshSteamUnfinished()
        let kept = "What is downloaded is kept in \(steamFolderDisplay)."
        if work.notOwned {
            steam.qr = nil
            steam.phase = .notOwned
        } else if !work.signedIn {
            let detail = work.timedOut ? "The code wasn’t approved in time. Nothing was downloaded."
                : work.failure == "Steam didn’t answer." ? "Steam didn’t answer. Check your connection and try again."
                : work.failure != nil ? "Steam didn’t accept the sign-in. Nothing was downloaded."
                : "The connection to Steam closed before the sign-in finished. Nothing was downloaded."
            steamStopped(title: "The sign-in didn’t go through", detail: detail, retry: "Try Again")
        } else if work.stopRequested {
            steamStopped(title: "The download stopped at \(steamAmountWords)", detail: "You stopped it. " + kept, retry: "Resume")
        } else if work.stalled {
            steamStopped(title: "The download stopped at \(steamAmountWords)", detail: "The connection to Steam was lost. " + kept, retry: "Resume")
        } else {
            checkSteamFiles(ended: work.finished && status == 0, generation: work.generation)
        }
    }
    /// What the tool said in its last try, beside the game's own logs, for when a download needs
    /// looking into. It never holds a sign-in code, it stays on this Mac, and no report sends it.
    private func keepSteamLog() {
        guard !snapshot, !steam.log.isEmpty else { return }
        let folder = home.appendingPathComponent("logs", isDirectory: true)
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        try? Data((steam.log.joined(separator: "\n") + "\n").utf8).write(to: folder.appendingPathComponent("steam-download.log"))
    }
    /// The tool has gone. The game counts as downloaded when the tool said it finished and the files
    /// agree: its exit code alone is 0 even when Steam skipped a depot, and a download cut short
    /// leaves every archive on disk at full size, empty.
    private func checkSteamFiles(ended: Bool, generation: Int) {
        steam.phase = .checking
        steam.secondsLeft = nil
        let folder = steam.folder
        steamWork.task = Task { [weak self] in
            let (valid, bytes, survey) = await Task.detached(priority: .userInitiated) {
                (Self.steamGameComplete(folder), Self.steamFolderBytes(folder), GameArchives.survey(folder))
            }.value
            guard let self, self.steamWork.generation == generation else { return }
            self.steamWork.task = nil
            if ended, let valid {
                try? FileManager.default.removeItem(at: URL(fileURLWithPath: folder).appendingPathComponent(".DepotDownloader"))
                try? FileManager.default.removeItem(at: self.steamScratch)
                self.forgetSteamUnfinished()
                self.steamWork.progress.finish()
                self.steam.doneBytes = self.steam.totalBytes
                self.steam.downloadedBytes = bytes
                self.attemptedDataPath = valid; self.dataPath = valid; self.dataError = ""
                self.steam.phase = .done
                if !NSApp.isActive { self.notifySteamDone() }
            } else if ended {
                let more = survey.missing.count - 1
                let missing = survey.missing.first.map { first in
                    more > 0 ? "\(first) and \(more) more game archive\(more == 1 ? " are" : "s are") missing." : "\(first) is missing."
                } ?? "The files for the language you chose are missing."
                self.steamStopped(title: "Steam finished, but the game isn’t complete",
                                  detail: missing + " Resume checks what is there and fetches the rest.", retry: "Resume")
            } else {
                self.steamStopped(title: "The download stopped at \(self.steamAmountWords)",
                                  detail: "The download tool stopped unexpectedly. What is downloaded is kept in \(self.steamFolderDisplay).", retry: "Resume")
            }
        }
    }
    /// The folder as game data, when all sixteen archives and the chosen language's own are there.
    nonisolated static func steamGameComplete(_ folder: String) -> String? {
        guard let valid = LauncherFindData(folder) else { return nil }
        let main = ((try? FileManager.default.contentsOfDirectory(atPath: valid + "/main")) ?? []).map { $0.lowercased() }
        return main.contains { $0.hasPrefix("localized_") && $0.hasSuffix(".iwd") } ? valid : nil
    }
    /// A folder a Steam download was started into and never finished: its archives exist at full size
    /// whether or not anything is in them, so the game data step must not take it as it is.
    nonisolated static func steamDownloadUnfinished(_ folder: String) -> Bool {
        var directory: ObjCBool = false
        return FileManager.default.fileExists(atPath: folder + "/.DepotDownloader", isDirectory: &directory) && directory.boolValue
    }
    nonisolated static func steamFolderBytes(_ folder: String) -> Int64 {
        let root = URL(fileURLWithPath: folder)
        guard let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.fileSizeKey, .isRegularFileKey]) else { return 0 }
        var total: Int64 = 0
        for case let file as URL in walk {
            if file.path.contains("/.DepotDownloader/") { continue }
            let values = try? file.resourceValues(forKeys: [.fileSizeKey, .isRegularFileKey])
            if values?.isRegularFile == true { total += Int64(values?.fileSize ?? 0) }
        }
        return total
    }
    private func notifySteamDone() {
        NSApp.requestUserAttention(.informationalRequest)
    }

    // MARK: Leaving
    /// The bar's Stop while downloading: the files stay, the sign-in goes.
    func stopSteam() {
        guard steam.phase == .downloading, let run = steamWork.run else { return }
        steamWork.stopRequested = true
        run.stop()
    }
    /// Ends whatever is running without a word: the screen has already moved on.
    private func abandonSteamRun() {
        steamWork.generation += 1
        steamWork.task?.cancel(); steamWork.task = nil
        steamWork.ticker?.cancel(); steamWork.ticker = nil
        letSteamSleep()
        let run = steamWork.run, session = steamWork.session
        steamWork.run = nil; steamWork.session = nil
        guard let run else {
            if let session { try? FileManager.default.removeItem(at: session) }
            return
        }
        run.stop()
        Task {
            await run.finished()
            if let session { try? FileManager.default.removeItem(at: session) }
        }
    }
    /// Back inside the Steam screens: to the offer, and from the offer to the game data step.
    func backFromSteam() {
        setupError = ""
        if steam.phase == .offer || steam.phase == .done {
            leaveSteam()
        } else {
            abandonSteamRun()
            steam.qr = nil
            steam.phase = .offer
            refreshSteamRoom()
        }
    }
    /// Out of the Steam flow altogether: nothing of the tool or a sign-in stays. Files already downloaded do.
    func leaveSteam() {
        abandonSteamRun()
        let scratch = steamScratch
        if !snapshot { Task.detached(priority: .utility) { try? FileManager.default.removeItem(at: scratch) } }
        steam = SteamSetup(language: steam.language, folder: steam.folder)
        refreshSteamUnfinished()
    }
    /// Choose Folder… from a Steam screen: back to the game data step with the folder panel open.
    func chooseFolderInsteadOfSteam() {
        leaveSteam()
        pickData()
    }
    /// The launcher is quitting: the tool goes with it, and so does anything it stored.
    func steamShutdown() {
        steamWork.task?.cancel(); steamWork.ticker?.cancel()
        steamWork.run?.stopNow()
        if !snapshot, FileManager.default.fileExists(atPath: steamScratch.path) { try? FileManager.default.removeItem(at: steamScratch) }
    }
    /// At start: a launcher that was killed mid-download may have left the tool and a sign-in behind.
    func sweepSteam() {
        guard !snapshot else { return }
        let scratch = steamScratch
        Task.detached(priority: .utility) { try? FileManager.default.removeItem(at: scratch) }
    }

    // MARK: Review harness
    func previewSteam(_ screen: String) -> Bool {
        guard screen.hasPrefix("setup-steam") else { return false }
        setupStep = .data; dataPath = ""
        steam.folder = NSHomeDirectory() + "/Games/CoD2"; steam.freeBytes = 212_000_000_000
        steam.totalBytes = SteamGame.bytes(.english)
        switch screen {
        case "setup-steam-unfinished": steamUnfinished = SteamUnfinished(folder: steam.folder, language: "english")
        case "setup-steam": steam.phase = .offer
        case "setup-steam-short": steam.phase = .offer; steam.freeBytes = 2_100_000_000
        case "setup-steam-starting": steam.phase = .starting
        case "setup-steam-qr": steam.phase = .qr; steam.qr = QRMatrix.preview
        case "setup-steam-not-owned": steam.phase = .notOwned
        case "setup-steam-downloading": steam.phase = .downloading; steam.doneBytes = 1_420_000_000; steam.secondsLeft = 360; steam.currentFile = "main/iw_07.iwd"
        case "setup-steam-checking": steam.phase = .checking; steam.doneBytes = steam.totalBytes; steam.currentFile = "main/iw_15.iwd"
        case "setup-steam-stopped":
            steam.doneBytes = 1_420_000_000
            steamStopped(title: "The download stopped at \(steamAmountWords)", detail: "The connection to Steam was lost. What is downloaded is kept in \(steamFolderDisplay).", retry: "Resume")
        case "setup-steam-stopped-details":
            steam.doneBytes = 1_420_000_000
            steamStopped(title: "The download stopped at \(steamAmountWords)", detail: "The connection to Steam was lost. What is downloaded is kept in \(steamFolderDisplay).", retry: "Resume")
            steam.detailsShown = true
            steam.log = ["Connecting to Steam3... Done!", "Logging in with QR code...", " Done!", "Got 214 licenses for account!", "Downloading depot 2632", " 38.20% main/iw_06.iwd"]
        case "setup-steam-sign-in-failed":
            steamStopped(title: "The sign-in didn’t go through", detail: "The code wasn’t approved in time. Nothing was downloaded.", retry: "Try Again")
        case "setup-steam-done": steam.phase = .done; steam.downloadedBytes = 3_697_552_613; dataPath = steam.folder
        default: return false
        }
        return true
    }
}
