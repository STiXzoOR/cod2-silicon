import SwiftUI
import AppKit

struct LauncherRoot: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.colorScheme) private var scheme
    var body: some View {
        HStack(spacing: 0) {
            sidebar
            VStack(alignment: .leading, spacing: 0) {
                HStack {
                    Text(model.onboard ? "WELCOME TO COD2 SILICON" : model.page.rawValue.uppercased()).font(.caption.weight(.semibold)).tracking(1.5).foregroundStyle(.secondary)
                    Spacer()
                    Label("Apple silicon", systemImage: "cpu").font(.caption).foregroundStyle(.secondary)
                }.padding(.horizontal, 36).padding(.top, 28).padding(.bottom, 20)
                ScrollView {
                    VStack(alignment: .leading, spacing: 24) {
                        if model.onboard { OnboardingView(model: model) }
                        else {
                            switch model.page {
                            case .home: HomeView(model: model)
                            case .servers: ServersView(model: model)
                            case .settings: SettingsView(model: model)
                            case .media: MediaView(model: model)
                            case .about: AboutView(model: model)
                            }
                        }
                        if !model.notice.isEmpty {
                            HStack {
                                Label(model.notice, systemImage: "info.circle").font(.callout).textSelection(.enabled)
                                Spacer()
                                if let report = model.crashReport { Button("Show Crash Report") { NSWorkspace.shared.activateFileViewerSelecting([report]) } }
                                Button { model.notice = "" } label: { Image(systemName: "xmark") }.buttonStyle(.plain).accessibilityLabel("Dismiss message")
                            }.padding(16).launcherGlass().accessibilityElement(children: .contain)
                        }
                    }.padding(.horizontal, 36).padding(.bottom, 36).frame(maxWidth: 1250, alignment: .leading)
                }
            }.frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(scheme == .dark ? Color(red: 0.09, green: 0.11, blue: 0.13) : Color(red: 0.95, green: 0.96, blue: 0.96))
        }.frame(minWidth: 1100, minHeight: 760)
    }
    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 26) {
            HStack(spacing: 12) {
                Image(systemName: "mountain.2.fill").font(.system(size: 30)).foregroundStyle(Color.accentColor)
                VStack(alignment: .leading, spacing: 3) { Text("CoD2").font(.title2.weight(.bold)); Text("SILICON").font(.caption.weight(.medium)).tracking(3).foregroundStyle(.secondary) }
            }.padding(.top, 36).padding(.bottom, 20).accessibilityElement(children: .combine)
            VStack(spacing: 8) {
                ForEach(LauncherPage.allCases) { page in
                    Button {
                        model.page = page
                        if page == .media { model.loadMedia() }
                    } label: {
                        HStack(spacing: 14) {
                            Image(systemName: page.symbol).frame(width: 22)
                            Text(page.rawValue).font(.body.weight(model.page == page ? .semibold : .regular))
                            Spacer()
                        }.padding(.horizontal, 16).padding(.vertical, 13)
                            .foregroundStyle(model.page == page ? Color.accentColor : Color.primary)
                            .background(model.page == page ? Color.accentColor.opacity(0.12) : .clear, in: RoundedRectangle(cornerRadius: 13))
                    }.buttonStyle(.plain).disabled(model.onboard).accessibilityAddTraits(model.page == page ? .isSelected : [])
                }
            }
            Spacer()
            VStack(alignment: .leading, spacing: 10) {
                Label("Native. Familiar. Yours.", systemImage: "sparkle").font(.caption.weight(.medium))
                Text("The classic game,\nat home on your Mac.").font(.callout).foregroundStyle(.secondary).lineSpacing(3)
                Text("VERSION \(model.version)").font(.system(size: 10, weight: .medium, design: .monospaced)).foregroundStyle(.tertiary).padding(.top, 12)
            }.padding(.bottom, 30)
        }.padding(.horizontal, 22).frame(width: 208).background(.regularMaterial)
    }
}

