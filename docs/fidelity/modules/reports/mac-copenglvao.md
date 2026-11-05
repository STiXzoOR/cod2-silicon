# Mac/DirectX 9/COpenGLVAO reconciliation

Updated 2026-11-05.

**5/5 reconciled: 4 equal, 1 fixed.** Mac 1.3 i386 is the authority. The legacy OFF bodies are unchanged.

Mac `ChooseAndRemoveBestTriangle` at 0x112422 selects a triangle only when its FIFO score is strictly greater than the current best. Native selected lower scores and replaced ties. The exact pending System 5f88bd5 correction is retained, preserving the first highest-scoring triangle.

The key accessor and comparison at 0x112304/0x112310, binding allocation at 0x112328, and deque scoring at 0x112382 were read against native source. Deque blocks contain 256 unsigned shorts. Binding ownership uses the typed native list supplied by the pending GL provider cd687d6; its actual source was inspected and compiled.

The key remains the existing UINT32 field. This audit does not establish that a nonzero native key can represent the original VARange pointer. The missing device VAO producers are recorded as unchecked in the CVAOPacket report.

Triage and explicit-range constant checks ran. The owned Apple arm64 client object and the actual pending COpenGL provider object compile; the dedicated target builds. Five OFF configurations also match with actual line-macro expansion. No tests were added or changed, and no renderer path was exercised. System's exact `Optimize` and deque support changes are preserved as dependencies; their separate SDK ledger IDs were not reassigned or closed here.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-arrays.noindex/`.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
