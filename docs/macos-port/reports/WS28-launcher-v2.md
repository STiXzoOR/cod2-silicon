# WS28 — launcher v2 and icon G

Branch: `port/launcher-v2`. Base: `port/launcher` at `4d56e0b`. Worktree:
`/Users/stix/Projects/cod2-native-wt/launcher-v2`. Verified on the PLAN host
(Apple M6, macOS 27.0.1, Xcode 27.0, Swift 6.4, macOS 27.0 SDK) with existing tools only.

## Result

The SwiftUI launcher now follows the approved Claude Design prototype on every
screen: Home, Servers, Settings ("Field manual"), the three-step Setup and the Library.
About is a new screen designed in the same language. The structure is a standard
`NavigationSplitView` with a sidebar, system toolbars and Liquid Glass on macOS
26 and later. Content stays opaque, and macOS 13–25 get a material fallback.
The app icon is icon G, built as an Icon Composer document and compiled by
`actool` into a layered Liquid Glass icon, an ICNS fallback and the bundle's
brass/olive accent colour.

WS25's architecture is unchanged. The outer launcher and the nested
`Contents/Helpers/CoD2 Game.app` behave as before: lifecycle, Dock hand-off,
force-quit reconnect, `cod2x://` delivery through Apple events, in-memory
passwords, crash reports, native setup, the validated config writer, and the
master/UDP discovery code. The deployment target stays macOS 13.

Commits on this branch, oldest first:

| Commit | Change |
| --- | --- |
| `50f4fc5` | Bundle the two OFL typefaces, pinned by SHA-256; CREDITS |
| `52d1cf3` | Icon G as `CoD2 Silicon.icon`; actool packaging for both apps; static fallback |
| `9d47e2e` | Tested presentation logic (maps, badges, cm/360, release notes, media, key tag, player name) |
| `00c428b` | Accent colour compiled with the icon (`NSAccentColorName`) |
| `749ad62` | The redesigned views, model and theme |
| `304e998` | Test-only review harness, contrast gate and extended `tests/launcher/run.sh` |
| `795f342` | Fixes from an independent review of the diff |
| `5187bd9` | Fixes for three pre-existing cold-link ordering bugs in the Dock hand-off |
| `00d017d` | Verbatim licence texts exempt from whitespace checks; the bundler skips dotfiles |
| `135aeb6` | This report (first version) |
| `9afdad3` | Icon G: top-left stencil bridge removed so the 2 is one shape at Dock sizes |
| `1b99cd7` | Real player name from the engine's active profile; server rows fade above the connect bar |

## Prototype fidelity, screen by screen

Measurements were read from the prototype's HTML/CSS and kept: window
coordinates (content at x = 300, 280 and 264 as drawn), type sizes and tracking
(converted from `em`), radii (22 panels, 20 groups, 16 rows and crates, 26
details panel, 30 setup card), the colour tokens, and every piece of copy except where
noted. The prototype was rendered with a small local runtime for its `dc`
template format, giving reference images in `output/ws28/prototype/`. Every
review image was compared side by side with them.

**Home.**
- The hero uses the design's own vector scene (desert dusk in dark, Normandy
  day in light) drawn from its path data. A Big Shoulders Stencil 132-point map
  title (0.86 line height, 0.02 em tracking) sits over it, with the eyebrow
  "LAST DEPLOYMENT · <REGION>".
- The meta line carries the server name in its colour codes, the mode, players
  and ping.
- Deploy (prominent) and Find a server (glass), with the "⌘ Return to deploy"
  hint.
- Glass status chips for the frame cap, resolution and mode, and shader state.
- Recent servers and Dispatches panels, and the player dog tag in the sidebar.
- Deploy rejoins the last server, or opens the game menu when there is none.
  The no-history state reads "READY FOR DEPLOYMENT · COD2 SILICON".
- With game data, the hero and thumbnails show the player's own loading screen
  for the map.

**Servers.**
- The glass toolbar holds search, the All / Stock 1.3 / CoD2x 1.4 segmented
  filter, Hide empty, Hide full, the count and Refresh (⌘R).
- Rows show the map thumbnail, the colour-coded name, the map · mode ·
  version badge, stencil player numerals (red when full, dimmed when empty),
  four-bar ping and a favourite star. The selected row gets the brass outline.
