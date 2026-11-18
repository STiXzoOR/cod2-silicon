import AppKit
import Foundation

/// Setup › Game data › From Steam, and Setup's crash reports question. The download tool is a
/// stand-in script that prints what DepotDownloader 3.4.0 prints; Steam is never contacted.
@main struct SteamDownloadTests {
    static func require(_ condition: @autoclosure () -> Bool, _ message: @autoclosure () -> String) {
        if !condition() { FileHandle.standardError.write(Data("FAIL: \(message())\n".utf8)); exit(1) }
    }
    @MainActor static func wait(_ what: String, seconds: Double = 12, until done: () -> Bool) async {
        let deadline = Date().addingTimeInterval(seconds)
        while !done() {
            if Date() > deadline { require(false, "timed out waiting for \(what)") }
            try? await Task.sleep(for: .milliseconds(20))
        }
    }

    /// A code as the tool prints it: two characters a module, four blank modules on every side.
    static func printed(_ code: QRMatrix) -> [String] {
        let blank = String(repeating: " ", count: (code.size + 8) * 2)
        let rows = (0..<code.size).map { row in
            "        " + (0..<code.size).map { code[row, $0] ? "██" : "  " }.joined() + "        "
        }
        return Array(repeating: blank, count: 4) + rows + Array(repeating: blank, count: 4)
    }

    static func main() async throws {
        let scratch = URL(fileURLWithPath: CommandLine.arguments[1]).resolvingSymlinksInPath()
        let home = URL(fileURLWithPath: NSHomeDirectory()).resolvingSymlinksInPath()
        require(home.path.hasPrefix(scratch.path + "/"), "these tests need a private home below \(scratch.path), not \(home.path)")
        if CommandLine.arguments.contains("--live") {
            try await live(scratch)
            return
        }
        parser()
        progress()
        try await tool(scratch)
        try await flows(scratch)
        await crashQuestion()
        print("PASS: Steam download (parser, progress, tool, eight flows) and the crash reports question")
    }

