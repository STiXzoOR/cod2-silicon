# script/scr_stringlist module reconciliation

Updated 2026-11-04.

**41/42 reconciled: 26 equal, 15 fixed; 1 unchecked.** Mac 1.3 i386 is the authority. Linux names were used to identify inline helpers; no reference source was copied. The legacy preprocessed source is unchanged.

All 25 exported Mac bodies were compared, including hashing, circular collision chains, move-to-head lookup, three insertion placements, string reclamation, restart traversal, user/refcount transitions and formatted string wrappers. Fourteen Linux-only rows were identified in those concrete callers. Format/error string bytes were checked directly in the Mach-O file. The hash uses signed bytes, 32-bit wrap, modulo 16383 plus one for lengths below 256, and `(len >> 2) % 16383 + 1` otherwise.

## Corrections

**Filename overflow reported the wrong cursor.** Mac 0x43f3d passes the advanced source pointer to `Com_Error`; native passed the original filename. Mac also sign-extends each input byte before lowercase conversion (0x43e97 and 0x43f25), while native converted an unsigned byte. Both details now match.

**Speculative corruption workarounds changed the Mac paths.** Native bounded searches in lookup, interning, unlinking and restart could return, allocate or recycle an unrelated hash bucket after their limit. Mac 0x42ec4–0x42f20, 0x432af–0x43311, 0x42c6a–0x42c83 and 0x43a93–0x43aac follow links until their normal stop. Native also suppressed reference additions when its side cache considered a matched string free; Mac 0x4336a–0x43388 and 0x43488–0x4349a applies the user/refcount transition directly. These workarounds are now excluded from `COD2_X64`; their legacy branches remain. An extra free-list-head check in the empty target-slot insertion path was also excluded, matching 0x43217–0x43264. These differences concern inconsistent/corrupt tables; they were not runtime exercises.

**Lowercasing a zero id skipped the reference reads.** Mac 0x43873–0x4390e enters its lowercase loop using the converted pointer; native skipped that loop for a null pointer and passed an uninitialized local buffer onward. The reconciled path follows the Mac reads. This does not make zero a valid string id.

## Unchecked identities

`SL_TransferSystem` and `Scr_SetStringFromCharString` still lack complete Mac operation mappings. The lowercase default-argument wrapper is now reconciled through the original forwarding seam described below. Missing symbols or callers do not establish equality or prove a missing native implementation.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled in this worker's worktree with low priority and two jobs. All corrected paths were compiled and reviewed, but none was exercised in a runtime scenario. No new tests, full CI, replays or Windows comparison were run. No platform-specific behavior was identified that called for a Windows decision. `COD2_X64=OFF` preprocessing is byte-identical to the assigned base for all three unit source files.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-memory.noindex/`.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.

## Remaining identity follow-up, 2026-11-05

All three unresolved helpers remain unchecked after concrete owner comparisons.
Mac `Scr_FindAnim` at 0x9c1e8–0x9c1fe and `AnimTreeParseInternal` at
0x9c380–0x9c393 supply allocation type 4 to the three-argument lowercase provider,
as native does. Those callers do not identify the separate two-argument type-6
wrapper named by the reconstruction.

Exact `Scr_SetString` at 0x43804–0x4383f retains the supplied numeric string ID,
releases the previous ID, then stores the new ID. It does not establish the
char-pointer setter's release-old/intern-new operation. `SL_ShutdownSystem` at
0x43784–0x43803 clears selected user bits and releases references, restarting
when the table mutates. `SL_TransferRefToUser` at 0x42b6a–0x42b98 handles one
string and may decrement its reference count. Neither is the unresolved
all-strings from/to user-bit transfer operation.

These are rejected candidate identities, not newly discovered production
defects or proof that the missing APIs are dead. No source, established verdict,
or fixture changed. No compilation, CI, tests or replay ran. Private evidence
is in `CoD2-native-decomp/recon-identity-script3.noindex`.

## Original C-string setter provenance

