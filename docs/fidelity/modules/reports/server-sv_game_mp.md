# server/sv_game_mp module reconciliation

Updated 2026-11-05.

**52/54 reconciled: 40 equal, 11 fixed, 1 intentional, 2 unchecked.** Mac 1.3 i386 is the authority. All 43 Mac-backed functions were read by hand, plus the identified inline helpers and the matrix-array wrapper's alternate Linux name. The legacy source is preserved in the inactive branches.

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

## Temporary-memory forwarding identities — 2026-11-05

Two more rows are reconciled: `SV_Hunk_AllocateTempMemoryInternal` is fixed
through the existing generic allocator correction; `SV_Hunk_FreeTempMemoryInternal`
is intentional through the established matching-backend release adaptation.
Current accounting is 39 equal, nine fixed, one intentional and five unchecked.
No new production defect or source edit was introduced in this follow-up.

Original Linux 1.0 address provenance identifies the wrappers at
0x08090470/0x08090484. The original 1.3 ELF independently places them at
0x08091d84–0x08091d98 and 0x08091d98–0x08091dac. Each forwards its unchanged
single argument, makes one allocator/release call and returns without adding
guards, state, cleanup or result conversion. The neighboring guarded permanent
allocators are distinct and remain unresolved. Linux provides original wrapper
identity; Mac remains the operation authority.

The complete Mac allocator at 0x38192–0x382a2 preserves the pre-hunk zeroed
allocation fallback, 16-byte alignment/header, low-temp update, overflow error,
magic value and rollback size including alignment padding. Native
`Hunk_AllocateTempMemoryInternal` has the prior missing 0x15 diagnostic marker
repair. The complete Mac release at 0x37c9c–0x37cf8 validates the header, marks
it released and subtracts the stored rollback without checking top-of-stack.
Native preserves this hunk behavior and the already approved pre-hunk
`Z_FreeInternal` path, which pairs optional guard allocations with their backend.

These are complete-operation mappings to existing native providers, not claims
of separately emitted Mac server exports. Primary provider verdicts remain
unchanged. Parent and independent reviews support both classifications; no
compilation, CI, tests or replay ran and no runtime coverage was added. Private
exact original bodies remain in `recon-provenance-parent4.noindex`.

## Original wrapper refinement (storage10)

Five rows remain unchecked. Original1.3 rotation/control wrappers0x8092260/0x8092292 load the entity number, resolve the server DObj and call distinct partBits/boneIndex setters. Existing tag setters additionally perform name lookup and local-tag mutation; neither they nor isolated bit setters establish these complete server wrappers. Original1.0 code-reference follow-up adds no actual consumer. Permanent allocators0x8091d1c/0x8091d4c require the loading-state guard and error path; the settled unguarded model-precache adapters remain rejected. NumForGentity0x80916d8 returns the signed game-pointer/base/stride quotient; the previously inspected server/game pointer converters still have different inputs and results. Approved temporary allocation and native ownership adaptations stay unchanged.


Private original bodies and new exact Mac/provider/state-owner evidence are retained in recon-provenance-storage5.noindex and recon-provenance-storage10.noindex. No source changes, compilation, tests, CI, replay or runtime exercise occurred. Missing names/callers are not completion evidence.

## Complete registered entity-index operation (entitynum30)

`SV_NumForGentity` is equal through the actual complete pointer-derived index phase in Mac spawning. Original1.3 body0x80916d8 throughRET0x80916f7 subtracts the server-registered entity base, signed-divides by its stride and returns the full quotient. The previous number-field reads and server-pointer conversions remain insufficient by themselves.

New positive producer evidence supplies the missing role. G_InitGame installs g_entities as level.gentities at0x1adfe0 and registers that same base with stride0x230 at0x1ae09d. Complete SV_LocateGameData stores those fields in server state. Raw level, g_entities and sv GOT values independently establish the object identities. On the actual non-exhausted fresh-slot path, G_Spawn constructs that array element and re-registers the same base/stride before the complete0x1b6f58–0x1b6f69 conversion. It subtracts g_entities, shifts by4, multiplies by0x8af8af8b and stores the full32-bit result in s.number. For a valid element index i,560i becomes35i after the shift, and the multiplier is the inverse of35 modulo2^32, producing i. G_InitGentity also contains the same complete phase.

Native uses the real typed1024-element g_entities array, registers sizeof(gentity_t), and consumes `(int)(e-g_entities)` in s.number. This preserves the complete scalar role and established native geometry. Equal classification applies to this unchanged conversion; surrounding classname, allocation, free-list and lifecycle repairs are outside its boundary. Scope is ordinary consistent engine registration, valid in-bounds element and successful preceding providers. It does not certify arbitrary alternate base/stride registration, null/unrelated/one-past destination, forged state, returned exhaustion errors, standalone ABI, whole spawning or runtime behavior.