- Column headers sort by name, players or ping.
- The floating glass details panel shows the art header, the address in
  Courier Prime, version, mod, frame cap and access, a password field when
  needed, Deploy, the favourite toggle and the roster.
- The "NO CONTACT" empty state, and a glass direct-connect capsule that floats
  over the list with the system scroll-edge effect.
- WS25's discovery, queries and cache are untouched.

**Settings ("Field manual").**
- Ammo-crate frame-cap presets 333/250/125, plus a custom value edited in place.
- Display: resolution (to 6016 × 3384, labelled 4K/5K/6K), Exclusive /
  Borderless / Native Space with the mode hint (Native Space says it is required
  for Game Mode), plus Window and Vertical sync rows.
- Mouse and sound: raw mouse switch, sensitivity, DPI with a live cm/360
  readout in stencil numerals, and volume.
- Graphics: Renderer Metal 4 / Classic OpenGL. This branch has no `r_renderer`
  dvar, so Metal 4 is unselectable and labelled. Render scale, MetalFX and HDR
  are dimmed and badged "0.3", matching `docs/ROADMAP.md`. Anisotropic
  filtering is wired to the engine's `r_anisotropy` (2×–16×, the dvar's range).
- Advanced dvars in Courier Prime accept `dvar value` (as typed in the console)
  or `dvar=value`.
- A Game data row offers "Set Up Again…".
- Save is the toolbar's one prominent action (⌘S), with "Applies on next launch"
  beside it.
- WS25's validated config writer and reserved-name rules are unchanged.

**Setup.**
- Full-bleed scene, "REPORTING FOR DUTY", the three-step indicator and a glass
  card.
- Step 1 shows the path in Courier Prime and "All 16 game archives present ·
  Version 1.3". It also has a not-found variant with Choose Folder…
- Step 2 is the CD key. Each typed character stamps onto the steel dog tag.
  The field groups in fours, live hints count down, and a full key shows
  "Checksum verified." or a mismatch, using WS21's native CRC. The key is stored
  mode 600, and the keystroke buffer is cleared after saving.
- Step 3 shows real extraction progress, "N / 834". It counts the payloads the
  native extractor writes into its private staging folder; there's no fake
  timer. An approximate-shaders path offers "Choose Mac Copy…".
- Then Deploy.

**Library.** Demos rows show the map thumbnail, the file name in Courier
Prime, a map hint from the name, a date stamp, the size and Play. Screenshots
show as a three-column grid of real thumbnails, made with ImageIO and
downsampled, with a stamped date. The prototype shows a demo's length. `.dm_1`
files have no cheap length field, so the row shows the file's date instead.

**About** (new): an eyebrow "SERVICE RECORD", icon G at 148 pt, a 96-point
stencil wordmark, the version line in Courier Prime and the tagline over a
Normandy-at-dusk band. Below are three equal panels: Updates (check on demand,
notification only), Credits and Licence. The app menu's "About CoD2 Silicon"
opens it.

Intentional differences from the prototype:

1. The sidebar is the system `NavigationSplitView` sidebar. In the harness
   window macOS 27 draws it full height, flush with the window edge, rather
   than as the prototype's inset rounded panel.
2. Sidebar icons follow the app accent (all brass in dark, olive in light),
   not just the selected row. The HIG says sidebar icons should show the accent
   colour people choose; a fixed palette would override that choice.
3. The Servers toolbar is three system glass groups (search; filters; count and
   Refresh), not one long capsule. The HIG says to aim for at most three groups.
4. Settings adds Window and Vertical sync, which WS25 already supported. The
   sidebar always lists favourites and the dog tag, where the prototype varies
   them per board.
5. CD-key privacy copy is corrected to what the engine does. It sends a one-way
   hash (`getKeyAuthorize`), never the key: "Stored only on this Mac, in a
   file only your account can read. Like the original game, joining a server
   sends the authorization service a one-way hash of it; the key itself never
   leaves this Mac."
6. The prototype's dog tag clipped its last line. It is set at 12 points so it
   fits.

## Apple HIG applied

Read directly from Apple's HIG pages through their JSON endpoints on 2026-10-04:

