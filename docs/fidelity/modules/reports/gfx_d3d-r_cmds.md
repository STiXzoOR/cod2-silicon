# gfx_d3d/r_cmds module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 5 equal.** The Mac bodies at 0xf328c through 0xf3394 were read against native source, supplemented by triage and constants. No source correction was needed.

The three command wrappers pass the original picmip call and screenshot types 1/0. All nine registration strings and all ten removal strings were decoded from the original binary and checked in order, including the extra r_vc_stats removal. Callback pointer slots were resolved to their original symbols: image list, material reload, sun load/save, and static-model cache stats/flush match the native direct-function aliases. This verifies registration identity, not the behavior of every registered callback.

Verification: r_cmds.c compiled with the actual local arm64 Release command. The dedicated target built at low priority with two jobs. No new tests, full CI or replays were run; console registration and screenshot capture were not runtime exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-commands.noindex/`.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
