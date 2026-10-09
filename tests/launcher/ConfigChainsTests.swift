import Foundation

@main struct ConfigChainsTests {
    static func require(_ value: Bool, _ message: String) {
        if !value { fputs("FAIL: \(message)\n", stderr); exit(1) }
    }
    static func rejects(_ message: String, _ operation: () throws -> Void) {
        do { try operation(); require(false, message) } catch {}
    }
    static func document(_ text: String) -> ConfigDocument { .init(data: Data(text.utf8), dialect: .cod2x) }
    static func tokens(_ trace: ConfigChainTrace) -> [[String]] { trace.commands.map { $0.map(ConfigDocument.text) } }
    static func reviewRegressions(_ scratch: URL) throws {
        let store = try ConfigProfileStore(homeURL: scratch.appendingPathComponent("review"), dialect: .cod2x)
        var failures: [String] = []
        func expectThrow(_ label: String, _ operation: () throws -> Void) {
            do { try operation(); failures.append(label) } catch {}
        }
        let oversized = "say " + String(repeating: "x", count: 1009) + "; vstr safe"
        var machine = ConfigChainMachine(dialect: .cod2x)
        _ = machine.execute(Data("seta a \"\(oversized)\"\nseta safe \"say harmless\"\nseta sa \"quit\"\n".utf8))
        if machine.variables["a"]?.count != 1022 { failures.append("native imported set truncation") }
        var meta = ConfigStudioMetadata(displayName: "Limit"); meta.locks = ["action:quit"]
        let limit = try store.create(id: "Limit", displayName: "Limit", document: document("seta a \"\(oversized)\"\nseta safe \"say harmless\"\nseta sa \"quit\"\n"), metadata: meta)
        expectThrow("truncated imported dependency bypasses action lock") { _ = try ConfigChains.prepare(chains: [.init(id: "truncated", key: "j", behavior: .commands(["vstr a"]))], read: limit) }

        let base = try store.create(id: "Original", displayName: "Original", document: document(""))
        _ = try ConfigChains.apply(ConfigChains.prepare(chains: [.init(id: "copy", key: "j", behavior: .commandCycle(steps: [["say one"], ["say two"]]))], read: base), to: store)
        let original = try store.read(id: "Original")
        let copyName = try ConfigStudioLaunch.chainBasename(profileID: "Copy")
        let fail = try ConfigProfileStore(homeURL: store.homeURL, dialect: .cod2x, writeFile: { data, url in
            try PrivateFile.write(data, to: url)
            if url.lastPathComponent == copyName { throw ConfigProfileError.cannotPublish }
        })
        expectThrow("copy failure injection") { _ = try fail.duplicate(id: "Original", as: "Copy", displayName: "Copy", expected: original.version) }
        do {
            let copy = try store.duplicate(id: "Original", as: "Copy", displayName: "Copy", expected: original.version)
            let count = try store.history(id: "Copy").count
            if copy.version.chain != original.version.chain || count != 1 { failures.append("copy journal exact recovery") }
        } catch { failures.append("interrupted copy cannot resume") }

        let source = try store.create(id: "Source", displayName: "Source", document: document("set source 5\nseta target 0\nbind j \"setfromdvar target source\"\n"))
        expectThrow("setfromdvar export loses source after rewrite") { _ = try ConfigChains.windowsExport(read: source) }
        let runtime = try store.create(id: "Runtime", displayName: "Runtime", document: document("seta script \"say harmless\"\nset script \"say \\\"hello world\\\"\"\nbind j \"vstr script\"\n"))
        expectThrow("effective archived quotes") { _ = try ConfigChains.windowsExport(read: runtime) }
        var transientDoc = document("seta script \"say harmless\"\n")
        try transientDoc.setBind(key: "j", binding: "set script \"say \\\"hello world\\\"\"; wait 1; set script safe")
        let transient = try store.create(id: "Transient", displayName: "Transient", document: transientDoc)
        expectThrow("archived quotes before a yielding wait") { _ = try ConfigChains.windowsExport(read: transient) }
        var oldMeta = ConfigStudioMetadata(displayName: "Old"); oldMeta.locks = ["action:quit"]
        let old = try store.create(id: "Old", displayName: "Old", document: document("seta act \"quit\"\nbind j \"vstr act\"\n"), metadata: oldMeta)
        expectThrow("old imported action lock") { _ = try ConfigChains.prepare(chains: [.init(id: "replace", key: "j", behavior: .commands(["say replacement"]))], read: old) }
        for failure in failures { fputs("REVIEW RED: \(failure)\n", stderr) }
        require(failures.isEmpty, "\(failures.count) review regressions")
        print("PASS: five review regressions: native truncation, interrupted copy, setfromdvar durability, runtime quotes and old transitive locks")
    }
    @MainActor static func main() throws {
        let scratch = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        try reviewRegressions(scratch)
        let stockStore = try ConfigProfileStore(homeURL: scratch.appendingPathComponent("stock"), dialect: .stock)
        let stock = try stockStore.create(id: "Stock", displayName: "Stock", document: .init(data: Data(), dialect: .stock))
        rejects("stock native escaped quote parser drift") {
            _ = try ConfigChains.prepare(chains: [.init(id: "quoted", key: "j", behavior: .commands(["say \"hello world\""]))], read: stock)
        }
        rejects("stock native quoted helper parser drift") {
            _ = try ConfigChains.prepare(chains: [.init(id: "quoted", key: "j", behavior: .commandCycle(steps: [["say \"hello world\""], ["say bye"]]))], read: stock)
        }
        var held = ConfigChainMachine(dialect: .cod2x)
        require(held.keyEvent(binding: Data("+attack; +holdbreath".utf8), key: 106, down: true, time: 100).issues.contains(.heldActionWithoutRelease), "only leading plus is released")
        print("PASS: stock quote-drift refusal and additional held-action warning")
        // Expected traces are authored independently of the compiler/preview.
        for (binding, down, up) in [
            ("+attack", [["+attack", "106", "100"]], [["-attack", "106", "200"]]),
            ("+attack; say hi", [["+attack"], ["say", "hi", "106", "100"]], [["-attack"], ["say", "hi", "106", "200"]]),
            ("toggleads; +holdbreath", [["toggleads"], ["+holdbreath"]], []),
            (" +attack", [["+attack"]], []),
            ("+attack; // apparent comment; quit", [["+attack"], ["quit", "106", "100"]], [["-attack"], ["quit", "106", "200"]])
        ] {
            var machine = ConfigChainMachine(dialect: .cod2x)
            require(tokens(machine.keyEvent(binding: Data(binding.utf8), key: 106, down: true, time: 100)) == down, "press \(binding)")
            require(tokens(machine.keyEvent(binding: Data(binding.utf8), key: 106, down: false, time: 200)) == up, "release \(binding)")
        }
        var machine = ConfigChainMachine(dialect: .cod2x, files: ["child.cfg": Data("vstr a\n".utf8)])
        _ = machine.execute(Data("set a \"say first; set next vstr b\"\nset b \"say second; set next vstr a\"\nset next \"vstr a\"\n".utf8))
        require(tokens(machine.execute(Data("vstr next\n".utf8))) == [["say", "first"], ["set", "next", "vstr", "b"]], "first press pointer mutation")
        require(tokens(machine.execute(Data("vstr next\n".utf8))) == [["say", "second"], ["set", "next", "vstr", "a"]], "second press pointer mutation")
        require(tokens(machine.execute(Data("exec child\n".utf8))).first == ["say", "first"], "recursive exec and vstr")
        _ = machine.execute(Data("set loop \"vstr loop\"\n".utf8))
        require(machine.execute(Data("vstr loop\n".utf8)).issues.contains(.nonYieldingRecursion), "immediate recursion")
        machine.files["loop.cfg"] = Data("vstr loop\n".utf8)
        _ = machine.execute(Data("set loop \"exec loop\"\n".utf8))
        require(machine.execute(Data("exec loop\n".utf8)).issues.contains(.nonYieldingRecursion), "mixed recursion")
        _ = machine.execute(Data("set loop \"wait 1; vstr loop\"\n".utf8))
        let yielding = machine.execute(Data("vstr loop\n".utf8))
        require(yielding.issues.contains(.yieldingLoop) && !yielding.issues.contains(.nonYieldingRecursion), "wait yields")
        machine.waitDisabled = true
        let competitive = machine.execute(Data("vstr loop\n".utf8))
        require(competitive.issues.contains(.waitDisabled) && competitive.issues.contains(.nonYieldingRecursion), "competitive wait does not yield")
        print("PASS: independent key traces, stateful exec/vstr, recursion and competitive wait")

        let store = try ConfigProfileStore(homeURL: scratch.appendingPathComponent("home"), dialect: .cod2x)
        let imported = Data("// imported; set ignored 1\r\nseta cmdcur \"vstr tgl1\"\r\nseta tgl1 \"say old; set cmdcur vstr tgl2\"\r\nseta tgl2 \"say new; set cmdcur vstr tgl1\"\r\nbind k \"vstr cmdcur\"\r\n".utf8)
        var read = try store.create(id: "Main", displayName: "Main", document: .init(data: imported, dialect: .cod2x))
        try store.select(id: "Main", expectedActive: nil)
        let original = read.version
        let importedInspection = ConfigChains.inspect(key: "k", document: read.document)
        require(importedInspection.kind == .opaque && importedInspection.binding == Data("vstr cmdcur".utf8), "imported vstr stays opaque")
        require(read.document.serialized() == imported, "import roundtrip")
        let chains = [ConfigChain(id: "fps", key: "j", behavior: .settingCycle(name: "com_maxfps", values: ["125", "250", "333"])),
                      ConfigChain(id: "chat", key: "l", behavior: .commandCycle(steps: [["say one"], ["say two", "wait 2"]]))]
        let edit = try ConfigChains.prepare(chains: chains, read: read)
        require(edit.document.effectiveBind(key: "j") == "toggle com_maxfps 125 250 333", "prefer toggle")
        require(edit.runtimeVariables.count == 3, "two steps plus pointer occupy runtime slots")
        require(edit.issues.contains(.usesWait), "wait is visible")
        require(edit.document.serialized().starts(with: imported), "imported archive bytes unchanged by append")
        let entry = try ConfigChains.apply(edit, to: store)
        read = try store.read(id: "Main")
        require(read.version.chain == edit.script && read.version.config == edit.document.serialized(), "bind and script published together")
        require(entry.beforeChain == nil && entry.chain == edit.script, "history captures script")
        let basename = try ConfigStudioLaunch.chainBasename(profileID: "Main")
        require(try Data(contentsOf: store.homeURL.appendingPathComponent("main/" + basename)) == edit.script, "F3 filename contract")
        let owned = try ConfigChains.ownedChains(read: read)
        require(owned == chains, "owned model reload")
        let export = try ConfigChains.windowsExport(read: read)
        require(ConfigDocument(data: export, dialect: .cod2x).effectiveSetting(named: edit.runtimeVariables.last!) != nil, "portable helper archive")
        let target = scratch.appendingPathComponent("windows.cfg")
        try ConfigChains.exportWindows(read: read, to: target, home: store.homeURL)
        require(try Data(contentsOf: target) == export, "export file exact")
        require(try store.read(id: "Main").version == read.version, "export does not mutate loadout")
        let copy = try store.duplicate(id: "Main", as: "Copy", displayName: "Copy", expected: read.version)
        require(try ConfigChains.ownedChains(read: copy) == chains && copy.version.chain == edit.script, "copied chain still resolves")
        rejects("export into active home") { try ConfigChains.exportWindows(read: read, to: store.homeURL.appendingPathComponent("main/exports.cfg"), home: store.homeURL) }
        _ = try store.restore(id: "Main", snapshotID: entry.id, expected: read.version, revision: .before)
        read = try store.read(id: "Main")
        require(read.version == original, "undo restores imported bytes and absent script")
        require(!FileManager.default.fileExists(atPath: store.homeURL.appendingPathComponent("main/" + basename).path), "undo deletes owned script")
        print("PASS: opaque import preservation, toggle/cycle compile, F2 history/undo, independent Windows export")

        let quoted = [ConfigChain(id: "quote", key: "j", behavior: .commandCycle(steps: [["say \"hello world\""], ["say bye"]]))]
        let quoteEdit = try ConfigChains.prepare(chains: quoted, read: read)
        _ = try ConfigChains.apply(quoteEdit, to: store)
        let quoteRead = try store.read(id: "Main")
        rejects("archive cannot preserve embedded quotes") { _ = try ConfigChains.windowsExport(read: quoteRead) }
        rejects("outer splitter unsafe semicolon") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commandCycle(steps: [["say \"a;b\""], ["say bye"]]))], read: read)
        }
        rejects("held actions hidden by vstr") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commandCycle(steps: [["+attack"], ["say bye"]]))], read: read)
        }
        rejects("bind limit") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commands(["say " + String(repeating: "x", count: 1024)]))], read: read)
        }
        rejects("plus key event truncation") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commands(["+attack " + String(repeating: "x", count: 1004)]))], read: read)
        }
        rejects("token limit") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commands([String(repeating: "a ", count: 513)]))], read: read)
        }
        rejects("duplicate engine key aliases") {
            _ = try ConfigChains.prepare(chains: [.init(id: "a", key: "x", behavior: .commands(["say a"])), .init(id: "b", key: "0x78", behavior: .commands(["say b"]))], read: read)
        }
        var locked = read.metadata; locked.locks = ["bind:j"]
        _ = try store.update(id: "Main", document: quoteRead.document, expected: quoteRead.version, metadata: locked, summary: "Lock")
        let lockedRead = try store.read(id: "Main")
        rejects("bind lock") { _ = try ConfigChains.prepare(chains: chains, read: lockedRead) }
        rejects("stale edit") { _ = try ConfigChains.apply(edit, to: store) }
        let busy = try ConfigProfileStore(homeURL: store.homeURL, dialect: .cod2x, gameIsRunning: { true })
        rejects("running game") { _ = try ConfigChains.apply(edit, to: busy) }
        print("PASS: quotes, engine limits, hidden held actions, duplicate keys, locks and stale/running saves")

        // Failure occurs after helper publication, before bind publication.
        let recoveryStore = try ConfigProfileStore(homeURL: scratch.appendingPathComponent("recovery"), dialect: .cod2x)
        let recoveryRead = try recoveryStore.create(id: "Main", displayName: "Main", document: document("bind j \"say old\"\nbind k \"say imported\"\n"))
        let recoveryEdit = try ConfigChains.prepare(chains: chains, read: recoveryRead)
        let failing = try ConfigProfileStore(homeURL: recoveryStore.homeURL, dialect: .cod2x, writeFile: { data, url in
            if url.lastPathComponent == "config_mp.cfg" { throw ConfigProfileError.cannotPublish }
            try PrivateFile.write(data, to: url)
        })
        rejects("interrupted companion transaction") { _ = try ConfigChains.apply(recoveryEdit, to: failing, replaceConflicts: true) }
        let pending = try recoveryStore.inspectRecovery(id: "Main")
        require(pending.current.config == recoveryRead.data && pending.current.chain == recoveryEdit.script, "mixed state journal receipt")
        let recovered = try recoveryStore.read(id: "Main")
        require(recovered.version.config == recoveryEdit.document.serialized() && recovered.version.chain == recoveryEdit.script, "replay finishes bind and chain")
        require(try recoveryStore.history(id: "Main").count == 2, "replay history exactly once")
        rejects("line limit") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commands(["say " + String(repeating: "x", count: 4095)]))], read: recoveryRead)
        }
        rejects("set helper native limit") {
            _ = try ConfigChains.prepare(chains: [.init(id: "bad", key: "x", behavior: .commandCycle(steps: [["say " + String(repeating: "x", count: 950)], ["say bye"]]))], read: recoveryRead)
        }
        rejects("whole command buffer limit") {
            let many = (0..<100).map { ConfigChain(id: "n\($0)", key: String(format: "0x%02x", $0 + 128), behavior: .commandCycle(steps: [["say " + String(repeating: "x", count: 800)], ["say bye"]])) }
            _ = try ConfigChains.prepare(chains: many, read: recoveryRead)
        }
        var toggler = ConfigChainMachine(dialect: .cod2x)
        _ = toggler.execute(Data("set fps 250\n".utf8))
        _ = toggler.execute(Data("toggle fps 125 250 333\n".utf8))
        require(toggler.variables["fps"] == Data("333".utf8), "toggle next value")
        _ = toggler.execute(Data("toggle fps 125 250 333\n".utf8))
        require(toggler.variables["fps"] == Data("125".utf8), "toggle wraps")
        var resolved = ConfigChainMachine(dialect: .cod2x)
        _ = resolved.execute(Data("set once \"set once say done; vstr once\"\n".utf8))
        require(tokens(resolved.execute(Data("vstr once\n".utf8))).last == ["say", "done"], "self redefinition terminates without false cycle")
        let conflictEdit = try ConfigChains.prepare(chains: [.init(id: "change", key: "x", behavior: .commands(["say replace"]))], read: recoveryRead)
        require(conflictEdit.conflictingKeys.isEmpty, "empty key no conflict")
        let replace = try ConfigChains.prepare(chains: [.init(id: "change", key: "k", behavior: .commands(["say replace"]))], read: recovered)
        rejects("review required") { _ = try ConfigChains.apply(replace, to: recoveryStore) }
        print("PASS: copy, interrupted transaction replay, remaining limits, toggle and terminating self redefinition")

        let loopRead = try recoveryStore.create(id: "Loop", displayName: "Loop", document: document("seta loop \"vstr loop\"\n"))
        rejects("imported immediate loop must not be accepted") {
            _ = try ConfigChains.prepare(chains: [.init(id: "loop", key: "j", behavior: .commands(["vstr loop"]))], read: loopRead)
        }
        let execRead = try recoveryStore.create(id: "Exec", displayName: "Exec", document: document("bind j \"exec dependency\"\n"))
        rejects("portable export cannot omit required file") { _ = try ConfigChains.windowsExport(read: execRead) }
        let commented = ConfigChains.inspect(key: "j", document: document("bind j \"// comment; quit\"\n"))
        require(commented.kind == .opaque && tokens(commented.press) == [["quit"]], "comment tail remains live and opaque")
        print("PASS: transitive imported loop and missing portable dependency checks")

        let unarchived = try recoveryStore.create(id: "Unarchived", displayName: "Unarchived", document: document("set step \"say transient\"\nbind j \"vstr step\"\n"))
        rejects("portable helper would be lost on rewrite") { _ = try ConfigChains.windowsExport(read: unarchived) }
        var rebind = recovered.document
        try rebind.setBind(key: "l", binding: "say changed")
        _ = try recoveryStore.update(id: "Main", document: rebind, expected: recovered.version, summary: "Game rebind")
        let rebound = try recoveryStore.read(id: "Main")
        require(ConfigDocument(data: try ConfigChains.windowsExport(read: rebound), dialect: .cod2x).effectiveBind(key: "l") == "say changed", "export preserves actual live rebind")
        var actionLock = loopRead.metadata; actionLock.locks = ["action:quit"]
        _ = try recoveryStore.update(id: "Loop", document: document("seta act \"quit\"\n"), expected: loopRead.version, metadata: actionLock, summary: "Lock action")
        let actionRead = try recoveryStore.read(id: "Loop")
        rejects("resolved action lock") { _ = try ConfigChains.prepare(chains: [.init(id: "act", key: "j", behavior: .commands(["vstr act"]))], read: actionRead) }
        rejects("native unquoted high bytes disagree with F1") { _ = try ConfigChains.prepare(chains: [.init(id: "act", key: "j", behavior: .commands(["name é"]))], read: recoveryRead) }
        print("PASS: durable export, saved rebind and transitive locks")

        let session = try ConfigStudioSession(home: scratch.appendingPathComponent("session"), dialect: .cod2x)
        try session.initialize(settings: GameSettings())
        let sessionEdit = try ConfigChains.prepare(chains: chains, read: session.current!)
        try ConfigChains.apply(sessionEdit, to: session)
        let ready = try session.prepareLaunch(settings: GameSettings())!
        let plan = try GameLaunchPlan(settings: GameSettings(), dataPath: "/Synthetic", homePath: session.home.path, configStudio: ready)
        require(plan.arguments.suffix(2) == ["+exec", try ConfigStudioLaunch.chainBasename(profileID: session.current!.profile.id)], "real F3 launch appends chain after profile settings")
        rejects("reserved launch blocks chain change") { try ConfigChains.apply(sessionEdit, to: session) }
        try session.cancelLaunch()
        session.setDraft(session.current!.document)
        rejects("draft blocks committed advanced edit") { try ConfigChains.apply(sessionEdit, to: session) }
        try session.discardDraft()
        print("PASS: session ownership, F3 optional +exec startup order and draft conflict")

        // Emitted text, never player data, consumed by the production Cbuf oracle.
        try edit.script.write(to: scratch.appendingPathComponent("emitted.cfg"))
        try Data((edit.document.effectiveBind(key: "l")! + "\n").utf8).write(to: scratch.appendingPathComponent("trigger.cfg"))
        try quoteEdit.script.write(to: scratch.appendingPathComponent("quoted.cfg"))
        try Data((quoteEdit.document.effectiveBind(key: "j")! + "\n").utf8).write(to: scratch.appendingPathComponent("quoted-trigger.cfg"))
    }
}
