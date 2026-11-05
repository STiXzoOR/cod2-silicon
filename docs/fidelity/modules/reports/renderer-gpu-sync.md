# Native GPU synchronization follow-up

Updated 2026-11-05.

**Five assigned rows reconciled: two fixed, three intentional.** The Mac i386
binary is the authority for control flow and arithmetic. The Apple native clock
and established API59 boundary are intentional adaptations, not claims of
identical hardware timing or a Mac API60 export layout.

## Caller, callback and clock

Mac `CL_SyncGpu` at 0x14a726 reads the low TSC, obtains an integer budget through
`re+0x154`, then polls `re+0x150`. It returns when the fence completes, elapsed
ticks have the sign bit set, or elapsed ticks reach the budget; unsuccessful
polls sleep for one millisecond. `CL_Input` uses this before sending active-client
input, and the fullscreen branch of `CL_RunOncePerClientFrame` uses it too.

The built-in native client requests API59 in `CL_InitRef`. That layout retains
`IsGpuFenceFinished`, a void `SyncGpu`, `GpuWaited(int)`, and `SetLodOrigin`.
Typed assignments now bind its GPU callbacks without return-value casts. The
void wait callback implements the complete Mac client loop inside the renderer,
where it reads the budget and uses the same clock as the backend. No slot,
structure size, or API version changes. The unused native `GpuWaited` setter is
preserved; the Mac loop does not consume its credit.

Mac `GetRefAPI` at 0xcaa68 instead checks version60 and exports the fence query,
integer delay query and LOD origin at 0x150, 0x154 and 0x158. The existing retail
Windows version60 adapter uses a different layout, including GPU mappings at
105/106 and LOD origin at107. It is preserved and is not a Mac API60 provider.
No built-in Apple client caller requests that adapter. Its Windows DLL path was
read but was not exercised or newly certified by this follow-up.

Apple native synthetic cycles are
`low32(floor(MacSystem_Nanoseconds() * snapshotHz / 1000000000))`.
`pthread_once` snapshots the existing `MacFeatures_GetCPUSpeedInGHz` provider,
including its established 800MHz fallback. Conversion to integer Hz truncates
the provider's float-derived value. Values outside `[1, 2^64)` use 800000000Hz,
which also excludes nonfinite conversion. The multiplication uses 128 bits;
the product of two 64-bit unsigned values fits before division and the final
modulo32 conversion. This scale is stable for the process. It preserves the
original integer tick arithmetic without interpreting 20000 cycles as a chosen
number of microseconds. The existing platform clock, millisecond rounding,
performance-counter units and frequency providers are unchanged.

## Fence ownership and corrections

Mac `RB_ExecuteRenderCommands` at 0xd8958 waits and issues a fence before drawing
only in mode1 (0xd8a04/0xd92bf). Mode2 waits for the previous fence and issues the
next after drawing. Mode3 polls after drawing: an already finished fence changes
the budget to `max(wrapped32(delay-20000),0)/128*127`; an unfinished fence waits
with one-millisecond sleeps and adds nonnegative elapsed ticks divided by32.
Other modes reset the budget and issue no fence. These paths replace the native
extra fences in modes0/1, misplaced mode3 wait, zero clock and unrelated mode2
moving-average calculation. Mac's negative-elapsed branch at 0xd957d abandons
the issued flag without deleting the old fence; that quirk is retained.

Completed-fence tests delete the ID and clear `flushGpuQueryIssued`, as Mac
0xd4472 does. Native mode0 additionally polls at backend entry, before disabled
rendering and lost-device exits. This retains the old native cleanup owner when
sync is switched off. No fence is replaced on those exits. Modes1/2 retire the
previous fence before replacing it; mode3 does so except for the Mac abandonment
quirk. Benchmark fence helpers retain their existing ownership.

`R_EndFrame` no longer performs the extra frontend delay update; Mac 0xc7ea3
starts with cache unlock. The already reviewed cache, temporary-storage and
command work remains intact. The actual Apple skinned-cache prerequisite always
uses DISCARD storage, independently of the GPU-sync mode.

