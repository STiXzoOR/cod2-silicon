# universal/com_sndalias module reconciliation

Updated 2026-11-04.

**25/26 reconciled: 20 equal, 5 fixed; 1 unchecked.** Mac 1.3 i386 is the authority. Linux was used for helper identities, not implementation authority. Changes are under COD2_X64; the legacy preprocessed source is unchanged.

## Corrections

`Com_LoadSoundAliases` at 0xacc20 retained UI aliases and the shared hash while loading; native added two clearing branches. Both originated in the initial source import, with no native-port rationale found. Native UI/CGame shutdown already calls `Com_UnloadSoundAliases`, and hunk allocation still zeroes storage. The extra clears are disabled on the native path. Complete curve initialization diagnostics, their 0x15 marker, and the missing-sound `va` format passed to `Com_Error` now match Mac.

`Com_UnloadSoundAliasSounds` at 0xad05a clears both loaded and primed aliases. Native skipped primed aliases; it now handles types 1 and 3 and compares previous aliases against the current type.

The inline curve-initialization row records the same diagnostic correction. The rest of the module was compared by hand, including hashing, list updates, listing/deduplication, offset lookup, and both weighted-selection passes. SSE additions and multiplications, strict selection comparisons, random consumption and sequence avoidance match Mac. The compiled arm64 random recurrence uses 32-bit multiply-add and extracts bits 16 through 30.

## Native boundaries and unresolved identities

Curve names occupy `volumeFalloffCurveNames[16][64]`. Native retains its 63-character copy bound; Mac derives an unchecked copy size from the filename length. This preserves defined native storage for oversized filenames. Dedicated builds continue to omit client sound stopping/loading and sound-list registration, while retaining engine alias metadata loading and selection.

`Com_GetSoundAliasName`, `Com_GetSoundAliasRandomSeed`, and `Com_SetSoundAliasRandomSeed` remain unchecked. Their Linux bodies were read, but no native body/caller or specific executable Mac identity was established. Mac has absolute `.eh` seed symbols under shorter names; those are not executable evidence.

## Verification and limits

The arm64 Release `cod2_macos_ded` build passed with two low-priority jobs. Both affected files also compiled with client flags. All five COD2_X64=OFF preprocessing configurations match the baseline, and neither file uses `__LINE__`. Triage and constant checks were used as leads, followed by complete hand comparison of the Mac bodies. The misaligned full-disassembly entry for `Com_GetVolumeFalloffCurveValue` was restarted at its exact symbol.

No new tests, full CI or replays were run. Corrections are compiled and reviewed, not exercised. Runtime validation belongs to integration.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-sound.noindex/`.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.

## Remaining-identity audit, 2026-11-05

All three unresolved rows remain unchecked and prior resolved verdicts are preserved. Mac `g_sa` starts at 0x1227420 and its seed occupies 0x1227424, matching the named native randSeed field. The eight direct seed references in ordinary Mac text form four load/update pairs inside `Com_PickSoundAliasFromList` (0xad450) and `Com_PickSoundAlias` (0xad97e). The pairs load the prior seed and write the recurrence result, at 0xad4a0/0xad4bd, 0xad532/0xad54f, 0xada38/0xada55 and 0xadad9/0xadaf6. Those are internal random-generation operations, not separately established seed getter/setter APIs. The absolute-zero seed EH symbols do not supply executable identity.

The 69 named calls to `Com_FindSoundAlias` were grouped under seven actual owners: sound, weapon and item registration, UI asset/menu/focus-sound loading, and script sound existence. The return sites store or consume the alias-list pointer, or test it for existence. They do not establish the full null-safe lookup-and-name-return wrapper attributed to `Com_GetSoundAliasName`. Native `Com_FindSoundAlias` and these callers retain their existing contracts. This bounded direct-call/reference inspection does not prove absence of an unrecognized inline wrapper.

The unchanged sound-alias module compiled with configured stock Release and CoDxRelease arm64 production commands. Triage and selected constant checks ran as leads; the random helper has no standalone Mac body, so exact selector disassembly supplied the seed-reference evidence. Existing audio-output and script-binding fixtures provide alias stubs, not these production helper bodies, and were not counted as runtime coverage. No source changes, new tests, cases, assertions, full CI or replay. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-storage-remaining.noindex/`.


