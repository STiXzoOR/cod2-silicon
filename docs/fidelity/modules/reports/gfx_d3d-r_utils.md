# gfx_d3d/r_utils module reconciliation

Updated 2026-11-05.

7/7 reconciled: 7 equal. Mac 1.3 i386 is the primary authority.

The seven functions were checked by hand: signed-byte hash sums, lowercase OR, plane dot-product order, A/R/G/B truncating conversion, material trace/string flags, xmodel name bounds and Fresnel trig/clamps. R_PickMaterial uses the native API59 trace slot and typed pointer-bearing trace/infoParm layouts; no ILP32 object storage was reintroduced.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
