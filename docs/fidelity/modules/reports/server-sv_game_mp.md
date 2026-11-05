# server/sv_game_mp module reconciliation

Updated 2026-11-05.

**47/54 reconciled: 39 equal, 8 fixed, 7 unchecked.** Mac 1.3 i386 is the authority. All 43 Mac-backed functions were read by hand, plus the identified inline helpers and the matrix-array wrapper's alternate Linux name. The legacy source is preserved in the inactive branches.

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

## Integrated batch verification

Integrated engine source through `868a08f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `b1f110d` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `b275adb` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.

## Independent math-caller follow-up — 2026-11-05

Current accounting is **39 equal, eight fixed, seven unchecked**. Fresh review
found the cylinder vertical gate at Mac 0x14e292–0x14e296 rejects only ordered
`entZ >= maxs[2]`. Native `entZ < maxs[2]` skipped unordered candidates. The
`COD2_X64` branch now uses `!(entZ >= maxs[2])`, leaving the subsequent top-Z
rejection and center/radius arithmetic intact. The owner changes from equal to
fixed with its original evidence retained and the correction appended.

Configured stock/CoD2x Release client/dedicated objects compile, and the
unchanged production server-contact fixture passes both modes under
ASan/UBSan. The unordered gate is compiled and reviewed, not specifically
exercised. Five inactive configurations and exact OFF source text match; there
are no `__LINE__` uses. No tests were added, and no full CI or replay was run.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.

## Remaining identity search — 2026-11-05

All seven unresolved identities remain unchecked. Exact Mac server conversion
and allocator owners reject two further candidate mappings:

- `SV_SvEntityForGentity` at 0x14d990–0x14d9de reads and validates
  `gEnt->s.number`, then returns the indexed server-entity pointer.
  `SV_GEntityForSvEntity` at 0x14e40c–0x14e458 converts a server-array
  pointer index into a game-entity pointer. Neither is `SV_NumForGentity`'s
  input game-pointer subtraction/division and integer-result interface.
- `SV_AllocXModelPrecache` at 0x14dbd2–0x14dbdc and
  `SV_AllocXModelPrecacheColl` at 0x14dbdc–0x14dbe6 tail-jump directly to
  `Hunk_AllocInternal`. Their complete native wrappers also forward directly;
  they do not establish `SV_Hunk_AllocInternal`'s loading-state guard or
  error behavior.

The corresponding native owners were read. These observations reject candidate
identities; they neither close the missing helpers nor prove dead operations.
No source defect was established. No source changed, and no compilation, tests,
CI or replay ran. Exact-entry evidence remains in the private
`CoD2-native-decomp/recon-identity-parent3.noindex` directory.