The original Linux 1.0 anonymous body at 0x080797ca and the original 1.3 entry at 0x08079e1a identify the missing `Scr_SetStringFromCharString` operation: remove the old destination reference when nonzero, intern the incoming C string with user zero, then store the resulting 16-bit id. This supplies an original address anchor, but no complete Mac owner has been established. The existing `Scr_SetString` takes an id and adds its reference before removing the old one; it cannot cover this different ordering and input contract. Mac vector-to-string branches found by a bounded call-order search also do not cover arbitrary C-string input. The row remains unchecked. This follow-up changes documentation only; no compilation, tests, CI or replay ran.

## Original default-argument wrapper and transfer provenance

`SL_GetLowercaseString` is equal through an original strict forwarding body. Original Linux 1.0 0x80793d2 and 1.3 0x8079a22 forward string and user unchanged, supply type 6, call the lowercase provider, and return its result. The actual original `Scr_AddStringSet` call at 0x807fdb4 and provider chain distinguish it from a similarly shaped ordinary-string wrapper. This supplies the concrete adapter that the earlier type-4 animation callers did not establish.

The complete primary Mac `SL_GetLowercaseString_` at 0x434b8–0x43555 calculates NUL-inclusive length, applies the unsigned 8192 bound, lowercases signed bytes including the NUL, forwards user/type/length, and returns the interned result or zero if the oversize error returns. At 0x434e3–0x434e6 it reads and forwards its generic type argument unchanged. Native preserves that complete provider. This mapping claims neither a separately emitted Mac wrapper nor a new native export; existing nested storage corrections and ownership adaptations remain intact. Independent review accepted this one-row closure.

Original `SL_TransferSystem` is now anchored from Linux 1.0 0x80799ca to 1.3 0x807a01a. Its complete live-record scan replaces selected user bits without reference release or restart. The previously examined Mac shutdown and single-string transfer providers remain distinct. It and the C-string setter remain unchecked. The full `Scr_AddStringSet` insertion operation also remains separate and unchecked.

This follow-up changed reports only. No compilation, tests, CI, replay or runtime exercise ran. Exact original bodies, the full primary provider and independent review are retained in `CoD2-native-decomp/recon-provenance-script9.noindex` and `units/review-provenance-script9-lowercase.md`.

## Complete C-string destination assignment (stringchar27)

`Scr_SetStringFromCharString` is fixed through the complete actual item-classname assignment. Original1.3 body0x8079e1a–0x8079e56 conditionally releases the old destination ID, interns its C-string input with user0, stores the returned16-bit ID and returns void. The new mapping supplies the full Mac operation rather than the previously rejected numeric-only setter: G_SetConstString0x1b5d06–0x1b5d3b passes its destination and0 to Scr_SetString, passes the unchanged C-string/user0 to SL_GetString, and stores returned AX through that destination. The complete providers cover old-reference release, strlen+1, type6 interning and reference acquisition. Native follows this sequence. Existing a56e4d4b release/unlink and interning/ownership corrections lie inside this complete operation, so the new row inherits fixed status without a new defect.

Actual input and output roles are established by Drop_Item0x1b1414 passing dropped->classname storage to G_GetItemClassname. Its two branches forward a real local weapon-name C-string or item->classname, with the unchanged destination, at0x1af970/0x1af989. This is a complete active argument/provider chain, not a hypothetical composition or constant initialization. It does not assert that this caller exercises a nonzero old reference or previously corrected corruption edges.

Mac's numeric clearing provider also writes0 between releasing the old reference and interning the replacement; the original Linux helper leaves the old numeric ID until its final store. The native operation retains Mac's intermediate zero. The mapping covers the actual returning item-classname role with valid destination/reference state, independent live NUL-terminated source/backing and successful ordinary providers. It does not claim identical original intermediate or nonlocal-failure state, a source pointer into released destination-string storage, arbitrary standalone alias/ABI behavior, reentrancy/concurrency, malformed-state safety, terminal-error continuation or whole owner equivalence. Earlier numeric-only and vector-format candidates remain insufficient on their own.

Independent review accepted this scope in `units/review-provenance-stringchar27.md`; fresh complete original/Mac bodies, real callers/providers and source hashes remain in `recon-provenance-lexset27.noindex`. One closure, no source change or new defect; no compilation, tests, CI, full build, replay or runtime verification. Existing compiled/reviewed provider fixes do not establish runtime coverage of this newly mapped operation. The bulk user-transfer helper remains unchecked.
