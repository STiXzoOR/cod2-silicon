# WS36 — first hands-on QA fixes

Date: 2026-10-05. Branch: `port/qa-fixes`. Worktree: `qa-fixes`.
The WS36 starting point was `3a916da`, including WS28 and the two orchestrator
focus/cursor fixes. No sibling worktree was accessed. No packages were installed,
remotes changed, commits pushed, PRs/issues opened, or proprietary content added.

## Completion status

**Partial; interactive acceptance is blocked.** The Mac became locked during
this session; at 08:58, 09:05 and 09:15 UTC both `CGSSessionScreenIsLocked` and
`IOConsoleLocked` were `Yes`, with loginwindow frontmost. No unlock, security
preference change or unowned-process termination was attempted. Remaining
live checks need an unlocked, isolated desktop. The verified fixes below are
committed; the unverified launcher candidate is saved separately, not included
in the branch's production changes.

## Evidence and limits

Game captures, traces and scratch homes are outside git at
`~/Library/Application Support/CoD2-native-ws36/`. Synthetic probes and gate logs
are ignored at `output/ws36/`. Commands below run from this worktree. Game runs
inventory processes first, use private homes/ports and bounded owned-process
cleanup. An existing installed launcher and other agents' games were left alone.

The UI defects and portal memory defects have failing-before/passing-after
fixtures. The reported intermittent floor disappearance was **not reproduced**
in either controlled sweep. The fullscreen tests cover a finite scenario matrix;
they do not establish an exhaustive guarantee over every macOS/display failure.

## Fullscreen and cursors

The pre-fix exclusive case reproduced an important part of the reported state:
the game remained frontmost, rendered its framebuffer, kept the physical
1920×1080 display mode, and had no on-screen WindowServer window during Mission
Control. The whole-screen capture showed Mission Control, rather than the user's
all-black display. Ordinary Finder/game focus cycles alone did not reproduce it.

SDL2 compatibility drops SDL3's occlusion notification. Mission Control can
occlude a still-key Cocoa window without sending SDL focus loss, so the existing
focus-loss step-down never ran. `macos_window.m` now queries actual Cocoa
visibility after the event pump. An occluded exclusive window releases its
display mode into desktop fullscreen. Input capture follows effective visibility
and clears held keys when a still-key window becomes occluded.

A first implementation restored exclusive mode whenever a Mission Control
thumbnail became visible. That caused repeated display transitions. The final
implementation latches desktop fullscreen until a real focus-loss/gain pair.
Display wake can also occlude a still-key window: it remains visible in the safe
desktop mode until that pair. The chosen render backing remains unchanged.

The inherited fullscreen-menu relative capture and hidden system cursor remain
covered by `tests/platform/input.c` and the CoD2x input fixture. Focus/occlusion
releases capture and restores the pointer; visibility restores capture. No OS
click/key synthesis or hot-corner preference change was used. Mission Control was
opened through LaunchServices; display sleep used `pmset displaysleepnow` followed
by `caffeinate -u -t 2`.

Reproduction and acceptance commands:

```sh
python3 tools/cod2x/make_macos_app.py build-macos-codx/cod2_macos \
  output/ws36/QA.app --replace --engine-only \
  --bundle-id io.github.stixzoor.cod2silicon.ws36.game --game-dir "$HOME/Games/CoD2"
python3 tests/platform/focus_visibility.py output/ws36/QA.app --mode exclusive \
  --sleep-wake --output "$HOME/Library/Application Support/CoD2-native-ws36/focus-exclusive-wake-final"
python3 tests/platform/focus_visibility.py output/ws36/QA.app --mode borderless \
  --sleep-wake --output "$HOME/Library/Application Support/CoD2-native-ws36/focus-borderless-final"
```

These exercise five Finder/game cycles, Mission Control, sleep/wake, active
visible windows, actual physical modes, game JPEGs and desktop restoration.
`focus-borderless-final` passed the complete matrix. Exclusive passed all five
ordinary cycles and Mission Control; the final wake retry kept a visible game
but ended windowed (1920×1112 window bounds on the 6K desktop) after the real
focus pair, so its exact-mode restoration assertion **failed**. That remains
open. A later tracing retry started after the screen locked and is excluded
from interactive acceptance. No black screen was observed in the successful
fixed scenarios, but universal black-screen exclusion is not established.
Borderless stays in Mission Control until the overview is explicitly dismissed;
opening an already-active game is not a request to dismiss the system overview.

### Default decision and panel result

