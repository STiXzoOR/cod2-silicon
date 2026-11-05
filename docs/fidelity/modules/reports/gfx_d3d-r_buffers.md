# gfx_d3d/r_buffers module reconciliation

Updated 2026-11-05.

6/6 reconciled: 6 equal. Mac 1.3 i386 is the primary authority.

All six static buffer wrappers match: creation uses usage 8, vertex pool 0/FVF0, index format 101/pool 0, and zero-range locks. Vertex errors remain fatal; index failure releases a successfully created buffer. The unusual alwaysfails loops and local null assignment in Free remain as Mac does. Pending native buffer allocation, member lifetime and fence providers were read; this is an engine caller comparison, not certification of their GPU lifetimes.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
