import AppKit
import Foundation

@main struct ArtworkTests {
    @MainActor static func main() async throws {
        let root = URL(fileURLWithPath: CommandLine.arguments[1])
        for value in [Int.min, Int.max, -13, -1, 0, 1, 13] {
            let seed = ArtworkVariation.normalize(value)
            precondition((0..<13).contains(seed) && seed * 7 <= 84)
        }
        func decode(_ name: String) throws -> MapPixels {
            try IWIImageDecoder.decode(Data(contentsOf: root.appendingPathComponent(name + ".iwi")))
        }
        func pixel(_ image: MapPixels, _ index: Int) -> [UInt8] {
            Array(image.rgba[(index * 4)..<(index * 4 + 4)])
        }
        let argb = try decode("argb")
        precondition(argb.width == 2 && argb.height == 1)
        precondition(pixel(argb, 0) == [33, 22, 11, 44] && pixel(argb, 1) == [77, 66, 55, 88])
        let dxt1 = try decode("dxt1")
        precondition(pixel(dxt1, 0) == [255, 0, 0, 255])
        precondition(pixel(dxt1, 1) == [0, 255, 0, 255])
        precondition(pixel(dxt1, 2) == [170, 85, 0, 255] && pixel(dxt1, 3) == [85, 170, 0, 255])
        let transparent = try decode("dxt1-alpha")
        precondition(transparent.rgba.count == 3 * 2 * 4 && pixel(transparent, 0) == [0, 0, 0, 0])
        let dxt3 = try decode("dxt3")
        for index in 0..<16 { precondition(pixel(dxt3, index) == [255, 0, 0, UInt8(index * 17)]) }
        let dxt5 = try decode("dxt5")
        let alphas: [UInt8] = [255, 0, 218, 182, 145, 109, 72, 36]
        for index in 0..<16 { precondition(pixel(dxt5, index) == [255, 0, 0, alphas[index % 8]]) }
        let dxt5Alpha = try decode("dxt5-alpha")
        let sixAlphas: [UInt8] = [10, 20, 12, 14, 16, 18, 0, 255]
        for index in 0..<16 { precondition(pixel(dxt5Alpha, index) == [170, 170, 170, sixAlphas[index % 8]]) }
        let mips = try decode("mips")
        precondition(mips.width == 2 && mips.height == 2 && pixel(mips, 0) == [0, 0, 255, 255])
        let valid = try Data(contentsOf: root.appendingPathComponent("argb.iwi"))
        for cut in 0..<valid.count { precondition((try? IWIImageDecoder.decode(Data(valid.prefix(cut)))) == nil) }
        for (index, byte) in [(0, UInt8(0)), (3, 6), (4, 6), (5, 6), (6, 0), (7, 32), (10, 2), (12, 0)] {
            var bad = valid; bad[index] = byte
            precondition((try? IWIImageDecoder.decode(bad)) == nil)
        }
        let archive = try IWDArtworkArchive(url: root.appendingPathComponent("main/iw_09.iwd"))
        let extractedMips = try archive.image(named: "images/loadscreen_mp_toujane.iwi")
        let expectedMips = try Data(contentsOf: root.appendingPathComponent("mips.iwi"))
        precondition(extractedMips == expectedMips)
        let stored = try IWDArtworkArchive(url: root.appendingPathComponent("stored.iwd"))
        let extractedStored = try stored.image(named: "images/loadscreen_mp_toujane.iwi")
        precondition(extractedStored == valid)
        let descriptor = try IWDArtworkArchive(url: root.appendingPathComponent("descriptor.iwd"))
        let extractedDescriptor = try descriptor.image(named: "images/loadscreen_mp_toujane.iwi")
        precondition(extractedDescriptor == valid)
        precondition((try? stored.image(named: "../images/loadscreen_mp_toujane.iwi")) == nil)
        for name in ["corrupt", "encrypted", "oversized", "method", "name-mismatch", "bad-offset"] {
            precondition((try? IWDArtworkArchive(url: root.appendingPathComponent(name + ".iwd")).image(named: "images/loadscreen_mp_toujane.iwi")) == nil)
        }
        for file in try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) where file.lastPathComponent.hasPrefix("truncated-") {
            precondition((try? IWDArtworkArchive(url: file)) == nil)
        }
        precondition(MapArtworkStore.mapName("toujane") == "mp_toujane")
        precondition(MapArtworkStore.mapName("MP_Carentan") == "mp_carentan")
        precondition(MapArtworkStore.mapName("../mp_toujane") == nil)
        let outside = root.appendingPathComponent("never-cache-here")
        let store = MapArtworkStore(home: outside)
        await store.loadPrivatePreview(dataPath: root.path, maps: ["mp_toujane", "mp_carentan", "missing", "../bad"])
        precondition(store.images.count == 2 && store.images["mp_toujane"] != nil)
        precondition(!FileManager.default.fileExists(atPath: outside.path))
        await store.load(dataPath: root.path, maps: ["mp_toujane"])
        precondition(!FileManager.default.fileExists(atPath: outside.path))
        let synchronous = MapArtworkStore(home: outside)
        synchronous.loadPrivatePreviewSynchronously(dataPath: root.path, maps: ["toujane", "carentan"])
        precondition(synchronous.images.count == 2 && !FileManager.default.fileExists(atPath: outside.path))
        let home = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Library/Application Support/CoD2 Silicon")
        let cached = MapArtworkStore(home: home)
        await cached.load(dataPath: root.path, maps: ["mp_dawnville"])
        let cacheRoot = home.appendingPathComponent("map-artwork-v1")
        let sources = try FileManager.default.contentsOfDirectory(at: cacheRoot, includingPropertiesForKeys: nil)
        precondition(sources.count == 1)
        let cachedFile = sources[0].appendingPathComponent("mp_dawnville.png")
        let attrs = try FileManager.default.attributesOfItem(atPath: cachedFile.path)
        precondition((attrs[.posixPermissions] as? NSNumber)?.intValue == 0o600)
        let directoryAttrs = try FileManager.default.attributesOfItem(atPath: sources[0].path)
        precondition((directoryAttrs[.posixPermissions] as? NSNumber)?.intValue == 0o700)
        let cachedAgain = MapArtworkStore(home: home)
        await cachedAgain.load(dataPath: root.path, maps: ["mp_dawnville"])
        func imageBytes(_ image: NSImage?) -> Data? {
            guard let bitmap = image?.cgImage(forProposedRect: nil, context: nil, hints: nil), let raw = bitmap.dataProvider?.data else { return nil }
            return raw as Data
        }
        precondition(imageBytes(cached.images["mp_dawnville"]) == imageBytes(cachedAgain.images["mp_dawnville"]))
        await cachedAgain.load(dataPath: root.path, maps: ["missing"])
        precondition(cachedAgain.images["mp_dawnville"] != nil)
        await cachedAgain.loadPrivatePreview(dataPath: root.appendingPathComponent("other").path, maps: ["missing"])
        precondition(cachedAgain.images.isEmpty)
        print("PASS: synthetic IWI DXT1/3/5/BGRA + mips; stored/deflated/descriptor/corrupt ZIPs; safe map names; private cache round-trip/permissions")
    }
}