Desktop fullscreen is now the native engine, launcher and bundler default.
Exclusive remains selectable (`r_fullscreen 1; r_borderless 0`, launcher selection,
or bundler `--exclusive`). Existing saved launcher selections are preserved.
Windowed renderer resets take precedence over the new borderless default.

The desktop mode is 3008×1692 points / 6016×3384 physical pixels at 60 Hz. Desktop
fullscreen leaves it intact; exclusive changes it to physical 1920×1080.
WS17's fixed CGL backing and off-screen framebuffer preserve the requested
1920×1080 render resolution in both modes. WindowServer scales the desktop
fullscreen image across the 6K panel. The decision favors the mode that avoids
physical mode switches and the associated occlusion transition.

The first locked 10-second pacing matrix was interrupted by another agent's
runs. Its snapshots show other fullscreen/platform fixtures before or after
all three completed cases, so the 240.3 FPS desktop/250, 179.3 FPS exclusive
request/250 and 660.1 FPS desktop/uncapped values are **exploratory**, not a
quiet A/B. The exclusive request also stepped between desktop and exclusive
four times during its trace. A fourth case was refused when another game was
running; that process was not stopped.

All saved registry snapshots show `IOConsoleLocked = No`. The old evidence
recorder only recognized `CGSSessionScreenIsLocked` and returned unknown on
this host; it now accepts the root console key as a fallback, with failing-
before/passing-after tests. Game Policy recognized each owned PID through
`LSSupportsGameMode`; the system session logged `on`, `paused` and `off` even
for desktop fullscreen. These global session transitions can be affected by
other apps and do not establish a performance benefit.

A valid complete quiet matrix remains blocked. The shared machine repeatedly
ran other games, fullscreen fixtures and heavy builds despite our reserved
slot; after those interruptions the screen became locked. The benchmark lock
was released. A quiet, unlocked run should use the command below for both
`--window-mode borderless` and `fullscreen`, caps 250 and 0, while holding the
PLAN lock. Record the process snapshots and require `screen_locked: false`.

```sh
SDL_VIDEO_MAC_FULLSCREEN_SPACES=0 timeout -k 10 120 \
  python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --app output/ws36/QA.app --resolution 1920x1080 --maxfps 250 --seconds 10 \
  --window-mode borderless --set net_port 29936 \
  --output "$HOME/Library/Application Support/CoD2-native-ws36/bench-new-borderless-250"
```

The existing `tools/macos/check-gamemode.sh` predicate was used for Game Policy
logs. It must not be read as proof that exclusive or a native Space is required
for Game Mode on this OS. No comparative latency benefit is established. Physical input-to-photon latency is not measured
by the software frame observer.

## Join Server: strings, live servers and tint

`SEH_IsDigit` read a four-byte rune table through eight-byte `unsigned long`
elements on LP64. The digit in `&&1` consequently was not recognized. Native
classification now uses `uint32_t`; the legacy expression remains intact.

The native owner-draw switch used pre-1.3 IDs. Patch 1.3's game-type filter is 253,
movie is 254 and map is 255. The matching typed native path now displays `All`.
Map names also used the old 0xa4-byte map-entry stride; native code now indexes
`mapList` directly. Server Info feeder 13 similarly now reads its typed pointer
rows rather than 32-bit offsets/strides.

An empty browser had two independent causes. Stock native 1.3 queried protocol
115 instead of 118. CoD2x already queried both 118 and 120, but the shared binary
insertion algorithm tried position 1 in a zero-element list, which insertion
rejected; later searches could also fail to converge. Native insertion now uses a
bounded lower-bound search. The native force-2 refresh path follows the reference
reset semantics. Patch 1.3's column 9 is PunkBuster and column 10 is ping; those
native feeder mappings and published `pb` metadata are corrected.

The cyan overlay came from 32-bit item colour offsets. Native fields are
`foreColor` 472, `backColor` 488 and `borderColor` 504. The old border offset 492
overwrote back-colour G/B/A and border R, producing approximately
`[0, .5, .5, .5]` across the list. Back-colour updates could also overwrite text
alpha. Native `Script_SetItemColor` now selects the typed field.

Real UDP responses were saved privately and passed through the production
`CL_ServersResponsePacket` parser under ASan/UBSan: 12 packets, 448 distinct
servers. At that query, Activision answered 353/79 addresses for protocols
118/120, and `master.cod2x.me` answered 297/380. These are observations from this
session, not stable server counts. A later complete game refresh displayed **350
servers and 539 players**. Its JPEG shows `Source: Internet`, `Game Type: All`,
translated stock map names, a ping column, and no cyan list-area tint.

