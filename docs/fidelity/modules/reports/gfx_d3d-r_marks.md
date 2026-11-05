# gfx_d3d/r_marks module reconciliation

Updated 2026-11-05.

4/4 reconciled: 3 equal, 1 fixed. Mac 1.3 i386 is the primary authority.

The AABB/cell/node traversals were read through their repeated inlined levels. Bounds, material filters, duplicate suppression, mark-count and capacity handling match native typed layouts. R_MarkFragments now treats unordered clipping distance as on-plane, as the setbe at 0xf50d2 does. Six-plane construction, rejection, eight-float interpolation, fragment limits, UVs and tangent generation were checked. Native stack storage grows with pointer width; no original bytes or binary text were committed.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