## Original identity follow-up — provenance-config7

Original Linux 1.0 IDB addresses were mapped to retained original 1.3 ELF bodies using source order, independent decompiler anchors and masked whole-body comparisons. Every unique whole-body match agrees with the mapped module order; ambiguous tiny bodies and changed implementations were located from neighboring unique anchors and inspected separately. An ambiguous non-target `FS_Path_f` comparison was not used as an anchor. The private packet retains the mapping alternatives and exact bodies. These addresses establish original identity and intent only; Mac i386 remains behavior authority.

| Helper | Original 1.0 | Original 1.3 | Verdict |
|---|---|---|---|
| `Com_GetSoundAliasName` | 0x80aaf4a | 0x80ad2e2 | unchecked |
| `Com_GetSoundAliasRandomSeed` | 0x80aaf7a | 0x80ad312 | unchecked |
| `Com_SetSoundAliasRandomSeed` | 0x80aaf84 | 0x80ad31c | unchecked |

The three full original bodies now have concrete addresses. No corresponding complete Mac name accessor or arbitrary seed getter/setter operation has been established. Internal random-seed recurrence does not prove these APIs; all three rows remain unchecked.

No production source changes, defects, compilation, tests, CI or replays in this follow-up. No runtime behavior is certified. Private evidence: `recon-provenance-config7.noindex`; focused ledger preserves all previously resolved rows unchanged.

## Defined seed arithmetic (config13)

One C-level defect was found in the shared random helper consumed by both selector owners. Complete Mac Com_PickSoundAliasFromList0xad450 and Com_PickSoundAlias0xad97e wrap the seed recurrence modulo2^32, store all32bits and extract bits16–30. Native used signed int multiplication/addition. The actual four stock/CoD2x client/dedicated commands omit wrap flags; the separate legacy CMake flags do not apply. Zero-initialized g_sa and repeated ordinary two-entry selections reach signed overflow on the second update:2531011*214013+2531011=541671788154, whose required stored word is0x1e278e7a. No arbitrary seed setter or exceptional input is required.

The COD2_X64 helper now uses unsigned32 multiplication/addition and memcpy for the stored int representation, then extracts the same15bits. The legacy helper text is unchanged. Selection probabilities, seed consumption, sequence handling and ownership are preserved. Existing inspected arm64 objects already use matching32-bit MADD/UBFX; this is a correction to the C contract, not a claim of observed generated-code failure. The two existing selector rows change equal→fixed for this one shared defect. The three unresolved name/seed helpers remain unchecked: internal recurrence does not establish their standalone contracts.

The changed translation unit manually compiled in all four actual production configurations. Inactive unifdef output and five full preprocessing comparisons, including an assertions-enabled variant, are byte-identical without replacing __LINE__; source assertions precede the added guard and retain their positions. Source SHA2564d5c07b53deb09accc2cdea45fcc8cd670b6701eb633b88cc156c2c4bc0c2ef0 and receipts are retained in units/provenance-config13.compile.json and recon-provenance-config13.noindex. No tests, CI, replay or runtime exercise occurred. Compiled/reviewed does not establish sound-path coverage.

Independent final review accepted the exact source hash, complete Mac arithmetic,32-bit native types and all four compile receipts. No additional compilation or runtime check was needed or performed.


## Sequence15 playback-history wrap repair

