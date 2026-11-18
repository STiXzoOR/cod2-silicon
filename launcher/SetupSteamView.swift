import SwiftUI

// Owner: ship-shell. Setup › Game data › From Steam (the six “From Steam” boards) and Setup › Crash
// reports (SetupCrash board): the cards under the step's heading. SetupView.swift owns the frame, the
// heading and the bottom bar; ModelSteam.swift owns what happens.

/// The sign-in code as the Steam app scans it: dark squares on a white card, never themed.
struct QRCodeView: View {
    var matrix: QRMatrix
    var body: some View {
        Canvas(rendersAsynchronously: false) { context, size in
            let cell = size.width / CGFloat(matrix.size)
            var squares = Path()
            for row in 0..<matrix.size {
                for column in 0..<matrix.size where matrix[row, column] {
                    // Each square reaches a hair into the next, so no light seam shows between two dark ones.
                    squares.addRect(CGRect(x: CGFloat(column) * cell, y: CGFloat(row) * cell, width: cell + 0.3, height: cell + 0.3))
                }
            }
            context.fill(squares, with: .color(Color(red: 0.078, green: 0.082, blue: 0.059)), style: FillStyle(antialiased: false))
        }
        .frame(width: 204, height: 204)
        .padding(14)
        .background(.white, in: RoundedRectangle(cornerRadius: 18, style: .continuous))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("QR code for the Steam app")
        .accessibilityAddTraits(.isImage)
    }
}

extension SetupView {
    // MARK: From Steam
    @ViewBuilder var steamPanel: some View {
        switch model.steam.phase {
        case .off, .offer: steamOffer
        case .starting, .qr: steamCode
        case .notOwned: steamNotOwned
        case .downloading, .checking: steamDownloading
        case .stopped: steamStopped
        case .done: steamDone
        }
    }