struct HomeView: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        PageHeading(title: "Good to be back.", subtitle: "Your next round starts here.")
        ZStack(alignment: .leading) {
            TerrainArtwork()
            LinearGradient(colors: [.black.opacity(0.7), .clear], startPoint: .leading, endPoint: .trailing)
            VStack(alignment: .leading, spacing: 18) {
                Text("THE CLASSIC. A NEW HORIZON.").font(.caption.weight(.semibold)).tracking(2.5).foregroundStyle(.white.opacity(0.75))
                Text("One more round.").font(.system(size: 48, weight: .bold, design: .rounded)).foregroundStyle(.white)
                Text("Call of Duty 2 multiplayer\nNative on Apple silicon.").font(.title3).foregroundStyle(.white.opacity(0.85)).lineSpacing(5)
                Button { model.play() } label: {
                    Label("Play", systemImage: "play.fill").font(.title3.weight(.semibold)).padding(.horizontal, 34).padding(.vertical, 14)
                }.buttonStyle(.plain).foregroundStyle(.white).background(Color.accentColor, in: Capsule()).keyboardShortcut(.return, modifiers: .command).accessibilityLabel("Play Call of Duty 2")
                Text("⌘ Return to play").font(.caption).foregroundStyle(.white.opacity(0.65))
            }.padding(40)
        }.frame(height: 340).clipShape(RoundedRectangle(cornerRadius: 24))
        HStack(spacing: 28) {
            Metric(value: "\(model.settings.fps) fps", label: "Your frame cap", symbol: "speedometer")
            Spacer(); Metric(value: model.settings.resolution, label: "Render resolution", symbol: "display")
            Spacer(); Metric(value: model.approximate ? "Approximate" : "Original shaders", label: "Rendering setup", symbol: "checkmark.shield")
        }.padding(.horizontal, 8).padding(.vertical, 4)
        HStack(alignment: .top, spacing: 22) {
            Panel(title: "Pick up where you left off") {
                if let address = model.library.recent.first {
                    let server = model.servers.first { $0.address == address }
                    QuakeName(name: server?.name ?? address).font(.headline)
                    Text("\(server?.map ?? "Last server") · \(address)").font(.callout).foregroundStyle(.secondary)
                    Button("Join last server") { model.directAddress = address; model.connect() }.buttonStyle(.bordered)
                } else { Text("Join a server to start your recent history.").foregroundStyle(.secondary); Button("Browse servers") { model.page = .servers } }
            }
            Panel(title: "From the project") {
                Text("Built for your Mac.").font(.headline)
                Text("Follow native renderer work, release notes and community updates.").font(.callout).foregroundStyle(.secondary)
                Text(model.updateMessage).font(.caption).foregroundStyle(.secondary)
                HStack { Button("Check updates", action: model.checkUpdates); Link("News & release notes ↗", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon/releases")!) }
            }
        }
        if model.approximate { Label("Approximate shaders are active. Some lighting and skies differ. Add your licensed Mac copy in setup for original rendering.", systemImage: "exclamationmark.triangle").font(.callout).foregroundStyle(.secondary) }
    }
}

