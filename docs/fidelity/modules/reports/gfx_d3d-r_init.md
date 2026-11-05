# gfx_d3d/r_init module reconciliation

Updated 2026-11-05.

20/22 reconciled: 14 equal, 5 fixed, 1 intentional, 2 unchecked. Mac 1.3 i386 is the primary authority.

The Mac resource constructor 0xcb24e creates one skinned-cache buffer. Native Apple now allocates/releases pool 0, after the commands unit forces pool 0 and the parent corrects Apple Lock to DISCARD. Non-Apple keeps its two-pool contract. Flush-query reset and fatal resource-creation diagnostics were restored. R_Shutdown now clears the window HWND, not its already-released swap-chain member, matching 0xcb0e9. Recovery uses typed native window/dvar reads, resets AA state when samples<=1, and matches reset/fatal progress messages.

Initialization now consumes pending device9f05cde caps after its CreateDevice establishes the SDL GL context, restores the verified capability fields and renderer-path selection, creates reset resources only for a new device, initializes the viewport and window bookkeeping, and initializes systems after reset resources. The exact pending fullscreen-word hunk is preserved. The native device deliberately consumes int[14] words (Windowed 7, interval 11, refresh 12); replacing that with an LP64 D3DPRESENT_PARAMETERS struct would break the actual provider. Native HWND is owned by SDL and is not packed into those words.

R_BeginRegistration remains unchecked: the original sorted mode/refresh domains, fallback AA/display retries, aspect-ratio negotiation, and two min/mag-anisotropy capability bytes have no complete matching native contract. Native DxGlobals has a selected anisotropy level, not those two capability fields; no shared header was changed. Native GL initialization and context lifetime also remain incomplete in the provider audit. GetRefAPI remains unchecked: Mac API60 has integer GpuSyncDelay at 0x154; native API59 separates void SyncGpu and GpuWaited. There is no defined native budget accessor/elapsed-clock contract. No return-value cast was introduced. The direct RB_IsGpuFenceFinished binding preserves the original trivial forwarding wrapper, but does not close the provider timing gap. Existing native presentation-owned gamma and ignore-hardware-gamma behavior are explicit adaptations.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.

## Batch 17 follow-up

GetRefAPI timing ownership is now reconciled by renderer-gpu-sync.md through typed API59 callbacks; the COpenGL lifecycle is restored by renderer-device-lifecycle.md. The original API60 budget getter remains separately tracked until the one-row budget follow-up. R_BeginRegistration capability/mode negotiation remains open for renderer-caps; this batch does not close it.

## Batch 19 follow-up

Renderer capability and registration negotiation are now reconciled by renderer-capabilities.md; native render-dimension,60Hz placeholder and unavailable MSAA boundaries remain explicit. The GPU budget getter is reconciled by renderer-gpu-budget.md through API59 folded ownership. Earlier open statements above describe the pre-follow-up source.
