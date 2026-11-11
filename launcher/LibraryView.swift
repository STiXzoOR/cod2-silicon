import SwiftUI
import AppKit
import QuickLook

struct LibraryView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    @State private var quickLookURL: URL?
    @FocusState private var libraryFocused: Bool
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }

    var body: some View {
        GeometryReader { geometry in
            let columns = columnCount(width: geometry.size.width)
            ScrollViewReader { reader in
                ScrollView {
                    VStack(alignment: .leading, spacing: 22) {
                        PageHeader(title: "Library", subtitle: model.libraryTab == .demos ? "Your recorded matches, newest first. Double-click a recording to play it."
                            : "Your screenshots, newest first. Double-click to open; press Space for Quick Look.")
                            .padding(.leading, 16)
                        if let message = model.mediaMessage, !model.media.isEmpty || !model.mediaError {
                            HStack {
                                Label(message, systemImage: model.mediaError ? "exclamationmark.triangle" : "checkmark.circle").font(.system(size: 13)).foregroundStyle(model.mediaError ? palette.negative : palette.secondary)
                                Spacer()
                                Button("Refresh", action: model.loadMedia).glassAction()
                            }
                        }
                        if model.media.isEmpty {
                            EmptyLibrary(tab: model.libraryTab, error: model.mediaError ? model.mediaMessage : nil, showFolder: model.openMediaFolder)
                        } else if model.libraryTab == .demos {
                            LazyVStack(spacing: 4) {
                                ForEach(model.media) { entry in
                                    mediaItem(entry) {
                                        DemoRow(entry: entry, image: entry.facts.map.flatMap { artwork.image(for: "mp_" + $0) }) { model.play(demo: $0) }
                                    }
                                }
                            }
                        } else {
                            LazyVGrid(columns: [GridItem(.adaptive(minimum: 260), spacing: 18)], spacing: 18) {
                                ForEach(model.media) { entry in
                                    mediaItem(entry) { ScreenshotTile(entry: entry, snapshot: model.snapshot) }
                                }
                            }
                        }
                    }
                    .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 24).padding(.top, 84).padding(.bottom, 32)
                }
                .focusable(interactions: .edit).focused($libraryFocused).focusEffectDisabled()
                .onKeyPress(keys: [.upArrow, .downArrow, .leftArrow, .rightArrow]) { press in
                    guard !model.media.isEmpty else { return .ignored }
                    let current = model.media.firstIndex { $0.url == model.selectedMedia }
                    let step = model.libraryTab == .demos ? 1 : columns
                    let delta = press.key == .upArrow ? -step : press.key == .downArrow ? step : press.key == .leftArrow ? -1 : 1
                    let index = current.map { min(model.media.count - 1, max(0, $0 + delta)) } ?? 0
                    model.selectedMedia = model.media[index].url
                    reader.scrollTo(model.media[index].id)
                    return .handled
                }
                .onKeyPress(.return) {
                    guard let entry = selected else { return .ignored }
                    open(entry); return .handled
                }
                .onKeyPress(.space) {
                    guard model.libraryTab == .screenshots, let entry = selected else { return .ignored }
                    preview(entry); return .handled
                }
                .accessibilityLabel(model.libraryTab == .demos ? "Recordings" : "Screenshots")
            }
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onAppear { model.loadMedia() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in model.loadMedia() }
        .onChange(of: model.libraryTab) { _, _ in model.selectedMedia = nil; model.loadMedia() }
        .quickLookPreview($quickLookURL, in: model.media.map(\.url))
        .sheet(item: $model.renamingMedia) { MediaRenameSheet(model: model, entry: $0) }
        .confirmationDialog("Move “\(model.trashingMedia?.url.lastPathComponent ?? "")” to Trash?", isPresented: Binding(get: { model.trashingMedia != nil }, set: { if !$0 { model.trashingMedia = nil } }), titleVisibility: .visible) {
            Button("Move to Trash", role: .destructive) { if let entry = model.trashingMedia { model.trashMedia(entry) }; model.trashingMedia = nil }
            Button("Cancel", role: .cancel) { model.trashingMedia = nil }
        } message: { Text("You can restore this file from the Trash in Finder.") }
        .toolbar {
            ToolbarItem(placement: .principal) {
                Picker("Library section", selection: $model.libraryTab) { ForEach(LibraryTab.allCases) { Text($0.rawValue).tag($0) } }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
            }
        }
        .trailingToolbar {
            Button(action: model.openMediaFolder) { Label("Show in Finder", systemImage: "folder").labelStyle(.titleAndIcon) }
                .help("Open the folder for this library section")
        }
    }
    private var selected: MediaEntry? { model.media.first { $0.url == model.selectedMedia } }
    private func columnCount(width: CGFloat) -> Int {
        let inset: CGFloat = max(24, 264 - origin.x)
        let available = width - inset - 24 + 18
        return max(1, Int(available / 278))
    }
    private func open(_ entry: MediaEntry) { if model.libraryTab == .demos { model.play(demo: entry.url) } else { model.openMedia(entry) } }
    private func preview(_ entry: MediaEntry) { if model.mediaFileExists(entry.url) { quickLookURL = entry.url } }
    private func mediaItem<Content: View>(_ entry: MediaEntry, @ViewBuilder content: () -> Content) -> some View {
        let selected = entry.url == model.selectedMedia
        return content()
            .background(selected ? palette.selection : .clear, in: RoundedRectangle(cornerRadius: 16))
            .overlay(RoundedRectangle(cornerRadius: 16).stroke(selected ? (libraryFocused ? palette.accent : palette.selectionStroke) : .clear, lineWidth: libraryFocused ? 3 : 1))
            .contentShape(RoundedRectangle(cornerRadius: 16))
            .onTapGesture(count: 2) { open(entry) }
            .onTapGesture { model.selectedMedia = entry.url; libraryFocused = true }
            .contextMenu {
                Button(model.libraryTab == .demos ? "Play" : "Open") { open(entry) }
                if model.libraryTab == .screenshots {
                    Button("Quick Look") { preview(entry) }
                    Button("Copy") { model.copyMedia(entry) }
                }
                Button("Show in Finder") { model.revealMedia(entry) }
                if model.libraryTab == .demos { Button("Rename…") { model.renamingMedia = entry } }
                Divider()
                Button("Move to Trash…", role: .destructive) { model.trashingMedia = entry }
            }
            .id(entry.id)
            .accessibilityAddTraits(selected ? .isSelected : [])
            .accessibilityAction { model.selectedMedia = entry.url; libraryFocused = true }
            .accessibilityAction(named: model.libraryTab == .demos ? "Play" : "Open") { open(entry) }
    }
}