struct OnboardingView: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        PageHeading(title: "Make yourself at home.", subtitle: "A few things from your licensed copy. Everything stays on your Mac.")
        HStack(spacing: 16) {
            ForEach(Array(["Game data", "CD key", "Shaders"].enumerated()), id: \.offset) { index, name in
                HStack(spacing: 10) {
                    Image(systemName: index < model.step ? "checkmark.circle.fill" : "\(index + 1).circle.fill")
                    Text(name).fontWeight(model.step == index ? .semibold : .regular)
                }.foregroundStyle(index <= model.step ? Color.accentColor : .secondary)
                if index != 2 { Rectangle().fill(.secondary.opacity(0.2)).frame(width: 55, height: 1) }
            }
        }.padding(.vertical, 12)
        HStack(alignment: .top, spacing: 26) {
            Panel(title: ["Bring your game", "Your multiplayer key", "Prepare the original look"][model.step]) {
                Image(systemName: ["folder.badge.plus", "key.horizontal", "sparkles"][model.step]).font(.system(size: 48, weight: .light)).foregroundStyle(Color.accentColor).padding(.vertical, 10).accessibilityHidden(true)
                if model.step == 0 {
                    Text("Choose your Call of Duty 2 folder.").font(.title2.weight(.semibold))
                    Text("We check for all 16 main game archives. Windows or Mac 1.3 data works. The download includes the engine; the game content comes from your own copy.").foregroundStyle(.secondary).lineSpacing(4)
                    TextField("Game folder", text: $model.dataPath).textFieldStyle(.roundedBorder).accessibilityLabel("Licensed game data folder")
                    Button("Choose folder…", action: model.pickData).controlSize(.large)
                    if !model.dataPath.isEmpty { Label("Folder found. Continue to validate it.", systemImage: "folder.badge.checkmark").font(.callout).foregroundStyle(.secondary) }
                } else if model.step == 1 {
                    Text("Unlock multiplayer.").font(.title2.weight(.semibold))
                    Text("Enter the 20 characters supplied with your licensed game. We validate the checksum and save the key in a private file readable only by your user account.").foregroundStyle(.secondary).lineSpacing(4)
                    SecureField("20-character CD key", text: $model.keyInput).textFieldStyle(.roundedBorder).onSubmit(model.nextSetup).accessibilityLabel("Call of Duty 2 CD key, twenty characters")
                    Label("Your key is never sent to the launcher or update service.", systemImage: "lock.shield").font(.callout).foregroundStyle(.secondary)
                } else {
                    Text(model.shaderBusy ? "Preparing your shaders…" : "Ready for the next round.").font(.title2.weight(.semibold))
                    Text("For matching lighting, the native extractor reads the shaders from your own Mac multiplayer executable. Nothing is downloaded or bundled.").foregroundStyle(.secondary).lineSpacing(4)
                    ProgressView(value: model.shaderProgress).accessibilityLabel("Shader setup progress")
                    Text(model.shaderMessage).font(.callout).foregroundStyle(model.approximate ? .orange : .secondary)
                    Button("Choose licensed Mac copy…", action: model.pickShaders).disabled(model.shaderBusy)
                }
                Divider().padding(.top, 12)
                HStack {
                    if model.step > 0 { Button("Back") { model.step -= 1 }.disabled(model.shaderBusy) }
                    Spacer()
                    Button(model.step == 2 ? (model.approximate ? "Continue with approximate shaders" : "Let's play") : "Continue", action: model.nextSetup)
                        .buttonStyle(.borderedProminent).controlSize(.large).disabled(model.shaderBusy).keyboardShortcut(.defaultAction)
                }
            }.frame(maxWidth: 660)
            VStack(alignment: .leading, spacing: 20) {
                TerrainArtwork().frame(height: 230).clipShape(RoundedRectangle(cornerRadius: 20))
                Eyebrow(text: "A familiar game. A fresh start.")
                Text("Your files, your Mac.").font(.title2.weight(.semibold))
                Text("Your game folder stays as it is. Configs, demos and screenshots get their own home in Application Support.").foregroundStyle(.secondary).lineSpacing(4)
                Link("Where to find game data ↗", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon#game-data")!)
            }.frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

struct ServersView: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        HStack(alignment: .top) {
            PageHeading(title: "Find your next round.", subtitle: "Stock 1.3 and CoD2x 1.4, together in one browser.")
            Spacer()
            Button(model.refreshing ? "Stop refresh" : "Refresh", systemImage: "arrow.clockwise") { model.refreshing ? model.cancelRefresh() : model.refresh() }.controlSize(.large)
        }
        HStack(spacing: 14) {
            TextField("Search servers, maps, mods or address", text: $model.search).textFieldStyle(.roundedBorder)
            Picker("Show", selection: $model.filter) { ForEach(["All servers", "Favorites", "Recent", "Stock 1.3", "CoD2x 1.4"], id: \.self) { Text($0) } }.frame(width: 220)
            Picker("Sort", selection: $model.sort) { ForEach(["Ping", "Players", "Name"], id: \.self) { Text($0) } }.frame(width: 150)
        }
        HStack {
            Toggle("Hide empty", isOn: $model.hideEmpty); Toggle("Hide full", isOn: $model.hideFull)
            Spacer()
            Text(model.refreshing ? model.queryProgress : "\(model.visibleServers.count) servers · \(model.library.cachedAt == nil ? "refresh to discover" : "cached until refreshed")").font(.caption).foregroundStyle(.secondary)
        }.toggleStyle(.checkbox)
        HStack(alignment: .top, spacing: 22) {
            Table(model.visibleServers, selection: $model.selectedServer) {
                TableColumn("Server") { server in
                    HStack(spacing: 8) {
                        Button { model.favorite(server.address) } label: { Image(systemName: model.library.favorites.contains(server.address) ? "star.fill" : "star").foregroundStyle(model.library.favorites.contains(server.address) ? .orange : .secondary) }.buttonStyle(.plain).accessibilityLabel("Toggle favorite for \(QuakeColors.plain(server.name))")
                        QuakeName(name: server.name).lineLimit(1)
                        if server.password { Image(systemName: "lock.fill").accessibilityLabel("Password required") }
                    }
                }.width(min: 200, ideal: 250)
                TableColumn("Map") { Text($0.map.replacingOccurrences(of: "mp_", with: "")).font(.callout) }.width(min: 80, ideal: 100)
                TableColumn("Mode") { Text($0.gametype.uppercased()).font(.caption) }.width(45)
                TableColumn("Players") { Text("\($0.playerCount)/\($0.maxPlayers)").monospacedDigit() }.width(60)
                TableColumn("Ping") { Text("\($0.ping) ms").monospacedDigit().foregroundStyle(.secondary) }.width(65)
            }.frame(height: 400).clipShape(RoundedRectangle(cornerRadius: 14))
            Panel(title: "Server details") {
                if let server = model.selected {
                    QuakeName(name: server.name).font(.headline)
                    Text(server.address).font(.caption.monospaced()).textSelection(.enabled).foregroundStyle(.secondary)
                    LabeledContent("Version", value: server.version)
                    LabeledContent("Mod", value: server.mod.isEmpty ? "Classic" : server.mod)
                    LabeledContent("FPS cap", value: server.fps)
                    LabeledContent("Password", value: server.password ? "Required" : "Open")
                    if server.password { SecureField("Server password", text: $model.serverPassword).textFieldStyle(.roundedBorder) }
                    Button("Join server") { model.connect(server) }.buttonStyle(.borderedProminent).controlSize(.large)
                    Divider()
                    Text("PLAYERS").font(.caption.weight(.semibold)).foregroundStyle(.secondary)
                    ScrollView { VStack(spacing: 9) { ForEach(server.players) { player in HStack { QuakeName(name: player.name); Spacer(); Text("\(player.score)").monospacedDigit(); Text("\(player.ping)").font(.caption).foregroundStyle(.secondary) } } } }.frame(height: 125)
                } else { Text("Select a server to see its players and settings.").foregroundStyle(.secondary) }
            }.frame(width: 270)
        }
        Panel(title: "Direct connect") {
            HStack {
                TextField("hostname or IP:port", text: $model.directAddress).textFieldStyle(.roundedBorder).onSubmit { model.connect() }
                SecureField("Password (optional)", text: $model.serverPassword).textFieldStyle(.roundedBorder).frame(width: 220)
                Button("Connect") { model.connect() }.buttonStyle(.bordered)
            }
            Text("Queries are throttled. Favorites and recent addresses stay on this Mac; passwords are never saved.").font(.caption).foregroundStyle(.secondary)
        }
    }
}

