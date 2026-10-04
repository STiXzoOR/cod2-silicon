import AppKit
import Combine
import CryptoKit
import Foundation
import zlib

struct MapPixels: Sendable {
    let width: Int
    let height: Int
    let rgba: Data
}
enum ArtworkError: Error { case invalidImage, invalidArchive, missingImage }

private struct ArtworkBytes {
    let data: [UInt8]
    init(_ data: Data) { self.data = Array(data) }
    func word(_ offset: Int) -> Int { Int(data[offset]) | Int(data[offset + 1]) << 8 }
    func dword(_ offset: Int) -> Int { word(offset) | word(offset + 2) << 16 }
}

enum IWIImageDecoder {
    static let maximumPixels = 4_194_304
    static let maximumFileBytes = 32 * 1024 * 1024

    // CoD2's v5 IWI stores its 28-byte header followed by smallest-to-largest mips.
    // This decoder is independent of the renderer and deliberately accepts 2D art only.
    static func decode(_ data: Data) throws -> MapPixels {
        guard data.count >= 28, data.count <= maximumFileBytes else { throw ArtworkError.invalidImage }
        let bytes = ArtworkBytes(data)
        guard bytes.data[0...3] == [0x49, 0x57, 0x69, 5], bytes.data[5] & 12 == 0,
              bytes.word(10) == 1, bytes.dword(12) == data.count else { throw ArtworkError.invalidImage }
        let width = bytes.word(6), height = bytes.word(8), format = Int(bytes.data[4])
        guard width > 0, height > 0, width <= 4096, height <= 4096,
              width * height <= maximumPixels, [1, 11, 12, 13].contains(format) else { throw ArtworkError.invalidImage }
        func size(_ w: Int, _ h: Int) -> Int {
            format == 1 ? w * h * 4 : ((w + 3) / 4) * ((h + 3) / 4) * (format == 11 ? 8 : 16)
        }
        var topOffset = 28
        if bytes.data[5] & 2 == 0 {
            var level = 0, extent = 1
            while extent < width || extent < height { extent *= 2; level += 1 }
            if level > 0 {
                for mip in stride(from: level, through: 1, by: -1) {
                    topOffset += size(max(1, width >> mip), max(1, height >> mip))
                }
            }
        }
        guard topOffset + size(width, height) == data.count else { throw ArtworkError.invalidImage }
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        if format == 1 {
            for pixel in 0..<(width * height) {
                let source = topOffset + pixel * 4, destination = pixel * 4
                pixels[destination] = bytes.data[source + 2]
                pixels[destination + 1] = bytes.data[source + 1]
                pixels[destination + 2] = bytes.data[source]
                pixels[destination + 3] = bytes.data[source + 3]
            }
        } else {
            let blocksWide = (width + 3) / 4, blockSize = format == 11 ? 8 : 16
            for blockY in 0..<((height + 3) / 4) {
                for blockX in 0..<blocksWide {
                    let block = topOffset + (blockY * blocksWide + blockX) * blockSize
                    let color = block + (format == 11 ? 0 : 8)
                    let endpoint0 = bytes.word(color), endpoint1 = bytes.word(color + 2)
                    func rgb(_ endpoint: Int) -> [Int] {
                        let r = (endpoint >> 11) & 31, g = (endpoint >> 5) & 63, b = endpoint & 31
                        return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)]
                    }
                    let first = rgb(endpoint0), second = rgb(endpoint1)
                    let transparent = format == 11 && endpoint0 <= endpoint1
                    let colors = [first, second,
                                  zip(first, second).map { transparent ? ($0 + $1) / 2 : (2 * $0 + $1) / 3 },
                                  zip(first, second).map { transparent ? 0 : ($0 + 2 * $1) / 3 }]
                    let indices = bytes.dword(color + 4)
                    var alpha: [Int] = []
                    var alphaIndices: UInt64 = 0
                    if format == 13 {
                        let a = Int(bytes.data[block]), b = Int(bytes.data[block + 1])
                        alpha = [a, b]
                        if a > b { for i in 1...6 { alpha.append(((7 - i) * a + i * b) / 7) } }
                        else {
                            for i in 1...4 { alpha.append(((5 - i) * a + i * b) / 5) }
                            alpha += [0, 255]
                        }
                        for i in 0..<6 { alphaIndices |= UInt64(bytes.data[block + 2 + i]) << (8 * i) }
                    }
                    for index in 0..<16 {
                        let x = blockX * 4 + index % 4, y = blockY * 4 + index / 4
                        guard x < width, y < height else { continue }
                        let code = (indices >> (index * 2)) & 3, destination = (y * width + x) * 4
                        for channel in 0..<3 { pixels[destination + channel] = UInt8(colors[code][channel]) }
                        if format == 11 { pixels[destination + 3] = transparent && code == 3 ? 0 : 255 }
                        else if format == 12 {
                            let packed = bytes.data[block + index / 2]
                            pixels[destination + 3] = ((packed >> ((index % 2) * 4)) & 15) * 17
                        } else { pixels[destination + 3] = UInt8(alpha[Int((alphaIndices >> (index * 3)) & 7)]) }
                    }
                }
            }
        }
        return MapPixels(width: width, height: height, rgba: Data(pixels))
    }
}

