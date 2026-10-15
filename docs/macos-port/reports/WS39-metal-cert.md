# WS39 — Metal admission, ownership and certification

Branch: `port/metal-cert`. Starting commit: `42d9b4e`, on WS37 `4df6fbf`.
Read the complete PLAN, ABI-1 interface document and WS35/WS37 reports before
working. All edits and commits are in this worktree. No pushes, PRs, issues,
remote changes, system installs or sibling-worktree access occurred. Licensed
data, recordings, captures, shader output, reference disassembly and complete
logs remain outside git.

**Retain GL as the default.** Admission and ownership fixes are implemented,
but this branch does not satisfy every default-renderer certification gate.
Full presentation freshness and strict certification remain incomplete.
Complete logical-input/factory receipts and visible smoke-cloud coverage
remain gaps. Mounted-turret firing now has
actual GL and Metal receipts after the native broadphase repairs.
The results below distinguish pixel diagnostics from certification.

## Changes and merge boundaries

- Native presentation admits at most one pending presentation GPU job before
  putting `waitForDrawable` on the shared queue. It preserves READY mailbox
  entries when admission is refused, then selects the newest completed entry.
  GPU completion releases admission credit. Physical presented callbacks retain
  only the observer function, so they can safely arrive after presenter destruction. GL presentation retains its
  existing path.
- Producer admission distinguishes healthy slot exhaustion from transport
  failure. Whole-frame skips stay latched until Present and are reported as
  `skipped frames (backpressure)` and `skipped_draws_backpressure`. Draw errors
  remain errors. Private counters report partial-frame and lifetime failures.
  The fixture fills both producer slots, retires them during a rejected frame,
  proves its suffix stays rejected, then proves the next scene is admitted.
- GPU feedback records actual Metal-4 `GPUStartTime`/`GPUEndTime`, commit time
  and CPU time spent enqueueing the drawable wait. Backing diagnostics report
  scene, IOSurface, row pitch, drawable count, view, layer and compositor scale.
  `wait_enqueue_ms` is an API-enqueue duration, **not** the GPU drawable-wait
  duration. Commit-to-GPU-start also includes preceding queue work.
- Native `Dvar_MakeExplicitType` removes string aliases in owner order. After
  current is removed, latched compares against the remaining current/reset;
  after latched is removed, reset compares against the remaining owners. The
  old saved-alias comparisons could leave nobody responsible for freeing a
  shared string. The ASan production fixture checks preserved integer/string
  values and balanced allocations over sixteen conversions/shutdowns. It
  failed on the starting ownership implementation and passes after the fix.
- Bundled SDL remains 3.4.10 with sdl2-compat 2.32.72 and a minimal Cocoa mode
  ownership patch. Its completion stamp includes the patch digest. BSD patch
  needs `--force` for the reverse dry-run: `--batch` can silently reverse its
  own interpretation and falsely report an unapplied patch as already applied.
- Opt-in `COD2_MAC_COMBAT_TRACE` reports actual player weapon indices, owned
  weapon ammo/clip counts, entity events, turret lock state, effect templates,
  advancing FX time and live primitive populations. It does not give weapons
  or manufacture gameplay outcomes.
- Mounted-turret testing exposed an independent reconstruction error in
  `Player_GetUseList`: the Mac reference at `0x1ccb20` tests byte +2 of
  `r.contents`, equivalent to `0x200000`; reconstructed code tested
  `r.svFlags & 0x20`. Stock turrets have contents `0x200004` and zero svFlags.
  The native-only correction restores that reference check. Its ASan
  production-function fixture fails on `42d9b4e` and passes on this branch,
  including unrelated svFlags and ordinary item cases. This shared gameplay
  correction applies to both native renderers; coordinate its focused commit
  `de154b2` with WS38 when merging.
- A second mounted-turret diagnosis found `ClientEndFrame` assigning living
  players `active=1` every server frame. Cursor-hint processing then returned
  before `Player_GetUseList`. The Mac reference writes byte +0x161, `takedamage`,
  at `0x1a4a5e`, and clears it at `0x1a4e37` for dead players. Both assignments
  are corrected only on native builds; turret activity is retained. Actual
  diagnostics and reference disassembly remain external. Commit `c663ad8`
  contains this correction. `unifdef -UCOD2_X64` + `cmp` of the complete file
  against `42d9b4e` passes. The next probe reached the use query but returned
  no candidates beside grounded stock turrets.