struct SettingsView: View {
    @ObservedObject var model: LauncherModel
    @State var advanced = false
    var body: some View {
        HStack(alignment: .top) {
            PageHeading(title: "Make it feel right.", subtitle: "Your settings apply on the next launch. Servers may enforce their own frame cap.")
            Spacer(); Button("Save settings", action: model.saveSettings).buttonStyle(.borderedProminent).controlSize(.large).keyboardShortcut("s", modifiers: .command)
        }
        HStack(alignment: .top, spacing: 22) {
            VStack(spacing: 22) {
                Panel(title: "Display") {
                    Picker("Resolution", selection: $model.settings.resolution) { ForEach(GameSettings.resolutions, id: \.self) { Text($0) } }
                    Picker("Fullscreen", selection: $model.settings.fullscreen) {
                        Text("Exclusive").tag("exclusive"); Text("Borderless").tag("borderless"); Text("Native Spaces • Game Mode eligible").tag("spaces"); Text("Windowed").tag("windowed")
                    }
                    HStack { Text("Frame cap"); Spacer(); Picker("Frame cap preset", selection: $model.settings.fps) { Text("333").tag(333); Text("250").tag(250); Text("125").tag(125); Text("Custom").tag(model.settings.fps) }.labelsHidden().frame(width: 130); TextField("FPS", value: $model.settings.fps, format: .number).textFieldStyle(.roundedBorder).frame(width: 75) }
                    Toggle("Vsync", isOn: $model.settings.vsync)
                    Text("333 targets competitive responsiveness; 250 and 125 match common server limits. Native Spaces keeps your desktop display mode.").font(.caption).foregroundStyle(.secondary)
                }
                Panel(title: "Graphics · coming next") {
                    HStack { Label("Renderer", systemImage: "cube.transparent"); Spacer(); Text("Classic OpenGL").foregroundStyle(.secondary) }
                    HStack { Text("Render scale"); Spacer(); Text("100% · upcoming").foregroundStyle(.secondary) }
                    Toggle("MetalFX upscaling · upcoming", isOn: .constant(false)).disabled(true)
                    Toggle("HDR / extended range · upcoming", isOn: .constant(false)).disabled(true)
                    Text("Metal, render scale, MetalFX and HDR controls will become available with the native renderer. Classic rendering remains the reference.").font(.caption).foregroundStyle(.secondary)
                }
            }
            VStack(spacing: 22) {
                Panel(title: "Mouse & sound") {
                    Toggle("Raw mouse input", isOn: $model.settings.rawMouse)
                    HStack { Text("Sensitivity"); Slider(value: $model.settings.sensitivity, in: 0.1...15, step: 0.1); Text(model.settings.sensitivity, format: .number.precision(.fractionLength(1))).monospacedDigit().frame(width: 40) }
                    HStack { Text("Mouse DPI"); Spacer(); TextField("DPI", value: $model.settings.dpi, format: .number).textFieldStyle(.roundedBorder).frame(width: 100) }
                    LabeledContent("Distance per turn", value: String(format: "%.1f cm / 360°", model.settings.cm360))
                    Text("Estimate uses m_yaw 0.022. Advanced yaw overrides and game/server modifiers change this distance.").font(.caption).foregroundStyle(.secondary)
                    Divider()
                    HStack { Image(systemName: "speaker.wave.2"); Slider(value: $model.settings.volume, in: 0...1).accessibilityLabel("Game volume"); Text("\(Int(model.settings.volume * 100))%").monospacedDigit().frame(width: 45) }
                    Text("Audio uses your Mac's current output device.").font(.caption).foregroundStyle(.secondary)
                }
                Panel(title: "Presets") {
                    HStack {
                        Button("Classic 333") { model.settings = GameSettings() }
                        Button("Competitive 250") { model.settings.fps = 250; model.settings.vsync = false; model.settings.rawMouse = true }
                    }
                    Text("Presets start from the existing release defaults. You can customize them before saving.").font(.caption).foregroundStyle(.secondary)
                    Button("Game data & shader setup…") { model.onboard = true; model.step = 0; model.keyInput = "" }
                }
            }
        }
        Panel(title: "Advanced dvars") {
            Text("One dvar=value per line. Values are validated before saving. Paths and credentials are managed by the launcher.").font(.callout).foregroundStyle(.secondary)
            TextEditor(text: $model.settings.advanced).font(.system(.body, design: .monospaced)).frame(height: 85).padding(8).background(.quaternary, in: RoundedRectangle(cornerRadius: 8)).accessibilityLabel("Advanced engine dvars")
        }
    }
}

