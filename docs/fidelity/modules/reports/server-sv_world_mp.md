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

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.

## Original single-entity trace refinement (storage10)

SV_LocationalTraceToEntity remains unchecked. Complete original1.3 body0x809e2aa resolves one supplied entity, clears36-byte trace output, initializes fraction/entity, copies extents and computes inverse delta, sets pass/ignore fields to-1 and contentmask/locational/priority inputs, then calls the entity trace provider. Fresh exact Mac SV_PointTraceToEntity0x1bf0e8 consumes existing clip/entity/result storage; the identified CM linked-entity traversal is a different owner and does not establish this complete initializer. Original1.0 code-reference follow-up adds no caller bridge. No generic trace-provider closure was inferred.


Private original bodies and new exact Mac/provider/state-owner evidence are retained in recon-provenance-storage5.noindex and recon-provenance-storage10.noindex. No source changes, compilation, tests, CI, replay or runtime exercise occurred. Missing names/callers are not completion evidence.
