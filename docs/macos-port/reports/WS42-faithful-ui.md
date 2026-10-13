# WS42: faithful menus, input and window behavior

Worktree: `faithful-ui`; branch: `port/faithful-ui`; base: `664f1a1`.
Evidence root: `~/Library/Application Support/CoD2-native-ws42/`.
All captures, extracted licensed menus, disassembly, trace data and generated
test libraries remain outside git. No system packages, remotes, pushes, PRs or
issues were created. Movement/view/bob and Metal implementation were not changed.

## 1. Listbox input

The three `UI_FeederCount` calls reinterpreted `item->special` as an `int`,
then converted its value to the function's `float` parameter. Feeder `2.0f`
therefore became roughly 1.07 billion instead of feeder 2, yielding zero rows.
This is a reconstruction bug also present in the legacy source. The Mac 1.3
calls at `0x16c8e4`, `0x16ce52` and `0x16bb30` pass the float dword directly.

The native branches now call `UI_FeederCount(item->special)`; each original
expression remains verbatim in its legacy branch. The extracted production
function harness covers servers (2), maps (4) and players (8), wheel down/up,
click selection of another row and a 150 ms double-click. It failed before
the fix and passes with ASan/UBSan afterward. See `listbox-before.log` and
`ui-after.log`. `tests/lp64/ui/run.py` invokes this harness for both builds.

`tools/abi/astdump_strict.py` provides a permanent Clang AST gate for numeric
conversion of integer/float bit reinterpretations. It uses the actual engine
compile flags and fails on compiler errors. Four synthetic tests distinguish
the two bad conversion directions from ordinary casts and verify error handling.
`tools/abi/check.sh` runs both the tests and gate.

## 2. Multi-choice labels and color

The reconstruction painted only the selected value at `textRect.x`. Mac 1.3
`Item_List_Paint` (`0x16ad16`) translates an `@` value, then paints the label
when present (`0x16ae63`–`0x16af13`) and places the value at label X + label
width + 8. The 8.0f literal is at `0x3262f8`. Neither the label nor no-label
reference path updates value extents (`0x16af2f`–`0x16afb7`); both draw
directly, including an empty value. This preserves the label's cached width.

The native function follows those paths. It also reproduces the reference's
inline focus pulse/foreColor calculation; using `Item_TextColor` for the value
would additionally apply fading and text-style rules absent from the original
value draw. The complete original function remains in the legacy branch.

The extracted paint harness intercepts both draws and checks their text,
positions, cached label extents, empty values, translation order and color.
`listpaint-before.log` records the old failure; `ui-after.log` records the
sanitized pass. The ABI manifest's single added scalar-coordinate expression
documents this verified eight-pixel gap, rather than approving an offset.
`cmdtab/home/main/screenshots/ws42-joinserver.jpg` shows the restored
“Connection:” label. The live Filter screenshot `input-live-stock-11/home/main/screenshots/input-003-filter_popmenu.jpg` shows Mods, Friendly Fire, Killcam and PunkBuster labels and values, with the expected right-aligned label column. The Join Server screenshot in that directory also shows Connection. Both were visually reviewed against `ui_mp/filter.menu` and `ui_mp/joinserver.menu` from the overriding `iw_15.iwd`; read-only definitions are retained in `menu-definition-evidence.txt`. Final acceptance captures are listed below.

## 3. Mouse policy, wheel direction and windowed coordinates

Fullscreen capture from `d0c3f4f` remains enabled in menus. Raw GCMouse delivery
is now selected only for an active client with no key catchers, relative capture,
an enabled raw preference/mode and an available raw device. Menus and an open
console use SDL relative events instead. `MacInput_UseRaw` is a pure function;
its tests cover menus, gameplay, console, capture, preference and availability.

Mac 1.3 takes Carbon system mouse deltas at the `mdta` event parameter around
`0x26a01` and forwards them to `CL_MouseEvent`. CoD2x applies its intentional
raw-input behavior in `window.cpp:347–371` during gameplay. SDL3 Cocoa forwards
accelerated NSEvent deltaX/Y for relative input. No new relative cursor scaling
was added. The native no-op `SDL_MAC_USE_GCMOUSE`, relative-warp and system-scale
hints were removed; their exact legacy lines remain guarded.

