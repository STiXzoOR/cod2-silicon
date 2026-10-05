# WS25 design v2

The user's review replaces the first visual direction: use CoD2 atmosphere with
Apple's current functional/content layering, not landscape branding or marketing
copy. Continue autonomously on `port/launcher`; preserve all lifecycle, identity,
privacy, macOS 13, standalone packaging and legacy-build guarantees.

## Direction

Use standard `NavigationSplitView`, sidebar `List`, searchable content and a
system toolbar. On SDK/runtime 26+ the system supplies navigation/control glass;
custom floating controls use regular glass, a shared container and stable IDs.
Images and information cards remain content, with concentric shapes and 8-point
spacing. One oxblood Deploy action is emphasized; other controls are neutral.
SF carries UI text; DIN Condensed is restricted to section labels and numerals;
American Typewriter appears only in a short dispatch. An original vector stencil
wordmark and the same cut-through 2 used in icon A replace the mountain logo.

A custom glass rail would perpetuate the mismatch with Apple apps. Covering all
content with glass would violate the HIG's hierarchy. Standard navigation plus
rich, quiet content is the selected approach.

## Work and ownership

1. Read current Apple HIG JSON/articles and WWDC25 transcripts directly. Verify
   API declarations/availability in the installed SwiftUI/SwiftUICore/AppKit SDK.
2. Independently implement bounded ZIP/IWI decoding with synthetic tests; load
   only map loadscreens from a player's selected install. App Support owns the
   decoded cache. Never publish or serialize game-derived imagery into logs.
3. Draw original layered icon A/B/C, verify `.icon` command-line tooling, render
   six system appearances, default A. Root integrates packaging only after proof.
4. Root refactors views/design, adds private/fallback snapshot modes, renders
   twice, critiques against Apple's Mac examples and fixes observed details.
5. Run launcher tests, both builds, ABI, standalone ZIP/installer audits and
   scratch-home menu/Toujane/return smoke; commit the final report update.

The decoder and icon tooling are independent delegated tasks with exclusive
file ownership. Root owns Model/Design/Views/Main, build integration and report.
No renderer/server/shared-header or sibling-worktree changes are authorized.

## Review and privacy

Render light/dark 1440×900 review PNGs under ignored
`output/ws25/screens-v2/{no-data,with-data}/`, plus 2× verification copies.
`no-data` uses only original art. `with-data` is private licensed content and
must never be committed, bundled, uploaded or emitted into conversation tools.
Keep keys empty and server rows fake. Original-only images can be emitted for
visual critique; private images are reviewed locally through layout/contrast
checks without transporting their pixels. Keep both iteration sets as evidence.

The key explanation must say it is stored locally and used by the game for
normal CD-key authentication when joining servers, and isn't sent to the
launcher update service. Server space below real results is an intentional
empty region, not fake grey rows or an indefinite loading skeleton.

HIG sources: materials, layout, color, typography, app icons, sidebars, toolbars,
buttons and navigation/search at `developer.apple.com/design/human-interface-guidelines/`.
WWDC25 source sessions: 219, 356 and 323. Exact applied quotes, URLs, SDK evidence,
two-pass render fixes and acceptance limits go in the workstream report.