private struct MediaRenameSheet: View {
    @ObservedObject var model: LauncherModel
    var entry: MediaEntry
    @Environment(\.dismiss) private var dismiss
    @Environment(\.palette) private var palette
    @State private var name: String
    @State private var error: String?
    @FocusState private var nameFocused: Bool
    init(model: LauncherModel, entry: MediaEntry) {
        self.model = model; self.entry = entry
        _name = State(initialValue: entry.url.deletingPathExtension().lastPathComponent)
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            Label("Rename recording", systemImage: "pencil").font(.system(size: 22, weight: .bold))
            Text("Choose a name you’ll recognize in your library.").font(.system(size: 13)).foregroundStyle(palette.secondary)
            SettingsGroup {
                SettingsRow("Name") { TextField("Recording name", text: $name).focused($nameFocused).accessibilityLabel("Recording name") }
            }
            if let error { Label(error, systemImage: "exclamationmark.triangle").font(.system(size: 13)).foregroundStyle(palette.negative) }
            HStack {
                Spacer()
                Button("Cancel") { dismiss() }.glassAction().keyboardShortcut(.cancelAction)
                Button("Rename") {
                    error = model.renameMedia(entry, to: name)
                    if error == nil { dismiss() }
                }.prominentAction().keyboardShortcut(.defaultAction).disabled(name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }.padding(24).frame(width: 480).onAppear { nameFocused = true }
    }
}

struct DemoRow: View {
    var entry: MediaEntry
    var image: NSImage?
    var play: (URL) -> Void
    @Environment(\.palette) private var palette
    @State private var hovering = false
    var body: some View {
        let map = entry.facts.map.map { "mp_" + $0 } ?? entry.url.lastPathComponent
        HStack(spacing: 16) {
            MapArt(map: map, image: image).frame(width: 96, height: 60)
                .clipShape(RoundedRectangle(cornerRadius: RowMetrics.artRadius, style: .continuous)).shapeAudit(.inset, .rounded(RowMetrics.artRadius), "row art")
            VStack(alignment: .leading, spacing: 5) {
                Text(entry.url.lastPathComponent).font(.typewriter(14, bold: true)).foregroundStyle(palette.dark ? Palette.hex(0xefe7d3) : palette.text).lineLimit(1).truncationMode(.middle)
                Text([entry.facts.map.map(MapCatalog.displayName), "Multiplayer demo"].compactMap { $0 }.joined(separator: " · "))
                    .font(.system(size: 12)).foregroundStyle(palette.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Text(entry.facts.stamp).font(.typewriter(13)).foregroundStyle(palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary).frame(width: 120, alignment: .trailing)
            Text(entry.facts.size).font(.system(size: 13)).foregroundStyle(palette.tertiary).frame(width: 70, alignment: .trailing)
            Button { play(entry.url) } label: { Label("Play", systemImage: "play.fill").font(.system(size: 13, weight: .semibold)) }
                .glassAction()
                .accessibilityLabel("Play demo \(entry.url.lastPathComponent)")
        }
        .padding(.vertical, RowMetrics.inset).padding(.leading, RowMetrics.inset).padding(.trailing, RowMetrics.buttonTrailing(.regular))
        .background((palette.dark ? Color.white : Color.black).opacity(hovering ? 0.045 : 0), in: RoundedRectangle(cornerRadius: RowMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(RowMetrics.radius), "demo row")
        .onHover { hovering = $0 }
        .accessibilityElement(children: .contain)
    }
}

struct ScreenshotTile: View {
    var entry: MediaEntry
    var snapshot: Bool
    @Environment(\.palette) private var palette
    @State private var thumbnail: NSImage?
    var body: some View {
        VStack(spacing: 0) {
            Group {
                if let thumbnail { Color.clear.overlay(Image(nsImage: thumbnail).resizable().aspectRatio(contentMode: .fill)).clipped() }
                else { SceneThumbnail(scenery: MapCatalog.scenery(entry.facts.map ?? entry.url.lastPathComponent), dark: palette.dark) }
            }
            .frame(height: 210)
            HStack {
                Text(entry.facts.map.map(MapCatalog.displayName) ?? entry.url.deletingPathExtension().lastPathComponent)
                    .font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text).lineLimit(1)
                Spacer()
                Text(entry.facts.stamp).font(.typewriter(12)).foregroundStyle(palette.secondary)
            }
            .padding(.vertical, 12).padding(.horizontal, 14)
        }
        .background(palette.panel)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(entry.url.lastPathComponent), \(entry.facts.stamp)")
        .accessibilityAddTraits(.isButton)
        .task(id: entry.url) {
            guard !snapshot else { return }
            let url = entry.url
            let image = await Task.detached(priority: .utility) { () -> CGImage? in
                guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
                return CGImageSourceCreateThumbnailAtIndex(source, 0, [kCGImageSourceCreateThumbnailFromImageAlways: true, kCGImageSourceThumbnailMaxPixelSize: 900] as CFDictionary)
            }.value
            if let image { thumbnail = NSImage(cgImage: image, size: NSSize(width: image.width, height: image.height)) }
        }
    }
}

struct EmptyLibrary: View {
    var tab: LibraryTab
    var error: String?
    var showFolder: () -> Void
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 8) {
            Text(error != nil ? "LIBRARY UNAVAILABLE" : tab == .demos ? "NO RECORDINGS" : "NO SCREENSHOTS").font(.stencil(26)).tracking(3.1).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            Text(error ?? (tab == .demos ? "In the game, enable the console in Options → Game Options, press ~, enter /record, then /stoprecord to finish." : "Take a screenshot using the key set in the game’s Controls. It appears here when you return."))
                .font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center).frame(maxWidth: 520)
            Button("Show Folder in Finder", action: showFolder).glassAction().padding(.top, 8)
        }
        .frame(maxWidth: .infinity).padding(.vertical, 100)
        .accessibilityElement(children: .contain)
    }
}

