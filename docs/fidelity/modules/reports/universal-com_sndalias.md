# universal/com_sndalias module reconciliation

Updated 2026-11-04.

**23/26 reconciled: 20 equal, 3 fixed; 3 unchecked.** Mac 1.3 i386 is the authority. Linux was used for helper identities, not implementation authority. Changes are under COD2_X64; the legacy preprocessed source is unchanged.

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