- `cebe239` restores the temporary box model's conservative brush aggregate
  mask (`-1`, terrain mask `0`), matching the Mac loader. The old zero aggregate
  made `CM_LinkEntity` unlink every temporary-model entity. Enabling linking
  exposed a real GL crash in `CM_UnlinkEntity` during teleport: list insertion
  used zero-based entity IDs, while query/unlink decode one-based IDs and zero
  terminates the chain. `ed2e6e4` corrects the two native helper conversions.
  Its ASan production-function regression fails before the correction and
  passes after it, covering entity IDs 0/1023, sorted queries and head/middle/
  tail unlinking. All LP64 game fixtures pass. The initializer and link repair
  are a dependency pair; do not merge the initializer alone.
  Actual final GL and Metal runs mount and fire the stock Carentan turret.
- Capture admission now gets candidate PIDs with `pgrep -f`, then verifies
  each executable with `ps`. `pgrep -fl` can emit multiline shell command
  continuations, which the old parser mistook for malformed process records.
  Malformed PID records still fail closed; no foreign process is killed.

No top-level CMake or `src/headers/*` changes. ABI-1 provider headers/tables are
unchanged. `metal_frame_stats.h` is private. No ABI-2 proposal is needed. GL
renderer code/default selection is unchanged; the native ownership and
reference gameplay corrections above are explicit common-engine changes.
Every production change that reaches the original build has an unchanged
`COD2_X64`-undefined branch.

## Machine, evidence and run safety

Host: macOS 27.0.1, arm64 Apple M6, Xcode clang 21. The PA27JCV desktop is
6016×3384 physical pixels, UI 3008×1692, 60 Hz. Development builds resolve
Homebrew SDL 3.4.16; the packaged/private-soak binary resolves the patched
pinned SDL 3.4.10 instead. Neither Homebrew nor global configuration changed.

All evidence is under:

```sh
E="$HOME/Library/Application Support/CoD2-native-ws39"
```

Launches checked actual `cod2_macos` processes, waited for other games, used
private homes/free UDP ports and GNU `timeout -k`. Failure cleanup addressed
only owned process groups. Timing runs acquired `_agents/bench.lock` and
released only the lock this driver acquired. WS38's status still said `start`,
so timings are marked contended; no quiet-machine claim is made. Initial session
snapshots reported a locked screen; the later timing matrix was unlocked.
The session state was not bypassed or changed.

An excluded observer attempt injected its handler into the owned `timeout`
wrapper instead of the leaks-launched game; SIGUSR1 terminated that wrapper's
run. It is a controller failure, not an observed engine crash, and contributes
no soak or pacing acceptance result. The clean repeat has no signal observer.

## Ordinary soak and admission

The final direct-game run, at `cebe239` with all broadphase repairs and the
verified patched pinned SDL, completed **603.599 seconds** with normal quit:

| Final source soak result | Observation |
| --- | ---: |
| Client frames | 74,119 |
| Skipped frames (backpressure) | 7,487 (10.101%) |
| Partial-frame drops | 0 |
| Lifetime drops | 0 |
| Encoded draws | 7,392,702 |
| Dropped / unready / rejected draws | 0 / 0 / 0 |
| Skipped draws (backpressure) | 831,506 |
| Longest skip streak | 29 client frames; 1,575.955 ms including loading |
| Project-owned leaks | 0 |

Every one of the 7,487 backpressure rows has zero encoded draws and zero
failures. All 74,119 frame rows have zero failures; GPU error callbacks are
zero. Producer GPU mean/p99/max is 4.383/19.291/75.008 ms, commit-to-start
p99/max 11.313/67.151 ms. Presenter GPU p99/max is 1.520/7.096 ms,
commit-to-start p99 20.882 ms and CPU wait-enqueue p99/max 0.0218/0.0543 ms.
The `leaks` wrapper exits 1 for its exact framework cycles; the driver and
fail-closed leak gate both exit 0. There is no physical observer in this final
packaged-dependency leak run, so it proves admission/ownership, not freshness.
Evidence is `$E/soak-world-final/`, including `ws39-summary.json`;
`$E/world-final-validation.json` also records the subsequent GL/Metal Toujane
smokes, package/signature checks and legacy comparison, all exit 0.

Earlier runs below document admission diagnosis and the separate physical
freshness evidence; they do not replace the final-source result.

The first ordinary soak ran 604.067 seconds with Metal validation,
MallocStackLogging, 1280×720 windowed, cap 125, menu-selected weapons, reloads,
grenades, map restart and map changes. `COD2_MAC_GOLDEN_DEMO_TIME` was absent.
It loaded the earlier SDL build before the patch-application guard correction;
the final repeat uses the verified patched dependency.