struct AboutView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    var body: some View {
        GeometryReader { proxy in
        ScrollViewReader { reader in
        ScrollView {
            VStack(alignment: .leading, spacing: 0) {
                PageHeader(eyebrow: "Service record", title: "About").padding(.leading, 16)
                HStack(alignment: .center, spacing: 30) {
                    BrandMark().frame(width: 148, height: 148).shadow(color: .black.opacity(palette.dark ? 0.45 : 0.18), radius: 22, y: 12)
                    VStack(alignment: .leading, spacing: 12) {
                        Text("COD2 SILICON").font(.stencil(96)).tracking(1.9).lineLimit(1).minimumScaleFactor(0.6).foregroundStyle(palette.dark ? palette.heading : Palette.hex(0x1f2214))
                            .shadow(color: .black.opacity(palette.dark ? 0.4 : 0), radius: 18, y: 6)
                        Text("VERSION \(model.version) · MACOS 26 OR LATER").font(.typewriter(14)).foregroundStyle(palette.dark ? Palette.hex(0xd9c79a) : palette.address)
                        Text("Call of Duty 2 multiplayer, native on Apple silicon. Plays on stock 1.3 and CoD2x 1.4.6.8 servers.")
                            .font(.system(size: 16)).foregroundStyle(palette.dark ? Palette.hex(0xddd5c3) : Palette.hex(0x3a382e))
                            .frame(maxWidth: 520, alignment: .leading).fixedSize(horizontal: false, vertical: true)
                    }
                }
                .padding(.leading, 16).padding(.top, 74)
                Spacer(minLength: 40)
                HStack(alignment: .top, spacing: 20) {
                    ContentPanel {
                        PanelHeading(title: "Updates", action: nil, perform: nil)
                        UpdateStatusView(model: model).padding(.top, 12)
                        Text("Nothing is downloaded or installed until you choose Install Update.").font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.top, 12)
                        Spacer(minLength: 0)
                    }
                    .id("updates")
                    ContentPanel {
                        PanelHeading(title: "Credits", action: "Full credits") {
                            NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/CREDITS.md")!)
                        }
                        VStack(alignment: .leading, spacing: 10) {
                            credit("opencod2", "The reconstructed engine this port builds on")
                            credit("CoD2x", "Protocol and client behaviour reference, 1.4.6.8")
                            credit("SDL3 · sdl2-compat", "Window, input and audio · zlib licence")
                            credit("vkd3d-shader", "Builds your original shaders · LGPL 2.1 or later")
                            credit("Big Shoulders · Courier Prime", "Typefaces · SIL Open Font License 1.1")
                        }
                        .padding(.top, 14)
                        Spacer(minLength: 0)
                    }
                    ContentPanel {
                        PanelHeading(title: "Licence", action: "Notice") {
                            NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/NOTICE.md")!)
                        }
                        Text("MIT covers this project's own changes. Upstream opencod2 keeps its authors' rights, and third-party licences apply.")
                            .font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(3).padding(.top, 12).fixedSize(horizontal: false, vertical: true)
                        Text("Call of Duty is a trademark of Activision. This independent project isn't affiliated with Activision, Infinity Ward, Aspyr or CoD2x. Bring your own licensed game.")
                            .font(.system(size: 13)).foregroundStyle(palette.body).lineSpacing(3).padding(.top, 10).fixedSize(horizontal: false, vertical: true)
                        Spacer(minLength: 0)
                    }
                }
                .frame(minHeight: max(290, proxy.size.height - 632))
            }
            .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 32).padding(.top, 30).padding(.bottom, 28)
            .frame(minHeight: max(proxy.size.height, 760), alignment: .top)
        }
        .scrollIndicators(.automatic)
        .task(id: model.checkingUpdates) {
            guard model.checkingUpdates else { return }
            await Task.yield()
            reader.scrollTo("updates", anchor: .center)
        }
        .background(alignment: .topLeading) {
            // Composition shifted right so the village sits beside the wordmark and copy, never behind them. The copy column
            // ends at window x 978; at this height the steeple's left edge lands at (window + shift - 1300) / 2 + 809, so
            // narrower windows shift further to keep it 24 pt clear.
            HeroBackdrop(map: "mp_carentan", image: nil, leading: origin.x - max(220, 1686 - (proxy.size.width + origin.x)))
                .frame(width: proxy.size.width, height: 560).heroExtension()
        }
        }
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
    }
    private func credit(_ name: String, _ detail: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(name).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
            Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary)
        }
        .accessibilityElement(children: .combine)
    }
}
