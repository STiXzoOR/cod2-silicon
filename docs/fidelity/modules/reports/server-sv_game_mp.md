# server/sv_game_mp module reconciliation

Updated 2026-11-04.

**47/54 reconciled: 40 equal, 7 fixed, 7 unchecked.** Mac 1.3 i386 is the authority. All 43 Mac-backed functions were read by hand, plus the identified inline helpers and the matrix-array wrapper's alternate Linux name. The legacy source is preserved in the inactive branches.

## Corrections

**Invalid model names silently returned NULL.** Mac `SV_XModelGet` calls `Com_Error(1, ...)` at 0x14dc0f with the bad name and the `\025bad model name '%s'` diagnostic. Native now follows that error path.

**Entity-lookup diagnostics omitted their control prefix.** The diagnostic at 0x2d5084 begins with `\025`; both `SV_SvEntityForGentity` at 0x14d9ab and its inlined copy in `SV_inSnapshot` at 0x14e92e now pass the same text.

**Skeleton bookkeeping used signed comparisons and undefined signed overflow.** Mac uses unsigned cache-position and allocation-size comparisons at 0x14eb0a/0x14eb1d and 0x14ec58/0x14ec6b. Native now preserves those unsigned comparisons and the 32-bit wrapping alignment, cache-position addition, and timestamp increment. The reset timestamp at 0x14dc88 also wraps before skipping zero. These changes cover both skeleton-creation functions and the inlined `SV_AllocSkelMemory` ledger entry.

## Other comparisons

The game-data and client adapters, DObj wrappers, game lifecycle, entity contact tests, gametype registration, entity parser, map lookup, and debug-box transform were compared with Mac. `SV_inSnapshot` preserves the unusual inclusive cluster-overflow scan and endpoint rejection already present in the native branch. The skeleton rollover retains Mac's warning frequency and its repeated loop for an allocation larger than the whole cache.

The Linux `SV_DObjGetRotTransArray` wrapper is represented by Mac/native `SV_DObjGetMatrixArray` at 0x14ddd2. `SV_DObjNumBones` was checked only as the identified inline call sequence in `SV_XModelDebugBoxes` at 0x14e464–0x14e47a. `SV_FreeWeaponInfoMemory` is inlined in `SV_ShutdownGameProgs` at 0x14e8b4–0x14e8bb. No standalone Mac symbols are claimed for these Linux names.

## Verification and limits

`taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos_ded -j 2` passed in an arm64 Release configuration. The `COD2_X64=OFF` preprocessed source is identical to the base revision. Triage and constant checks ran before and after the edits; the changed allocator, reset, lookup, and model-loader entries were also disassembled from exact mangled symbols.

No new tests, full CI, replays, or runtime checks were run. Existing contact/visibility and point-trace fixtures were read and left unchanged. The corrections are compiled and reviewed, not exercised. The dedicated build excludes the unchanged debug-box function.

Seven Linux-only rows remain unchecked: `SV_NumForGentity`, `SV_DObjSetControlRotTransIndex`, `SV_DObjSetRotTransIndex`, `SV_Hunk_AllocInternal`, `SV_Hunk_AllocAlignInternal`, `SV_Hunk_AllocateTempMemoryInternal`, and `SV_Hunk_FreeTempMemoryInternal`. None has a native body or an identified Mac code symbol/caller. Mac lists zero-valued exception metadata for `SV_NumForGentity` and the two temporary-memory wrappers, which does not locate executable code. The Linux bodies were read without treating their presence as Mac equivalence.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-world.noindex/`.
