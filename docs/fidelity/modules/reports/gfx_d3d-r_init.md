# gfx_d3d/r_init module reconciliation

Updated 2026-11-05.

20/22 reconciled: 14 equal, 5 fixed, 1 intentional, 2 unchecked. Mac 1.3 i386 is the primary authority.

The Mac resource constructor 0xcb24e creates one skinned-cache buffer. Native Apple now allocates/releases pool 0, after the commands unit forces pool 0 and the parent corrects Apple Lock to DISCARD. Non-Apple keeps its two-pool contract. Flush-query reset and fatal resource-creation diagnostics were restored. R_Shutdown now clears the window HWND, not its already-released swap-chain member, matching 0xcb0e9. Recovery uses typed native window/dvar reads, resets AA state when samples<=1, and matches reset/fatal progress messages.

Initialization now consumes pending device9f05cde caps after its CreateDevice establishes the SDL GL context, restores the verified capability fields and renderer-path selection, creates reset resources only for a new device, initializes the viewport and window bookkeeping, and initializes systems after reset resources. The exact pending fullscreen-word hunk is preserved. The native device deliberately consumes int[14] words (Windowed 7, interval 11, refresh 12); replacing that with an LP64 D3DPRESENT_PARAMETERS struct would break the actual provider. Native HWND is owned by SDL and is not packed into those words.

R_BeginRegistration remains unchecked: the original sorted mode/refresh domains, fallback AA/display retries, aspect-ratio negotiation, and two min/mag-anisotropy capability bytes have no complete matching native contract. Native DxGlobals has a selected anisotropy level, not those two capability fields; no shared header was changed. Native GL initialization and context lifetime also remain incomplete in the provider audit. GetRefAPI remains unchecked: Mac API60 has integer GpuSyncDelay at 0x154; native API59 separates void SyncGpu and GpuWaited. There is no defined native budget accessor/elapsed-clock contract. No return-value cast was introduced. The direct RB_IsGpuFenceFinished binding preserves the original trivial forwarding wrapper, but does not close the provider timing gap. Existing native presentation-owned gamma and ignore-hardware-gamma behavior are explicit adaptations.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