    // MARK: What the tool prints
    static func parser() {
        let code = QRMatrix.preview
        require(code.size == 29 && code[0, 0] && code[0, 28] && code[28, 0] && !code[7, 7], "the harness code has its three finder squares")
        var reader = DepotOutputParser()
        require(reader.feed("Connecting to Steam3... Done!") == [.connecting], "connecting")
        require(reader.feed("Logging in with QR code...").isEmpty, "the login line means nothing yet")
        require(reader.feed("Use the Steam Mobile App to sign in with this QR code:").isEmpty, "the header starts a code")
        var events: [DepotEvent] = []
        let lines = printed(code)
        for (index, line) in lines.enumerated() {
            let out = reader.feed(line)
            if index < lines.count - 1 { require(out.isEmpty, "no code before its last row (row \(index))") }
            events += out
        }
        require(events == [.qr(code)], "the code is read the moment its quiet zone closes, without waiting for another line")
        require(reader.feed("").isEmpty && reader.feed("The QR code has changed:").isEmpty, "a renewed code is announced")
        _ = reader.feed("Use the Steam Mobile App to sign in with this QR code:")
        var second: [DepotEvent] = []
        for line in lines.dropLast(2) { second += reader.feed(line) }
        require(second.isEmpty && reader.idle() == [.qr(code)], "a code cut short of its last rows is read once the tool goes quiet")
        require(reader.idle().isEmpty, "and only once")
        require(reader.feed(" Done!") == [.signedIn], "the phone's approval")
        require(reader.feed("Got 214 licenses for account!").isEmpty, "signed in is said once")
        require(reader.feed("Processing depot 2632").isEmpty, "a depot being read is not a depot being downloaded")
        require(reader.feed("Downloading depot 2632") == [.depot(2632)], "a depot starts")
        require(reader.feed("Pre-allocating /Games/CoD2/main/iw_07.iwd") == [.checking(path: "/Games/CoD2/main/iw_07.iwd")], "making room for a file")
        require(reader.feed("Validating /Games/CoD2/main/iw_07.iwd") == [.checking(path: "/Games/CoD2/main/iw_07.iwd")], "checking a file on disk")
        require(reader.feed(" 38.20% /Games/CoD2/main/iw 07.iwd") == [.file(percent: 38.2, path: "/Games/CoD2/main/iw 07.iwd")], "a finished file, spaces in its path kept")
        require(reader.feed("100.00% /Games/CoD2/main/iw_15.iwd") == [.file(percent: 100, path: "/Games/CoD2/main/iw_15.iwd")], "the last file of a depot")
        require(reader.feed("250.00% nonsense").isEmpty && reader.feed("50% done").isEmpty == false, "only a percentage between 0 and 100 counts")
        require(reader.feed("Depot 2632 - Downloaded 1360000000 bytes (1360000000 bytes uncompressed)").isEmpty, "a depot's own total is not the end")
        require(reader.feed("Total downloaded: 3697552613 bytes (3697552613 bytes uncompressed) from 4 depots") == [.finished], "the closing line")

        var refused = DepotOutputParser()
        _ = refused.feed("Use the Steam Mobile App to sign in with this QR code:")
        for line in lines { _ = refused.feed(line) }
        require(refused.feed(" Done!") == [.signedIn], "approved")
        require(refused.feed("App 2630 (Call of Duty 2) is not available from this account.") == [.notOwned], "an account without the game")
        var failed = DepotOutputParser()
        require(failed.feed("Failed to authenticate with Steam: AccessDenied") == [.signInFailed("Failed to authenticate with Steam: AccessDenied")], "a refused sign-in")
        require(failed.feed("Unable to get steam3 credentials.").isEmpty && failed.feed("Error: InitializeSteam failed").isEmpty, "and its echoes are not three failures")
        var early = DepotOutputParser()
        require(early.feed(" Done!").isEmpty && early.feed("Got 3 licenses for account!").isEmpty, "nothing counts as approval before a code was shown")

        require(QRMatrix(lines: ["  ██  ", "      "]) == nil, "three modules are not a code")
        require(QRMatrix(lines: printed(code).map { String($0.dropLast(20)) }) == nil, "a code that lost its right edge is not shown")

        let arguments = DepotTool.arguments(language: .german, folder: URL(fileURLWithPath: "/Games/CoD2"), loginID: 123_456_789)
        require(arguments == ["-app", "2630", "-depot", "2631", "2632", "2633", "2636", "-os", "windows", "-language", "german", "-qr", "-dir", "/Games/CoD2", "-loginid", "123456789"],
                "every depot is named, the code sign-in is asked for and nothing else: \(arguments)")
        require(!arguments.contains("-username") && !arguments.contains("-password") && !arguments.contains("-remember-password"), "no name or password ever reaches the tool")
        require(Set(SteamLanguage.allCases.map(\.depot)) == [2634, 2636, 2637, 2638, 2639], "one depot per language")
        require(SteamGame.gigabytes(SteamGame.bytes(.english)) == "3.70" && SteamGame.gigabytes(1_420_000_000) == "1.42", "sizes as the boards say them")
    }

    static func progress() {
        var bar = SteamProgress(depots: SteamGame.depots(.english))
        require(bar.total == 3_702_000_000 && bar.done == 0, "nothing yet")
        bar.depot(2631); bar.file(percent: 100)
        require(bar.done == 12_000_000, "the first depot is 12 MB")
        bar.depot(2632); bar.wrote(100_000_000)
        require(bar.done == 12_000_000 + 100_000_000, "bytes written move the bar before any file of a depot is finished")
        bar.file(percent: 50)
        require(bar.done == 12_000_000 + 680_000_000, "half the second depot: bytes written before a finished file are already in its count")
        bar.wrote(150_000_000)
        require(bar.done == 12_000_000 + 680_000_000 + 50_000_000, "bytes written since move the bar between files")
        bar.file(percent: 51)
        require(bar.done == 12_000_000 + 680_000_000 + 50_000_000, "the bar never goes back when a file line lands under it")
        bar.file(percent: 10)
        require(bar.done == 12_000_000 + 1_360_000_000 + 80_000_000, "a depot that wasn't announced is noticed when the count starts over")
        bar.depot(2634); bar.file(percent: 100); bar.wrote(99_000_000_000)
        require(bar.done < bar.total, "the bar keeps a sliver until the files are checked")
        bar.finish()
        require(bar.done == bar.total, "then it is full")
    }

