# gfx_d3d/r_dvars module reconciliation

Updated 2026-11-05.

2/2 reconciled: 1 equal, 1 intentional. Mac 1.3 i386 is the primary authority.

A private script decoded 138 Mac registration calls and compared immediate value bits, domains, types and flags with preprocessed native registrations. The scalar differences are the existing native fullscreen archive/latch flags and explicit dx9 preference. Native mode/refresh dvars register here through SDL; Mac builds their domains later in initialization. CoD2x retains its established lodScale range. Enum backing strings, sun registration order and outdoorFeather modification were checked. The supplied Windows server executable has no rendererPreference/fullscreen identities, so no Windows renderer claim is made.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
