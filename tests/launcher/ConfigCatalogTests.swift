import Foundation
import Darwin

@main struct ConfigCatalogTests {
    static func require(_ condition: @autoclosure () -> Bool, _ message: String) {
        if !condition() {
            FileHandle.standardError.write(Data(("FAIL: " + message + "\n").utf8))
            exit(1)
        }
    }
    static func rejects(_ bytes: Data, _ message: String) {
        do { _ = try ConfigCatalog(data: bytes); require(false, message) }
        catch { }
    }
    static func changed(_ bytes: Data, _ edit: (inout [String: Any]) -> Void) throws -> Data {
        var object = try JSONSerialization.jsonObject(with: bytes) as! [String: Any]
        edit(&object)
        return try JSONSerialization.data(withJSONObject: object)
    }
    static func main() async throws {
        let url = URL(fileURLWithPath: CommandLine.arguments[1])
        let bytes = try Data(contentsOf: url)
        let catalog = try ConfigCatalog(data: bytes)
        require(catalog.sourceRevision == "bb908124", "catalog must describe the requested integration")
        let speed = catalog.setting(named: "SENSITIVITY")!
        require(speed.name == "sensitivity" && speed.group == .aim, "case-insensitive setting lookup")
        require(catalog.settings(in: .aim).contains(speed), "group lookup")
        require(catalog.command(named: "MP_QUICKMESSAGE")?.name == "mp_QuickMessage", "correct quick-message command")
        require(catalog.command(named: "quickmessage") == nil, "old spelling must not be claimed")
        require(catalog.command(named: "+attack")?.bindable == true, "bindable action")
        require(catalog.commands(in: .other).contains { $0.name == "+attack" }, "command group lookup")
        require(catalog.setting(named: "r_swapInterval") != nil, "correct display-sync spelling")
        require(catalog.setting(named: "r_anisotropy") != nil, "correct filtering spelling")
        require(catalog.setting(named: "r_vsync")?.availability == .unavailable, "unavailable spelling is explicit")
        require(catalog.setting(named: "scr_sd_roundlimit")?.availability == .scriptOrMod, "script settings are separate")
        require(catalog.setting(named: "m_enable")?.availability == .referenceOnly, "upstream-only settings are separate")
        require(catalog.setting(named: "m_enable")?.isEditable(in: .cod2x) == false, "no native control for upstream-only setting")
        require(catalog.setting(named: "r_mode")?.variant(for: .stock)?.enumValues?.status == .dynamic, "display choices are dynamic")
        require(catalog.setting(named: "fs_homepath")?.variant(for: .stock)?.defaultValue.status == .dynamic, "home path is dynamic")
        let frameCap = catalog.setting(named: "com_maxfps")!
        require(frameCap.variant(for: .stock)?.defaultValue.value == .number(85), "stock frame-cap registration")
        require(frameCap.variant(for: .cod2x)?.defaultValue.value == .number(250), "port CoD2x frame-cap registration")
        require(catalog.setting(named: "snaps")?.variant(for: .cod2x)?.defaultValue.value == .number(40), "port startup preference stays distinct from policy")
        require(catalog.setting(named: "g_competitive")?.variant(for: .cod2x)?.flagsDynamic == true, "run-time flag choice")
        require(catalog.setting(named: "g_competitive")?.isEditable(in: .cod2x) == false, "server policy is protected")
        let privacy = catalog.setting(named: "cl_matchSummaries")!
        for mode in ConfigCatalogMode.allCases {
            let variant = privacy.variant(for: mode)!
            require(variant.sites.count == 1 && variant.sites[0].flags == 0x1000, "privacy constructor consumes startup choice")
            require(variant.flags == 0x1040, "privacy retains the post-registration read-only freeze")
            require(!privacy.isEditable(in: mode), "startup privacy cannot become a loadout control")
        }
        require(catalog.setting(named: "r_lodScale")?.isEditable(in: .stock) == false, "stock cheat protection")
        require(catalog.setting(named: "r_lodScale")?.isEditable(in: .cod2x) == true, "intentional CoD2x flag change")
        for setting in catalog.settings {
            require(!setting.title.isEmpty && !setting.summary.isEmpty && !setting.details.isEmpty, "every setting has copy")
            for mode in ConfigCatalogMode.allCases {
                guard let variant = setting.variant(for: mode) else { continue }
                if variant.flags & 0xd0 != 0 {
                    require(!setting.isEditable(in: mode), "protected setting cannot become an editable control")
                    require(!catalog.guidedSettings(in: setting.group, mode: mode).contains(setting), "protected setting is absent from Guided")
                }
            }
        }
        let encoded = try JSONEncoder().encode(catalog)
        let decoded = try ConfigCatalog(data: encoded)
        require(decoded.settings == catalog.settings && decoded.commands == catalog.commands, "Codable round-trip")
        let detached = await Task.detached { catalog.setting(named: "sensitivity")?.title }.value
        require(detached == speed.title, "immutable catalog crosses actors")
        rejects(try changed(bytes) { $0["schemaVersion"] = 999 }, "unknown schema accepted")
        rejects(try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            var duplicate = entries[0]; duplicate["name"] = (duplicate["name"] as! String).uppercased()
            entries.append(duplicate); object["settings"] = entries
        }, "case-insensitive duplicate accepted")
        rejects(try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            entries[0]["group"] = "invented"; object["settings"] = entries
        }, "unknown group accepted")
        let malformedCommandFields: [(String, Any)] = [
            ("name", "invented"), ("path", ""), ("line", 0), ("mechanism", "")
        ]
        for (field, value) in malformedCommandFields {
            rejects(try changed(bytes) { object in
                var entries = object["commands"] as! [[String: Any]]
                var variants = entries[0]["variants"] as! [String: Any]
                let mode = variants.keys.sorted()[0]
                var variant = variants[mode] as! [String: Any]
                var sites = variant["sites"] as! [[String: Any]]
                sites[0][field] = value
                variant["sites"] = sites; variants[mode] = variant
                entries[0]["variants"] = variants; object["commands"] = entries
            }, "malformed command site accepted: \(field)")
        }
        // Do not trust a resource's editable hint when its source flags forbid editing.
        let unsafe = try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            let i = entries.firstIndex { ($0["name"] as! String).lowercased() == "r_lodscale" }!
            var variants = entries[i]["variants"] as! [String: Any]
            var stock = variants["stock"] as! [String: Any]; stock["editable"] = true
            variants["stock"] = stock; entries[i]["variants"] = variants; object["settings"] = entries
        }
        let unsafeCatalog = try ConfigCatalog(data: unsafe)
        require(unsafeCatalog.setting(named: "r_lodScale")?.isEditable(in: .stock) == false, "editable hint bypassed flags")
        let unsafePrivacy = try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            let i = entries.firstIndex { ($0["name"] as! String).lowercased() == "cl_matchsummaries" }!
            var variants = entries[i]["variants"] as! [String: Any]
            for mode in ["stock", "cod2x"] {
                var variant = variants[mode] as! [String: Any]; variant["editable"] = true
                variants[mode] = variant
            }
            entries[i]["variants"] = variants; object["settings"] = entries
        }
        let unsafePrivacyCatalog = try ConfigCatalog(data: unsafePrivacy)
        for mode in ConfigCatalogMode.allCases {
            require(unsafePrivacyCatalog.setting(named: "cl_matchSummaries")?.isEditable(in: mode) == false,
                    "editable hint bypassed the startup privacy freeze")
        }
        let disputed = try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            let i = entries.firstIndex { $0["name"] as! String == "sensitivity" }!
            var variants = entries[i]["variants"] as! [String: Any]
            var stock = variants["stock"] as! [String: Any]
            var sites = stock["sites"] as! [[String: Any]]
            var other = sites[0]; other["default"] = ["status": "literal", "value": 9.0]
            sites.append(other); stock["sites"] = sites
            variants["stock"] = stock; entries[i]["variants"] = variants; object["settings"] = entries
        }
        let disputedCatalog = try ConfigCatalog(data: disputed)
        require(disputedCatalog.setting(named: "sensitivity")?.variant(for: .stock)?.defaultValue.status == .ambiguous, "conflicting defaults guessed from file order")
        rejects(try changed(bytes) { object in
            var entries = object["settings"] as! [[String: Any]]
            let i = entries.firstIndex { $0["name"] as! String == "sensitivity" }!
            var variants = entries[i]["variants"] as! [String: Any]
            var stock = variants["stock"] as! [String: Any]
            var sites = stock["sites"] as! [[String: Any]]
            sites[0]["default"] = ["status": "literal", "value": "invented"]
            stock["sites"] = sites; variants["stock"] = stock
            entries[i]["variants"] = variants; object["settings"] = entries
        }, "default with the wrong type accepted")
        let fixture = FileManager.default.temporaryDirectory.appendingPathComponent("catalog-\(UUID().uuidString).bundle")
        defer { try? FileManager.default.removeItem(at: fixture) }
        let resources = fixture.appendingPathComponent("Contents/Resources")
        try FileManager.default.createDirectory(at: resources, withIntermediateDirectories: true)
        try bytes.write(to: resources.appendingPathComponent("ConfigCatalog.json"))
        let plist = ["CFBundleIdentifier": "org.cod2.catalog-fixture", "CFBundlePackageType": "BNDL"]
        try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0).write(to: fixture.appendingPathComponent("Contents/Info.plist"))
        let bundled = try ConfigCatalog.load(bundle: Bundle(url: fixture)!)
        require(bundled.settings == catalog.settings, "bundle resource load")
        do { _ = try ConfigCatalog.load(bundle: Bundle(path: "/System/Library/Frameworks/Foundation.framework")!); require(false, "missing resource silently loaded") }
        catch { }
        print("PASS: ConfigCatalog Swift 6 tests (\(catalog.settings.count) settings, \(catalog.commands.count) commands)")
    }
}
