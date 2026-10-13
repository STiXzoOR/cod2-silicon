# WS46 — Config Studio design foundation

Date: 7 October 2026. Base: `84d054b`, `port/config-studio`. Scope: approved design mapping and reusable presentation primitives, not Config Studio business logic.

## Delivered

- [UI contract](../config-studio-ui-contract.md) maps Guided/Expert, Light, Binds List/Visual, device configuration, advanced binds, Directory, Player detail, Import review, Windows profiles, Check-up, History, server ready check and app Settings. It specifies native view decomposition, state/service responsibilities, autosave transaction boundaries, unknown/error/read-only states, narrow-window behavior and accessibility acceptance.
- `launcher/Theme.swift` adds the exact canvas `control`, `caution` and `compare` colors in both appearances. Custom glass surfaces and fallback buttons now omit material under Reduce Transparency; fallback button edges use the shared contrast-aware token.
- `launcher/ConfigStudioStyle.swift` adds presentation-only metrics, typography, `StudioStatusLabel`, `StudioBanner`, `StudioSettingLabel`, `StudioCompareValue`, and role-colored `StudioKeyCap` labels for native buttons. Status never depends on color alone. Unavailable keys are hatched and named. Selected keys use an outside outline that remains visible against brass Movement keys in dark appearance.

The current Model/Root/Core/Settings/Presentation/build structure was inspected without editing it. Existing `ContentPanel` and native controls remain the recommended building blocks. There are no sample loadouts, fabricated measurements, fake import operations or placeholder functional screens in this change.

## Source and decisions

Inspected the local saved `DesignRules.dc.html` and every Config Studio board under the canvas project. The hosted Claude artifact did not yield usable content, so the saved source was used without modification. Consulted Apple's primary [Materials](https://developer.apple.com/design/human-interface-guidelines/materials), [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility) and [Reduce Transparency](https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldreducetransparency) guidance.

Confirmed user decisions are recorded: launcher-owned chain files and Windows-profile import. The contract leaves network defaults unchanged pending upstream verification. Research facts override canvas examples: no editable FOV, no unrecorded “hits” claims in match-summary copy, no invented rule limits/player permissions, no suggestion presented as measured without evidence. Live game-file updates use the spec's watcher semantics, correcting the board's “when it closes” banner sentence.

## Focused verification

Both passed with SDK 27.0, Swift 6 strict concurrency, warnings as errors, and deployment target macOS 13:

```sh
xcrun swiftc -typecheck -swift-version 6 -strict-concurrency=complete -warnings-as-errors -target arm64-apple-macos13 launcher/Theme.swift launcher/ConfigStudioStyle.swift
xcrun swiftc -typecheck -DCOD2_LIQUID_GLASS -swift-version 6 -strict-concurrency=complete -warnings-as-errors -target arm64-apple-macos13 launcher/Theme.swift launcher/ConfigStudioStyle.swift
```

An isolated temporary `ImageRenderer` harness compiled and rendered the actual SwiftUI primitives with bundled fonts, in dark/light and standard/increased-contrast palettes. All four renders were inspected. A dark-mode selected-key visibility defect found during inspection was fixed and re-rendered. Local review images are `/tmp/config-studio-{dark,light}-{standard,contrast-palette}.png`; the temporary harness is `/tmp/ConfigStudioStyleReview.swift`. Neither is part of the shipped product or a functional-screen test.

Targeted sRGB contrast calculations passed: caution text on panel 10.13:1 dark / 7.19:1 light; comparison text 8.24:1 / 5.71:1; six key-role text/background pairs range from 4.63:1 to 9.99:1. This checks these solid-color pairs, not arbitrary artwork or native glass composites. `git diff --check` passed.

No engine build, game, benchmark, commit, push or private research/asset copy was performed. Reduce Transparency paths were source-inspected and typechecked; the temporary renderer cannot set SwiftUI's read-only accessibility environment properties. It used palette overrides for contrast, not a full system accessibility simulation. Native glass rendering, interactive keyboard/VoiceOver traversal and complete screen snapshots belong to integration's autonomous checks. There is no requirement for the user to conduct a manual playtest.

## Integration handoff

Add `ConfigStudioStyle` to the build script's explicit view source list. The Foundation F3 unit consumes the primitives in its real Settings and History views. It owns routing/shortcuts in Model/RootView/Main, store bindings and lifecycle, Settings migration, profile launch semantics, and actual catalog resources. Later units consume the same status/keycap/compare components for Binds, imports, Check-up and Servers. The UI contract lists the precise mapping and required state for each.

This bounded unit is complete. Full Config Studio implementation and screen acceptance remain separate engineering work; typechecking shared presentation code is not evidence that those features exist.