struct MediaView: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        HStack(alignment: .top) {
            PageHeading(title: "Keep the good rounds.", subtitle: "Your demos and screenshots, in their own home.")
            Spacer(); Button("Open folder", systemImage: "folder", action: model.openMediaFolder).controlSize(.large)
        }
        Picker("Library", selection: $model.mediaTab) { Text("Demos").tag("Demos"); Text("Screenshots").tag("Screenshots") }.pickerStyle(.segmented).frame(width: 320).onChange(of: model.mediaTab) { _ in model.loadMedia() }
        if model.media.isEmpty {
            Panel(title: "Your collection starts here") {
                Label("Record a demo or take a screenshot in the game. It will appear here after you return.", systemImage: "photo.on.rectangle.angled").foregroundStyle(.secondary)
            }
        } else {
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 250))], spacing: 22) {
                ForEach(model.media, id: \.path) { url in
                    Panel(title: "") {
                        ZStack {
                            if model.mediaTab == "Screenshots", let image = NSImage(contentsOf: url) { Image(nsImage: image).resizable().scaledToFit() }
                            else { TerrainArtwork(); Image(systemName: model.mediaTab == "Demos" ? "play.circle.fill" : "photo").font(.system(size: 50, weight: .thin)).foregroundStyle(.white.opacity(0.85)) }
                        }.frame(height: 150).clipShape(RoundedRectangle(cornerRadius: 12))
                        Text(url.lastPathComponent).font(.headline).lineLimit(1)
                        Text(model.mediaTab == "Demos" ? "Recorded multiplayer demo" : "Saved game capture").font(.caption).foregroundStyle(.secondary)
                        Button(model.mediaTab == "Demos" ? "Play demo" : "Open screenshot") { if model.mediaTab == "Demos" { model.play(demo: url) } else { NSWorkspace.shared.open(url) } }.buttonStyle(.bordered)
                    }
                }
            }
        }
        Text("Files live in Application Support/CoD2 Silicon/main. Demo playback uses the game's original playback system.").font(.callout).foregroundStyle(.secondary)
    }
}

