# Native GPU budget accessor follow-up

Updated 2026-11-05.

**One assigned row reconciled: one intentional.** This verdict covers
`RB_GpuSyncDelay` at Mac 0xd44d8 in the built-in Apple native renderer. It
requires GPU sync commit `8cfdc588b47050236f77c511e3650acddab04939` to be integrated
first. That immutable commit was read directly in the GPU sync worktree; it is
not present in this report's starting checkout. The old zero-clock source alone
does not satisfy this verdict. This follow-up changes no production source.

## Original accessor and callers

Exact-symbol disassembly of Mac 0xd44d8-0xd44e7 is a plain integer getter:
load the `dx` pointer, load its word at offset 0x2d60, return it unchanged. The
binary pointer at 0x1accf39 resolves to the `dx` symbol at 0x1272820. The accessor
does not wait, mutate the delay, consume prior wait credit or retire a fence.

Mac `GetRefAPI` at 0xcaa68 accepts version 60. Its static export table places
`R_IsGpuFenceFinished` at offset 0x150, this integer getter at 0x154, and
`R_SetLodOrigin` at 0x158. The pointer loaded at 0xcadfe resolves from 0x1acd111
to 0xd44d8. `R_IsGpuFenceFinished` at 0xcaa5e only tail-calls
`RB_IsGpuFenceFinished`; it adds no synchronization or lifetime owner.
`CL_InitRef` requests version 60 at 0x148424 and copies the 0x15c-byte table into
client `re` at 0x1311620.

The budget consumer is `CL_SyncGpu` at 0x14a726. It snapshots the low 32 TSC bits,
calls `re+0x154` once, then tests `re+0x150` before reading elapsed time. A
finished fence returns immediately. An unfinished fence returns when the
32-bit elapsed subtraction has its sign bit set or is at least the signed budget;
otherwise it sleeps one millisecond and tests the fence again. A zero or
negative budget still permits that first fence test. `CL_Input` calls it before
active-client command sending at 0x18a1e2; the fullscreen per-frame branch calls
it at 0x14a842. A full-text reference scan found those two direct callers and the
single client budget-slot call, followed by hand reading of their exact entries.

## Native API ownership

The active native `refexport_t` declaration retains API59's
`IsGpuFenceFinished(void)`, void `SyncGpu(void)`, `GpuWaited(int)` and
`SetLodOrigin` fields. The built-in `CL_InitRef` requests version 59 and copies the
typed table. In the required commit, Apple native `GetRefAPI` assigns the first three
callbacks directly, with no return-value cast. `CL_SyncGpu` calls the void
callback, and the fullscreen branch also calls `CL_SyncGpu`.

That callback, `RB_AdaptiveGpuSyncWait`, owns the original getter operation and
client loop together. It reads the clock, snapshots `dx.gpuSyncDelay` once, then
performs the same fence-first polling, signed 32-bit timeout and one-millisecond
sleep order. Returning on timeout leaves the issued fence intact. A completed
fence is deleted and its issued flag cleared by `RB_IsGpuFenceFinished`, matching
Mac 0xd4472. Neither the getter replacement nor the polling loop updates the
budget. `RB_GpuWaited` remains a setter for `gpuSyncAlreadyWaited`; no native
client caller uses it, and the Apple wait path does not read or clear that field.
Mac's consumer has no such credit operation.

The renderer export table is static; client `re` and `dx` are global storage.
Imports are copied rather than retained from the client's stack-local table.
The callback retains no local pointer or budget snapshot after returning.
`CL_ShutdownRef` synchronizes, shuts down and clears client `re`. The native
frame path calls the backend directly, and the required commit removes the
extra frontend budget update. Budget adjustment remains in the renderer's
mode 3 backend paths, using the same `dx` state and clock as the wait callback.

The existing version 60 route in native `GetRefAPI` is a Windows-slot adapter,
not the Mac table above. Its mapping sends API59 slots 84/85/86 to Windows
slots 105/106/106 and LOD slot 87 to 107. The built-in Apple client does not select
it. This verdict does not certify that Windows adapter or an external Mac API60
consumer, and adds no purported Mac API60 accessor export.

