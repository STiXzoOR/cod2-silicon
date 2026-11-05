# gfx_d3d/r_caps module reconciliation

Updated 2026-11-05.

1/1 reconciled: 1 intentional. Mac 1.3 i386 is the primary authority.

All36 bit-table rows and all 8 integer-table rows were decoded from the binary, including every response and message; they match. The response branches and unsigned integer bounds match. Native Apple video-memory output is MiB, so >64 replaces the original byte threshold >=0x4000001. This is the specific native units adaptation, not permission to bypass caps checks.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
