# xanim/xmodel module reconciliation

Updated 2026-11-04.

**25/30 reconciled: 24 equal, 1 intentional; 5 unchecked.** Complete Mac functions were compared with native getters, cache wrappers, string cleanup, default-model fallback and collision tracing. Linux-only default helpers were matched to their complete inline blocks in `XModelPrecache` at 0x458b0.

`Com_ValidXModelName` now accepts both slash and backslash after the case-insensitive `xmodel` prefix. Mac 0x45ae0 accepts only slash. Windows 0x490950-0x490977 explicitly accepts both, with separator comparisons at 0x490967 and 0x49096b. This follows the requested Windows precedence for player-visible platform differences.

`XModelTraceLine` at 0x45bc4 was compared through both quaternion transforms, broadphase, triangle tests, 0.125 trace bias, barycentric tolerances and final normal rotation. Its existing native SSE grouping matches Mac. `AxisTransformVector` belongs to `universal/com_math.c`; its three column products and addition order match Mac 0x2bc62. No shared source was edited. The transpose helper was compared in the Mac trace caller.

The five unchecked rows are `XModelGetCollLod`, `XModelReplace`, `XModelSetUnusedValue`, `XModelReturnZero` and `XModelIgnoreChar`. Their Linux definitions were read and native sources searched, but no native body/caller or unique complete Mac identity was established. Linux-only definitions were not used to invent native implementations.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. The separator change is compiled and reviewed, not exercised. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source. Exact-symbol disassembly corrected the full-binary tool's misaligned `XModelBad` entry.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.

## Remaining-helper and fixture follow-up — 2026-11-05

The five unchecked helper rows remain unchecked. Mac loader and default-model writes establish collision-LOD storage, but the current native tree contains no read implementing the complete getter contract. The type metadata and existing model cleanup do not establish the replacement copy, preserved name and acquired references. No executable identity was established for the three unused-value/zero/no-op helpers. Their names and apparent deadness are insufficient evidence, and no implementation was copied or inferred from Linux alone.

The existing `TestNames` backslash expectation was obsolete: it rejected `xmodel\test`, while the preserved intentional `Com_ValidXModelName` verdict accepts Windows separators. Independent Windows disassembly reconfirmed slash at 0x490967 and backslash at 0x49096b within 0x490950–0x490977. The existing assertion was changed to expect acceptance, with the same assertion and case count. This is the only code change in this follow-up; production behavior and all prior ledger evidence are unchanged.

Configured stock/CoD2x Release objects compiled. After that expectation correction, the existing complete xanim/model/skeleton fixture passes both modes under ASan/UBSan. Initial failure logs are retained. Existing synchronized notification and game-animation fixtures also pass. No new tests, cases or assertions, full CI or replays were introduced. Private follow-up evidence is in `recon-animation-remaining.noindex`.


## Original-address follow-up (inline-effects4)

The original Linux 1.3 replacement-error string anchors XModelReplace at0x80c574e–0x80c583f (exclusive end). Its body checks the replacement, preserves the old name, frees the old model, copies the replacement structure, restores the name and retains referenced LOD surface strings. This improves the identity target but does not identify a complete Mac replacement operation. Mac XModelFree consumers alone do not establish that sequence, and a missing error string or direct caller is not grounds for closure. XModelReplace and the other unresolved helpers remain unchecked. No source changes or runtime verification.

## Original getter and small-helper anchors

Original Linux 1.0 0x80c33d6 maps uniquely to 1.3 0x80c59b2: `XModelGetCollLod` returns the sign-extended short at offset 0x7e. Existing Mac model writes establish the storage but not that complete read/result operation. The input-to-global helper at 0x80c5330, integer-zero body at 0x80c5e4c and byte-consuming body at 0x80c5e56 now also have original anchors. Identical tiny bodies alone are ambiguous; full adjacent original model blocks uniquely bind their locations. Those anchors do not establish a Mac global-write, result or no-op identity, and reconstruction names do not prove deadness. All five remain unchecked, including the previously anchored replacement operation.

Evidence and full-block relocation receipts are retained in `recon-provenance-animation10.noindex`. Targeted PPC source/owner comparison adds no new xmodel pairs. No source changes, compilation, tests, CI, replay or runtime exercise ran.


## Model21 original replacement identity and actual cache ownership

XModelReplace remains unchecked, but the original identity is now independently
bounded. Original 1.0 IDB sub_80C3172 is 0x80c3172–0x80c3263 exclusive: 241
executable bytes ending in RET at 0x80c3262. Matching the entire body against
original 1.3 gives one result, 0x80c574e–0x80c583f. The comparison retains 221
bytes, including all internal branch displacements; only four relocated call
operands and one diagnostic address are masked. The replacement-error string,
model bad/free providers and full structure/reference sequence support the
reconstruction name; the original IDB label remains unnamed. Linux supplies
identity and intent, not behavior authority.

The complete original operation checks replacement validity, calls Com_Error
on a bad model, then saves the destination name at +0x88, calls XModelFree on
the destination, copies 0x90 bytes from the replacement, restores the old name,
and acquires every referenced surface-name string from four nonnull LOD name
arrays using signed-short surface counts. The diagnostic is not an early return
if the error handler returns. This is in-place object mutation plus reference
ownership, not just a cache-address update. No arbitrary overlap/error contract
or original incidental EAX value is promoted into a new native API.

A concrete actual Mac replacement candidate was followed completely:
G_OverrideModel 0x1b5eac gets the old model configstring, obtains the selected
model through SV_XModelGet at 0x1b5ecd, writes cached_models at 0x1b5ed2, then
calls Hunk_OverrideDataForFile at 0x1b5eeb with type 4 and configstring+7. The
complete cache provider at 0x37776 scans the hash chain by type/name and replaces
only the matching record's data pointer at 0x377c7, or returns without a match.
Native g_utils_mp.c and com_memory.c preserve this actual pointer-rebinding
operation. It neither mutates the old model nor transfers its LOD references
and old name. No original call/ownership bridge identifies it as the consumed
XModelReplace operation.

The object distinction has a real consumer: Mac DObjCreate loads each supplied
model pointer at 0x75411 and retains it in the DObj at 0x75413; native dobj.c
likewise assigns obj->models from dobjModel->model. This is evidence that model
pointers are stored independently of the cache, not a claim that a particular
gameplay override occurs with a live alias. Cache rebinding cannot silently be
substituted for mutation of that pointed-to model.

The surrounding actual lifecycle was also inspected. Mac XModelPrecache 0x458b0
returns existing cached data or loads/inserts a newly allocated model, storing
the newly allocated cache name at 0x45911. XModelLoad allocates and acquires its
own surface-name IDs through SL_GetString_ at 0xbb9d7; failure branches call
XModelFree at 0xbbfe0/0xbc0d1 and return null. Full XModelFree 0x45b56 skips bad
models, releases each LOD name and clears each name-array pointer. Hunk_ClearData
unlinks matching records before model cleanup at 0x37e7e/0x37eb1. These are real
load/cache/free paths, but combining their fragments would invent the missing
replacement operation. Existing primary verdicts and ownership decisions are
preserved.

One unchecked row refined; no closures, new defects or source changes. The
related DObj and no-notify animation wrappers were not reopened without a new
complete owner lead. Exact Mac owner/provider packets, original binary/body
identity script and relocation receipt remain in recon-provenance-model21.noindex.
Static review only: no compilation, tests, CI, replay or runtime verification.