| First soak result | Observation |
| --- | ---: |
| Client frames | 73,465 |
| Skipped frames (backpressure) | 324 (0.441%) |
| Partial-frame drops | 0 |
| Lifetime drops | 0 |
| Encoded draws | 7,646,672 |
| Dropped / unready / rejected draws | 0 / 0 / 0 |
| Skipped draws (backpressure) | 23,683 |
| Longest skip streak | 9 client frames; 136.132 ms including loading |

The patched-dependency clean repeat completed 602.551 seconds: 73,727 frames,
3,334 backpressure skips (4.522%), 0 partial and 0 lifetime drops. It encoded
7,625,820 draws with 0 dropped/unready/rejected and 378,731 skipped draws.
All 3,334 skipped trace rows had 0 encoded draws and 0 failures; no GPU error
receipt occurred. Maximum skip streak was 39 frames / 1,887.232 ms including
loading. Its producer GPU mean/p99/max was 4.554/15.304/57.352 ms; queue delay
p99/max 9.465/315.766 ms. The leaks wrapper exited 1 for the allowlisted cycles;
console receipts show normal client/server shutdown. The driver completed.

These two soaks predate `326598e`. The first physical matrix exposed that
waiting for a **physical callback** to release admission held credit too long:
1080p presented at about 20 FPS / p99 83.3 ms despite producer GPU p99 under
6 ms and roughly 60 Hz display-link updates. The final implementation releases
credit on GPU completion. The completion-credit fixture fails on the old source
and passes after this correction; both complete platform suites also pass.

After `326598e` and the single-count correction, the observed ordinary soak
completed 603.633 seconds with normal quit: **74,060 client frames, 3,244
backpressure skips (4.380%), 0 partial and 0 lifetime drops**. Encoded draws
were 7,662,721; dropped/unready/rejected were 0/0/0; skipped draws were 382,436.
Every skipped trace row had zero encoded draws and failures. There were zero
GPU error callbacks. The longest producer skip streak was 24 frames /
1,530.740 ms including loading.

The physical observer recorded 31,556 intervals: 52.594 presented FPS,
p99 **46.202 ms**, max 1,750.024 ms including map changes. Producer GPU
mean/p99/max was 4.686/7.957/28.104 ms; presenter GPU p99 was 0.224 ms.
Producer and presenter commit-to-start p99 were 14.018 and 19.208 ms;
CPU drawable-wait enqueue p99/max was 0.021/0.058 ms. Thus zero-drop admission
passes, but the physical freshness gate fails in this run. Raw events and GPU
feedback are preserved in `$E/soak-observed/`.

The observer was loaded into the verified game PID through a post-leaks `env`
argument. That launch produced **no leak total**; the exact tool/exec interaction
was not established. The fail-closed gate correctly exits 1 for a missing total.
It contributes pacing/admission diagnostics only. The earlier clean leak soak
remains separate evidence.

With layer synchronization and `c663ad8`, a further ordinary soak completed
604.087 seconds: **74,023 client frames, 318 skips (0.430%), 0 partial and
0 lifetime drops**. It encoded 8,838,977 draws, with dropped/unready/rejected
0/0/0 and 26,000 skipped draws. All skipped rows contain zero draws/failures;
GPU error callbacks and presented-sequence regressions are zero. Maximum skip
streak is 9 frames / 104.451 ms including loading. Physical intervals are
56.816 FPS, p99 33.334 ms, maximum 1,800.024 ms including map changes. Producer
GPU mean/p99/max is 5.092/8.070/55.809 ms; queue p99 is 0.317 ms. Presenter GPU
p99 is 0.301 ms and wait-enqueue p99 is 0.0219 ms.

A diagnostic subset with at least 60 producer completions/second and every
completed producer job below 16.667 ms has 33,784 intervals, p99 16.667 ms,
maximum 100.001 ms. This is an explicit filter, not a replacement for the
ordinary-soak gate. It shows that the remaining maximum gaps are not explained
solely by long scene GPU execution. The no-skip-starvation guarantee remains
unestablished under this contended run.

Both observer-assisted pinned runs also loaded Homebrew SDL through the
observer dylib, producing duplicate SDL Cocoa-class warnings. They are mixed-
dependency diagnostics, not packaged-dependency/leak acceptance. The wrapper
experiment again supplied no leak total. The final direct-game leak check below
uses only the patched pinned SDL and no observer.