The two primary selectors remain fixed, now with one additional shared C-level
arithmetic defect corrected. Exact Mac Com_PickSoundAliasFromList adds/stores the
32-bit sequence at 0xad503/0xad506; Com_PickSoundAlias does so at 0xadab1/0xadab4.
The selected alias receives the signed maximum's incremented bit pattern,
including INT_MAX wrapping to INT_MIN. Native previously evaluated the increment
as signed int, while the four actual optimized arm64 compile commands contain no
signed-wrap option. COD2_X64 now increments an unsigned value and copies its
representation into the existing int field. Signed comparisons, repeat avoidance,
RNG operation, alias selection and the approved seed fix stay unchanged. Legacy
source remains exact.

The overflow input has a defined producer and a valid repeated-call path.
Com_MakeSoundAliasesPermanent writes zero history at Mac 0xb6e50 and native
com_sndalias_load_obj.c; CSV sequence belongs to temporary build ordering and is
not the playback-history initializer. A valid one-entry list follows
0xad494->0xad4fb->0xad500 without RNG or repeat avoidance. Starting at zero, its
first 2147483647 selections have representable history; the next selection wraps
on Mac. Neither complete selector requires an intervening reset. Reconstructing
aliases resets new storage, but this is not required by selection. The full
SND_PlayLocalSoundAlias/ByName owners at 0x4c17a/0x4c124 select before playback and
supply no sequence cap. This is a finite call-contract proof, not a claim of a
practical session reaching that many selections or an observed optimizer failure.

The unchanged source hash reviewed for compilation is
8806dc3ebf6d03a3561eb4d05d15f1646901e323c4cb0182ce8936832e380056.
Four manual changed-TU compilations succeeded (stock/CoD2x client/dedicated).
Exact OFF bytes and five full inactive preprocessing comparisons, including the
assertion-enabled variant and actual line expansions, are identical. No tests,
CI, replay, full build or runtime exercise occurred. No unresolved helper closes;
this repair adds evidence to the two existing fixed primary rows only. Private
complete owner/producer packets and receipts are retained under
recon-provenance-sequence15.noindex and manual-sequence15-compile-8806dc3ebf6d.noindex.

## Complete consumed seed read/store (soundseed25)

`Com_GetSoundAliasRandomSeed` and `Com_SetSoundAliasRandomSeed` are equal for
the complete scalar operations consumed by actual sound selection. The original
getter0x80ad312 returns the full word at0x85a9844; setter0x80ad31c stores its
full argument there unchanged. Full original RNG0x80ad574 uses that same state,
and actual selector calls0x80ad394/0x80ad42a establish its sound-selection role.
Original bytes supply identity/intent, not Mac behavior authority.

Exact Mac read/store pairs0xad4a0/0xad4bd,0xad532/0xad54f,0xada38/0xada55 and
0xadad9/0xadaf6 read and write all32bits at g_sa+4. The entire prior word feeds
the strength-reduced recurrence. The entire completed result is stored BEFORE
the separate15-bit random extraction. A valid three-entry list has two first-pass
updates: after the first store, the actual loop reloads that persistent word
without an intervening call or seed write. Thus both the scalar-read consumer
and the scalar-store input/output roles are real; no external observer or
reseed API is invented. Native reads the same typed state and uses its already
established memcpy to preserve the full completed representation.

The read operation ends at the complete prior-state input to recurrence. The
write operation begins after recurrence calculation and ends at persistent
store. Existing config13 arithmetic and sequence15 history repairs remain
untouched and are not counted as getter/store defects. This narrower complete
operation evidence supersedes the earlier absence of standalone API identity;
standalone/export ABI, arbitrary external reseed timing, concurrency ordering,
literal inline ancestry and whole RNG/selector equivalence remain unclaimed.
`Com_GetSoundAliasName` remains unchecked for its separate lookup/name-output gap.

Independent review: review-provenance-soundseed25.md; parent concurred. Fresh
complete original/Mac owners, real state backing and native source hashes are
retained in recon-provenance-soundseed25.noindex. Two equal closures, no source
change or new defect. No compilation, tests, CI, full build, replay or runtime
verification.
