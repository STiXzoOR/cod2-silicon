import Foundation
import Darwin

@main struct ConfigDocumentTests {
    static func bytes(_ string: String) -> Data { Data(string.utf8) }
    static func require(_ condition: @autoclosure () -> Bool, _ message: String) {
        if !condition() { FileHandle.standardError.write(Data(("FAIL: " + message + "\n").utf8)); exit(1) }
    }
    static func rejects(_ document: ConfigDocument, _ change: (inout ConfigDocument) throws -> Void) {
        var copy = document
        do { try change(&copy); require(false, "lossy edit accepted") }
        catch { require(copy.serialized() == document.serialized(), "rejected edit changed document") }
    }
    static func grammarFixtures(_ root: URL) throws {
        func rows(_ name: String) throws -> [[String: Any]] {
            try JSONSerialization.jsonObject(with: Data(contentsOf: root.appendingPathComponent(name))) as! [[String: Any]]
        }
        func expand(_ value: Any) -> String {
            if let string = value as? String { return string == "\\n" ? "\n" : string }
            let spec = value as! [String: Any]
            if let concatenated = spec["concat"] as? [Any] { return concatenated.map(expand).joined() }
            if let repeated = spec["char"] as? String { return String(repeating: repeated, count: spec["repeat"] as! Int) }
            return Array(repeating: spec["token"] as! String, count: spec["count"] as! Int).joined(separator: spec["sep"] as! String) + (spec["suffix"] as! String)
        }
        var count = 0
        for row in try rows("tokenize.json") {
            let input = try ConfigDocument.encode(expand(row["input"] ?? row["input_spec"]!))
            for dialect in [ConfigDialect.stock] {
                let result = ConfigDocument.tokenize(data: input, dialect: dialect, tokenLimit: row["limit"] as? Int ?? 0)
                if let expected = row["argv"] as? [String] { require(result.tokens.map(\.text) == expected, "independent tokenizer fixture \(row["id"]!)") }
                if let argc = row["argc"] as? Int { require(result.tokens.count == argc, "independent token count \(row["id"]!)") }
                count += 1
            }
        }
        for row in try rows("tokenize_cod2x.json") {
            let input = try ConfigDocument.encode(row["input"] as! String)
            for dialect in [ConfigDialect.stock, .cod2x] {
                let result = ConfigDocument.tokenize(data: input, dialect: dialect)
                let expected = row[dialect == .stock ? "argv_stock_1_3" : "argv_cod2x_1_4_6_8"] as! [String]
                require(result.tokens.map(\.text) == expected, "independent dialect fixture \(row["id"]!)")
                count += 1
            }
        }
        for row in try rows("cbuf_split.json") {
            let input = try ConfigDocument.encode(expand(row["buffer"] ?? row["buffer_spec"]!))
            let result = ConfigDocument(data: input, dialect: .stock)
            if let expected = row["commands"] as? [String] {
                require(result.commands.map { ConfigDocument.text(input.subdata(in: $0.range)) } == expected, "independent splitter fixture \(row["id"]!)")
            }
            if let lengths = row["command_lengths"] as? [Int] { require(result.commands.map { $0.range.count } == lengths, "independent splitter lengths") }
            if let tokens = row["argv"] as? [[String]] { require(result.commands.map { $0.tokens.map(\.text) } == tokens, "independent split tokens") }
            count += 1
        }
        print("PASS: \(count) independent grammar fixture checks; content withheld")
    }
    static func main() throws {
        for dialect in [ConfigDialect.stock, .cod2x] {
            let raw = Data([239, 187, 191]) + bytes("// comment\r\nseta name \"") + Data([167, 255]) + bytes("\"\r\nbind J \"+attack\"; mystery x\n") + Data([0, 255, 13, 10])
            var doc = ConfigDocument(data: raw, dialect: dialect)
            require(doc.serialized() == raw, "raw round trip")
            require(doc.effectiveSettingBytes(named: "name") == Data([167, 255]), "quoted bytes retained")
            require(doc.ignoredSuffixRange == (raw.count - 4)..<raw.count, "NUL suffix range")
            try doc.setSetting(named: "name", value: Data([228]))
            require(doc.serialized() == Data([239, 187, 191]) + bytes("// comment\r\nseta name \"") + Data([228]) + bytes("\"\r\nbind J \"+attack\"; mystery x\n") + Data([0, 255, 13, 10]), "mutation preserves unrelated bytes")
            require(doc.effectiveBind(key: "j") == "+attack", "bind lookup")

            let commands = ConfigDocument(data: bytes("// note; quit\r\nbind j \"say \\\"a;b\\\"\"\n"), dialect: dialect).commands
            require(commands.count == 5, "splitter quote parity and CRLF empties")
            require(commands[1].tokens.map(\.text) == ["quit"], "semicolon after comment executes")
            require(commands[3].tokens.map(\.text) == ["bind", "j", "say \"a"], "escaped quote counts for splitter")
            let tokens = ConfigDocument.tokenize(data: bytes("a/*/b \"\" \"c\"d"), dialect: dialect)
            require(tokens.tokens.map(\.text) == ["a", "b", "", "c", "d"], "overlap comment and adjacent tokens")
            require(tokens.comments == [1..<4], "comment span")
            require(tokens.tokens[3].range == 9..<12, "quoted token span")
            require(ConfigDocument.tokenize(data: bytes("bind j \"say \\\"hello\\\"\""), dialect: dialect).tokens.last?.text == "say \"hello\"", "escaped quote emitted")
            for high in UInt8(128)...UInt8(255) {
                let parsed = ConfigDocument.tokenize(data: Data([97, high, 98, 32, 34, high, 34]), dialect: dialect)
                require(parsed.tokens.map(\.value) == [Data([97]), Data([98]), Data([high])], "signed byte whitespace")
            }
            let limited = ConfigDocument.tokenize(data: bytes("one two   /* comment */ \"three four\""), dialect: dialect, tokenLimit: 3)
            require(limited.tokens.map(\.text) == ["one", "two", "  /* comment */ \"three four\""], "verbatim final remainder")
            require(limited.tokens.last?.kind == .remainder, "remainder identified")
            let limitAfterQuote = ConfigDocument.tokenize(data: bytes("one \"two\"  three"), dialect: dialect, tokenLimit: 3)
            require(limitAfterQuote.tokens.last?.text == " three", "one separator consumed after quote")
            let maximal = bytes(Array(repeating: "a", count: 512).joined(separator: " "))
            require(ConfigDocument.tokenize(data: maximal, dialect: dialect).tokens.count == 512, "512 tokens at EOF")
            require(ConfigDocument.tokenize(data: maximal + bytes(" "), dialect: dialect).failure == .tooManyTokens, "512 with trailing separator fails")
            require(ConfigDocument.tokenize(data: maximal + bytes(" b"), dialect: dialect).tokens.isEmpty, "overflow drops whole command")
            let upload = ConfigDocument.tokenize(data: bytes("upload https://host/demo // comment"), dialect: dialect)
            require(upload.tokens.map(\.text) == ["upload", dialect == .stock ? "https:" : "https://host/demo"], "URL dialect")

            var duplicates = ConfigDocument(data: bytes("seta sensitivity \"1\"\r\nunknown p\nSeT sensitivity 2 /* kept */\t// tail\n"), dialect: dialect)
            require(duplicates.effectiveSetting(named: "SENSITIVITY") == "2", "last assignment wins")
            try duplicates.setSetting(named: "sensitivity", value: "3")
            require(duplicates.serialized() == bytes("seta sensitivity \"1\"\r\nunknown p\nSeT sensitivity \"3\" /* kept */\t// tail\n"), "only effective assignment changes")
            try duplicates.removeSetting(named: "sensitivity")
            require(duplicates.effectiveSetting(named: "sensitivity") == nil, "remove all assignments avoids resurrection")
            require(duplicates.serialized() == bytes("\r\nunknown p\n\n"), "script removal preserves separators")

            var binds = ConfigDocument(data: bytes("bind A \"first\"\nbind 0x61 \"second\"\nunbind A\nbind J say \"two words\"\n"), dialect: dialect)
            require(binds.effectiveBind(key: "a") == "second", "key aliases and uppercase unbind")
            require(binds.effectiveBind(key: "j") == "say \"two words\"", "bind argument reassembly")
            try binds.setBind(key: "A", binding: "+attack")
            require(binds.serialized() == bytes("bind A \"first\"\nbind 0x61 \"+attack\"\nunbind A\nbind J say \"two words\"\n"), "bind changes last effective duplicate")
            try binds.removeBind(key: "a")
            require(binds.effectiveBind(key: "a") == nil, "remove all matching bind assignments")
            require(ConfigDocument(data: bytes("bind a \"old\"\nunbindall\nbind b \"new\""), dialect: dialect).effectiveBind(key: "a") == nil, "unbindall clears bindings")
            var afterUnbind = ConfigDocument(data: bytes("bind a \"old\"\nunbind a\n"), dialect: dialect)
            try afterUnbind.setBind(key: "a", binding: "+attack")
            require(afterUnbind.effectiveBind(key: "a") == "+attack", "edit after unbind appends effective bind")

            let base = ConfigDocument(data: bytes("seta x \"ok\"\nbind j \"ok\"\n"), dialect: dialect)
            rejects(base) { try $0.setSetting(named: "x", value: "a\"b") }
            rejects(base) { try $0.setSetting(named: "x", value: "snowman ☃") }
            rejects(base) { try $0.setSetting(named: "bad-name", value: "1") }
            rejects(base) { try $0.setSetting(named: "x", value: "a\nb") }
            rejects(base) { try $0.setSetting(named: "new_value", value: "tail\\") }
            rejects(base) { try $0.setBind(key: "j", binding: "say \\\"a;b\\\"") }
            rejects(base) { try $0.setBind(key: "j", binding: "tail\\") }
            rejects(base) { try $0.setBind(key: "missing-key", binding: "x") }
            rejects(base) { try $0.setBind(key: "j", binding: String(repeating: "x", count: 1024)) }
            var longest = base
            try longest.setBind(key: "j", binding: String(repeating: "x", count: 1023))
            require(longest.effectiveBindBytes(key: "j")?.count == 1023, "bind maximum retained")
            var escaped = base
            try escaped.setBind(key: "k", binding: "say \"hi\"; say bye")
            require(escaped.effectiveBind(key: "k") == "say \"hi\"; say bye", "safe escaped quotes retain binding")
            try escaped.setSetting(named: "empty", value: "")
            require(escaped.effectiveSettingBytes(named: "empty") == Data(), "empty setting is an assignment")
            try escaped.setSetting(named: "accent", value: "é")
            require(escaped.effectiveSettingBytes(named: "accent") == Data([233]), "new string is encoded as Latin-1")
            let interiorComment = ConfigDocument(data: bytes("seta x one /* preserve */ two\n"), dialect: dialect)
            rejects(interiorComment) { try $0.setSetting(named: "x", value: "3") }
            let overlong = ConfigDocument(data: Data(repeating: 97, count: 5000), dialect: dialect)
            require(overlong.commands.map { $0.range.count } == [4095, 904], "split limit skips one byte")
            require(overlong.commands[0].separatorRange == 4095..<4096, "discarded byte span")
            require(overlong.diagnostics.contains { if case .lineLimit = $0 { return true }; return false }, "split limit diagnosed")
            rejects(base) { try $0.setSetting(named: "x", value: String(repeating: "x", count: 4090)) }
            let tooLarge = ConfigDocument(data: Data(repeating: 32, count: 65536), dialect: dialect)
            rejects(tooLarge) { try $0.setSetting(named: "x", value: "1") }
        }
        let canonical = "// generated by Call of Duty, do not modify\r\nunbindall\r\nunbindall\r\nbind A \"+attack\"\r\nbind MOUSE1 \"+attack\"\r\nseta beta \"2\"\r\nseta zeta \"3\"\r\n"
        var doc = ConfigDocument(data: bytes(canonical), dialect: .stock)
        require(doc.isCanonical, "canonical full shape")
        try doc.setSetting(named: "alpha", value: "1")
        try doc.setBind(key: "j", binding: "+reload")
        require(doc.serialized() == bytes(canonical.replacingOccurrences(of: "bind MOUSE1", with: "bind j \"+reload\"\r\nbind MOUSE1").replacingOccurrences(of: "seta beta", with: "seta alpha \"1\"\r\nseta beta")), "canonical sorted insertion and CRLF")
        try doc.removeSetting(named: "beta")
        require(!String(decoding: doc.serialized(), as: UTF8.self).contains("seta beta"), "canonical removes whole line")
        require(doc.isCanonical, "canonical remains canonical")
        for script in [canonical + "exec extra\r\n", canonical.replacingOccurrences(of: "seta beta", with: "seta zzz"), canonical.replacingOccurrences(of: "bind A", with: " bind A")] {
            var document = ConfigDocument(data: bytes(script), dialect: .stock)
            require(!document.isCanonical, "script not treated as canonical")
            try document.setSetting(named: "alpha", value: "1")
            require(document.serialized() == bytes(script + "seta alpha \"1\"\r\n"), "script insertion appends")
        }
        var noEOL = ConfigDocument(data: bytes("// trailing comment"), dialect: .stock)
        try noEOL.setSetting(named: "x", value: "1")
        require(noEOL.serialized() == bytes("// trailing comment\nseta x \"1\"\n"), "append escapes final comment")
        let dependencies = ConfigDocument(data: bytes("exec other; vstr chain; toggle x 1 2\n"), dialect: .stock)
        require(dependencies.unresolvedCommands.count == 3, "runtime dependencies exposed")
        var dynamic = ConfigDocument(data: bytes("seta x \"1\"\ntoggle x 1 2\n"), dialect: .stock)
        require(dynamic.effectiveSetting(named: "x") == nil, "dynamic dvar change is unresolved")
        try dynamic.setSetting(named: "x", value: "3")
        require(dynamic.serialized() == bytes("seta x \"1\"\ntoggle x 1 2\nseta x \"3\"\n"), "direct edit follows dynamic barrier")
        print("PASS: config document byte preservation, splitter/tokenizer, effective edits, limits and insertion")
        if CommandLine.arguments.count > 1 {
            var files = 0, byteCount = 0
            var arguments = Array(CommandLine.arguments.dropFirst())
            while !arguments.isEmpty {
                let path = arguments.removeFirst()
                if path == "--grammar" { try grammarFixtures(URL(fileURLWithPath: arguments.removeFirst())); continue }
                if path == "--corpus" { continue }
                let url = URL(fileURLWithPath: path)
                guard let enumerator = FileManager.default.enumerator(at: url, includingPropertiesForKeys: [.isRegularFileKey]) else { preconditionFailure("corpus directory unavailable") }
                for case let file as URL in enumerator where ["cfg", "txt"].contains(file.pathExtension.lowercased()) {
                    let input = try Data(contentsOf: file)
                    for dialect in [ConfigDialect.stock, .cod2x] {
                        require(ConfigDocument(data: input, dialect: dialect).serialized() == input, "private corpus round trip failed")
                    }
                    files += 1; byteCount += input.count
                }
            }
            if files > 0 { print("PASS: corpus round trip, \(files) files, \(byteCount) bytes, both dialects; content withheld") }
        }
    }
}