The skip-streak duration measures producer admission and includes loading. It
is not a presented interval. The first two soaks had no physical observer. The
final observed soak provides the failed physical freshness result above. Zero
partial drops are independently checked by rejecting any backpressure trace
row with encoded draws or failures.

```sh
python3 "$E/drivers/soak-world-final.py"
python3 tools/metal/leak_gate.py "$E/soak-world-final/console.log"
```

## Project-owned leaks and SDL upstream draft

The minimal `tests/platform/sdl_cocoa_leaks.m` opens a small SDL/Cocoa window,
pumps its event/run loop, destroys it and quits. It contains no game code.
The Homebrew and pinned-dependency repros both produced the same three
AppIntents/LinkServices NSXPCConnection cycles (417 objects, 26,496 bytes in
the minimal repro). The first game soak had 414 objects / 26,320 bytes, all
under those same three cycle roots. No project-owned root remained.

`tools/metal/leak_gate.py` accepts only one stack block with exactly three
NSXPCConnection cycle roots and the thirteen ordered library/symbol frames
recorded in `FRAMEWORK_CYCLE`. Addresses/offsets may vary. A changed symbol,
unknown additional root/stack or ambiguous/missing total fails. Four parser
fixtures verify the fail-closed behavior. Framework-private objects are untouched.

The patched pinned minimal repro again had 417 objects / 26,496 bytes under
those exact three framework roots; the gate passed. The patched-dependency
clean game soak had 411 objects / 26,112 bytes, all under the same signature:
`project_owned_leaks=0`, gate passed. Commands and logs are
`$E/cocoa-repro-patched*.driver.log` and `$E/soak-final-clean/console.log`.
A direct-game check after layer synchronization and player-activity
correction ran 83.385 seconds with the patched pinned SDL, no observer and
normal shutdown. It found 411 objects / 26,112 bytes under the exact framework
signature; **project-owned leaks=0**, gate passed. The package was rebuilt and
its deep strict signature verified again. `$E/last-validation.json` contains
those exits. The final-source ten-minute run found 417 objects / 26,496 bytes,
all in the same exact three framework cycles: **project-owned leaks=0**,
gate passed. Its final package/signature checks also passed. No raw
CFArray/CFString leak root was allowlisted.