    private var steamOffer: some View {
        panel {
            group {
                row("arrow.down.circle", "Call of Duty 2", "The whole game: multiplayer and the campaign · \(model.steamSizeWords)") { StatusLabel(.ready, "Selected") }
                divider()
                row("globe", "Language", "Voices and text. Steam has English, French, German, Italian and Spanish.", control: true) {
                    PopUpButton("Language", selection: $model.steam.language, options: SteamLanguage.allCases.map { (value: $0, title: $0.title) })
                }
                divider()
                row("folder", model.steamFolderDisplay, steamRoomWords, mono: true, negative: model.steamRoomShort, control: true) {
                    Button("Change…", action: model.pickSteamFolder).glassAction().controlSize(.large)
                }
            }
            if model.steamRoomShort {
                cornered {
                    Banner(.error, symbol: "exclamationmark.triangle",
                           message: "There isn’t enough room here. The game needs about \(String(format: "%.0f", Double(SteamGame.roomNeeded(model.steam.language)) / 1_000_000_000)) GB. Choose another place, or free some space.")
                }
            }
            group {
                steamFact("arrow.down.circle", "First, a small download tool", "DepotDownloader, free and open source (32 MB), from its own page on GitHub. It is checked before it runs.")
                divider()
                steamFact("checkmark.shield", "You sign in on your phone", "A QR code for the Steam app. Your Steam name and password never pass through CoD2 Silicon.")
                divider()
                steamFact("lock", "Signed out when it ends", "The sign-in and the tool are removed afterwards. Nothing about your Steam account stays on this Mac.")
            }
            note("Steam only lets an account download games it owns. The download tool is not made by Valve or by this project.")
                .padding(.leading, 18).padding(.bottom, 8)
        }
    }
    private var steamRoomWords: String {
        guard let free = model.steam.freeBytes else { return "On this Mac" }
        let gigabytes = Double(free) / 1_000_000_000
        return (gigabytes >= 10 ? String(Int(gigabytes)) : String(format: "%.1f", gigabytes)) + " GB free on this Mac"
    }
    private func steamFact(_ symbol: String, _ title: String, _ detail: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).font(.system(size: 14, weight: .medium)).foregroundStyle(palette.secondary).frame(width: 30).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 16).padding(.vertical, 8).frame(minHeight: 44)
        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
    }

    private var steamCode: some View {
        panel(padding: 8) {
            HStack(alignment: .center, spacing: 28) {
                if let code = model.steam.qr {
                    QRCodeView(matrix: code)
                } else {
                    RoundedRectangle(cornerRadius: 18, style: .continuous).fill(palette.well)
                        .overlay(RoundedRectangle(cornerRadius: 18, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                        .overlay { ProgressView().controlSize(.regular) }
                        .frame(width: 232, height: 232)
                        .accessibilityHidden(true)  // the line beside it says what is happening
                }
                VStack(alignment: .leading, spacing: 14) {
                    VStack(alignment: .leading, spacing: 8) {
                        steamStep(1, Text("Open the \(Text("Steam").fontWeight(.semibold)) app on your phone."))
                        steamStep(2, Text("Tap the shield at the bottom, then \(Text("Scan a QR Code").fontWeight(.semibold))."))
                        steamStep(3, Text("Point it at this code and approve the sign-in."))
                    }
                    HStack(spacing: 8) {
                        ProgressView().controlSize(.small)
                        Text(model.steam.qr != nil ? "Waiting for you to approve on your phone…"
                             : model.steam.reachingSteam ? "Connecting to Steam…" : "Getting the download tool…")
                            .font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
                    }
                    .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                    note("Your phone will call this sign-in “DepotDownloader”, the download tool’s name. The code renews itself every little while.")
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(8)
            note("No Steam name or password is typed on this Mac, and the sign-in is removed when the download ends.", symbol: "lock")
                .padding(.leading, 8).padding(.bottom, 8)
        }
    }
    private func steamStep(_ number: Int, _ text: Text) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text("\(number).").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.secondary).monospacedDigit().frame(width: 18, alignment: .trailing)
            text.font(.system(size: 14)).foregroundStyle(palette.text).fixedSize(horizontal: false, vertical: true)
        }
        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
    }

    private var steamNotOwned: some View {
        panel {
            cornered {
                Banner(.error, symbol: "exclamationmark.triangle", title: "Steam says this account doesn’t have Call of Duty 2",
                       message: "Sign in with the account that bought it. If your copy came on a disc or from another shop, use its folder instead.")
            }
            cornered {
                HStack(spacing: 8) {
                    Button("Use Another Account") { model.startSteam() }.glassAction().controlSize(.large)
                    Button("Choose Folder…") { model.chooseFolderInsteadOfSteam() }.glassAction().controlSize(.large)
                    Spacer(minLength: 8)
                    Link("Call of Duty 2 on Steam", destination: SteamGame.storePage).font(.system(size: 13)).foregroundStyle(palette.link)
                }
            }
        }
    }

    private var steamDownloading: some View {
        let checking = model.steam.phase == .checking
        let fraction = model.steam.totalBytes > 0 ? Double(model.steam.doneBytes) / Double(model.steam.totalBytes) : 0
        return panel(padding: 8) {
            VStack(alignment: .leading, spacing: 10) {
                HStack(alignment: .firstTextBaseline) {
                    Text("Call of Duty 2 · \(model.steam.language.title)").font(.system(size: 14, weight: .semibold)).foregroundStyle(palette.text)
                    Spacer(minLength: 8)
                    Text(checking ? "Checking the files…" : [model.steamAmountWords, model.steamTimeWords].compactMap { $0 }.joined(separator: " · "))
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).monospacedDigit()
                }
                .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
                ProgressBar("Downloading Call of Duty 2", value: checking ? nil : fraction)
                    .accessibilityValue(checking ? "Checking the files" : model.steamAmountWords)
                Text(checking || model.steam.currentFile.isEmpty ? " " : model.steam.currentFile)
                    .font(.typewriter(12)).foregroundStyle(palette.secondary).lineLimit(1).truncationMode(.middle)
                    .accessibilityHidden(true)
            }
            .padding(8)
            group {
                steamProgressRow("Download tool") { StatusLabel(.ready, "Checked") }
                divider(inset: 16)
                steamProgressRow("Steam sign-in") { StatusLabel(.ready, "Approved on your phone") }
                divider(inset: 16)
                steamProgressRow("Game files") {
                    if checking { StatusLabel(.ready, "Downloaded") } else { steamPending("Downloading", symbol: "arrow.down.circle", active: true) }
                }
                divider(inset: 16)
                steamProgressRow("Check every file") { steamPending(checking ? "Checking" : "Next", symbol: "clock", active: checking) }
            }
            note("You can keep this window in the background. If you stop, what is downloaded stays, and the next try picks up from there.")
                .padding(.leading, 8).padding(.bottom, 8)
        }
    }
    private func steamProgressRow<Trailing: View>(_ title: String, @ViewBuilder trailing: () -> Trailing) -> some View {
        HStack(spacing: 12) {
            Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
            Spacer(minLength: 8)
            trailing()
        }
        .padding(.horizontal, 16).frame(minHeight: 40)
        .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
    }
    /// A step that is under way or still to come: the status label's shape, without a verdict's colour.
    private func steamPending(_ text: String, symbol: String, active: Bool) -> some View {
        HStack(spacing: 6) {
            Image(systemName: symbol).font(.system(size: 13, weight: .semibold))
            Text(text).lineLimit(1)
        }
        .font(.system(size: 12, weight: .semibold)).foregroundStyle(active ? palette.accent : palette.secondary).fixedSize()
        .accessibilityElement(children: .ignore).accessibilityLabel(text)
    }

    private var steamStopped: some View {
        panel {
            cornered { Banner(.error, symbol: "exclamationmark.triangle", title: model.steam.stoppedTitle, message: model.steam.stoppedDetail) }
            cornered {
                HStack(spacing: 8) {
                    Button(model.steam.retryTitle) { model.startSteam() }.glassAction().controlSize(.large)
                    Button("Choose Folder Instead…") { model.chooseFolderInsteadOfSteam() }.glassAction().controlSize(.large)
                    Spacer(minLength: 8)
                    if !model.steam.log.isEmpty {
                        Button(model.steam.detailsShown ? "Hide Details" : "Show Details") { model.steam.detailsShown.toggle() }.quietAction()
                    }
                }
            }
            if model.steam.detailsShown, !model.steam.log.isEmpty {
                ScrollView {
                    Text(model.steam.log.suffix(60).joined(separator: "\n"))
                        .font(.typewriter(11)).foregroundStyle(palette.secondary).textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading).padding(12)
                }
                .frame(height: 132)
                .background(palette.well, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(palette.wellStroke, lineWidth: 1))
                .accessibilityLabel("What the download tool said")
            }
            if model.steam.retryTitle == "Resume" {
                note("Resume shows a new code to scan, because your Steam sign-in is not kept between tries.").padding(.leading, 8).padding(.bottom, 8)
            }
        }
    }

    private var steamDone: some View {
        panel {
            group {
                row("folder", model.dataPathDisplay,
                    "\(model.dataFoundSummary) · \(SteamGame.gigabytes(model.steam.downloadedBytes)) GB from Steam", mono: true) {
                    StatusLabel(.ready, "Downloaded and checked")
                }
                divider()
                HStack(spacing: 12) {
                    Image(systemName: "lock").font(.system(size: 14, weight: .medium)).foregroundStyle(palette.secondary).frame(width: 30).accessibilityHidden(true)
                    Text("Signed out of Steam. The sign-in and the download tool were removed from this Mac.")
                        .font(.system(size: 13)).foregroundStyle(palette.secondary).fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                }
                .padding(.horizontal, 16).padding(.vertical, 8).frame(minHeight: 44)
                .accessibilityElement(children: .combine).accessibilityAddTraits(.isStaticText)
            }
            note("Your CD key is in Steam: right-click the game, then Manage › CD keys. The next step asks for it.").padding(.leading, 18).padding(.bottom, 8)
        }
    }

    // MARK: Crash reports
    var crashPanel: some View {
        panel {
            group {
                ForEach(Array(CrashReportsQuestion.rows.enumerated()), id: \.offset) { index, item in
                    if index > 0 { divider() }
                    row(item.symbol, item.title, item.detail) { EmptyView() }
                }
            }
            HStack(spacing: 8) {
                note(CrashReportsQuestion.changeLater)
                Spacer(minLength: 8)
                CrashReportDetailsButton(model: model)
            }
            .padding(.leading, 18).padding(.trailing, 2).padding(.bottom, 2)
        }
    }
}

/// See What’s Sent…: the fields of a report with this Mac's values, as Settings › Privacy shows them.
struct CrashReportDetailsButton: View {
    @ObservedObject var model: LauncherModel
    @State private var shown = false
    var body: some View {
        Button("See What’s Sent…") { shown = true }.glassAction().controlSize(.large)
            .popover(isPresented: $shown, arrowEdge: .bottom) { CrashReportContents(model: model) }
    }
}