struct IWDArtworkArchive: Sendable {
    private struct Entry: Sendable {
        let name: Data
        let flags: Int
        let method: Int
        let crc: UInt32
        let compressed: Int
        let expanded: Int
        let offset: Int
    }
    private let url: URL
    private let length: Int
    private let centralOffset: Int
    private let entries: [String: Entry]

    init(url: URL) throws {
        self.url = url
        let file = try FileHandle(forReadingFrom: url)
        defer { try? file.close() }
        let end = try file.seekToEnd()
        guard end >= 22, end < UInt64(UInt32.max) else { throw ArtworkError.invalidArchive }
        length = Int(end)
        let tailStart = max(0, length - 65_557)
        try file.seek(toOffset: UInt64(tailStart))
        let tail = ArtworkBytes(try file.read(upToCount: length - tailStart) ?? Data())
        guard tail.data.count == length - tailStart else { throw ArtworkError.invalidArchive }
        var trailer: Int?
        for offset in stride(from: tail.data.count - 22, through: 0, by: -1) {
            if tail.dword(offset) == 0x06054b50 && offset + 22 + tail.word(offset + 20) == tail.data.count {
                trailer = offset; break
            }
        }
        guard let trailer, tail.word(trailer + 4) == 0, tail.word(trailer + 6) == 0,
              tail.word(trailer + 8) == tail.word(trailer + 10) else { throw ArtworkError.invalidArchive }
        let count = tail.word(trailer + 10), centralSize = tail.dword(trailer + 12)
        centralOffset = tail.dword(trailer + 16)
        guard count <= 32_768, centralSize <= 16 * 1024 * 1024,
              centralOffset + centralSize == tailStart + trailer else { throw ArtworkError.invalidArchive }
        try file.seek(toOffset: UInt64(centralOffset))
        let central = ArtworkBytes(try file.read(upToCount: centralSize) ?? Data())
        guard central.data.count == centralSize else { throw ArtworkError.invalidArchive }
        var offset = 0, indexed: [String: Entry] = [:]
        for _ in 0..<count {
            guard offset + 46 <= centralSize, central.dword(offset) == 0x02014b50 else { throw ArtworkError.invalidArchive }
            let nameSize = central.word(offset + 28), extraSize = central.word(offset + 30), commentSize = central.word(offset + 32)
            let next = offset + 46 + nameSize + extraSize + commentSize
            guard nameSize > 0, nameSize <= 1024, next <= centralSize, central.word(offset + 34) == 0 else { throw ArtworkError.invalidArchive }
            let nameData = Data(central.data[(offset + 46)..<(offset + 46 + nameSize)])
            if let name = String(data: nameData, encoding: .utf8)?.lowercased(),
               name.hasPrefix("images/loadscreen_mp_"), name.hasSuffix(".iwi") {
                guard indexed[name] == nil else { throw ArtworkError.invalidArchive }
                indexed[name] = Entry(name: nameData, flags: central.word(offset + 8), method: central.word(offset + 10),
                                      crc: UInt32(central.dword(offset + 16)), compressed: central.dword(offset + 20),
                                      expanded: central.dword(offset + 24), offset: central.dword(offset + 42))
            }
            offset = next
        }
        guard offset == centralSize else { throw ArtworkError.invalidArchive }
        entries = indexed
    }