```sh
python3 tests/online/run.py build-macos/compile_commands.json
python3 tests/online/run.py build-macos-codx/compile_commands.json
python3 tests/rendering/server_browser.py build-macos-codx/cod2_macos \
  --output "$HOME/Library/Application Support/CoD2-native-ws36/browser-final2"
```

The live harness invokes the game's actual Join Server item action and refresh
script. Opening Join Server before the first UI frame or closing its background
`main` menu reopens `main_text` above it; those early diagnostic attempts were
discarded. A null item-name bug in an early local probe was also a probe failure,
not an engine crash. The committed probe checks names and runs the real action.

The existing WS8 Wine baseline produced a stock 1.3/CoD2x Join Server JPEG
at `original-join/home/main/screenshots/ws36-original-join.jpg`, with exit 0.
It shows `Source: Internet`, `Game Type: All`, olive column headers and clear
column bodies. The native refreshed/hover JPEGs match those features and now
show live rows. The original capture did not refresh, so it is a layout/colour
reference, not a matched server-count comparison. The native Connection row
still omits the original `Connection:` label; that separate UI difference is
open. No reference images or game menus were committed.

## Hover flicker

The focused-item pulse divided integer milliseconds by 22, while the Mac 1.3
reference uses 75. That made its nominal sine cycle about 7.23 Hz rather than
2.12 Hz. Seven native UI pulse paths and the refresh-time pulse now use the
reference divisor. The colour-offset fix also prevents hover scripts from
accidentally changing text alpha.

The failing-before/passing-after `ui_text_color.c` fixture dumps 500 nominal
250 Hz frames with stable focus flags. The live browser probe recorded **955
presented hover frames**, one cursor position and constant flags `65607` (focus
held). Every RGBA sample matched the 75 ms integer phase, with maximum error
`5.34e-7`. This rejects the old fast pulse and constant-colour false passes.
The item retains its intentional slow pulse, with no observed focus toggling.
The trace does not measure input latency or physical mouse jitter.

The live reproduction command is the server-browser command above. Its
`join-hover.jpg`, `hover.csv` and `results.json` remain external.

## Floor disappearance and shared visibility fixes

The portal traversal contained independently verified native defects:

- A duplicate hull-pool initialization loop wrote an eight-byte pointer at
  offset 0x20000, beyond the exact 128 KiB pool. The existing correct chain and
  terminal node remain; the native duplicate is removed.
- Portal screen projection used the inverse matrix and interpreted byte offsets
  as float indices. Native traversal uses the forward view-projection matrix and
  float indices 3/7/11/15 for W.
- A clipping caller supplied 128 vertices while its helper requires two
  contiguous 128-vertex scratch buffers. Native scratch now holds 256.
- Far-plane code tested part of a native pointer and clipped the parent plane
  again. It now tests the full pointer and clips the actual far-plane values.

The reference Mac disassembly confirms the pool chain and projection offsets.
No dump was added to git. Production-code synthetic probes fail under ASan for
the former pool/projection/scratch paths; the new `portal_visibility.c` fixture
passes ASan/UBSan, including explicit far-plane rejection. This is shared engine
DPVS code in `src/PC/gfx_d3d/r_dpvs.c`; WS35's Metal renderer receives the fixes
when merged, as does OpenGL. Legacy branches retain their original bodies.

Native `setviewpos x y z yaw [pitch]` now supports pitch; its four-argument legacy
contract is preserved. The committed automated sweep loads Toujane, joins Allies
with an Enfield, stabilizes a noclip camera, excludes user mouse motion by
activating Finder, and captures 66 views: two positions, pitches 10/15/20 and
yaw 170–190 in two-degree steps. It checks rendered camera origins and flags a
greater-than-50% adjacent decrease in lower-frame luminance or 3D draw/primitives.

```sh
sh tests/lp64/renderer/run.sh
python3 tests/rendering/visibility_sweep.py build-macos-codx/cod2_macos \
  --port 29936 --output "$HOME/Library/Application Support/CoD2-native-ws36/floor-after3"
python3 -m unittest discover -s tests/rendering -p test_visibility_luma.py -v
```

| Controlled sweep | Frames | Abrupt-change candidates | Floor luminance range | 3D batch range |
|---|---:|---:|---:|---:|
| Old portal object + same camera tooling (`floor-before2`) | 66 | 0 | 34.29–58.12 | 103–127 |
| Corrected traversal (`floor-after3`) | 66 | 0 | 34.29–58.12 | 109–140 |