    // MARK: The tool itself
    /// A stand-in tool as a release archive. Every one records its arguments, stores a pretend
    /// sign-in the way the real tool does, and shows a code before `body` runs.
    static func archive(_ name: String, in scratch: URL, body: String) throws -> DepotTool {
        let folder = scratch.appendingPathComponent("tools/\(name)")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let code = printed(QRMatrix.preview).joined(separator: "\n")
        let script = """
        #!/bin/bash
        printf '%s\\n' "$@" > '\(scratch.path)/\(name).args'
        dir=""; while [ $# -gt 0 ]; do [ "$1" = "-dir" ] && dir=$2; shift; done
        store="$HOME/Library/Application Support/IsolatedStorage/a/b"
        mkdir -p "$store" && echo refresh-token > "$store/account.config"
        echo "$HOME" > '\(scratch.path)/\(name).home'
        echo "Connecting to Steam3... Done!"
        echo "Logging in with QR code..."
        echo "Use the Steam Mobile App to sign in with this QR code:"
        cat <<'CODE'
        \(code)
        CODE
        approve() { sleep 0.3; echo " Done!"; echo "Got 3 licenses for account!"; }
        files() { mkdir -p "$dir/main" "$dir/.DepotDownloader"; for i in 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15; do echo "Pre-allocating $dir/main/iw_$i.iwd"; printf data > "$dir/main/iw_$i.iwd"; done; }
        \(body)
        """
        let tool = folder.appendingPathComponent(DepotTool.executableName)
        try Data(script.utf8).write(to: tool)
        try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: tool.path)
        let zip = scratch.appendingPathComponent("tools/\(name).zip")
        let pack = Process()
        pack.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
        pack.arguments = ["-c", "-k", folder.path, zip.path]
        try pack.run(); pack.waitUntilExit()
        require(pack.terminationStatus == 0, "the stand-in tool \(name) was packed")
        return DepotTool(archive: zip, sha256: DepotTool.digest(of: zip) ?? "")
    }

    static func tool(_ scratch: URL) async throws {
        require(DepotTool.pinned.archive.host == "github.com" && DepotTool.pinned.archive.path.hasPrefix("/SteamRE/DepotDownloader/releases/download/")
                && DepotTool.pinned.sha256.count == 64, "the real tool comes from its own release page, pinned by checksum")
        let good = try archive("plain", in: scratch, body: "exit 0")
        let folder = scratch.appendingPathComponent("install/tool")
        let executable = try await good.install(into: folder)
        require(FileManager.default.isExecutableFile(atPath: executable.path) && executable.lastPathComponent == "DepotDownloader", "the tool is unpacked and can run")
        require(!FileManager.default.fileExists(atPath: folder.appendingPathComponent("tool.zip").path), "its archive is not kept")
        let stamp = try FileManager.default.attributesOfItem(atPath: executable.path)[.modificationDate] as? Date
        _ = try await good.install(into: folder)
        let again = try FileManager.default.attributesOfItem(atPath: executable.path)[.modificationDate] as? Date
        require(again == stamp, "a checked copy is used again, not fetched again")
        do {
            _ = try await DepotTool(archive: good.archive, sha256: String(repeating: "0", count: 64)).install(into: folder)
            require(false, "a tool with the wrong checksum must not be installed")
        } catch let error as DepotToolError {
            require(error == .checksum && !FileManager.default.fileExists(atPath: folder.path), "the wrong checksum leaves nothing behind")
        }
        do {
            _ = try await DepotTool(archive: scratch.appendingPathComponent("tools/missing.zip"), sha256: good.sha256).install(into: folder)
            require(false, "a tool that can't be fetched must fail")
        } catch let error as DepotToolError { require(error == .fetch, "an unreachable archive is a fetch failure") }
    }

    // MARK: Setup, end to end
    @MainActor static func flows(_ scratch: URL) async throws {
        _ = NSApplication.shared
        let files = FileManager.default
        let model = LauncherModel()
        model.steamTimings = SteamTimings(signIn: 1.5, connect: 4, stall: 1.5, stallUnseen: 1.5, tick: 0.1)
        model.onboard = true; model.setupStep = .data; model.dataPath = ""
        let games = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Games")
        let scratchTool = model.steamScratch
        func sessions() -> [String] { ((try? files.contentsOfDirectory(atPath: scratchTool.path)) ?? []).filter { $0.hasPrefix("session-") } }
        func recorded(_ name: String) -> String { (try? String(contentsOf: scratch.appendingPathComponent(name), encoding: .utf8)) ?? "" }

        // The offer: what, which language, where.
        require(model.setupCanContinue == false, "game data is needed before Continue")
        model.offerSteam()
        require(model.steam.phase == .offer && model.steam.folder == games.appendingPathComponent("CoD2").path, "the offer names ~/Games/CoD2: \(model.steam.folder)")
        require(model.steamSizeWords == "3.70 GB" && model.steam.freeBytes ?? 0 > 0 && model.setupCanContinue, "size and room are shown, and Continue works")
        require(model.steamHeading.title == "Download your copy from Steam", "the offer's heading")
        try files.createDirectory(at: games.appendingPathComponent("CoD2/main"), withIntermediateDirectories: true)
        require(LauncherModel.steamDefaultFolder().lastPathComponent == "CoD2 from Steam", "a folder with something in it is left alone")
        try files.removeItem(at: games.appendingPathComponent("CoD2"))
        let other = scratch.appendingPathComponent("elsewhere")
        try files.createDirectory(at: other.appendingPathComponent("photos"), withIntermediateDirectories: true)
        model.chooseSteamFolder(other)
        require(model.steam.folder == other.appendingPathComponent("Call of Duty 2").path, "a folder that holds other things gets a folder of the game's own")
        model.chooseSteamFolder(scratch.appendingPathComponent("bad;name"))
        require(!model.setupError.isEmpty && model.steam.folder == other.appendingPathComponent("Call of Duty 2").path, "a path the game can't be given is refused")
        let target = scratch.appendingPathComponent("game")
        model.chooseSteamFolder(target)
        require(model.setupError.isEmpty && model.steam.folder == target.path, "an empty place is used as it is")
        model.steam.freeBytes = 1_000_000_000
        require(model.steamRoomShort && !model.setupCanContinue, "too little room blocks Continue")
        model.backSetup()
        require(model.steam.phase == .off && model.setupStep == .data, "Back from the offer is the game data step")
        model.offerSteam()
        require(model.steam.folder == target.path, "the chosen place is remembered")
        model.steam.language = .german

        // 1. The checksum doesn't match: the tool never runs.
        let good = try archive("ok", in: scratch, body: """
        approve
        echo "Processing depot 2631"; echo "Downloading depot 2631"; files
        echo " 50.00% $dir/main/iw_03.iwd"; sleep 0.3; echo "100.00% $dir/main/iw_15.iwd"
        echo "Downloading depot 2636"; printf data > "$dir/main/localized_german_iw00.iwd"; echo "100.00% $dir/main/localized_german_iw00.iwd"
        echo "Total downloaded: 3697552613 bytes (3697552613 bytes uncompressed) from 4 depots"
        """)
        model.steamTool = DepotTool(archive: good.archive, sha256: String(repeating: "f", count: 64))
        model.nextSetup()
        require(model.steam.phase == .starting && !model.setupCanContinue, "Continue starts the try")
        await wait("the checksum refusal") { model.steam.phase == .stopped }
        require(model.steam.stoppedTitle == "The download couldn’t start" && model.steam.stoppedDetail.contains("checksum") && model.steam.retryTitle == "Try Again",
                "a tool that fails its check is said so: \(model.steam.stoppedDetail)")
        require(recorded("ok.args").isEmpty, "and is never run")

        // 2. The code is never approved.
        model.steamTool = try archive("unapproved", in: scratch, body: "sleep 30")
        model.startSteam()
        await wait("the code") { model.steam.phase == .qr }
        require(model.steam.qr == QRMatrix.preview && model.steamHeading.title == "Sign in with the Steam app", "the tool's code is on screen")
        require(sessions().count == 1, "the try has a home of its own for the sign-in")
        await wait("the sign-in to time out") { model.steam.phase == .stopped }
        require(model.steam.stoppedTitle == "The sign-in didn’t go through" && model.steam.stoppedDetail.hasPrefix("The code wasn’t approved in time") && model.steam.retryTitle == "Try Again",
                "an unapproved code ends the try: \(model.steam.stoppedDetail)")
        require(sessions().isEmpty && model.steam.qr == nil, "nothing of the sign-in is kept")
        require(!model.steam.log.isEmpty && !model.steam.log.contains { $0.contains("█") }, "the details hold what the tool said, never a code")
        let kept = (try? String(contentsOf: model.home.appendingPathComponent("logs/steam-download.log"), encoding: .utf8)) ?? ""
        require(kept.contains("Connecting to Steam3") && !kept.contains("█"), "the same is kept beside the game's logs, again without a code")

        // 3. The account doesn't have the game.
        model.steamTool = try archive("notowned", in: scratch, body: "approve\necho 'App 2630 (Call of Duty 2) is not available from this account.'\nexit 1")
        model.startSteam()
        await wait("Steam's refusal") { model.steam.phase == .notOwned }
        require(model.steamHeading.title == "This account doesn’t have the game" && sessions().isEmpty && !model.setupCanContinue, "an account without the game is told so")

        // 4. The connection goes mid-download.
        model.steamTool = try archive("stall", in: scratch, body: "approve\necho 'Downloading depot 2631'; files\necho \" 25.00% $dir/main/iw_03.iwd\"\nsleep 30")
        model.startSteam()  // Use Another Account
        await wait("the download") { model.steam.phase == .downloading && model.steam.currentFile == "main/iw_03.iwd" }
        require(model.steam.doneBytes == 3_000_000 && model.steam.totalBytes == 3_702_000_000 && model.setupAlternateTitle == "Stop",
                "a quarter of the first depot, and Stop is offered: \(model.steam.doneBytes)")
        await wait("the stall to be noticed") { model.steam.phase == .stopped }
        require(model.steam.stoppedTitle == "The download stopped at 0.00 of 3.70 GB" && model.steam.stoppedDetail.hasPrefix("The connection to Steam was lost.")
                && model.steam.retryTitle == "Resume", "a lost connection is a stop that can be resumed: \(model.steam.stoppedTitle)")
        require(files.fileExists(atPath: target.appendingPathComponent("main/iw_15.iwd").path) && sessions().isEmpty, "the files stay, the sign-in goes")
        require(recorded("stall.home").hasPrefix(scratchTool.path + "/session-"), "the tool's home was the try's own folder")
        require(!files.fileExists(atPath: recorded("stall.home").trimmingCharacters(in: .newlines)), "and that folder, with the sign-in in it, is gone")

        // The folder now looks complete to a plain check: sixteen archives, none finished.
        require(LauncherFindData(target.path) != nil && LauncherModel.steamDownloadUnfinished(target.path), "an unfinished download has every archive on disk")
        model.backSetup(); model.backSetup()
        require(model.steam.phase == .off, "back to the game data step")
        require(!model.chooseData(target) && model.dataIsUnfinishedSteam && !model.setupCanContinue, "the folder picker won't take it as it is")
        model.steam.folder = ""
        model.finishSteamDownload()
        require(model.steam.phase == .offer && model.steam.folder == target.path && model.dataError.isEmpty, "Finish the Download… opens the offer on that folder")

        // 5. The tool dies mid-download.
        model.steamTool = try archive("crash", in: scratch, body: "approve\necho 'Downloading depot 2631'; files\nexit 1")
        model.nextSetup()
        await wait("the crash to be reported") { model.steam.phase == .stopped }
        require(model.steam.stoppedDetail.hasPrefix("The download tool stopped unexpectedly") && model.dataPath.isEmpty, "a tool that dies is not a finished download")

        // 6. Steam finishes without the language's files (a skipped depot): exit 0 means nothing.
        model.steamTool = try archive("skipped", in: scratch, body: "approve\nfiles\necho 'Total downloaded: 0 bytes (0 bytes uncompressed) from 3 depots'\nexit 0")
        model.startSteam()
        await wait("the incomplete finish") { model.steam.phase == .stopped }
        require(model.steam.stoppedTitle == "Steam finished, but the game isn’t complete" && model.steam.stoppedDetail.hasPrefix("The files for the language you chose are missing.")
                && model.dataPath.isEmpty, "the files decide, not the exit code: \(model.steam.stoppedDetail)")

        // 7. Stop, by the player.
        //    Between two finished files the bar moves by what the tool writes to disk: here 8 MB, by the tool's own process.
        model.steamTool = try archive("slow", in: scratch, body: "approve\necho 'Downloading depot 2632'; files\necho \" 50.00% $dir/main/iw_05.iwd\"\nsleep 0.5\nprintf '%*s' 8000000 '' > \"$dir/.DepotDownloader/part\"\nsleep 30")
        model.startSteam()
        await wait("the download") { model.steam.phase == .downloading && model.steam.doneBytes >= 12_000_000 + 680_000_000 }
        await wait("the tool's disk writes to move the bar") { model.steam.doneBytes >= 12_000_000 + 680_000_000 + 7_000_000 }
        require(model.steam.doneBytes < 12_000_000 + 680_000_000 + 12_000_000, "by about what was written: \(model.steam.doneBytes)")
        model.alternateSetup()
        await wait("the stop") { model.steam.phase == .stopped }
        require(model.steam.stoppedTitle == "The download stopped at 0.70 of 3.70 GB" && model.steam.stoppedDetail.hasPrefix("You stopped it."), "Stop says where it stopped: \(model.steam.stoppedTitle)")

        // 8. Resume, all the way.
        model.steamTool = good
        model.startSteam()
        await wait("the code") { model.steam.phase == .qr }
        await wait("the download") { model.steam.phase == .downloading }
        require(model.steamHeading.title == "Downloading Call of Duty 2", "the download's heading")
        await wait("the end") { model.steam.phase == .done }
        let folder = URL(fileURLWithPath: LauncherFindData(target.path) ?? "").resolvingSymlinksInPath().path
        require(URL(fileURLWithPath: model.dataPath).resolvingSymlinksInPath().path == folder && model.dataError.isEmpty && model.setupCanContinue,
                "the downloaded folder is the game data: \(model.dataPath)")
        require(model.steam.downloadedBytes == 17 * 4 && model.steam.doneBytes == model.steam.totalBytes, "what is in the folder is counted")
        require(!files.fileExists(atPath: scratchTool.path), "the tool and every sign-in are gone")
        require(!files.fileExists(atPath: target.appendingPathComponent(".DepotDownloader").path), "the tool's own notes are out of the game folder")
        let arguments = recorded("ok.args").split(separator: "\n").map(String.init)
        require(Array(arguments.prefix(7)) == ["-app", "2630", "-depot", "2631", "2632", "2633", "2636"] && arguments.contains("-qr")
                && arguments.firstIndex(of: "-dir").map { arguments[$0 + 1] } == target.path, "the tool was asked for German, by code, into the chosen folder: \(arguments)")
        model.nextSetup()
        require(model.setupStep == .key && model.steam.phase == .off, "Continue goes on to the CD key")
        let remembered = try String(contentsOf: model.home.appendingPathComponent("data-path.txt"), encoding: .utf8)
        require(remembered.trimmingCharacters(in: .newlines) == model.dataPath, "and the folder is remembered")
        model.backSetup()
        require(model.setupStep == .data && !model.steamActive && !model.dataPath.isEmpty, "Back shows the folder as found")

        // Leaving mid-try, and quitting, leave nothing behind either.
        model.dataPath = ""
        model.offerSteam(); model.chooseSteamFolder(scratch.appendingPathComponent("second"))
        model.steamTool = try archive("waiting", in: scratch, body: "sleep 30")
        model.nextSetup()
        await wait("the code") { model.steam.phase == .qr }
        model.backSetup()
        require(model.steam.phase == .offer && model.steam.qr == nil, "Back from the code is the offer")
        await wait("the abandoned try's sign-in to go") { sessions().isEmpty }
        model.nextSetup()
        await wait("the code") { model.steam.phase == .qr }
        model.steamShutdown()
        require(!files.fileExists(atPath: scratchTool.path), "quitting ends the tool and removes it and the sign-in")
    }

    // MARK: The real tool, as far as its code
    /// `steam_download.sh --live`: the pinned tool is fetched from its release page, checked, and run
    /// by the launcher's own code until Steam shows a code. Nobody signs in and nothing is downloaded
    /// from Steam. Not part of the ordinary run: it needs the network, GitHub and Steam.
    @MainActor static func live(_ scratch: URL) async throws {
        _ = NSApplication.shared
        let files = FileManager.default
        let model = LauncherModel()
        model.onboard = true; model.setupStep = .data; model.dataPath = ""
        model.offerSteam()
        let target = scratch.appendingPathComponent("game")
        model.chooseSteamFolder(target)
        let started = Date()
        model.nextSetup()
        require(model.steam.phase == .starting, "Continue starts the try")
        await wait("the tool to be fetched and checked", seconds: 180) { model.steam.reachingSteam || model.steam.phase == .stopped }
        require(model.steam.phase != .stopped, "the real tool was fetched and matched its checksum: \(model.steam.stoppedDetail)")
        let tool = model.steamScratch.appendingPathComponent("tool/DepotDownloader")
        require(files.isExecutableFile(atPath: tool.path), "the tool is on disk and can run")
        print("LIVE: the tool was fetched and checked in \(String(format: "%.1f", Date().timeIntervalSince(started))) s")
        await wait("Steam's code", seconds: 90) { model.steam.phase == .qr || model.steam.phase == .stopped }
        require(model.steam.phase == .qr, "Steam showed a code: \(model.steam.stoppedTitle) \(model.steam.stoppedDetail) \(model.steam.log.suffix(6))")
        guard let first = model.steam.qr else { require(false, "the code is on screen"); return }
        print("LIVE: a \(first.size)×\(first.size) code after \(String(format: "%.1f", Date().timeIntervalSince(started))) s")
        require(first[0, 0] && first[0, first.size - 1] && first[first.size - 1, 0] && !first[first.size - 1, first.size - 1] && first[6, 6] && !first[1, 1] && first[3, 3],
                "the code has a QR code's three corner squares")
        let sessions = ((try? files.contentsOfDirectory(atPath: model.steamScratch.path)) ?? []).filter { $0.hasPrefix("session-") }
        require(sessions.count == 1, "the tool runs in a home of its own")
        let session = model.steamScratch.appendingPathComponent(sessions[0])
        require(files.fileExists(atPath: session.appendingPathComponent("Library/Application Support/IsolatedStorage").path),
                "what the tool stores about a sign-in lands inside that home")
        require(!files.fileExists(atPath: NSHomeDirectory() + "/Library/Application Support/IsolatedStorage"), "and not in the launcher's")
        await wait("a renewed code", seconds: 60) { model.steam.qr != first || model.steam.phase != .qr }
        require(model.steam.phase == .qr && model.steam.qr != nil, "a renewed code replaces the first: \(model.steam.phase)")
        print("LIVE: the code renewed itself after \(String(format: "%.1f", Date().timeIntervalSince(started))) s")
        model.backSetup()
        require(model.steam.phase == .offer && model.steam.qr == nil, "Back leaves the sign-in")
        await wait("the sign-in's home to go", seconds: 15) { !files.fileExists(atPath: session.path) }
        model.backSetup()
        await wait("the tool to go", seconds: 15) { !files.fileExists(atPath: model.steamScratch.path) }
        let left = ((try? files.contentsOfDirectory(atPath: target.path)) ?? []).filter { $0 != ".DepotDownloader" }
        require(left.isEmpty, "nothing was downloaded: \(left)")
        print("PASS: the real download tool, fetched, checked and run by the launcher as far as Steam's code; nothing of it is left")
    }

    // MARK: Send crash reports?
    @MainActor static func crashQuestion() async {
        let plain = LauncherModel()
        require(plain.crashReportService == nil && plain.setupSteps == [.welcome, .data, .key, .shaders, .yours, .ready] && plain.setupStepCount == 6,
                "a build with nowhere to send reports doesn't ask")
        plain.answerCrashReports(send: true)
        plain.flushSettings()
        let reopened = LauncherModel()
        require(reopened.settings.sendCrashReports && reopened.settings.crashReportsAsked, "the answer is on disk for the next launch")
        reopened.answerCrashReports(send: false)
        reopened.flushSettings()
        require(!LauncherModel().settings.sendCrashReports && LauncherModel().settings.crashReportsAsked, "and so is a change of mind")
        let model = LauncherModel(snapshot: true)
        require(model.setupSteps.count == 7 && model.setupSteps[5] == .crash, "the question is the sixth of seven steps")
        require(!model.settings.sendCrashReports && !model.settings.crashReportsAsked, "nothing is sent, and nothing is answered, to begin with")
        model.onboard = true; model.setupStep = .yours
        model.nextSetup()
        require(model.setupStep == .crash && model.setupStepNumber == 6 && model.setupPrimaryTitle == "Send Crash Reports" && model.setupAlternateTitle == "Don’t Send",
                "after Make it yours comes the question, with both answers named")
        model.alternateSetup()
        require(model.setupStep == .ready && !model.settings.sendCrashReports && model.settings.crashReportsAsked, "Don’t Send is an answer")
        model.backSetup()
        require(model.setupStep == .crash, "Back from Ready is the question")
        model.nextSetup()
        require(model.setupStep == .ready && model.settings.sendCrashReports && model.settings.crashReportsAsked, "Send Crash Reports turns it on")
        model.setupStep = .yours; model.alternateSetup()
        require(model.setupStep == .crash, "Skip on Make it yours still reaches the question")
        require(CrashReportsQuestion.owed(hasService: true, asked: false, sending: false) == .ask, "someone set up before the question is asked")
        require(CrashReportsQuestion.owed(hasService: true, asked: false, sending: true) == .answered, "someone who already turned reports on has answered")
        require(CrashReportsQuestion.owed(hasService: true, asked: true, sending: false) == .nothing, "an answer is never asked for again")
        require(CrashReportsQuestion.owed(hasService: false, asked: false, sending: false) == .nothing, "a build with nowhere to send reports asks nothing")
        var saved = GameSettings(); saved.crashReportsAsked = true
        let decoded = try? JSONDecoder().decode(GameSettings.self, from: JSONEncoder().encode(saved))
        require(decoded?.crashReportsAsked == true, "the answer is saved with the settings")
        let older = try? JSONDecoder().decode(GameSettings.self, from: Data("{\"sendCrashReports\":false}".utf8))
        require(older?.crashReportsAsked == false, "settings from before the question count as not asked")
    }
}