    func image(named name: String) throws -> Data {
        guard let entry = entries[name.lowercased()] else { throw ArtworkError.missingImage }
        guard entry.flags & 0x41 == 0, [0, 8].contains(entry.method),
              entry.expanded > 0, entry.expanded <= IWIImageDecoder.maximumFileBytes,
              entry.compressed > 0, entry.compressed <= IWIImageDecoder.maximumFileBytes,
              entry.offset + 30 <= centralOffset else { throw ArtworkError.invalidArchive }
        let file = try FileHandle(forReadingFrom: url)
        defer { try? file.close() }
        guard try file.seekToEnd() == UInt64(length) else { throw ArtworkError.invalidArchive }
        try file.seek(toOffset: UInt64(entry.offset))
        let header = ArtworkBytes(try file.read(upToCount: 30) ?? Data())
        guard header.data.count == 30, header.dword(0) == 0x04034b50,
              header.word(6) == entry.flags, header.word(8) == entry.method,
              header.word(26) == entry.name.count else { throw ArtworkError.invalidArchive }
        if entry.flags & 8 == 0 {
            guard header.dword(14) == Int(entry.crc), header.dword(18) == entry.compressed,
                  header.dword(22) == entry.expanded else { throw ArtworkError.invalidArchive }
        }
        let payloadOffset = entry.offset + 30 + header.word(26) + header.word(28)
        guard payloadOffset + entry.compressed <= centralOffset,
              try file.read(upToCount: entry.name.count) == entry.name else { throw ArtworkError.invalidArchive }
        try file.seek(toOffset: UInt64(payloadOffset))
        let compressed = try file.read(upToCount: entry.compressed) ?? Data()
        guard compressed.count == entry.compressed else { throw ArtworkError.invalidArchive }
        let expanded: Data
        if entry.method == 0 {
            guard entry.compressed == entry.expanded else { throw ArtworkError.invalidArchive }
            expanded = compressed
        } else {
            var stream = z_stream()
            guard inflateInit2_(&stream, -MAX_WBITS, zlibVersion(), Int32(MemoryLayout<z_stream>.size)) == Z_OK else { throw ArtworkError.invalidArchive }
            defer { inflateEnd(&stream) }
            var output = Data(count: entry.expanded)
            let result = compressed.withUnsafeBytes { source in
                output.withUnsafeMutableBytes { destination in
                    stream.next_in = UnsafeMutablePointer(mutating: source.bindMemory(to: Bytef.self).baseAddress)
                    stream.avail_in = uInt(entry.compressed)
                    stream.next_out = destination.bindMemory(to: Bytef.self).baseAddress
                    stream.avail_out = uInt(entry.expanded)
                    return inflate(&stream, Z_FINISH)
                }
            }
            guard result == Z_STREAM_END, stream.total_out == entry.expanded,
                  stream.total_in == entry.compressed else { throw ArtworkError.invalidArchive }
            expanded = output
        }
        let checksum = expanded.withUnsafeBytes { crc32(0, $0.bindMemory(to: Bytef.self).baseAddress, uInt(expanded.count)) }
        guard UInt32(checksum) == entry.crc else { throw ArtworkError.invalidArchive }
        return expanded
    }
}

@MainActor final class MapArtworkStore: ObservableObject {
    @Published private(set) var images: [String: NSImage] = [:]
    private let home: URL
    private var request = UUID()
    private var sourcePath = ""
    init(home: URL) { self.home = home }

    nonisolated static func mapName(_ map: String) -> String? {
        guard !map.isEmpty, map.utf8.count <= 64,
              map.utf8.allSatisfy({ (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0) || $0 == 95 }) else { return nil }
        let lower = map.lowercased()
        return lower.hasPrefix("mp_") ? lower : "mp_" + lower
    }
    func image(for map: String) -> NSImage? { Self.mapName(map).flatMap { images[$0] } }

    func load(dataPath: String, maps: [String]) async { await populate(dataPath: dataPath, maps: maps, cache: true) }

    // Review mode decodes locally without creating a second cache or modifying preferences.
    func loadPrivatePreview(dataPath: String, maps: [String]) async { await populate(dataPath: dataPath, maps: maps, cache: false) }

    func loadPrivatePreviewSynchronously(dataPath: String, maps: [String]) {
        request = UUID()
        if sourcePath != dataPath { images = [:]; sourcePath = dataPath }
        let wanted = Array(Set(maps.compactMap(Self.mapName))).prefix(64)
        guard !dataPath.isEmpty, !wanted.isEmpty else { images = [:]; return }
        publish(Self.readArtwork(dataPath: dataPath, maps: Array(wanted), cache: nil))
    }

    private func populate(dataPath: String, maps: [String], cache: Bool) async {
        let generation = UUID(); request = generation
        let wanted = Array(Set(maps.compactMap(Self.mapName))).prefix(64)
        guard !dataPath.isEmpty, !wanted.isEmpty else { images = [:]; return }
        if sourcePath != dataPath { images = [:]; sourcePath = dataPath }
        let directory = cache ? Self.cacheDirectory(home: home, dataPath: dataPath) : nil
        let decoded = await Task.detached(priority: .utility) {
            Self.readArtwork(dataPath: dataPath, maps: Array(wanted), cache: directory)
        }.value
        guard generation == request, !Task.isCancelled else { return }
        publish(decoded)
    }

    private func publish(_ decoded: [String: MapPixels]) {
        for (map, pixels) in decoded {
            guard let image = Self.cgImage(pixels) else { continue }
            images[map] = NSImage(cgImage: image, size: NSSize(width: pixels.width, height: pixels.height))
        }
    }