struct AboutView: View {
    @ObservedObject var model: LauncherModel
    var body: some View {
        PageHeading(title: "A classic, carried forward.", subtitle: "An independent native port for Apple silicon.")
        HStack(alignment: .top, spacing: 26) {
            VStack(alignment: .leading, spacing: 24) {
                TerrainArtwork().frame(height: 230).clipShape(RoundedRectangle(cornerRadius: 20))
                Text("CoD2 Silicon").font(.system(size: 38, weight: .bold, design: .rounded))
                Text("Version \(model.version) · Native arm64\nCoD2x client compatibility 1.4.6.8").font(.body).foregroundStyle(.secondary).lineSpacing(6)
                Text("Built on opencod2, with SDL and Apple frameworks. Thank you to the engine reconstruction and CoD2x communities.").foregroundStyle(.secondary).lineSpacing(4)
                Link("Credits & provenance ↗", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/CREDITS.md")!)
            }.frame(maxWidth: .infinity, alignment: .leading)
            VStack(spacing: 22) {
                Panel(title: "Updates, on your terms") {
                    Text(model.updateMessage).foregroundStyle(.secondary)
                    Button("Check for updates", action: model.checkUpdates).buttonStyle(.bordered)
                    if let url = model.releaseURL { Link("View release ↗", destination: url) }
                    Text("Notification only. Updates are never installed automatically.").font(.caption).foregroundStyle(.secondary)
                }
                Panel(title: "License & notice") {
                    Text("MIT covers this project's own changes. Upstream opencod2 retains its authors' rights; third-party licenses apply.").font(.callout).foregroundStyle(.secondary)
                    Text("Call of Duty is a trademark of Activision. This independent project is not affiliated with Activision, Infinity Ward, Aspyr or CoD2x. Bring your own licensed game content.").font(.callout).foregroundStyle(.secondary)
                    Link("Read license notices ↗", destination: URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/NOTICE.md")!)
                }
            }.frame(maxWidth: .infinity)
        }
    }
}
