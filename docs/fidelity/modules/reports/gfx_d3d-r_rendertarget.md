# gfx_d3d/r_rendertarget module reconciliation

Updated 2026-11-05.

4/4 reconciled: 2 equal, 2 fixed. Mac 1.3 i386 is the primary authority.

The format switch table at 0x32b0e0 was decoded: cases 23,32,70 were missing and unsupported 22/71 descriptions were present. All nine recognized formats now match. GetSwapChain at 0xea4bc now writes windows[0].swapChain; the native code had written at the beginning of the window record and overwritten hwnd. Shared depth AddRef/create paths, render-target aliases 1/7, the two 128x128 targets, five quarter-resolution targets and release/clear order were checked against the actual pending surface/image providers.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