    private nonisolated static func cacheDirectory(home: URL, dataPath: String) -> URL? {
        let expected = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Library/Application Support/CoD2 Silicon", isDirectory: true)
        guard home.standardizedFileURL.path == expected.standardizedFileURL.path else { return nil }
        let main = URL(fileURLWithPath: dataPath).appendingPathComponent("main", isDirectory: true)
        var identity = main.standardizedFileURL.path
        for index in 0..<16 {
            let file = main.appendingPathComponent(String(format: "iw_%02d.iwd", index))
            if let values = try? file.resourceValues(forKeys: [.fileSizeKey, .contentModificationDateKey]) {
                identity += "|\(index):\(values.fileSize ?? 0):\(values.contentModificationDate?.timeIntervalSince1970 ?? 0)"
            }
        }
        let digest = SHA256.hash(data: Data(identity.utf8)).map { String(format: "%02x", $0) }.joined()
        return home.appendingPathComponent("map-artwork-v1", isDirectory: true).appendingPathComponent(digest, isDirectory: true)
    }

    private nonisolated static func readArtwork(dataPath: String, maps: [String], cache: URL?) -> [String: MapPixels] {
        var result: [String: MapPixels] = [:]
        // Fixed stock archive names prevent a downloaded mod from replacing launcher art.
        let main = URL(fileURLWithPath: dataPath).appendingPathComponent("main", isDirectory: true)
        if let cache, cacheIsSafe(cache) {
            for map in maps {
                let file = cache.appendingPathComponent(map + ".png")
                if let values = try? file.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]),
                   values.isRegularFile == true, values.isSymbolicLink != true,
                   (values.fileSize ?? Int.max) <= IWIImageDecoder.maximumFileBytes,
                   let data = try? Data(contentsOf: file), let pixels = cachedPixels(data) { result[map] = pixels }
            }
        }
        let order = [9] + Array((0..<16).reversed()).filter { $0 != 9 }
        for index in order where result.count < maps.count {
            guard !Task.isCancelled else { break }
            let file = main.appendingPathComponent(String(format: "iw_%02d.iwd", index))
            guard let archive = try? IWDArtworkArchive(url: file) else { continue }
            for map in maps where result[map] == nil {
                guard let data = try? archive.image(named: "images/loadscreen_" + map + ".iwi"),
                      let pixels = try? IWIImageDecoder.decode(data) else { continue }
                result[map] = pixels
                if let cache, cacheIsSafe(cache), let image = cgImage(pixels),
                   let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) {
                    try? FileManager.default.createDirectory(at: cache, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
                    if cacheIsSafe(cache) {
                        let destination = cache.appendingPathComponent(map + ".png")
                        try? png.write(to: destination, options: .atomic)
                        try? FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: destination.path)
                    }
                }
            }
        }
        return result
    }

    private nonisolated static func cacheIsSafe(_ directory: URL) -> Bool {
        var path = directory
        // Reject symlink redirects for every directory between the cache and app home.
        for _ in 0..<3 {
            if let values = try? path.resourceValues(forKeys: [.isSymbolicLinkKey]), values.isSymbolicLink == true { return false }
            path.deleteLastPathComponent()
        }
        return true
    }

    private nonisolated static func cgImage(_ pixels: MapPixels) -> CGImage? {
        guard let provider = CGDataProvider(data: pixels.rgba as CFData),
              let space = CGColorSpace(name: CGColorSpace.sRGB) else { return nil }
        return CGImage(width: pixels.width, height: pixels.height, bitsPerComponent: 8, bitsPerPixel: 32,
                       bytesPerRow: pixels.width * 4, space: space, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue),
                       provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
    }

    private nonisolated static func cachedPixels(_ data: Data) -> MapPixels? {
        guard let representation = NSBitmapImageRep(data: data), let image = representation.cgImage,
              image.width <= 4096, image.height <= 4096, image.width * image.height <= IWIImageDecoder.maximumPixels,
              let space = CGColorSpace(name: CGColorSpace.sRGB) else { return nil }
        var pixels = Data(count: image.width * image.height * 4)
        let succeeded = pixels.withUnsafeMutableBytes { memory -> Bool in
            guard let context = CGContext(data: memory.baseAddress, width: image.width, height: image.height,
                                          bitsPerComponent: 8, bytesPerRow: image.width * 4, space: space,
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return false }
            context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
            return true
        }
        guard succeeded else { return nil }
        // CGContext emits premultiplied bytes; the decoder/image provider uses straight RGBA.
        for pixel in 0..<(image.width * image.height) {
            let offset = pixel * 4, alpha = Int(pixels[offset + 3])
            if alpha > 0 && alpha < 255 {
                for channel in 0..<3 { pixels[offset + channel] = UInt8(min(255, (Int(pixels[offset + channel]) * 255 + alpha / 2) / alpha)) }
            }
        }
        return MapPixels(width: image.width, height: image.height, rgba: pixels)
    }
}
