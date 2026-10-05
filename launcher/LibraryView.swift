import SwiftUI
import AppKit

struct LibraryView: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var artwork: MapArtworkStore
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    init(model: LauncherModel) { self.model = model; artwork = model.artwork }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                PageHeader(title: "Library", subtitle: model.libraryTab == .demos ? "Recorded matches play back in the game, from any point."
                                                                                   : "Screenshots you take in game, kept in your CoD2 Silicon folder.")
                    .padding(.leading, 16)
                if model.media.isEmpty {
                    EmptyLibrary(tab: model.libraryTab)
                } else if model.libraryTab == .demos {
                    VStack(spacing: 4) { ForEach(model.media) { DemoRow(entry: $0, image: $0.facts.map.flatMap { artwork.image(for: $0) }) { url in model.play(demo: url) } } }
                } else {
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 18), count: 3), spacing: 18) {
                        ForEach(model.media) { ScreenshotTile(entry: $0, snapshot: model.snapshot) }
                    }
                }
            }
            .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 24).padding(.top, 84).padding(.bottom, 32)
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .onChange(of: model.libraryTab) { _ in model.loadMedia() }
        .toolbar {
            ToolbarItem(placement: .principal) {
                Picker("Library section", selection: $model.libraryTab) { ForEach(LibraryTab.allCases) { Text($0.rawValue).tag($0) } }
                    .pickerStyle(.segmented).labelsHidden().fixedSize()
            }
        }
        .trailingToolbar {
            Button(action: model.openMediaFolder) { Label("Show in Finder", systemImage: "folder").labelStyle(.titleAndIcon) }
        }
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
        .onTapGesture(count: 2) { if !snapshot { NSWorkspace.shared.open(entry.url) } }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isButton)
        .accessibilityAction(named: "Open") { NSWorkspace.shared.open(entry.url) }
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
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(spacing: 8) {
            Text(tab == .demos ? "NO RECORDINGS" : "NO SCREENSHOTS").font(.stencil(26)).tracking(3.1).foregroundStyle(palette.dark ? Palette.hex(0xcfc6b1) : palette.text)
            Text(tab == .demos ? "Record a demo in game with /record. It appears here when you return." : "Take a screenshot in game. It appears here when you return.")
                .font(.system(size: 13)).foregroundStyle(palette.secondary)
        }
        .frame(maxWidth: .infinity).padding(.vertical, 120)
        .accessibilityElement(children: .combine)
    }
}

struct AboutView: View {
    @ObservedObject var model: LauncherModel
    @Environment(\.palette) private var palette
    @State private var origin = CGPoint.zero
    var body: some View {
        GeometryReader { proxy in
        ScrollView {
            VStack(alignment: .leading, spacing: 0) {
                PageHeader(eyebrow: "Service record", title: "About").padding(.leading, 16)
                HStack(alignment: .center, spacing: 30) {
                    BrandMark().frame(width: 148, height: 148).shadow(color: .black.opacity(palette.dark ? 0.45 : 0.18), radius: 22, y: 12)
                    VStack(alignment: .leading, spacing: 12) {
                        Text("COD2 SILICON").font(.stencil(96)).tracking(1.9).foregroundStyle(palette.dark ? palette.heading : Palette.hex(0x1f2214))
                            .shadow(color: .black.opacity(palette.dark ? 0.4 : 0), radius: 18, y: 6)
                        Text("VERSION \(model.version) · ARM64 · MACOS 13 OR LATER").font(.typewriter(14)).foregroundStyle(palette.dark ? Palette.hex(0xd9c79a) : palette.address)
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
                        Text(model.updateMessage).font(.system(size: 13)).foregroundStyle(palette.body).padding(.top, 12).fixedSize(horizontal: false, vertical: true)
                        HStack(spacing: 10) {
                            Button(model.checkingUpdates ? "Checking…" : "Check for Updates", action: model.checkUpdates).glassAction().disabled(model.checkingUpdates)
                            if let url = model.releaseURL { Link("View release", destination: url).foregroundStyle(palette.link).font(.system(size: 13)) }
                        }
                        .padding(.top, 14)
                        Text("Notification only. Nothing is downloaded or installed for you.").font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.top, 12)
                        Spacer(minLength: 0)
                    }
                    ContentPanel {
                        PanelHeading(title: "Credits", action: "Full credits") {
                            NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon/blob/main/CREDITS.md")!)
                        }
                        VStack(alignment: .leading, spacing: 10) {
                            credit("opencod2", "The reconstructed engine this port builds on")
                            credit("CoD2x", "Protocol and client behaviour reference, 1.4.6.8")
                            credit("SDL3 · sdl2-compat", "Window, input and audio · zlib licence")
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
                .frame(height: max(242, proxy.size.height - 632))
            }
            .padding(.leading, max(24, 264 - origin.x)).padding(.trailing, 32).padding(.top, 30).padding(.bottom, 28)
            .frame(minHeight: max(proxy.size.height, 760), alignment: .top)
        }
        .scrollIndicators(.never)
        .background(alignment: .topLeading) {
            // Composition shifted right so the village sits beside the wordmark, never behind it.
            HeroBackdrop(map: "mp_carentan", image: nil, leading: origin.x - 220).frame(width: proxy.size.width, height: 560).heroExtension()
        }
        }
        .ignoresSafeArea(.container, edges: .top)
        .background(WindowOriginReader(origin: $origin))
        .trailingToolbar {
            Button(action: model.checkUpdates) { Label("Check for updates", systemImage: "arrow.down.to.line") }.disabled(model.checkingUpdates)
        }
    }
    private func credit(_ name: String, _ detail: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(name).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text)
            Text(detail).font(.system(size: 12)).foregroundStyle(palette.secondary)
        }
        .accessibilityElement(children: .combine)
    }
}