## Clock adaptation and verification limits

The required source replaces the old constant-zero clock with
`low32(floor(MacSystem_Nanoseconds() * snapshotHz / 1000000000))`. A
`pthread_once` snapshot obtains integer Hz from the established CPU-frequency
provider, with an 800 MHz fallback for an invalid scale. Both backend adjustment
and client waiting use this helper. The monotonic nanosecond conversion uses
128-bit multiplication before division; it does not reinterpret the original
20000-tick budget decrement as a fixed duration. Its rate and instruction costs
are an explicit native adaptation, not identical Intel TSC timing.

The accessor, API construction, client initialization, both callers, fence
retirement and relevant backend budget writes were hand compared with the Mac
binary and the required source. The triage, constant and view helpers correctly
report no native getter body; that missing symbol is accounted for by the
reached typed wait callback, rather than an unused surrogate. Read-only
preprocessing with the dependency's actual arm64 Release database confirmed the
active API declarations, assignments and built-in API59 call path.

No new tests, cases, assertions, build, full CI, replay or GPU execution ran for
this report-only follow-up. The required commit's production changes were not
recompiled or exercised here; integration and combined client validation remain
the integration owner's responsibility. No assigned behavior remains unresolved
within the documented built-in Apple API/clock adaptation. Private evidence remains under
`~/Library/Application Support/CoD2-native-decomp/recon-gpu-budget.noindex/`.

## Integrated batch 19 verification

Production 6ae3bd5 and the catalog/existing-fixture follow-ups through 4ee16df passed both dedicated builds, full local CI02–13, source guards, actual OFF line expansion checks including the C++ bridge and Objective-C owner, unit parent/independent reviews and independent combined-source review. Integrated production/build/fixture files exactly match the reviewed private composition. Both 2,000-frame dedicated replays exactly match both d873794 baselines in players, state hashes and entities; all six Linux logic fields agree, maximum origin/velocity drift0.11792/2.0.

Existing raster/saved-screen checks and 18 depth cases passed ASan/UBSan. The depth fixture uses the actual typed vertex-shader owner and provider accessors, real borrowed-buffer AddRef/Release methods, existing outer depth defaults and actual fence/index/dispatch providers. Its fallback mode explicitly covers the approximation path, not shader-free native validation. GL cases and result assertions are unchanged. The catalog was generated twice identically: only the restored r_mode/r_displayRefresh registration sites change factual metadata; other changes are source provenance. Initial CI02 failed stale coverage and its complete retry passed after regeneration. The existing Swift loader then passed after updating its revision literal and display consensus expectation; no assertions/cases were added or removed. Fresh source/resource review confirms the preserved copy/restrictions and consumer contract. No tests, cases or assertions were added.

This batch closes 24 capability, defined-boundary, GPU budget, render-state, shader factory, mutex/Optimize exception, VM recovery, startup splash, indexed-draw and retired platform-boundary entries. Two formerly equal splash no-op lifecycle verdicts become intentional Windows-style native ownership. Indexed drawing restores GL context state and separate logical caches around the explicit approximation; extended formats use the original driver and independent color buffers. The inherited multi-color scratch alias found during fresh review is corrected. Licensed/source-shader approximation and bounded storage adaptations remain explicit; no visual-equivalence claim follows from whole-row intentional closure. Original fresh-context state defaults are explicitly restored where native Reset retains its context. Exception cleanup/output ordering, malformed opcodes and unreached registration/state paths remain compiled and reviewed where fixtures did not execute them. Existing builtin/setter VM checkpoints are an explicit native adaptation, not original zero-opcode longjmp equivalence. Actual startup visibility, sampled-texture journal entries, fallback-to-normal transitions, multiple converted colors and their reallocation/reset branches remain compiled/reviewed only. Dedicated replay does not exercise rendering; this is not a complete rendered-frame acceptance.