Mac `DoMouseWheel` at `0x10b830` maps positive wheel delta to MWHEELUP and
negative delta to MWHEELDOWN. SDL Cocoa has already applied the system scrolling
direction before attaching FLIPPED metadata. Undoing that metadata reversed
natural scrolling. The native pump now preserves the supplied sign and
accumulates `preciseY` fractions until a complete wheel step is available. Both
menu scroll and weapon bindings use the same engine key path. Focus loss clears
the fractional remainder. Tests cover four quarter-steps, reversal and flipped
metadata in the actual production SDL pump.

Sources for SDL implementation details:
[SDL3 Cocoa mouse](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/video/cocoa/SDL_cocoamouse.m)
and [sdl2-compat event bridge](https://github.com/libsdl-org/sdl2-compat/blob/main/src/sdl2_compat.c).

The first live absolute-motion assertion found an additional native bug:
`MacPlatform_MapWindowPoint` returns render pixels, while
`UI_MouseEventAbsolute` consumes the cursor's 640×480 virtual coordinates.
At logical 640×360/render 1280×720, intended UI (466,12) reached (640,18).
The pump now converts the presentation-mapped pixels back to virtual coordinates,
inverse to the cursor's full-screen alignment. Relative deltas are unaffected.
`absolute-before.log` fails the production-pump test; `absolute-after.log`
passes it. Pure tests cover widescreen, 4:3 and an unavailable render size.
The failed live evidence is retained in `input-live-stock-1/`.

## 4. Borderless Cmd-Tab

Previously the focus-loss display handler suspended only exclusive fullscreen,
and SDL minimized neither borderless windows nor the application. The native
borderless mode now observes `NSApplicationDidResignActiveNotification` and
calls `[NSApp hide:nil]`. It does not observe key-window loss, so taking the
key window for Spotlight does not hide the game. Mode release removes the
observer; the exclusive path is unchanged. Hidden applications are excluded
from the native visibility predicate.

The existing focus-loss input path clears engine key states, deactivates raw
mouse, releases relative capture and restores the cursor. Production input
tests cover those transitions. `app_hide` distinguishes key-window resignation
from application resignation. Its baseline failure and fixed test output are
retained in the evidence root. Audio pause/mute policy was not changed: the Mac
reference activation handlers contain cursor/system-lock operations, without
an added audio pause or mute call. Audio adapter tests remain green; these tests
do not claim a physical listening comparison.

The live external test activates Finder using `osascript`, then reopens the QA
application. `cmdtab/states.json` records active/on-screen before (true/1),
away (false/0, Finder), and return (true/1, game). `cmdtab/cmdtabwatch.log`
records the window at level 25, absent while away, and on-screen at level 25
after return. `cmdtab/result.log` records PASS. The QA app is under `.noindex`
and was unregistered from LaunchServices. No Accessibility permission was used.

## 5. Release identity, shortversion and browser filters

Native CoD2x uses `CoD2x MP: 1.4.6.8> ` and the footer
`CoD2 Silicon <release> · CoD2x 1.4.6.8 · macOS arm64`. Stock arm64 retains
its stock prompt and uses `macOS arm64` in its Build footer. Legacy text is
unchanged. `RELEASE_VERSION` defaults to 0.2.0; packaging, install, bundler,
the app plist/launcher and the engine compile definition share that source.
A package version override is forwarded to both engine and app. This brief
explicitly authorized those small release-script changes. Naming the file
`VERSION` first collided with C++ `<version>` on the case-insensitive filesystem;
the final name avoids that collision, and both real builds pass.

Mac console reference: prompt `0x15fd1f`–`0x15fd36`, footer
`0x15d2a6`–`0x15d2b7`. CoD2x `src/mss32/hook.cpp:349–352` intentionally
patches the prompt to its APP_VERSION. The extracted complete console draw
functions pass ASan/UBSan tests for stock, CoD2x, two release overrides, feature
gates and prompt-dependent input width. See `version/`. The production CMake
resolver and bundle parser are exercised too; packaging first-run QA passes
with a private fake key/home and `.noindex` bundle.

`shortversion` stays unchanged. Mac `Com_Init_Try_Block_Function` at `0x31070`
registers stock 1.3 with flags `0x1044`; CoD2x `src/shared/common.cpp:361–363`
intentionally patches it to APP_VERSION 1.4.6.8. These flags include SERVERINFO
(`0x4`), ROM and reset, **not USERINFO (`0x2`)**. Connect userinfo requests bit
2 and separately supplies protocol 118. Server status/configstrings advertise
shortversion. Browser compatibility uses protocol 118/120, and CoD2x server
checks protocol, protocol_cod2x >= 6 and a 32-character HWID2; none uses
shortversion as the client version check. The detailed read-only evidence is
in `version/version-evidence.md` outside the repository.

Filter defaults match Mac UI_Init: full/empty/password/no-password=1,
pure=1 (pure servers only), dedicated=0, and mods/friendly fire/killcam/PB=-1
(unrestricted). The reference applies the PB predicate at `0x1524dc`, but the
reconstruction omitted it. Both native display-list branches now reject a
different PB value when the setting is >=0. The extracted production predicate
test fails before the fix and checks unrestricted/on/off afterward.

The original “102 servers not listed (filtered out by game browser settings)”
message is total cached addresses minus displayed rows (`0x1548bf`–`0x1548dd`).
It also includes pending/unanswered addresses with no positive ping; it is not
solely a counter of filter rejections. The exact causes for the user's 102
cannot be reconstructed without that run's per-server evidence. LAN discovery
initially stores dirty=0/ping=-1, as Mac `CL_ServerInfoPacket` does at
`0x161358`/`0x161383`; the live fixture uses real Quick Refresh after its
announcements and adjusts only its private filter for a non-pure test server.

## 6. Quiet default releases

Tagged reconstruction output and named native trace files now require one
documented startup opt-in, `COD2_NATIVE_TRACE=1`. Unset, empty and 0 disable
it. Retained diagnostics include connection, screenshot, UI sweep, combat,
renderer and debug heap traces. Original behavior-changing reconstruction
probes remain compiled out natively. Explicit output-path measurement tools
retain their own off-by-default settings; see [native-tracing.md](../native-tracing.md).

The startup flag is shared and initialized before WinMain diagnostics and again
idempotently in Com_Init. No getenv remains in renderer/opcode hot paths, even
at O0. The new module is HEADER_FILE_ONLY when COD2_X64 is off, preserving the
legacy object set. Review caught a trace-guarded increment used for DirectX
fatal escalation; it now increments independently. A sanitized extracted
production test verifies 999 returns followed by the 1000th fatal error, with
zero diagnostic prints.

`tools/check_trace_guards.py` rejects unguarded tags and trace-file opens.
Its 13 synthetic tests include sibling/else guards, inverted/OR conditions,
unrelated env variables and comments. The baseline scan found 68 unguarded
diagnostics in the 29 cleanup files; the fixed repository scan finds zero in
721 source files. It and its unit tests are in the merge gate. Startup caching
has separate unset/0/1 CTests. Live default-output evidence is recorded below.

The final console review additionally found five untagged probe sites:
server/client `ui_allow_join` dvar prints, script-menu join-dvar output, and
renderer command-dump `target=`/`text=` continuations. Guarding only the dump's
tagged prefixes had left the continuations and blank lines in default output.
The entire native dump now returns immediately unless tracing is enabled, and
its enabled output includes the prefixes. All three other probes use the same
flag. The scanner now recognizes these named untagged formats too;
`trace-untagged-before.log` catches all five old sites, and the fixed scan is
zero. Its thirteenth regression covers these formats and their guards.

Six reconstructed renderer progress labels also differed from the reference.
Native `R_CreateForInitOrReset` restores the original newline-terminated
render-target, static-cache, dynamic-buffer, particle-buffer, query and initial
state messages, without moving any calls. Mac 1.3 function `0xcb24e` prints them
at `0xcb265`, `0xcb27f`, `0xcb299`, `0xcb3ee`, `0xcb408`, `0xcb457`; the
corresponding strings are `0x24ef9c`, `0x24efbc`, `0x24efe0`, `0x24f06c`,
`0x24f094`, `0x24f0b4`. The legacy strings remain exact. The `total N` lines
are retained: they originate in the stock `_fx.gsc` asset, rather than a
reconstruction probe. Final quiet live runs below verify the complete default
startup/browse/join/play/quit path after these last output fixes.

## 7. Real input acceptance suite

`tests/input/` builds a test-only interposer linked to the same SDL install name
as the game. It injects SDL events through the real SDL queue and production
`SDL_PumpInputEvents`, with timed button edges/double-clicks, absolute/relative
motion, keys, UTF-8 text and precise fractional wheels. SDL2-compat drops precise
wheel input and rejects text in its SDL2→SDL3 bridge, so these two event types
use the already loaded SDL3 via RTLD_NOLOAD. No second SDL is loaded. Without
the explicit test paths, the library is inert; the shipped engine has no test
library dependency. Runtime observation is read-only and redacts credential
values. Feeder callbacks are avoided because they mutate selection/caches.

The committed scenarios use runtime menu geometry and assert browser scroll,
row selection, both join paths reaching CA_ACTIVE, Start New Server/listen
game, Options choices/sliders/tabs, bind/clear, Filter and Server Info labels,
Favorites add/remove, password text and Quit confirmation. Local browse filler
addresses are simulated loopback identities; joins are forwarded to an owned
real dedicated server. Start New Server separately exercises a real listen
server. This division makes the long scroll fixture deterministic and is
explicit in the README; it does not prove physical LAN broadcast discovery.

The real suite caught two additional reconstruction errors that the handler-only
harnesses missed. Mac `CL_KeyEvent` (`0x141516`–`0x141543`) consults
`UI_checkKeyExec` only with `cl_bypassMouseInput` enabled. The reconstruction
consulted it unconditionally, sending F8 to gameplay even while the UI was
waiting for a bind. Native routing now defaults to UI when bypass is disabled;
`UI_CheckExecKey` itself is unchanged. Its reference global at import pointer
`0x1acd5f1` resolves to `g_editingField` (`0x34b344`), not bind-pending state.

Mac `Menus_HandleOOBClick` (`0x171329`–`0x171355`) calls `UI_Pause(0)` only
when no menu remains visible, then always performs cinematic cleanup. The
reconstruction unpaused with visible sibling panels, clearing the UI catcher
immediately after a map-row click. Native code now follows the reference;
legacy text remains exact. `input_routes.py`, wired into `tests/lp64/ui/run.py`,
extracts both production routing/cleanup regions and verifies normal/bypassed
keys, F8/Backspace, visible/forced-visible menus, last-menu closure and cinematic
cleanup under ASan/UBSan. Baseline failures are `input-routes-before.log` and
`oob-route-before.log`; both fixed builds pass `input-routes-*-after.log`.
Live routing and capture failures are retained in the numbered stock runs and `input-maps-stock-1/`. The subsequent two-slot failure is retained in `input-live-final-stock/`.

The next live pass also caught two faithful-input errors. The reference bind
handler (`0x16d4bc`–`0x16d4fb`) replaces two existing keys with one new key,
clearing both old engine bindings. The reconstruction overwrote slot 2 and left
an orphan key active. Native replacement now clears the old bindings first;
the extracted handler verifies first, second, third and clear operations.
`third-bind-before.log` records the baseline failure.

The reference OOB handler restricts text-rectangle hits only for type 0
(`0x170f4d`/`0x170f55`); the reconstruction applied that restriction to non-text
controls. Clicking the value side of a multi in another panel could therefore
fail to transfer focus. The native predicate is corrected, with its exact
legacy expression preserved. The full extracted OOB function tests a multi's
value area, a text item's excluded area and actual label area, alongside
capture and cinematic behavior. `oob-hit-before.log` records the failure.

The combined real scenario also exposed a remote-to-listen lifecycle bug: a
listen game's challenge is correctly zero, but native `CL_Netchan_Decode`
substituted `clc_x64_lastChallenge` from a previous remote connection. The result
was a corrupted server message and error dialog, although an isolated fresh
listen launch passed. Mac 1.3 reads the current challenge byte directly at
`0x18feca`, XORing it with the current sequence at `0x18fed1`, without a
fallback. The native decoder now uses the actual challenge, including zero;
the original fallback remains in the legacy branch. The extracted decoder
regression checks stale remote state, zero/nonzero challenges, sequence-byte
truncation, reliable-ack ring wrapping and command-dependent keys under
ASan/UBSan. `netchan-decode-before.log` fails the payload assertion, while
`netchan-decode-{stock,codx}-after.log` pass. The unchanged combined live
scenario is used to confirm the fix below; failed warm runs remain retained.

Python tests cover geometry, precise UI command matching, strict input ownership and real UDP transport. The standalone SDL
and engine-pump characterization also proves the no-opt-in case is inert.
`CONTRIBUTING.md` now runs these and both windowed live suites plus a short
fullscreen server-list pass. CI runs the data-free trace scanner, conversion
checker, both UI suites and both no-window SDL characterizations; the licensed
live cases remain a local gate. CI YAML parses, and the corresponding commands
pass locally; no hosted CI job was triggered. See [tests/input/README.md](../../../tests/input/README.md)
for commands, dependencies and the exact fixture boundaries.

## Gate evidence

Both Release clients and the stock dedicated fixture build successfully:
`cmake --build build-macos --target cod2_macos cod2_macos_ded --parallel 3`
and `cmake --build build-macos-codx --target cod2_macos --parallel 3`.
See `build-stock-quiet-final.log` and `build-codx-quiet-final.log`.

The fixture/static portion of every CONTRIBUTING merge-gate command passed,
except the explicitly nonblocking inactive-CoD2x checker. Its result is 212
mismatches in 39 files across 17 configurations (`inactive-final.log`).
`gates/results-reconciled.json` records each command and its final log;
`online-verified-{stock,codx}.log` and `ui-verified-{stock,codx}.log` supersede
older reruns. Online harnesses link the actual diagnostic module so ASan's
strong global reference does not leave the weak-import accessor unresolved.
`fixes-{stock,codx}-final.log` include the renderer/opcode hot-path checks.

Both `tools/abi/check.sh` runs pass: stock 624 TUs and CoD2x 641 TUs,
zero Clang errors, zero ABI mismatches/new classes, 218 callback bindings
with zero violations, zero bit conversions, and zero proven extra/missing
import dereferences. See `abi-{stock,codx}-verified.log`.
Raw-offset sweeps find zero unreviewed sites: stock 391 sources/1197 sites,
CoD2x 404 sources/1203 sites. Both legacy checkers pass with zero mismatches (37 source files × 10
configurations and 38 files × 5 COD2_X64-off configurations),
and the additional `unifdef -UCOD2_X64` + `cmp` proof is byte-identical for all
39 touched existing C/header/Objective-C sources (`legacy-text/results.txt`).
The trace scanner reports zero unguarded diagnostics; all 13 scanner tests pass.

The actual x86 artifact comparison could not run on this arm64 Mac:
`tools/ci/compare-x86.sh` requires an x86_64 Linux host and exited 2
(`x86-compare.log`). The source/preprocessor and object-set preservation checks
pass; this is not a claim that an x86 binary was built here. No missing tools
were installed.

The latest no-window input acceptance passes for both compile databases:
`input-selftest-complete-{stock,codx}/results.json` records SDL bridge,
production pump and inert-without-opt-in success. Python driver/UDP tests pass 8/8
in `input-python-verified.log`.

## Final live evidence and merge notes

The final real-input results are:

| Evidence directory | Result | Client lifetime |
| --- | --- | --- |
| `input-live-final-stock-4/` | 87 checks, both real joins, warm listen `mp_burgundy`, Quit exit 0 | 105.525 s |
| `input-live-final-codx/` | 87 checks, both real joins, warm listen `mp_burgundy`, Quit exit 0 | 106.855 s |
| `input-live-final-fullscreen/` | 8 browser/input checks at 1920×1080, 333 fps, raw preferences enabled, Quit exit 0 | 29.459 s |

Each directory contains `results.json`, atomic state snapshots, actual SDL
acknowledgments, private console/server logs and screenshots. Stock and CoD2x
have 22 captures each; fullscreen has four. The final stock Join Server
`input-live-final-stock-4/home/main/screenshots/input-001-joinserver.jpg` and CoD2x Filter
`input-live-final-codx/home/main/screenshots/input-003-filter_popmenu.jpg` were visually inspected:
Connection and all four previously missing Filter labels are present, and their
label/value placement agrees with the overriding licensed menu definitions.
The same runs assert row 23 selection, real double-click/Join button connections,
Options dvar changes, pending-bind transitions/clear, Favorites address changes,
password length, cancel and confirm. Single-language fallback is recorded but
excluded from changed-multi coverage. Private dvar restoration does not promise
to reverse every UI-script side effect.

Fullscreen injected SDL `[10, -6]` produces UI `[10, -6]` exactly, render
`[30, -13.5]`, and Cocoa content `[47, -21.15]` points. With this desktop/content
size 3008×1692 points and backing scale 2, the measured content-point gain is
4.7 horizontal / 3.525 vertical points per SDL delta unit. This is the existing
640×480 virtual-cursor transform, with no added relative scaling. The passive
physical-pointer probe recorded no movement (`system-pointer-probe.log`), so
there is no measured hardware speed or physical Magic Trackpad feel comparison.
The live suite proves the event/coordinate path, not raw hardware delivery,
gesture synthesis or physical LAN broadcast discovery.

Final `input-quiet-final-{stock,codx}/` revalidations each pass all 87 checks
(109.587 s / 108.543 s) and contain zero tagged or named untagged reconstruction
diagnostics and zero production `x64_*_trace.txt` / heap diagnostic files
(`default-output-final.json`). The earlier complete runs each had 50 untagged
probe lines (`default-output-untagged-comparison.json`); the final runs have
zero. The test-only input files are explicit opt-in
artifacts in the evidence directories.

The platform suites pass 34/34 tests with ASan/UBSan and 36/36 without
(`platform-{sanitizers,plain}-verified.log`). Both menu sweeps pass all 59
front-end and 73 in-game menus, plus 46 safe script commands; their results
and captures are in `menu-{stock,codx}-final/`. The bounded windowed
`tools/parity/record.py` smoke validates 100 nonempty-world frames and a final
entity dump (`smoke-final/`, `parity-final.log`). This records a native run;
it does not claim parity against an original executable. `--windowed` keeps
this local merge-gate smoke at 640×480/60 fps with `timeout -k`.

Both complete ABI commands finish with exit 0. Stock checks 1777 active import
sites and CoD2x checks 1779, across 514 referenced import symbols; both find
zero proven extra/missing dereferences. Full reports are in
`abi-{stock,codx}-routes-final/`, with command output in the verified logs above.
The final menu/parity commands and exit statuses are in
`live-final-results.json`; complete quiet input command lines/statuses are in
`quiet-live-results.json`. The earlier platform commands/results remain in
`live-root-results.json`. No owned game or fixture server remains running.

Merge the focused DCO commits in order. The only existing shared CMake change
is the five-line native-only diagnostic-module source treatment; no
`src/headers/*`, movement/bob or Metal implementation changed. The release
source is `RELEASE_VERSION`; carry its small CMake/app/packaging consumers
together. Carry `port_debug.c` with its guarded callers and the sanitized
fixture linkage. Keep the new UI/input tests and trace/AST gates with their
CI and CONTRIBUTING commands. Licensed evidence stays solely at
`~/Library/Application Support/CoD2-native-ws42/` and must not be merged into git.