Independent review accepted this scope in units/review-provenance-entitynum30.md. Fresh full original and four Mac owners, raw data and source hashes remain in private recon-provenance-entitynum30.noindex. One equal closure; no source change or new defect. No compilation, tests, CI, full build, replay or runtime verification.


## Complete server bone-bit operations (serverbits38)

`SV_DObjSetRotTransIndex` and `SV_DObjSetControlRotTransIndex` are fixed through actual controller operations. Complete original1.3 wrappers0x8092260–RET0x8092290 and0x8092292–RET0x80922c2 load the entity number, resolve its server DObj, pass partBits/index to the complete bit provider and return its Boolean. They add no null guard. The earlier whole-tag-setter substitution remains rejected.

New positive wiring supplies the complete selected role. Mac turret_controller loads entity number0x1bbf17→Com_GetServerDObj0x1bbf1c and forwards the resulting object and original partBits to control-tag setters0x1bbf45/63/91. G_PlayerController0x1a297c–0x1a299c does the same through BG_Player_DoControllers, which forwards both to local-tag setter0x183018 and control-tag setter0x182f47. Actual model/hierarchy tag lookup produces the full integer bone index; it is not an assumed adapter. Native preserves these real argument chains and the approved object stride.

The complete local phase0x74ebd–0x74ee2 computes the word/mask, rejects a clear requested bit or existing skeleton bit, then ORs animation state. Control phase0x74f6b–0x74f94 additionally ORs control before animation. Each false outcome returns zero and skips the angle write; the successful bit operation is consumed by entering that later write. This accounts for all original bit effects and both Boolean outcomes without claiming the entire name/angle owner or its later return timing.

Both selected providers contained the same newly identified C99 defect: signed `1 << 31` was undefined even though Mac preserves0x80000000. Actual model creation permits totals through127, and hierarchy lookup permits found indices31/63/95. COD2_X64 now uses a uint32_t shift and memcpy into the existing signed mask. These two helper verdicts inherit that one shared fix; later angle arithmetic repairs are outside this mapping. The two primary setter rows change equal→fixed separately.

Scope is a stable live mapped server object, successful actual tag-derived in-range index and valid partBits/skeleton backing. No arbitrary index API, negative index, lookup-failure, unmapped/null-object or later angle-failure continuation equivalence is claimed: the original provider can return on a clear requested bit before dereferencing a null object, whereas the selected actual tag lookup already accesses that object. The two permanent-loading allocator wrappers remain unchecked.

Independent review: units/review-provenance-serverbits38.md. Complete originals, fresh Mac owners/providers, hashes and receipts are private in recon-provenance-serverbits38.noindex. Four actual changed-TU configurations compiled; exact OFF bytes and five inactive preprocessing outputs match. One new shared C-level defect, two closures. No tests, CI, full build, replay or runtime verification; compiled/reviewed does not mean playable.


## Permanent allocation guard and lifecycle follow-up (serverhunk39)

Both permanent wrappers remain unchecked after a distinct actual-owner review. Complete original1.3 bodies0x8091d1c–RET0x8091d4b and0x8091d4c–RET0x8091d82 require server state1, otherwise call ERR_DROP with their specific loading-only diagnostic; allocation follows even if that error returns. They forward the supplied bytes, and alignment for the second wrapper, then return the allocator pointer. This guard/error/continuation is part of the operation.

Fresh Mac SV_SpawnServer provides real allocation/result consumers but not that guard: snapshot pointers are allocated/stored at0x1430a4/a9 and0x1430c4/c9 before the unconditional server state1 store0x1431be. Game initialization and three initial frames follow; state2 is stored0x1433d3. This lifecycle envelope does not replace a checked per-allocation state/error role. The stronger actual G_ModelIndex→SV_XModelGet candidate checks level.initializing at0x1b578f–0x1b5794 and uses Scr_Error0x1b57d6, rather than the original server-state/ERR_DROP contract. Raw GOT values establish separate level0x1aa2280 and sv0x17fdd00; G_InitGame clears level.initializing0x1ae12d before return while server loading persists. The distinct guard is not a substitute for the missing wrapper.

Actual animation-tree and mantle callbacks0x9bc9a/0x9e71c forward size with fixed alignment4 to Hunk_AllocAlignInternal, without adding the loading guard. Complete high-hunk providers0x38358/0x37844 have capacity checks, not the requested state check; Hunk_AllocXAnimServer instead forwards to the low allocator. These concrete paths leave the full guarded size/alignment/result role unresolved. This is not a claim that absent symbols prove the operation unused, nor authorization to invent a new allocation policy.

Private recon-provenance-serverhunk39.noindex retains fresh complete original/Mac packets, raw state identities and diagnostics, and seven source hashes. Two unchecked refinements, no closures or new defects. No source changes, compilation, tests, CI, full build, replay or runtime execution; existing ownership/guardheap adaptations remain unchanged.
