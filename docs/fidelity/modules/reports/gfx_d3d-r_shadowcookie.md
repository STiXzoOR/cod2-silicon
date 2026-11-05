# gfx_d3d/r_shadowcookie module reconciliation

Updated 2026-11-05.

1/1 reconciled: 1 equal. Mac 1.3 i386 is the primary authority.

The exact entry 0xf99fc stores10.0f and-1 in weightCap and lastTime. Whole-binary disassembly was corrupt at the entry; exact-symbol restart confirms the two stores.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
