# gfx_d3d/r_screenshot module reconciliation

Updated 2026-11-05.

9/9 reconciled: 1 equal, 2 fixed, 6 intentional. Mac 1.3 i386 is the primary authority.

Cubemap lighting now matches the unordered dot-product and saturation branches at 0xe9ac7 and 0xe9c3c/0xe9c89/0xe9ccd. The exact 2.2f and 0.45454545f exponents, six-face axes, accumulation, counts and byte rounding were checked.

R_TakeResampledScreenshot has a defined Mac quirk:0xe904a reads into allocation base, then 0xe906b begins resampling after the header. Native had moved the readback destination past the header. This prefix shift is restored. Exact Mac Z_MallocInternal 0x37d4c and the native allocator zero the entire allocation, so the trailing bytes are defined zero; no uninitialized storage was introduced.

Native keeps the existing RGB readback/BGR file adapter, owned six-face validation/cleanup, safe resampling pointer guards, and screenshot queue consumed before Present after two frames. The original immediate front-buffer capture has different platform ownership. These are concrete native boundaries, not a whole-presentation parity claim. The savegame argument guard now requires exactly three arguments, as 0xe9fda does; native had accepted extra arguments. Levelshot/savegame/file headers, names, number limit, quality and silent branches were compared.

The existing screenshot fixture was updated, with parent authorization, to use the real zeroing Z_MallocInternal contract and to feed its independent rational resampling oracle from the shifted source with zero tail. Its prefix assertion now checks captured bytes instead of untouched garbage. All 14 existing cases pass with ASan/UBSan, including levelshot, savegame, mixed resize, single pixel, boundary and failure cases; no cases were added. This exercises synthetic GL and stubbed file writes, not a GPU or actual file capture. The new argc==3 rejection is compiled and reviewed, not separately exercised.

All assigned functions received triage/constants passes and exact-symbol Mac reads where whole-binary output was misaligned. The native arm64 Release dedicated target and the actual Apple client objects for all 13 assigned modules compile at low priority with at most two jobs. Changed source preserves five OFF configurations, including actual diagnostic line expansions; native-on changes are intentional. No new tests, full CI or replays were run. Unless separately stated, corrections are compiled and reviewed, not renderer/GPU/runtime verified. Parent owns integration and runtime checks.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-frame.noindex/`. The exact per-entry ledger is external in the assigned gfx-frame unit ledger.