- [Materials](https://developer.apple.com/design/human-interface-guidelines/materials): "Liquid Glass
  forms a distinct functional layer for controls and navigation", and "Don't use
  Liquid Glass in the content layer."
  - Glass covers the sidebar, the toolbars, buttons, the status chips (they are
    buttons into Settings), the floating server details panel, the
    direct-connect bar, the setup card and the notice banner.
  - Panels, rows, crates, groups and art are opaque content.
  - Effects are limited to important functional elements, and a
    `GlassEffectContainer` groups the chips and the Deploy/favourite pair.
- [Sidebars](https://developer.apple.com/design/human-interface-guidelines/sidebars):
  - "Extend visually rich content beneath the sidebar … by applying a
    background extension effect." The hero uses `backgroundExtensionEffect()`.
  - "By default, sidebar icons use your app's accent color … make sure your
    sidebar icons display the color people choose." This drove difference 2
    above.
- [Toolbars](https://developer.apple.com/design/human-interface-guidelines/toolbars):
  - "Use the .prominent style for key actions … put it on the trailing side":
    Settings' Save.
  - "Aim for a maximum of three" groups.
  - "Don't title windows with your app name": the title is removed from the
    toolbar.
  - Standard components carry concentric radii.
- [Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons)
  and [Color](https://developer.apple.com/design/human-interface-guidelines/color):
  - The accent colour is applied "to the background in prominent buttons". One
    prominent action per view: Deploy, Continue/Deploy, Save, Connect.
  - "Refrain from adding color to the background of multiple controls":
    secondary actions are neutral glass.
  - The app accent colour (macOS 11+, used when the system accent is
    multicolor) is set through `NSAccentColorName`.
  - Custom colours have light, dark and increased-contrast variants
    (`colorSchemeContrast`).
- [Layout](https://developer.apple.com/design/human-interface-guidelines/layout):
  - Safe areas are respected. Content is aligned to the design's grid in window
    coordinates, measured from the detail column's real origin.
  - The background extension effect is used where a sidebar would cover art.
  - Spacing follows an 8-point rhythm, with radii concentric between panel and
    row.
- [Typography](https://developer.apple.com/design/human-interface-guidelines/typography):
  - SF for all UI text. The custom faces are limited to display titles,
    eyebrows and numerals (Big Shoulders Bold/ExtraBold) and timestamps,
    addresses and file names (Courier Prime). No light weights.
- [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility)
  and [Motion](https://developer.apple.com/design/human-interface-guidelines/motion):
  - VoiceOver labels on rows (name, map, mode, version, players, ping, lock),
    chips, crates, steps, the dog tag and the cm/360 readout.
  - Selected traits, headers, and `updatesFrequently` on the key hint.
  - Keyboard access: ⌘↩ Deploy, ⇧⌘↩ game menu, ⌘2/⌘3/⌘, pages, ⌘F search,
    ⌘R refresh, arrow keys in the server list, Return for the details Deploy,
    and ⌘S Save.
  - Every animation uses one spring and checks Reduce Motion. Reduce
    Transparency makes the fallback surfaces opaque; the system adapts glass.
- [App icons](https://developer.apple.com/design/human-interface-guidelines/app-icons):
  layered vector artwork, a gradient background in Icon Composer, clearly
  defined edges, and appearance variants annotated in the document.

## APIs used, verified against the SDK

Every name was checked in the installed macOS 27.0 SDK's
`SwiftUI.swiftinterface` and `SwiftUICore.swiftinterface` before use.

| API | Declared availability | Use |
| --- | --- | --- |
| `glassEffect(_:in:)`, `Glass.regular.tint(_:).interactive(_:)` | macOS 26.0 | Floating panels, chips, bars and banner (`GlassSurface`) |
| `GlassEffectContainer(spacing:)` | macOS 26.0 | Chips; Deploy plus favourite |
| `.buttonStyle(.glass)` / `.glassProminent` | macOS 26.0 | Secondary and prominent actions |
| `backgroundExtensionEffect()` | macOS 26.0 | Home and About hero under the sidebar |
| `safeAreaBar(edge:spacing:content:)` | macOS 26.0 | Direct-connect bar insetting the list (rows fade out above it) |
| `scrollEdgeEffectStyle(_:for:)` | macOS 26.0 | Evaluated for the bar (probe); the automatic style under toolbars was kept, per the HIG's "prefer the automatic scroll edge effect style" |
| `ToolbarSpacer(.flexible)`, `sharedBackgroundVisibility(.hidden)` | macOS 26.0 | Trailing groups; the plain save note |
| `toolbar(removing: .title)` | macOS 15.0 | Untitled toolbar (`navigationTitle("")` before) |
| `sidebarRowSize(.large)`, `controlSize(.extraLarge)`, `buttonBorderShape(.circle)` | macOS 14.0 | Sidebar rows, hero buttons, favourite button |

The glass API calls sit behind `#if COD2_LIQUID_GLASS` (defined for SDK 26+)
and `if #available(macOS 26.0, *)`. Toolbar variants are chosen at the view
level, because `if #available` inside a toolbar builder is only safe from macOS
14.5. `tests/launcher/run.sh` typechecks all views without `COD2_LIQUID_GLASS`
under Swift 6 strict concurrency with warnings as errors.

## Typefaces and licensing

Big Shoulders Stencil Display (variable `wght` font; Bold 700 and ExtraBold 800
instances by PostScript name) and Courier Prime Regular/Bold come unmodified
from `github.com/google/fonts` at commit `9710da1eacb3be272583c3224dcb70f9da6eadbb`.

- `scripts/fetch-launcher-fonts.sh` records the upstream paths and SHA-256 of
  all five files (three fonts, two `OFL.txt`). `--check` verifies the
  committed copies, and fails on tampering (tested).
- Both are SIL OFL 1.1 with no Reserved Font Name. Designers, from METADATA.pb:
  Patric King and Alan Dague-Greene.
- They ship in `Contents/Resources/Fonts` with their licence texts and register
  through `ATSApplicationFontsPath`. Development builds register them with
  `CTFontManagerRegisterFontsForURL`.
- `CREDITS.md` lists both families and the Icon Composer tools.
- If the files are missing, the code falls back to condensed SF and SF Mono.

## Icon G: pipeline and renditions

`tools/cod2x/icon-source/CoD2 Silicon.icon` holds `icon.json` and seven flat SVG
layers written by `layers.py`. The script is reproducible: the worn-paint chips
are seeded, and the design's 100–924 tile is rescaled to the full 1024 canvas.

- `fill-specializations`: an olive gradient from `#6f7a44` to `#323819`, and an
  olive-black Dark gradient from `#262a1b` to `#0c0d08`.
- Groups, front to back:
  1. **Stencil 2:** layer-colour shadow at 0.5, no translucency. Dark uses
     deeper paint, and mono uses a solid white fill.
  2. **Raised star:** neutral shadow at 0.5. Dark and mono use darker facets.
  3. **Plate:** bevel line and rivets, translucency 0.3, no shadow.
- No glass is baked into the layers.
- Schema keys were checked against strings in Icon Composer's frameworks. A
  key and its `-specializations` form must not coexist: during development, the
  dark variants were silently ignored until the plain keys were removed. Pixel
  sampling confirmed the fix.

Renditions were exported with `ictool --platform macOS --width 1024 --height
1024 --scale 1`. Each was reviewed over three iterations and at 128, 64, 32 and
16 points:

| Rendition | File |
| --- | --- |
| Default | `output/ws28/icon/CoD2 Silicon-Default.png` |
| Dark | `output/ws28/icon/CoD2 Silicon-Dark.png` |
| ClearLight | `output/ws28/icon/CoD2 Silicon-ClearLight.png` |
| ClearDark | `output/ws28/icon/CoD2 Silicon-ClearDark.png` |
| TintedLight | `output/ws28/icon/CoD2 Silicon-TintedLight.png` |
| TintedDark | `output/ws28/icon/CoD2 Silicon-TintedDark.png` |
| All six | `output/ws28/icon/CoD2 Silicon-all-renditions.png` |

`scripts/compile-launcher-icon.sh` runs `xcrun actool "CoD2 Silicon.icon"
launcher/Resources/Accent.xcassets --compile … --platform macosx
--minimum-deployment-target 13.0 --app-icon "CoD2 Silicon" --accent-color
AccentColor`.

- It emits `Assets.car` with no warnings. That holds `IconImageStack`/`IconGroup`
  renditions for Aqua, DarkAqua and tintable appearances, flattened sizes and
  the accent colour.
- It also emits `CoD2 Silicon.icns` and a partial plist with
  `CFBundleIconFile`, `CFBundleIconName` and `NSAccentColorName`.
- `make_macos_app.py` installs these into both the launcher and the nested game
  helper before signing.
- `actool` ships only with Xcode. A Command Line Tools-only build gets exit code
  3, and then `tools/cod2x/app_icon.swift` draws icon G statically into an
  ICNS. Both paths were tested by packaging with each developer directory.
- The sidebar mark draws icon G natively, with quieter star facets at 34 points.
- The superseded concepts A–C were removed.

## Review renders and how they were captured

Glass draws through the window server, so `cacheDisplay` renders it blank (I
confirmed this). The test-only `LauncherSnapshots` harness instead hosts the real
`LauncherRoot` in a titled, full-size-content window configured like the app's
hidden-title-bar window, with toolbar bridging. It then captures its own window
with `CGWindowListCreateImage`, which needs no screen-recording permission for
the app's own windows.

The session's screen was locked, so the host app couldn't activate. The
harness window therefore overrides `isKeyWindow` and the private
`_hasActiveAppearance` family, so controls draw as focused. That override
exists only in `tests/launcher/Snapshots.swift`; the shipping binary contains
no private selectors.

Like an app bundle, the harness embeds an `Info.plist` naming `AccentColor` and
loads the compiled `Assets.car` beside it. SwiftUI's `Window` scene itself never
opens a window while the session is locked (probed), hence the hosting window.

| Set | Path | Content |
| --- | --- | --- |
| Final, fallback art | `output/ws28/no-data/*.png` | 13 screens × dark/light at 1440×900, real glass |
| Material fallback | `output/ws28/fallback/*.png` | The macOS 13–25 styling of the same screens |
| Private game art | `output/ws28/with-data/*.png` | Player's own loading screens. Local only; never committed or uploaded |
| 2× crispness | `output/ws28/scale2/home-{dark,light}@2x.png` | Offscreen 2880×1800 of Home (material styling) |
| Prototype | `output/ws28/prototype/*.png` | The approved design, rendered for comparison |
| Iterations | `output/ws28/pass1` … `pass4` | Earlier passes kept as evidence |

Screen names: `home`, `home-first`, `servers`, `servers-empty`, `settings`,
`library-demos`, `library-screenshots`, `about`, `setup-data`,
`setup-data-missing`, `setup-key`, `setup-shaders`, `setup-ready`. Fake servers
use the RFC 5737 documentation ranges. Player names and the CD key are invented.

`with-data` images were not opened in any conversation tool, so the licensed
pixels never left the Mac. Their legibility was checked numerically instead.
`LauncherSnapshots --data … --contrast` renders each stock map's hero backdrop
offscreen without text. It then compares the 98th-brightest (dark) or
2nd-darkest (light) background pixel in the eyebrow, title, meta and hint boxes
with those exact text colours, and keeps only numbers. The final result is
**30 heroes (15 maps × 2 appearances), worst 4.66:1, all at least 4.5:1**
(`output/ws28/with-data/contrast.json`).

### What changed after each review pass

- **Pass 1 → 2:**
  - The harness window grew to 952 points because the hosting controller sized
    it to the content; it is now pinned at 1440×900.
  - The hero was centred in a `.top`-aligned background, shifting the art 116
    points. It is now a top-leading window-space slice with
    `backgroundExtensionEffect`.
  - A root `.tint` coloured the secondary glass buttons. The accent now comes
    from `NSAccentColorName`, and only prominent actions are tinted.
  - Trailing toolbar items appeared on the leading side (`.primaryAction` is
    leading on macOS).
  - The upcoming "0.3" badges overlapped values; they are now inline.
  - The save capsule turned entirely brass and covered the eyebrow. The note now
    sits on the bar, with a separate prominent Save.
  - About was top-heavy; it gained a hero band and three equal panels.
  - The disabled Connect was invisible. It is now enabled and asks for an
    address.
  - The key field was auto-selected in renders.
  - The light desert town was too heavy.
- **Pass 2 → 3:**
  - Explicit flexible spacers now pin the trailing groups.
  - The sidebar icons were system blue in the harness. The harness now resolves
    the accent the way a bundle does.
  - The Servers toolbar went from four groups to three.
  - Increase Contrast and Reduce Transparency variants were added.
- **Pass 3 → 4:**
  - About's tagline crossed the church spire. The composition was shifted and
    the tagline capped at 520 points.
  - The sidebar mark's star competed with the numeral at 34 points, so it was
    quietened.
- **Contrast work on real art:**
  - Linear-light scrim maths underestimated what sRGB compositing does, which
    left light mode at 2.90:1. Both scrims now model gamma-space blending.
  - Scrims are sized from the dimmest text (the brass eyebrow and hint in dark,
    the brown eyebrow in light) and from the worst text box.
  - The result went from 2.90 → 3.47 → 4.23 → 4.66 (passing).
- **Final review:**
  - The data step's icon was blank (`folder.badge.checkmark` is not an SF
    Symbol). Both setup glyphs are now drawn from the prototype's own paths.
  - Every other symbol name was verified to exist.

## Review and bugs fixed along the way

A separate read-only reviewer checked the branch diff against WS25's
behaviour. Fixed in `795f342`:

1. The details panel's Deploy was the window's default button. Return would
   then fire it from the search or direct-connect fields, joining the
   selected server instead of the typed address. Instead:
   - the server list deploys on Return only while it has focus (macOS 14+);
   - double-click on a row deploys;
   - so does a VoiceOver Deploy action.
2. "About CoD2 Silicon" and the Deploy command could bypass setup. Both now
   respect `onboard`.
3. Xcode 16's `actool` exists but can't compile `.icon`. The icon script now
   reports it unavailable for SDKs before 26, so packaging uses the static
   icon there.
4. The CD key field is visible, by design, so the tag can be stamped. It now
   turns on secure event input while focused, as `NSSecureTextField` does, so
   other processes can't observe the keystrokes.

Re-running WS25's `cold_url.py` with its scratch home reused, so setup was
already complete, exposed three ordering bugs that predate this branch. A fresh
home hides them. lldb traces of my own test process established the
call order. Fixed in `5187bd9`:

1. LaunchServices delivers a cold link before `applicationDidFinishLaunching`.
   The link started the game, and the delegate then made the launcher a
   regular, active app again: a Dock tile beside the game. WS25's original
   launcher shows policy 0 throughout. Links that arrive before launch finishes
   now start the game from `boot()`.
2. The one-shot `--play`/link request wasn't consumed, so a later setup
   completion could start a second game.
3. `setActivationPolicy(.regular)` after the game was sometimes refused (it
   returned false) while AppKit already reported `.regular`. The launcher now
   steps through accessory and retries; regular returns within about 0.5 s.

`cold_url.py` now waits for each policy, since another process sees it
asynchronously, and asserts that exactly one game starts.

## Orchestrator review of pass 3

1. **Servers' last row showed through the direct-connect bar.**
   - A probe compared the default, soft and hard scroll-edge styles
     ([Scroll views](https://developer.apple.com/design/human-interface-guidelines/scroll-views))
     with a fade.
   - Soft and default still let the half row read through the glass. Hard hid
     it, but sliced the row above through its text and added a footer-like
     band.
   - The list now fades out over its last 36 points above the bar, on every
     macOS version. No row is sliced at rest, nothing sits under the glass, and
     `safeAreaBar`'s inset lets the last row clear the bar at the end of the
     list. This matches the prototype, where the list ends above the bar.
   - The automatic edge effect remains under the toolbars.
2. **The sidebar looked like an opaque panel.**
   - It is the standard sidebar's real Liquid Glass, captured through the
     window server, not offscreen.
   - Sampling the dark Home render matches the prototype's sidebar tones at
     every height, for example `#3d3829` against the prototype's `#353223` where
     the hero lies beneath. The `backgroundExtensionEffect` mirror shows through.
   - A probe with a vivid striped hero shows the same sidebar plainly
     translucent. AppKit's own `NSSplitViewController` sidebar is also full
     height in this macOS 27 window.
   - HIG ([Color](https://developer.apple.com/design/human-interface-guidelines/color))
     notes that "Liquid Glass appears more opaque in larger elements like
     sidebars."
   - The inset, rounded panel in the prototype is not what the system draws in
     the harness window. Check the shipping window unlocked (see What's left).
3. **The profile name.** The dog tag reads the in-game name the way the engine
   loads it:
   - `main/players/active.txt` gives the active profile;
   - its `players/<profile>/config_mp.cfg` is read first, then
     `main/config_mp.cfg`;
   - the result is "Player" when no name is set.

   Profile names are sanitized (no path separators or `..`) and unit-tested.
   Only review renders use an invented name.
4. **Icon.**
   - At 32–64 px the top-left stencil bridge read as a separate tick. Five
     variants were compared at 64, 32 and 256 px: narrower, moved to the crown,
     both, and removed.
   - Removing that bridge keeps the 2 one shape at every size. The stencil
     character stays in the diagonal and baseline bridges.
   - The change is applied to the `.icon` layers, the static fallback and the
     sidebar mark. All six renditions were re-rendered.

## Tests and gate evidence

All logs are under ignored `output/ws28/gate/`. Builds and tests ran under
`taskpolicy -b nice -n 19`. Another agent held the benchmark lock during this
work, and no performance was measured.

| Check | Result |
| --- | --- |
| `sh tests/launcher/run.sh` at `1b99cd7` | **PASS** (`launcher-tests-1b99cd7.log`). Core and wire fixtures, the new presentation tests, the Network.framework UDP test, the IWI/IWD artwork tests, font checksums, the pre-26 typecheck, and 26 screens plus the fallback rendered and checked |
| New unit tests (`CoreTests`) | cm/360 (800 DPI × 5 = 10.39 cm, invariance, invalid input), both advanced-dvar forms and reservations, settings and library migration from older JSON, the release-notes parser against a synthetic fixture (drafts, Markdown, CRLF, truncation, link host, newest stable), map names and regions with unsafe names, server facts, media facts, the key tag, the player name, and sRGB-composited scrims |
| Strict build | Swift 6, complete concurrency, warnings as errors, `arm64-apple-macos13`: launcher, harness and tests |
| `python3 tests/launcher/lifecycle.py` | **PASS** at `1b99cd7` (`lifecycle-1b99cd7.log`): nested child, Dock policies, live links, no duplicate engine, force-quit reconnect, crash report |
| `python3 tests/launcher/cold_url.py` | **PASS** three times at `1b99cd7` (`cold-url-1b99cd7-{1,2,3}.log`). Earlier, at `5187bd9`, it passed five times in a row with a completed scratch home and once with a fresh home (`cold-url-fresh.log`). Cold LaunchServices `cod2x://` with a `+` password is deferred and never in argv; accessory during the game, regular after it, one game only. No test processes left behind |
| `python3 tests/packaging/first_run.py` | **PASS** (`first-run.log`) |
| Stock engine build (CONTRIBUTING configuration) | **PASS**, `build-stock.log` |
| CoD2x engine build | **PASS**, `build-codx.log` |
| `sh tools/abi/check.sh build-macos/compile_commands.json output/ws28/abi-stock` | **exit 0**: 620 TUs, 0 errors, **0 mismatches**; 218 renderer bindings with 0 table/cast/floating mismatches; 514 imports and 1,776 native sites with 0 proven extra or missing dereferences |
| Same for `build-macos-codx` | **exit 0**: 637 TUs, **0 mismatches**; 218 bindings clean; 1,778 native sites, 0 proven extra or missing |
| Private contrast gate | **PASS** at `1b99cd7`, worst 4.66:1 over 30 heroes |
| `COD2_BUILD_BACKGROUND=1 scripts/package-release.sh --build-dir build/package/ws28` at `1b99cd7` | **exit 0**. `dist/CoD2-Silicon-0.1.0-macos-arm64.zip`, SHA-256 `c205400b69b3f71f5e65c9f80981714b642c1d92b6a2245a62c4c044abbaf067`; `shasum -c SHA256SUMS` OK (`package-1b99cd7.log`). The earlier `00d017d` package was `2e7a4d36…adf5` |
| `python3 tests/packaging/launcher_bundle.py` on the extracted `1b99cd7` zip | **PASS** (`bundle-audit-1b99cd7.log`). Launcher plus the isolated Game Mode helper, one URL owner; four arm64 Mach-Os at `minos 13.0` (SDK 27.0); portable links; `codesign --verify --deep --strict`; no game content; icon G (`Assets.car`, ICNS, `CFBundleIconName`, `NSAccentColorName`) in both apps; the five font and licence files |
| `python3 tests/packaging/release_smoke.py` on the extracted zip (under `gtimeout -k 10 300`; no other game running, benchmark lock free) | **PASS** at `00d017d`. Empty home → launcher non-interactive setup → engine menu → `devmap mp_toujane` → two captures → scripted quit, exit 0 → "returned to launcher"; bundled SDL2/SDL3 loaded. `output/ws28/release-smoke-00d017d/results.json`. **Pending** at `1b99cd7`: another agent holds the benchmark lock, so the run waits for it (`output/ws28/gate/release-smoke-1b99cd7.log`) |
| System-resolved app icon | `NSWorkspace` icons of the extracted `1b99cd7` outer app and nested helper show icon G with two bridges (`output/ws28/icon/finder-icon-{outer,helper}.png`) |
| Static checks | `shellcheck` clean on the changed scripts; Python compiles; `git diff --check 4d56e0b` clean outside the verbatim OFL texts, whose upstream trailing spaces are exempt through `launcher/Resources/Fonts/.gitattributes` |

The CONTRIBUTING fixture suites (online, fixes13, lp64, platform and so on) were
not re-run. This branch changes no engine source, header, CMake or data blob
(`git diff 4d56e0b -- src CMakeLists.txt cmake build/lp64_gen` is empty).
The engine builds and both full ABI audits ran on this branch, and no engine
input has changed since, so they still describe `1b99cd7`. The release smoke's captures are game content; they
stay in ignored `output/`.

## What's left

- **Accepting the real `Window` scene.**
  - SwiftUI opens no scene windows while the session is locked. The review
    images come from the hosting-window harness, so the shipping window's
    sidebar may draw differently: it could be the floating, inset Tahoe
    sidebar.
  - Look at the app unlocked, on macOS 26 and 27, and on macOS 13–15 for the
    material fallback. That was not possible here.
- **VoiceOver and keyboard walkthrough.** Labels, traits and shortcuts are in
  place, but nobody has toured the app with VoiceOver.
- **Renderer controls.**
  - Metal 4, render scale, MetalFX and HDR need the engine's `r_renderer` and
    related dvars from the Metal workstreams. When they land, remove
    `upcoming:` in `SettingsView.graphicsGroup` and extend
    `GameSettings.dvars()`.
  - The reserved-name list already holds these names.
- **Demo length.** It needs a `.dm_1` reader; the row shows the date for now.
- **Dispatches update only on demand.** Fetches happen when someone checks for
  updates (Home toolbar or About), keeping WS25's no-network-on-launch rule.
  The two bundled entries show until the first check.
- **Font licensing.** Big Shoulders is a variable font, and only its
  700/800 named instances are used. No other font licensing questions are open.

## Merge notes for `port/launcher-v2`

- **Merging.** Merge after `port/launcher`, which is this branch's base.
- **Conflicts with WS25.**
  - WS25 owned `launcher/Views.swift` and `launcher/Design.swift`, which this
    branch deletes.
  - Any later change to those files on `port/launcher` must be re-applied in
    the per-screen files.
  - `launcher/Model.swift` keeps every WS25 method; the diff adds to it.
- **Packaging and icon.**
  - `make_macos_app.py` changes only icon, font and accent installation.
    Bundle identity, Info keys, signing order, the SDL rpath, the helper's
    Game Mode and automatic-setup flags and the sole URL owner are unchanged.
  - `Native.icns` is gone. The bundle's icon is now `CoD2 Silicon` from
    `Assets.car` and the ICNS.
  - Keep `tools/cod2x/icon-source/CoD2 Silicon.icon` together with
    `layers.py`.
- **Build integration.**
  - CMake needs no change: the existing `launcher/*` glob covers the new files.
  - `scripts/build-launcher.sh` lists the view files explicitly. Add new launcher
    sources there.
- **Docs index.** Add WS25 and WS28 to the index in `docs/macos-port/README.md`
  when merging. I left the shared index alone to avoid conflicts.
- **Housekeeping.**
  - No pushes, PRs, issues, remote changes, system packages, sibling-worktree
    access or benchmark measurements.
  - Test bundles registered with LaunchServices were unregistered.
  - Only processes this session started were stopped: test launchers left
    behind by debugging runs.
