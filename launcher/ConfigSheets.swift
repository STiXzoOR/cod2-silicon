import SwiftUI

// Owner: ship-configs. The loadout sheets the Loadout menu and Configs open (AppSheet's ship-configs
// group): New Loadout and Rename, on the shared SheetScaffold. Each names its main action for what it
// does and, when it can't proceed, says why beside the buttons.

struct NewLoadoutSheet: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @State private var name = ""
    @State private var copy = false
    @State private var error: String?
    init(model: LauncherModel) { self.model = model; studio = model.studio }

    var body: some View {
        let source = studio.current?.profile.displayName
        let reason = LoadoutName.problem(name, existing: studio.profiles.map(\.displayName), readOnly: studio.isReadOnly)
        SheetScaffold(icon: .symbol("plus.square.on.square"), title: "New loadout",
                      message: "A loadout is its own Call of Duty 2 player profile. It isn’t used for your next game until you choose it.",
                      actions: [.cancel { model.dismissSheet() },
                                .primary("Create Loadout", enabled: reason == nil) { create(copying: copy && source != nil) }]) {
            VStack(alignment: .leading, spacing: 12) {
                // A sheet that asks for a name shows one field across its width, like every other sheet's field.
                LauncherTextField("Loadout name", text: $name, prompt: "Name, for example Scrim", shape: .capsule, autofocus: true)
                FormGroup {
                    FormRow("Start from") { PopUpButton("Start from", selection: $copy, options: startOptions(source)) }
                }
                if let error { Banner(.error, title: "Couldn’t create the loadout", message: error) }
            }
        } note: {
            if let reason { Text(reason) }
        }
        .onChange(of: name) { _, _ in error = nil }
    }

    private func startOptions(_ source: String?) -> [(value: Bool, title: String)] {
        var options: [(value: Bool, title: String)] = [(value: false, title: "The game’s defaults")]
        if let source { options.append((value: true, title: "A copy of \(source)")) }
        return options
    }

    private func create(copying: Bool) {
        if studio.createLoadout(named: name, copyingCurrent: copying) { model.dismissSheet(); return }
        // Said here, where the person is looking, rather than on the page behind the sheet.
        error = studio.failure?.message ?? "Try again."
        studio.failure = nil
    }
}

struct RenameLoadoutSheet: View {
    @ObservedObject var model: LauncherModel
    @ObservedObject var studio: ConfigStudioModel
    @State private var name: String
    init(model: LauncherModel) {
        self.model = model; studio = model.studio
        // Starts from the current name, set before the field first appears (no onAppear race with typing).
        _name = State(initialValue: model.studio.current?.profile.displayName ?? "")
    }

    var body: some View {
        let current = studio.current?.profile
        let others = studio.profiles.filter { $0.id != current?.id }.map(\.displayName)
        let unchanged = name.trimmingCharacters(in: .whitespacesAndNewlines) == current?.displayName
        let reason = current == nil ? "This loadout no longer exists."
            : LoadoutName.problem(name, existing: others, readOnly: studio.isReadOnly) ?? (unchanged ? "Type a new name." : nil)
        SheetScaffold(icon: .symbol("pencil"), title: "Rename “\(current?.displayName ?? "loadout")”",
                      message: "The new name shows in CoD2 Unbound. The game’s own profile menu keeps showing the folder name, \(current?.id ?? "").",
                      actions: [.cancel { model.dismissSheet() },
                                .primary("Rename", enabled: reason == nil) {
                                    studio.rename(name, undo: model.launcherWindow?.undoManager); model.dismissSheet()
                                }]) {
            // A taken name is said beside the buttons, and Rename stays off; the field keeps its usual edge.
            LauncherTextField("Loadout name", text: $name, shape: .capsule, autofocus: true)
        } note: {
            if let reason { Text(reason) }
        }
    }
}

/// A loadout's display name: something to call it, not already another loadout's name.
enum LoadoutName {
    static func problem(_ name: String, existing: [String], readOnly: Bool) -> String? {
        if readOnly { return "Configs are read-only while Call of Duty 2 is running." }
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty { return "Type a name for the loadout." }
        if existing.contains(where: { $0.caseInsensitiveCompare(trimmed) == .orderedSame }) { return "You already have a loadout called “\(trimmed)”." }
        return nil
    }
}
