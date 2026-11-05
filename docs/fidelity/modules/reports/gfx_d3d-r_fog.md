# gfx_d3d/r_fog module reconciliation

Updated 2026-11-05.

4/4 reconciled: 4 equal. Mac 1.3 i386 is the primary authority.

The five 32-byte settings and index 4-byte archive contract match. SetFog preserves B/G/R/255 packing, >=1 density clamp/technique and raw-density alternative. SwitchFog preserves the slot 2/slot 3/slot 4 previous/target copy sequence and transition timestamps. No changes were needed.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
