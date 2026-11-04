# server/sv_world_mp module reconciliation

Updated 2026-11-04.

**13/14 reconciled: 10 equal, 3 fixed, 1 unchecked.** Mac 1.3 i386 is the authority. All 12 Mac-backed functions were read by hand, together with the angle-snapping code inlined into `SV_LinkEntity`. The legacy source is preserved in the inactive branches.

## Corrections

**Simple sight traces could use an uninitialized locational flag.** Mac `SV_SightTrace` clears that field at 0x1be1f5. Native now sets it to zero before asking the world to trace a point against entities.

**Three trace dispatchers grouped their extent arithmetic differently.** Mac alternates subtraction and addition in `SV_SightTrace` at 0x1be19b–0x1be1ae, `SV_Trace` at 0x1be3ed–0x1be400, and `SV_TracePassed` at 0x1be6ea–0x1be6fd. Native summed three independently calculated differences. The corrected order can change point-versus-box dispatch: with mins `(0, 16777216, 0)` and maxs `(1, 16777216, 0)`, single-precision grouped arithmetic yields 1 while Mac's sequence yields 0.

## Other comparisons

The entity content and owner filters, clip-model selection, broad-phase bounds, transformed traces, and hit metadata agree. The existing 4×3 model-space transforms agree with Mac: the stored origin occupies row 3 of the stack matrix in both locational point and sight traces. `SV_LinkEntity` agrees on solid encoding, angle snapping, bounds expansion, leaf-cluster overflow, and 2D model bounds. Mac omits some model-bound Z writes because `CM_LinkEntity` consumes only X/Y; native's additional Z initialization does not reach that boundary.

The actor bounds were read from the binary data, and the shared box-edge table was compared with its native definition. No shared source or header was changed.

## Verification and limits

`taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos_ded -j 2` passed in an arm64 Release configuration. The `COD2_X64=OFF` preprocessed source is identical to the base revision. Triage and constant checks ran before and after the edits; the changed functions were also disassembled from their exact mangled-symbol entries.

No new tests, full CI, replays, or runtime checks were run. The existing `tests/lp64/server/point_trace.c` fixture was read and left unchanged. The corrections are compiled and reviewed, not exercised.

`SV_LocationalTraceToEntity` remains unchecked. The Linux-only ledger row has no native body, no Mac code symbol, and no identified Mac caller containing its full wrapper. Its reference implementation was read, but that does not establish a Mac identity.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-world.noindex/`.
