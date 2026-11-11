# gfx_d3d/r_caps module reconciliation

Updated 2026-11-05.

1/1 reconciled: 1 intentional. Mac 1.3 i386 is the primary authority.

All36 bit-table rows and all 8 integer-table rows were decoded from the binary, including every response and message; they match. The response branches and unsigned integer bounds match. Native Apple video-memory output is MiB, so >64 replaces the original byte threshold >=0x4000001. This is the specific native units adaptation, not permission to bypass caps checks.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.

## Integrated batch 16 verification

Engine d7e6ca9 and fixture-only follow-up 935519b passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
