# gfx_d3d/r_light module reconciliation

Updated 2026-11-05.

4/4 reconciled: 2 equal, 1 fixed, 1 intentional. Mac 1.3 i386 is the primary authority.

Light selection now follows the unordered-aware scan branches at 0xedea5/0xedf64, restarting its forward scan after each swap. The surface-distance branch at 0xee0da accepts unordered values too. Radius-square operation order, reverse surface append order and partition compaction were checked. RegisterLightDef keeps defined error arguments: the original full-limit diagnostic supplies one value to a format requiring two; copying that invalid variadic read would not give defined native behavior.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