The pool/clip fixes are confirmed; their relation to the user's intermittent
floor symptom remains unconfirmed. Both controlled sweeps kept the floor. The
draw trace counts batches, not individual BSP surfaces, and the JPEG is delayed
two frames after its request. Stable cameras make comparison useful, but it is
not exhaustive coverage or exact same-frame surface instrumentation. Broader
angles or the user's precise camera/demo remain necessary to close this item.

## Launcher hand-off

The existing WS28 launcher suite, lifecycle and cold-link tests pass. A new real
engine harness uses Play, the actual Deploy menu action (test-only AppKit
invocation, no synthesized input) and actual LaunchServices cold URL delivery.
It records frontmost windows and verifies the system pointer is hidden while
relative capture is active, then checks game exit 0 and a visible active launcher.
It makes a uniquely identified test copy of the package, because the user's
installed launcher may already be running; that application is never stopped.

Repeated real-engine hand-off acceptance **failed**, including a private-ID
LaunchServices rerun. In the baseline Play case the real game got focus, relative
capture and a hidden system cursor, then quit 0; the launcher return log appeared,
but the tested launcher had no visible window. A test-only AppKit trace found
`NSApp.windows` empty throughout startup and return. The first test had used
direct executable launch; LaunchServices showed the same missing-window state,
so this was not resolved by changing the test entry point or bundle identity.
The existing fixture lifecycle/cold tests check process/Dock restoration, which
was insufficient to establish a visible, active real launcher.