Mac 0x14a7bf-0x14a7dd clamps a negative or wrapped `frame_msec` to200 using an
unsigned comparison. Native used a signed comparison. Native subtraction now
uses modulo32 arithmetic and the clamp is unsigned. The fullscreen branch calls
the common `CL_SyncGpu` entry. Avidemo, FPS and realtime paths were hand compared.
The two fill-test warning strings at Mac 0xd8b26/0xd8b37 were also corrected.

## Verification and limits

All four owned source files compiled as arm64 Release objects using the copied
actual client database remapped to this worktree, with
`-Werror=incompatible-function-pointer-types`. The dedicated Release target
`cod2_macos_ded` configured and linked successfully. The final backend object was
recompiled after the warning-string correction. Builds ran at background nice19
priority with no more than two build jobs.

The existing hardware and frame-wait tests passed in Debug with assertions
enabled: the frequency provider reported 4.788GHz and absolute waits did not
return before their deadlines. These exercise providers, not the GPU loops.
The existing OFF guard check passed20 comparisons, and a separate preprocessing
check retaining real `__LINE__` expansions passed20 comparisons. All four
modified sources contain no `__LINE__` macro. The heuristic triage and constant
checks ran for the four modules; every assigned function was also read from
exact-symbol `otool` output. `git diff --check` passed.

Source review began at624d3967581c90dc951cac42681f29b0985bd57e, whose three seeded
renderer files preserve the reviewed frame/backend/GL prerequisites above
integration c1bb3b06430cd1df781d83ce47532542ff8ed198. Actual pending providers were
read from device9f05cde, buffers33c898e, worldafa511f, GLcd687d6, arrays510b8a7,
shaders51608e5 and frame d187901. Their shared declarations already supported the
owned-file compilation; no private overlays were required. The dedicated link
does not validate the combined client renderer graph. Parent integration owns
that prerequisite chain and its client verification.

No new tests, full CI, replays, client launch or live GPU synchronization runs
were performed. The GPU paths and frame-clamp correction are compiled and
reviewed, not exercised. No assigned row remains unchecked within the documented
Apple native adaptations. Private disassembly and logs remain under
`~/Library/Application Support/CoD2-native-decomp/recon-gpu-sync.noindex/`.

## Integrated batch 17 verification

Combined source and existing fixtures at a0de0a7 passed both dedicated builds, complete local CI steps 02–13, source guards with five inactive configurations unchanged, actual legacy line maps, per-unit parent/independent reviews and corrective combined-source review. The initial combined review found a matrix-mode cache surviving GL context recreation. Lifecycle, stream and target ownership were integrated together so the real COpenGL initialization fixes that cache contract before acceptance; the earlier validation cycle was superseded. Corrective review also found stale retained-context light defaults; the native initializer now explicitly uploads them. Full client linking exposed the missing native byte draw-flag import, now bound to actual storage initialized from original data0x34142c. The final full gate restarted after both repairs. Its ABI audit then exposed a dedicated-only placeholder still declared void(void); the native dedicated signature now matches the seven-parameter client provider, preserving its inactive link body. The complete stock/CoDx ABI step was rerun using the exact Release compile-command snapshots saved before fixture assertion adjustment; the failed original log is retained. Initial08 fixture imports and09/10 client-link failures remain in the superseded logs.

Existing GL fixtures now supply the restored state/math providers and required viewport setup. The dispatch fixture resolves the additional Classic Clear imports with its existing failure continuation, and depth now uses the actual draw-flag provider. Their scenarios and result assertions are retained; the obsolete native aborting matrix shim was replaced by the real provider. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. The all-file guard helper cannot preprocess the unchanged Apple-only dispatch restriction in its six synthetic configurations; the source-only guard run passes, and separate diagnostics restrict all12 preprocessing errors to that fixture in base/head. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This combined batch closes 108 color/math, asset, GPU timing, shader compiler and device state/lifecycle/stream/target entries. Full render-state dispatch, shader factory, registration/capability and indexed-draw follow-ups remain separately tracked. Original undefined-storage cases and native API/context/buffer mapping adaptations are explicit. Dedicated replay does not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
