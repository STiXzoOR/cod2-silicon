import Foundation

@main struct ConfigImportSafetyTests {
    static func require(_ value: Bool, _ message: String) {
        if !value { FileHandle.standardError.write(Data("FAIL: \(message)\n".utf8)); exit(1) }
    }
    static let catalog = ConfigImportCatalogSnapshot(settings: ["motd": .init(archived: true)],
                                                     commands: ["+attack": .init(bindable: true)])
    static func review(_ text: String, dialect: ConfigDialect) -> ConfigImportReview {
        let file = ConfigImportFile(path: "private.cfg", data: Data(text.utf8), dialect: dialect)
        return ConfigImportSafety(catalog: catalog).review(.init(source: .init(kind: .paste), entryPath: file.path, files: [file]))
    }
    static func main() {
        for dialect in [ConfigDialect.stock, .cod2x] {
            if !CommandLine.arguments.contains("--vstr-only") {
            let hidden = review("bind k say \"a ;connect 1.2.3.4 \"\n", dialect: dialect).candidates[0]
            require(hidden.classification == .blocked && !hidden.importable && hidden.reasons.contains(.malformed),
                    "\(dialect): a binding whose saved spelling exposes connect is blocked")
            for text in ["bind k say trailing\\\n", "seta motd trailing\\\n",
                         "seta motd \"hello \\\"world\\\"\"\n", "seta helper \"say \\\"hello world\\\"\"\n",
                         "seta helper harmless\nset helper \"say \\\"hello world\\\"\"\n"] {
                let candidate = review(text, dialect: dialect).candidates.last!
                require(candidate.classification == .blocked && !candidate.importable && candidate.reasons.contains(.malformed),
                        "\(dialect): changed saved binding or archived value is blocked: \(text.debugDescription)")
            }
            for text in ["bind k say \"hello world\"\n", "bind k \"say hello; say bye\"\n", "seta motd \"hello world\"\n"] {
                require(review(text, dialect: dialect).candidates[0].importable,
                        "\(dialect): stable quotes and command chains remain importable")
            }
            }
            for count in [1018, 1019] {
                let value = "say " + String(repeating: "x", count: count)
                let definition = "seta h \"\(value)\"\n"
                for use in ["vstr h\n", "bind k \"vstr h\"\n", "seta next \"vstr h\"\nbind k \"vstr next\"\n"] {
                    let candidate = review(definition + use, dialect: dialect).candidates.last!
                    require(candidate.importable == (count == 1018), "\(dialect): executed vstr values obey the 1022-byte formatter boundary")
                    if count == 1019 {
                        require(candidate.classification == .blocked && candidate.reasons.contains(.limit),
                                "\(dialect): oversized direct and transitive dependencies are blocked as limits")
                    }
                }
            }
            let ordinary = "seta motd \"" + String(repeating: "x", count: 1500) + "\"\n"
            let unexecuted = review(ordinary, dialect: dialect)
            require(unexecuted.candidates[0].importable && unexecuted.bundle.files[0].document.serialized() == Data(ordinary.utf8),
                    "\(dialect): long unexecuted archived setting bytes remain intact")
        }
        print("PASS: engine persistence spelling and executed-vstr limits in both dialects")
    }
}