A candidate explicitly opens the SwiftUI singleton scene through `openWindow`,
starts boot after the root first appears, guards repeated boot, and opens the
scene again on return. It compiled for macOS 13 and a trace confirmed an actual
`SwiftUI.AppKitWindow`, but the complete focus/cursor/return gate did not pass.
A subsequent activation candidate uses LaunchServices; the Mac then locked,
preventing valid acceptance. It was not merged into this branch. Its source is
preserved locally as `output/ws36/launcher-activation-candidate.patch` and the
scoped stash named `WS36 unverified launcher candidate while the Mac is locked`.
Do not merge that candidate without the full real hand-off and existing launcher
gates. [Apple's singleton Window documentation](https://developer.apple.com/documentation/swiftui/window)
describes the supported `openWindow` presentation action.

The committed diagnostic now records and rejects a locked/unknown screen before
launching, records private frontmost/bundle/window state, and bounds owned cleanup.
It invokes the actual Deploy menu action after fresh onboarding. The final
bounded guard attempt refused an unowned WS39 soak before launching any game;
that process was left running. The lock-key parser's synthetic red/green checks
cover both registry keys and unknown state. That route and
the two-cycle cold-link matrix remain unverified with the real engine because
the run stopped at the first failed Play case. No claim of six successful real
launches is made.

```sh
sh tests/launcher/run.sh
python3 tests/launcher/lifecycle.py
python3 tests/launcher/cold_url.py
timeout -k 10 360 python3 tests/launcher/real_handoff.py 'build/package/ws36/CoD2 Silicon.app' \
  --game "$HOME/Games/CoD2" \
  --mac-binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --output "$HOME/Library/Application Support/CoD2-native-ws36/handoff-unique-final"
```

## Gates and merge notes

Both stock and CoD2x native builds pass. Saved engine compile databases retain
engine flags for ABI audits; fixture databases use `-UNDEBUG` so assertions run.
The full ABI checks report 621 stock / 638 CoD2x translation units, **zero
errors/mismatches/new entries**, 218 renderer bindings with zero table/cast/
floating-call mismatches, and zero proven import-indirection errors.

All ordinary CONTRIBUTING fixture suites passed, including tools/datagen (including 16 tools tests after the lock-state fix),
ABI checker units (4 audit + 6 import tests), shader setup, Wine parser, sanitized CoD2x/full/native/HTTPS, SDK identity,
stock/CoD2x online and fixes13, perf fixtures, LP64 game/script/renderer,
real-CGL raster/mips/volume uploads, HUD/marks/FX and dedicated FX. Private
datagen round-trip, layout and fixes13 reference checks passed. Platform CTest
passed **25/25 sanitized** and **27/27 plain**, including windowed, borderless,
exclusive, backing, input, audio, diagnostics and crash tests. Launcher checks
passed 26 primary and two fallback snapshots with clean shape audits.

```sh
cmake --build build-macos --target cod2_macos --parallel 3
cmake --build build-macos-codx --target cod2_macos --parallel 3
ctest --test-dir output/ws36/platform-san --output-on-failure
ctest --test-dir output/ws36/platform-plain --output-on-failure
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws36/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws36/abi-codx
COD2_BUILD_BACKGROUND=1 scripts/package-release.sh --build-dir build/package/ws36
python3 tests/packaging/launcher_bundle.py 'build/package/ws36/CoD2 Silicon.app'
python3 tests/packaging/first_run.py
timeout -k 10 180 python3 tests/packaging/release_smoke.py 'build/package/ws36/CoD2 Silicon.app' \
  --game "$HOME/Games/CoD2" \
  --mac-binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --output "$HOME/Library/Application Support/CoD2-native-ws36/release-smoke-new"
python3 tools/abi/legacy.py --base 3a916da
python3 tests/fixes13/legacy.py --base 3a916da
git diff --check
```

The package builds with pinned portable SDL libraries and passes the bundle
audit, first-run checks and release smoke: empty scratch home, menu, Toujane,
two game JPEGs, quit 0, launcher return log and bundled SDL2/SDL3 only. The
release artifact is ad-hoc signed, not notarized. Headless parity recorded 100
contiguous nonempty-world frames using `tools/parity/record.py --dedicated
--frames 100 --port 29936`; the client rerun still fails the strict contiguous-frame reader (record 9).
The observer runs after `Com_Frame`, while a client outer frame can advance
multiple simulation frames. Headless success is not a substitute for this
client trace gate. The reader was not weakened; client trace instrumentation
remains open.

`tools/abi/legacy.py` now reports **15 modified files × five COD2_X64-off
configurations, zero mismatches**, including the modified fixture bodies.
Native-only fixture additions were guarded so their inactive bodies also match.
The raw CoD2x inactive-feature checker reports 65 differences on this whole
workstream: its `arm64-codx-off` and `COD2_X64=0`-defined configurations include
intentional stock-native fixes. It is a CoD2x-feature-only checker, not an
applicable zero-diff assertion for stock-native behavior changes. It was run and
was not weakened. The separate native-off checker is green.

Linux i386 object/binary byte comparison remains an external gate. This Mac
cannot build/run that Linux multilib configuration; `tools/ci/compare-x86.sh`
rejects the host. The orchestrator must run the documented comparison on the
appropriate Linux host before claiming byte-identical binaries. No legacy build
flags, data blobs or shared headers were changed here.

Merge the WS36 commits after `3a916da`, or merge the branch while retaining its
existing WS28/focus ancestry. Shared-file changes are confined to adding the
native Cocoa bridge/AppKit link in `cmake/macos-arm64.cmake`; `CMakeLists.txt` and
`src/headers/*` are untouched. `r_dpvs.c` is the important overlap with Metal:
retain the native pool, projection, scratch and far-plane fixes. The display
bridge/input latch and launcher/bundler default changes must travel together.

Post-fix review: reuse, quality and efficiency reviewers examined the WS36
scope read-only. One redundant camera-origin expression was removed. Bounded
test-log polling was retained for clarity. Native-window caching was deferred
because the window lifetime across SDL fullscreen transitions was not verified.
New live probes were reviewed and strengthened to reject stale onboarding
markers, missing focus and wrong pulse cadence. Final runtime evidence, rather
than synthetic tests alone, determines the open items above.

## Open work for the orchestrator

- Unlock/isolate the shared desktop for the real launcher acceptance and quiet
  borderless/exclusive matrix; the branch is not complete hands-on QA acceptance.
- Resolve exclusive sleep/wake mode restoration and verify the user's precise
  all-black symptom. Keep the default desktop fullscreen and visibility latch.
- Confirm and close the intermittent floor symptom with broader sweeps or the
  user's precise camera/demo; current finite sweeps did not reproduce it.
- Fix the real fast-launch/return window and focus lifecycle, using the saved
  candidate only as diagnostic work, then rerun all Play/Deploy/cold routes.
- Correct client parity instrumentation: both retries emitted frames 7–14, then
  18, yielding 100 records but a strict-reader failure at record 9. Dedicated
  recording passed. Preserve the fail-closed reader.
- Run the external Linux i386 artifact comparison. The local native-off source
  checks supplement that proof and do not replace it.
- The native Connection label remains a UI difference against the Wine image.

Every new WS36 commit has the CONTRIBUTING DCO sign-off. Only our unpublished
WS36 sequence was amended to add those trailers; its base/WS28 ancestry is
unchanged. No push, PR, issue or remote operation was performed.