Checked upstream release 3.4.18 and main. Their generic
`SDL_SetDesktopDisplayMode` still frees the old internal allocation without
releasing Cocoa's owned CF mode array, and ignores new modes while fullscreen
is active. Cocoa's reconfiguration callback still supplies those allocations.
Thus a version bump alone would not fix the verified ownership gap. Sources:
[SDL 3.4.18 Cocoa](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.18/src/video/cocoa/SDL_cocoamodes.m),
[generic setter](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.18/src/video/SDL_video.c),
[main Cocoa](https://raw.githubusercontent.com/libsdl-org/SDL/main/src/video/cocoa/SDL_cocoamodes.m).

Draft upstream issue, **not sent**:

> Cocoa desktop-mode replacement loses ownership of the CF display-mode array.
> On SDL 3.4.10, also present in 3.4.18/main, GetDisplayMode creates Cocoa mode
> data owning its modes CFArray. The reconfiguration callback passes it to
> SDL_SetDesktopDisplayMode. The generic setter frees old internal data but
> cannot release its Cocoa CF ownership. When fullscreen_active is true, it
> ignores the new allocation as well. Release the old Cocoa modes before a
> real replacement; when the setter ignores a temporary fullscreen change,
> release the newly allocated modes/internal data instead. This port carries
> that small callback patch and retains the pinned-source checksums. A minimal
> SDL/Cocoa app and leaks --fullStacks --atExit can isolate framework cycles
> from SDL-owned allocations. Please advise whether platform cleanup should
> instead be centralized in a display-mode ownership callback.

The package command, plist validation and deep strict signature verification
passed. `otool -L` shows the app uses `@rpath` bundled SDL; the bundle contains
no Homebrew dependency or game data.

```sh
COD2_BUILD_BACKGROUND=1 scripts/build-sdl.sh "$E/sdl"
python3 tools/cod2x/make_macos_app.py build-macos-codx/cod2_macos \
  "$E/CoD2 Silicon WS39.app" --frameworks "$E/sdl/install" --replace
plutil -lint "$E/CoD2 Silicon WS39.app/Contents/Info.plist"
codesign --verify --deep --strict "$E/CoD2 Silicon WS39.app"
```

## Corrected-clock goldens and combat

`tests/metal/run_fx_clock.py` now expects advancing ordinary `mTime`, typed
freeze, forward stepping and backward warp with the golden environment
removed. The i386 fixture view remains its old view. The demo frame-time
freeze remains separate; there is no golden-only FX clock override.

All six corrected-clock map replays passed WS24's pixel metrics. GL/Metal
readbacks stay pre-gamma, lossless, unregistered and unmasked. They are
diagnostic results with `certified=false`: the common per-draw/pass/resource
digests and factory counters still are not emitted by the engine.

| Map | Views | Worst normalized RGB MAE | Fraction pixels >16 |
| --- | ---: | ---: | ---: |
| Toujane | 5 | 0.0000954932 | 0.000686849 |
| Carentan | 3, including interior | 0.0000044311 | 0 |
| Burgundy | 3 | 0.0000086692 | 0.0000238715 |
| Matmata | 3 | 0.0000153711 | 0.0000282118 |
| Leningrad | 3 | 0.0000142236 | 0.0000444878 |
| Dawnville | 3 | 0.0000492494 | 0.0000054253 |

Metal repetitions were byte-identical in all six. GL Leningrad spawn/weapon
repeats differed by MAE 0.0000011858, within the repeatability gate; do not call
those exact deterministic GL goldens. The other five GL maps repeated exactly.
The owned recordings and scene recipes are referenced by commands in
`$E/runtime.json`; no recording or map payload is committed.

Four additional ordinary-action recordings/replays named smoke-cloud,
explosion, muzzle-flash and decals passed their pixel metrics, worst MAE
0.000018672. Naming a shot is not effect coverage. Visual inspection found no
visible smoke cloud; the explosion target preceded the actual explosion
event. Live ammo consumption, offhand events and explosion effect creation
are real, but these four shots **do not certify dense visible FX**. The
explosion receipt was below the map; smoke consumption had no event-191/cloud
receipt in that recording. Primary fire produced actual muzzle/impact effect
templates, but a frozen frame must also visibly contain the effect/mark.

A latest-source repeat after the broadphase repairs (`$E/combat-dense3/`)
still consumed smoke 1→0 without event 191/cloud creation. The frag emitted
actual event 188 and `explosions/grenadeexp_blacktop`, but its origin was
`126.055,2274.358,-6447.427`, below the map. Waiting nine seconds for smoke and
five seconds for the frag did not establish visible coverage. The smoke replay
completed its pixel comparison (MAE 0.0000197511, fraction >16 0.0000108507,
exact GL and Metal repeats), while still containing no verified smoke cloud; remaining empty-scene diagnostic replays were
stopped using only verified owned controller/game process IDs and groups.
Their interrupted runs are excluded from acceptance. This common gameplay/
collision/effect-creation problem blocks dense grenade FX certification on
both renderers; renderer parity cannot certify an effect that was never shown.

| Authored class | Actual gameplay receipts | Coverage |
| --- | --- | --- |
| Rifle | Enfield clips 10→3; scoped Enfield 10→8; Bren 30→0; BAR 20→4; reloads | Menu-selected, fired |
| SMG | Sten 32→9; Thompson 20→4 | Menu-selected, fired |
| Pistol | Webley 6→0 and Colt 7→0 | Natural secondary, fired |
| Spread | Shotgun 6→3 | Menu-selected, fired |
| Frag | British 3→0; offhand events 167/165/166; explosion 188 and effect template | Consumed and exploded; visibility gap |
| Smoke | British 1→0; offhand events 167/165/166 | Consumed; cloud receipt/visibility gap |
| Rocket launcher | No authored pickup found in all fifteen installed stock MP BSP entity blocks | Conditional stock-map case unavailable; no `give` substitute |
| Mounted turret | GL and Metal: viewlocked=1, locked entity 397, weapon index 2; repeated event 175 firing receipts | Stock Carentan 30cal mounted and fired |

The initial turret attempts had viewlocked=0 and are excluded. A controller
also confused origin height with the eye-height argument to `setviewpos`; the
corrected attempts still failed until the common-engine repairs above. Final
receipts are `$E/combat-turret-world-{gl,metal}/console.log`, normal exit 0 on
both renderers. The final use-list/broadphase fixtures and actual mounted
firing are separate evidence. No `give` command supplied a turret or weapon.

## 4K, desktop fullscreen and performance

Before the presentation-admission change, a contended/locked 4K cap-250 probe
had producer GPU mean 3.967 ms / p99 5.705 ms (5,945 samples). Only four
presenter GPU jobs existed at startup. Client timing was 250 FPS / p99
4.039 ms; those are not display timing. This cannot reproduce WS37's physical
47–50 ms p99 while the screen is locked.

The unlocked 15-second matrix used the shared lock but was contended. GL
physical observations used `r_present metal` and `CAMetalDrawable.presentedTime`;
this is GL rendering with the presentation proxy, not proof of default GL swap
physical cadence. Client and presented intervals are separate.

The following baseline is after completion-credit admission and before layer
synchronization. Whole-frame skip rates are clipped to each physical-observer
measurement window, excluding preparation/shutdown. All native rows have zero
partial-frame rows and zero failure rows.

| Scenario | Client FPS | Client p99 ms | Presented FPS | Presented p99 ms | Backpressure skips |
| --- | ---: | ---: | ---: | ---: | ---: |
| gl-1920x1080-250 | 250.00 | 4.026 | 52.13 | 45.837 | n/a |
| gl-1920x1080-0 | 999.60 | 1.028 | 52.06 | 46.450 | n/a |
| gl-2560x1440-250 | 250.00 | 4.035 | 52.53 | 53.242 | n/a |
| gl-2560x1440-0 | 989.00 | 1.203 | 52.68 | 45.938 | n/a |
| gl-3840x2160-250 | 250.00 | 4.071 | 52.54 | 46.118 | n/a |
| gl-3840x2160-0 | 937.42 | 3.305 | 50.02 | 49.934 | n/a |
| gl-6016x3384-250 | 250.00 | 4.045 | 60.00 | 16.667 | n/a |
| gl-6016x3384-0 | 531.10 | 2.884 | 26.30 | 66.668 | n/a |
| metal-1920x1080-250 | 250.00 | 4.033 | 55.39 | 45.837 | 13.043% |
| metal-1920x1080-0 | 460.46 | 3.669 | 57.06 | 42.612 | 29.806% |
| metal-2560x1440-250 | 250.00 | 4.003 | 55.17 | 45.628 | 8.609% |
| metal-2560x1440-0 | 572.27 | 2.821 | 54.59 | 45.938 | 47.857% |
| metal-3840x2160-250 | 250.00 | 4.012 | 55.66 | 44.126 | 10.211% |
| metal-3840x2160-0 | 533.47 | 3.171 | 55.33 | 45.657 | 48.981% |
| metal-6016x3384-250 | 250.00 | 4.003 | 59.87 | 16.667 | 51.505% |
| metal-6016x3384-0 | 686.07 | 3.595 | 60.00 | 16.667 | 77.765% |

At 4K cap 250, producer GPU p99 is 5.883 ms and commit-to-start p99 is
14.057 ms. At 6K cap 250 those are 13.818 and 14.416 ms. Windowed p99 spikes
occur at every tested size on both renderers; the 6K desktop-fullscreen native
runs meet the 18.333 ms p99 gate. Producer execution alone does not explain
the windowed pacing. Exact 4K backing is scene/surface/drawable 3840×2160,
row pitch 15360, drawable count 2, view/layer 1920×1080 at scale 2 and compositor
scale 1×1. The desktop fullscreen backing is scene/surface/drawable 6016×3384,
pitch 24064, view/layer 3008×1692 at scale 2, compositor scale 1×1. There is no
extra desktop-size scale target. Producer timing includes the scene-to-mailbox
texture copy; presenter timing covers the gamma/output draw.

Paired 4K cap-250 windowed experiments isolated `displaySyncEnabled`. With
sync disabled, the two runs presented 56.996/55.661 FPS and p99 46.574/46.118 ms.
With native sync enabled, they presented 59.399/59.532 FPS and p99 33.334/16.667 ms;
display-link target intervals stabilized to 16.667 ms. The native-only change
is committed in `aade94e`. GL retains its previous setting. Two private latency-2
experiments gave p99 16.667 ms but no clear extra benefit, so latency stays 1.
The estimated queue gap after earlier submitted GPU work (presenter start
minus max(commit time, earlier GPU ends)) has p99 16.018 ms before sync and
0.123 ms after sync at 4K cap 250. This is a drawable-wait/scheduling proxy,
not an isolated GPU-wait timestamp; GPU overlap and driver scheduling limit
its attribution. CPU wait-enqueue duration is reported separately. No
target-size, scale-pass or bandwidth change was justified. See
[Apple's display-link timing documentation](https://developer.apple.com/documentation/quartzcore/cametaldisplaylink)
and [preferredFrameLatency](https://developer.apple.com/documentation/quartzcore/cametaldisplaylink/preferredframelatency).

The synchronized native repeat (15 seconds each, unlocked, contended; `0`
means uncapped) is:

| Native scenario | Client FPS | Client p99 ms | Presented FPS | Presented p99 ms | Backpressure skips |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1920x1080-250 | 250.00 | 4.031 | 59.47 | 16.667 | 0.800% |
| 1920x1080-0 | 530.48 | 2.804 | 59.67 | 16.667 | 36.709% |
| 2560x1440-250 | 250.00 | 4.032 | 59.40 | 33.334 | 0.880% |
| 2560x1440-0 | 503.05 | 2.826 | 59.53 | 16.667 | 33.501% |
| 3840x2160-250 | 250.00 | 4.032 | 59.67 | 16.667 | 0.400% |
| 3840x2160-0 | 513.71 | 3.109 | 59.53 | 16.667 | 42.499% |
| 6016x3384-250 | 250.00 | 4.028 | 60.00 | 16.667 | 38.017% |
| 6016x3384-0 | 723.26 | 3.364 | 60.00 | 16.667 | 80.018% |

Each native row has zero partial rows and zero failures. Native presented
p99 is at least as good as the GL proxy baseline at all eight scenarios here.
Capped 1440p still fails the 18.333 ms gate. The other seven p99 values pass,
but windowed max gaps of 33.334 ms overlap backpressure in the trace; producer
skip spans reached 35.873 ms at capped 1080p. Those traces cannot establish the
no-skip-starvation guarantee. At synchronized 4K cap 250, producer GPU mean/p99/max is
3.762/4.451/6.810 ms, queue p99 0.983 ms, presenter GPU p99 1.193 ms and CPU
wait-enqueue p99 0.0126 ms. At 6K cap 250, producer GPU p99 is 6.903 ms and
presenter GPU p99 2.803 ms. The synchronized layer removes the large queue
delay as well as the compositor-phase spikes. GPU timings, exact observer
bounds and raw CSVs are in `$E/sync-performance-summary.json`. The ordinary
soaks above are separate gates, not inferences from these short static samples.


Private three-drawable trials at capped 1080p/1440p/4K gave presented p99
16.667 ms in each but still maximum gaps of 33.334 ms. Skip rates were
1.627%/0.507%/0.720%, compared with 0.800%/0.880%/0.400% for two drawables.
This does not establish a consistent gain or remove the remaining gaps, so
no drawable-count change was adopted.

A quiet-machine repeat remains required. Layer synchronization fixes the
large windowed compositor phase spikes measured here; isolated 33.333 ms gaps
and the complete ordinary-soak freshness result still determine admission
certification, independently of average FPS.

## Build and merge gates

Both stock and CoD2x arm64 Release builds passed. The actual Release engine
compile databases retain `-DNDEBUG`; copied fixture databases use `-UNDEBUG`.
Both full ABI checks reached 0 errors / 0 mismatches / 0 new declarations:
stock 635 TUs, CoD2x 652; each 218 renderer bindings with 0 table/cast mismatches
and 0 unprototyped floating calls. Each import audit examined 515 symbols and
proved 0 extra / 0 missing dereferences. Both latest commands exited 0; logs
are `/tmp/ws39-abi-{stock,codx}-turret.log` and
`$E/gates/abi-{stock,codx}-turret/`. The two final broadphase corrections
change private helper arithmetic and initialization, not public signatures;
their ASan production fixture and final legacy comparison pass separately.

The sequential gate ledger is `$E/gates/results.json`. Passed suites:
tools tests (21 on repeat), datagen tests, ABI/import checker units, shader
setup, Wine trace parser, sanitized CoD2x full suite, CoD2x native suite,
SDK identity, online stock/CoD2x, fixes13 stock/CoD2x, perf source fixtures,
LP64 game and script, renderer/real shader raster/texture mip/volume upload,
HUD matrices, impact marks, FX events/primitives/cloud, dedicated FX,
datagen roundtrip, native layouts and fixes reference. The initial tools
timeout-cleanup fixture failed under contention; its repeat passed.

Every Metal runner passed: core, resources, states, targets, target shim,
draws, GPU draws, shader provider, raster, fixed, setup, observed states,
tess overflow, demo clock, ordinary FX clock, map lifetime, goldens unit tests.
`tools/metal/run.sh` translated/compiled the owned external shader cache.
The new production use-list ASan runner also passed (integrated into the
existing LP64 game runner). Translation produced 417/417 programs plus metadata
with zero failures; the provider loaded 417 libraries / 420 PSOs and stayed sealed.

Fresh final platform CTest passed 34/34 with sanitizers and 36/36 without,
including fullscreen/Spaces. The completion-credit regression failed on the
old presenter and passed on the corrected source in both builds. Final-source
Toujane smoke passed on both GL and Metal with normal quit and menu/courtyard
files (`$E/world-smoke-{gl,metal}.driver.log`).
The mandatory bounded CoD2x parity recorder ran and exited 1: line 9 of
statehash.txt has noncontiguous frames or a map restart. It is not a passing
parity recording; no trace was renumbered to hide the failure.

Canonical `COD2_X64`-undefined comparison reports eleven files × five
configurations, **0 mismatches**. Explicit `unifdef -UCOD2_X64` plus `cmp` of
dvar.c against `42d9b4e` passed. fixes13 legacy comparison passed. The
CoD2x-inactive checker reports 107 mismatches: it also requires native changes
to disappear when CoD2x is off and treats `COD2_X64=0` as defined. Those are
not this workstream's contract (CMake OFF omits the definition). It is recorded
as a failing check, not relabeled a pass. No CoD2x-specific behavior was added.
The Linux i386 object/binary gate exits 2 on this Mac: x86_64 Linux multilib is
unavailable. **Byte-identical i386 objects/binaries are not certified here.**
No packages were installed. `git diff --check` passed.

An unqualified all-target build also tried `cod2_macos_ded` and failed with
unresolved client/renderer symbols from generated data (for example
`R_RenderScene` and `CColorArray_Enable`). The required stock/CoD2x client
targets pass; do not report the all-target command as a pass. Logs are
`/tmp/ws39-build-{stock,codx}-sync.log`; the latest client-only builds are
`/tmp/ws39-build-{stock,codx}-world.log`. Dedicated FX fixtures pass separately.

Representative merge gates (full command/exit ledgers remain external):

```sh
cmake --build build-macos --target cod2_macos --parallel 3
cmake --build build-macos-codx --target cod2_macos --parallel 3
sh tools/abi/check.sh build-macos/compile_commands.engine.json "$E/gates/abi-stock-turret"
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json "$E/gates/abi-codx-turret"
python3 tools/abi/legacy.py --base 42d9b4e
ctest --test-dir build-macos/platform-tests --output-on-failure
ctest --test-dir build-macos/platform-tests-plain --output-on-failure
COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh
sh tests/cod2x/run_native.sh
python3 tests/lp64/game/run.py --build build-macos
python3 -m unittest discover -s tools/tests -v
python3 tests/metal/run_fx_clock.py
python3 -m unittest discover -s tests/metal -p test_goldens.py -v
sh tools/metal/run.sh "$HOME/Library/Application Support/CoD2 Silicon/shaders" "$E/gates/translated"
python3 tools/metal/leak_gate.py "$E/soak-world-final/console.log"
```

## Default-renderer decision and remaining work

Do not change the default on macOS 26+. Exact remaining blockers:

1. Full freshness/no-skip-starvation is not established. Capped 1440p p99 is
   33.334 ms; aggregate ordinary-soak p99 is 33.334 ms. A steady-GPU diagnostic
   subset meets p99 but has a 100.001 ms maximum gap. A quiet, controlled
   repeat and resolution of remaining gaps are required.
2. Strict logical draw/pass/resource/factory receipts are absent. Six-map and
   interior pixel comparisons pass, but cannot be promoted to certification
   without those matching input/resource receipts.
3. Dense visible FX and the complete combat gate remain incomplete. Smoke
   consumption produces no cloud creation receipt; the verified frag
   explosion is below the map. A target frame visibly containing explosion,
   muzzle flash and decals also remains unproved. Mounted firing now passes
   on both renderers.

The rejected CoD2x parity recording, unavailable Linux multilib comparison and
unqualified dedicated-target link failure are additional merge-verification
limits, separate from the default-renderer criteria above.

Merge the focused commits in order, including the BSD patch guard correction.
Keep the SDL patch and checksum-stamp logic together. Coordinate `de154b2`
with WS38 because it corrects common native gameplay, not Metal encoding.
Also coordinate `c663ad8`, `ed2e6e4` and `cebe239` (player activity, broadphase
links and the temporary-model content mask). Merge the list repair before the
initializer; the initializer alone exposes a teleport/unlink crash.
ABI-1 headers and GL default selection require no conflict resolution.
All captured/private evidence stays outside the merge.
