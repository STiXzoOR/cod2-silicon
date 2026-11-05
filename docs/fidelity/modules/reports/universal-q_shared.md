# universal/q_shared module reconciliation

Updated 2026-11-05.

**Current: 72/89 reconciled: 58 equal, 7 fixed, 7 intentional; 17 unchecked.**

Original pass: 63/90 reconciled: 54 equal, 6 fixed, 3 intentional; 27 unchecked. Mac 1.3 is the authority. Every assigned row and every live shared-source function was inspected. Linux definitions were read only for identity and intent.

## Corrections

Restored the Mac diagnostic text and 0x15 prefixes in `va` (0x28750), `Info_SetValueForKey` (0x28b08), `Info_SetValueForKey_Big` (0x28ce6), `Info_ValueForKey` (0x29478), `I_strncat` (0x296fe), and `ParseConfigStringToStruct` (0x2975c). The info setters also had different newline layout and different overflow/formatting messages. No ordinary info-string append branch needed correction.

The existing launcher fixture expected the old `va string too long` message. Its one-line expectation now uses the Mac diagnostic at 0x287b5. No new tests were written.

## Comparison and native boundaries

Compared byte swaps, signed-byte string comparisons, color escapes, filename scans, info-string parsing, TLS bindings, lean arithmetic and each orientation transform. Decoded the eight `ParseConfigStringToStruct` targets at 0x328380 and compared callbacks, string capacities and conversion order.

Linux names `Com_GetFilenameSubString`, `FloatFromBits`, the two float-as-int helpers and four orientation helpers were matched to their complete Mac/native aliases. The complete bit predicates and stores were checked in `DObjGetHierarchyBits` and `BG_TakePlayerWeapon`; this certifies those helpers only. The ASCII alpha/alnum/print predicates were checked in their complete inlined ranges.

Preserved overlap-safe `memmove` in both removers, optional network bounds in `Info_NextPair`, the native 1024-byte append/NUL bound, and CoD2x's 8192-byte Big info capacity. Stock Big still retains Mac's unusual combined-length limit of 1024. These adaptations are recorded explicitly rather than undoing defined native behavior.

## Evidence gaps

The 27 unchecked rows are: `BigLong`, `BigShort`, `CanKeepStringPointer`, `Com_AssembleFilepath`, `Com_StripFilename`, `Com_WideToNarrow`, `FloatNoSwap`, `FloatSwap`, `I_stricmpwild`, `I_strnicmpInline`, seven `Little*` wrappers/header helpers, `OrientationConcatenate`, two plane transforms, four scaled transforms, both endian initialization helpers, and the misfiled `CG_OffsetThirdPersonView`.

For these Linux-only APIs, no complete native body/caller and matching Mac implementation was established. A related scalar helper, a function-pointer binding, or a zero-address Mac `.eh` label does not establish the complete operation. The private ledger names the individual gaps. In particular, `I_strnicmpInline` is an original-name-unknown Linux claim using locale `tolower`, so it was not silently equated with ASCII uppercasing.

`CG_OffsetThirdPersonView` belongs to `cgame_mp/cg_view_mp.c`. A private routing entry preserves its unchecked status. Mac 0x1d39dd-0x1d39f7 rounds `atan2` to float before degree multiplication; native keeps double precision there. The cgame owner must reconcile that candidate and the CoD2x orbit boundary together. No cgame file was changed.

## Verification and limits

Configured arm64 Release and built `cod2_macos_ded` successfully with `taskpolicy -b nice -n 19`, two jobs. The existing CoD2x large-info fixture passed under ASan/UBSan. The existing launcher config-chain suite and its stock/CoD2x production engine fixtures passed under ASan/UBSan, exercising the corrected `va` overrun diagnostic. Other changed diagnostic paths were compiled and reviewed, not exercised.

Five inactive native-port preprocessing configurations preserve the original legacy tokens. Neither changed C file uses `__LINE__`. Full CI and replays are left to integration.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-text.noindex/`.

## Ownership correction

1 unchecked whole-caller rows were moved to their actual engine modules after integration. Original Mac attribution, IDs and unchecked verdicts are preserved. The preceding audit totals describe the original assignment. This module now holds 89 rows: 54 equal, 6 fixed, 3 intentional, 26 unchecked. The destination owners must audit each complete caller; matching a shared inline helper does not certify it.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.

## Remaining-helper consumer pass (2026-11-05)

Three complete original scalar operations were located in current native owners. `LittleShort` is the unchanged signed word write/read at Mac `MSG_WriteShort` 0x173998–0x17399b and `MSG_ReadShort` 0x1735de–0x1735e5. `LittleLong` is the unchanged full-word write/read at 0x1739c8–0x1739cb and 0x17360c–0x173612. The native message cores preserve the complete scalar results. These verdicts certify only the shared identity operators; existing caller verdicts are unchanged.

`BigShort` is the complete eight-bit word rotate in `NET_StringToAdr` 0x1584e3–0x1584e7 and rotate plus signed extension in `NET_AdrToString` 0x1583b4–0x1583b9. Native narrows the parser input before its shift/or permutation and retains the signed stock formatting cast. CoD2x's later unsigned display remains its existing caller boundary. No reconstructed Big dispatch table is asserted.

Both complete setup branches of live `Swap_Init` also match native: 0x29313–0x29346 assigns five swapped targets and 0x29347–0x2937a assigns five no-swap targets. Independent review checked original STABS: the whole `Swap_Init` has an executable N_FUN record and the branch lines precede its source line, but both setup-helper `.eh` labels are zero-valued private absolute symbols with no executable range. This supports inlined setup operations without securely binding each missing whole-helper identity or its complete state contract. `Swap_InitLittleEndian` and `Swap_InitBigEndian` therefore remain unchecked. No Big-pointer or typed-float wrapper contract is certified from the five matching assignments.

Real neighboring consumers were also checked without extending the verdicts. `FX_GetBoneOrientation` composes its axes and translation using `MatrixMultiply` and `MatrixTransformVector43`, but adds translation after the dot; the q_shared position transform adds the origin first. `R_GetRigidTransform` scales axes before the dot. Weighted surface transforms scale a translated result and accumulate it. These are different float operations from the outstanding concatenate/scaled-position helpers. `FS_BuildOSPath_Internal` inserts separators and handles a fixed OS-path bound, and `FindFirstFileA` bounds a copy that excludes the slash; neither supplies the generic filepath/strip-filename contract. Dvar copy ownership does not supply the stack-plus-TLS pointer predicate. Bounded compares have a concrete libc owner, but the reconstructed inline helper's normalized result and signed-char conversion contract remain unproved.

No source defect was established and no engine source changed. Release stock and CoD2x objects compiled for `q_shared.c`, `msg_mp.c` and `net_chan_mp.c` (six objects). The existing `f1_msg` source fixture passed both configurations under ASan/UBSan, exercising short/long operations through usercmd serialization. The network swap and setup branches were compiled and reviewed, not exercised by that fixture. Triage and constant scripts supplemented exact-symbol disassembly. No tests, cases or assertions were added; no full CI or replay ran.

The 23 remaining unchecked entries retain explicit interface/identity gaps. This pass neither closes helpers through absence nor certifies the complete containing callers. Private follow-up evidence: `~/Library/Application Support/CoD2-native-decomp/recon-shared-remaining.noindex/`.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.

## Retained original-address provenance

The retained 1.0 IDA database and original Linux 1.3 ELF now provide concrete
addresses for 22 of the 23 remaining identities. Masked whole-body comparison
maps 73 q_shared bodies with a common +0x25d8 relocation; 13 identical-body
ambiguities are resolved by the surrounding module sequence. Examples are
Com_AssembleFilepath 0x80b4d74 to 0x80b734c, BigLong 0x80b4fb8 to 0x80b7590 and
OrientationConcatenate 0x80b6a34 to 0x80b900c. Original bytes and the resulting
address map are retained privately in recon-provenance-math4.noindex.

These are identity and intent findings, not Mac behavior authority. No q_shared
row closes from this mapping alone: complete corresponding Mac operations are
still required. I_strnicmpInline remains without an original header-copy
mapping. All 23 previously unchecked rows remain unchecked, and existing
resolved evidence is preserved. No source change, compilation, test, CI, replay
or runtime exercise was performed for this module.

## Defined float byte reversal and retained adapter gaps

Fresh complete provider comparison found a defined-input arithmetic defect in FloatReadSwap. Mac0x28516–0x2854d builds the reversed32-bit word using byte lanes, then loads it as float. Native shifted a signed byte value left24; for values at least0x80 that operation was undefined. Input raw0x00000080 demonstrates the required result,0x80000000 (negative zero), without any invalid storage assumption. The native-only branch now uses unsigned shifts/OR and an unsigned union word. The exact permutation and float reinterpretation stay intact. No runtime failure or changed compiler output is claimed.

The original FloatSwap0x80b7834, FloatNoSwap0x80b7868 and its FloatIdentity provider0x80b933a now have exact executable identities. Their instruction-level representation operations overlap the Mac raw-int-input providers, but that does not establish the missing float-input consumer/adapter or permit C/arm64 callback interchange. Independent review rejected those three closures; both q_shared rows and the separate math header row remain unchecked. FP exceptional representations are not newly certified by source-level union return.

Independent review accepted the concrete FloatReadSwap repair. Manual compilation of changed q_shared.c passed in stock/CoD2x client and dedicated configurations; reviewed source hash3b7767d708c0 matches those receipts. Conditional OFF bytes and five inactive preprocessing line/assert maps are unchanged. No tests, CI, replay, full build or runtime exercise occurred. Private evidence: recon-float-provider7.noindex and manual-float-provider-compile-3b7767d708c0.noindex.


## Original startup endian setup — shared8

Intentional existing startup endian-backend replacement. Original Com_Init 0x80620c0 calls Swap_Init 0x80b7956 at 0x8062102; its host probe positively calls the complete original little setup 0x80b78be at 0x80b796b or big setup 0x80b790a at 0x80b7972. Each helper writes all seven original callback slots 0x85bc800–0x85bc818 (BigShort, LittleShort, BigLong, LittleLong, LittleLong64, LittleFloat, LittleFloatAsInt). Corresponding complete Mac Com_Init_Try_Block_Function calls Swap_Init at 0x30c82; exact Swap_Init 0x292fe probes the host and initializes the selected five-slot Mac backend, swapped at 0x29313–0x29346 and no-swap at 0x29347–0x2937a. Native common.c calls this existing q_shared.c five-slot setup with the exact LittleShort/LittleLong/LittleLong64/LittleFloatRead(int bits)/LittleFloatWrite(float) bindings. This closes the positively mapped consumed startup setup through its concrete existing Mac/native replacement, not equality of seven and five slots, an arbitrary manually invoked/rebound setup API, Big callback availability, or runtime float/wrapper type contracts. All runtime wrapper rows remain independently unchecked. The normal little-endian target selection is established; no execution coverage of either branch is claimed. Independent review: review-provenance-shared8-setup.md. No source change or new table policy; static review only, no compilation/tests/CI/replay/runtime.

Both setup helper rows are now intentional; 21 q_shared rows remain unchecked. This classification does not change the preceding FloatReadSwap repair, its compilation limits, or the separate unresolved FloatNoSwap/FloatSwap adapter contracts. Original LittleLong64 callers were also positively recovered: MSG_WriteInt64 0x8068138 calls it at 0x806816f and MSG_ReadInt64 0x80684ac at 0x80684e1. Their complete Mac/native eight-byte operation still requires its own evidence; no runtime wrapper closure follows from this inventory. Private evidence: `recon-provenance-shared8.noindex`.


## Original sound-prefix comparator — shared9

Intentional existing libc comparison backend for the positively identified prefix operation consumed by Com_LoadSoundAliases. Complete original 1.0 anonymous comparator bodies 0x80ab144/0x80b3458 map to four retained original 1.3 copies by masked whole-body comparison; the actual sound owner 0x80ad0aa uniquely calls 0x80ad4dc at 0x80ad0c9 with maps/mp/, count8 and at 0x80ad0ff with maps/, count5. Full original helper includes signed-byte tolower, normalized -1/0/1 returns and bounded/NUL tail. Fresh complete Mac Com_LoadSoundAliases 0xacc20 replaces those same comparisons with direct strncasecmp calls at 0xacc4f/0xacd97; success selects the +8/+5 input before Com_StripExtension at 0xacc6c/0xace67, while neither match copies the full name at 0xacdb1. Native com_sndalias.c uses the same counts, literals and result-selected paths; Mac/Tools/i5stdlib.c strnicmp forwards the exact arguments to strncasecmp, as does complete Mac strnicmp 0x25902. Original and Mac literal bytes independently checked. This certifies the concrete selected library backend in that consumed prefix role, not equality inferred only from zero/sign use or full normalized-helper/locale/signed-char/all-count return contracts, arbitrary reusable API, other callers or the whole sound loader. No new locale policy or source change. Independent parent review: review-provenance-shared9-strnicmp.md. Static binary/source review only; no compilation, tests, CI, replay or runtime verification.

The prior generic comparison rejection is preserved: sign/zero consumption alone does not prove the full normalized comparator. This closure adds the complete original executable helper and actual owner mapping to the observed Mac/native library replacement. The callback-slot scan in this follow-up found only setup stores among literal references in Mac text/coalesced text; this is bounded evidence and supplies no closure for runtime wrappers. Twenty q_shared rows remain unchecked. Private evidence: `recon-provenance-shared9.noindex`.


## Complete bone/entity orientation composition

Complete consumed bone/entity orientation composition identified, with existing approved native packing adaptation. Original OrientationConcatenate0x80b900c sends all3 axis rows through full OrientationDirToWorld0x80b8dde, then origin through OrientationPosToWorld0x80b8d30. Fresh exact Mac FX_GetBoneOrientation0x59230 validates/selects actual bone quaternion+translation and obtains the entity axis+translation scratch. At0x593db MatrixMultiply consumes the full local tagAxis and entity axis and writes all9 output-axis values at orient+12; adjacent0x593f4 MatrixTransformVector43 consumes that same bone translation+16 and same entity scratch and writes all3 output-origin values. Complete Mac math providers and native all-axis/origin operation were read. Native retains the already-approved origin-first CG orientation to axis-first scratch packing (primary mac:0x00059230 intentional); no new layout policy or source change. Intentional covers this complete disjoint consumed composition and existing adapter, not an exported arbitrary-overlap API, invented original caller or whole-FX-owner recertification. Prior Linux-origin-first arithmetic order is identity/intent evidence only: authoritative Mac products-then-origin behavior is retained, not replaced with Linux x87 ordering. Independent review accepted full operation and classification: review-provenance-orientation11.md. Private fresh bodies: recon-orientation11.noindex. No compilation, tests, CI, replay or runtime verification.

This supersedes the earlier incomplete comparison for this row. The positive complete active operation, all twelve outputs and actual input/storage relationship carry the verdict; missing original external callers or zero-address metadata do not. Mac floating-point order remains authority throughout.

## Additional concrete owner comparisons

The remaining wildcard comparator cannot be supplied by `Com_Filter`. Exact Mac 0x47f84–0x4828a returns boolean success one, accepts when the pattern ends and implements bracket/range grammar. Original `I_stricmpwild` at 0x80b7c3c has a recursive star/question-mark operation and zero/normalized-sign comparison results. Native filesystem filter and platform wildcard consumers do not establish its complete interface.

The complete Mac `MacStrings::GetCString` at 0xa144–0xa1ac consumes a length-prefixed UTF-16 object, initializes output empty, creates/converts/releases a CFString and passes a signed-short capacity. Original `Com_WideToNarrow` at 0x80b80c8 instead copies each odd source byte and writes a final NUL at length minus one. The current character-conversion owner is not that input/termination contract.

Exact Mac `R_FrustumClipPlanes` at 0xeef0e–0xef030 computes four-coefficient matrix products, normalizes the plane and emits culling metadata. It does not establish the four missing orientation-plane helpers' direction-plus-distance operations. A hypothetical matrix adapter with normalization removed would not be a concrete existing owner. The separate scaled-position, filepath, ownership-predicate and endian-wrapper gaps retain their earlier evidence and decisions.

These six refined entries remain unchecked. Orientation concatenation is handled by a separate complete-owner review. No source changes, compilation, tests, CI, replay or runtime exercise ran. Fresh exact candidate bodies and the focused owner review are retained in `CoD2-native-decomp/recon-provenance-shared11.noindex`; prior original bodies/callers remain in `recon-provenance-shared8.noindex` and `recon-provenance-shared9.noindex`.

## Complete consumed scaled point transform (scaledpos14)

`ScaledOrientationPosToWorld` is equal for the actual disjoint three-coordinate point transform consumed as the fourth row of `R_GetRigidTransform`. Original 1.0 0x80b6afe and 1.3 0x80b90d6 identify the complete point/axis/uniform-scale/origin operation. Fresh complete primary Mac `R_GetRigidTransform` scales all nine axis components into float locals at 0xcfd4d–0xcfdbf, then at 0xcff4c–0xcffd6 transforms bone position +0x30/+0x34/+0x38 with those axis columns, adds the corresponding origin and stores all three results at output +0x24/+0x28/+0x2c. No other bone row, weight, normalization or extra translation enters this point result. Native `r_model.c` preserves the complete Mac calculation. Linux dot-then-scale grouping supplies intent only, not behavioral authority.

This result has an actual input and consumer. Mac `R_SkinStaticModelCachedCmd` copies the base-pose translation directly into a separate stack bone matrix at 0xdfdd4–0xdfdec, then at 0xdfe21 passes it with the instance origin/axis/scale and a distinct output matrix. Both normal and DX7 vertex paths consume all three output translation coordinates at 0xe00f7–0xe0181 and 0xe03a3–0xe042e. Native uses separate boneMatrix/useAxis/normAxis arrays and the same typed instance fields. Its typed origin/layout adapter predates this reconciliation baseline; this identifies no new defect or adaptation. The complete vertex transform also uses bone rotation and source vertex coordinates and is not equated with the helper. The earlier transformed position row itself is the complete consumed operation.

Native stores the three dot results before adding origins, whereas Mac finishes one coordinate at a time. The established disjoint storage makes that scheduling immaterial to the compared final coordinates. Arbitrary overlap, concurrent observation and floating-point trap timing are excluded, as are an exported helper ABI, literal inline ancestry and whole-renderer equivalence. This supersedes the prior rejection based only on axis pre-scaling; weighted surface transforms remain a distinct contract.

The original helper identity, complete Mac provider and actual source/result chain were independently reviewed. One equal row, zero new defects, no production changes. No compilation, tests, CI, replay or runtime exercise; earlier verification is not expanded. Private exact bodies: `recon-provenance-scaledpos14.noindex`; independent review: `units/review-provenance-scaledpos14.md`.


## Scaledlocal16 consumed uniform-model inverse position

ScaledOrientationPosToLocal is equal for the complete actual uniform-model
inverse point operation. The recovered original 1.0 identity 0x80b6c0e and 1.3
body 0x80b91e6 establish orientation origin, axes, scalar scale and inverse-point
intent. Linux's direct reciprocal/dot grouping does not supply Mac behavior.

The explicit Mac modelscale branch converts atof once to float32 and writes the
same scalar into all three scale slots at 0x77cbe/0x77cc3/0x77cc8. When that value
is not superseded by a later modelscale_vec key, the actual parsed origin and
AnglesToAxis orientation feed nine row-scaled coefficients, then MatrixInverse
at 0x77aec writes the same model's persistent inverse storage at +0x14. The full
provider at 0x2a79e unconditionally computes determinant, reciprocal determinant
and all nine signed cofactors. Native cm_staticmodel_load_obj.c and com_math.c
preserve this actual input/storage connection and the Mac arithmetic sequence;
no hypothetical precomputed-matrix adapter is needed.

CM_TraceStaticModel 0xbd750 and Complete 0xbd86e subtract that model's origin from
both actual endpoints and pass this same inverse to MatrixTransformVector at
0xbd7a5/0xbd7e7 and 0xbd8c0/0xbd902. The complete provider at 0x2a946 writes all
three ordered column dots to distinct localStart/localEnd buffers, which the real
XModelTraceLine calls consume. Native cm_staticmodel.c preserves those typed
fields, disjoint buffers, complete coordinate outputs and consumers. The LP64
layout uses typed fields, not copied i386 offsets.

The loader's ordered zero guards send both signed zeros to Com_Error; their parity
branches let NaN bypass those guards. Reaching stored inverse data therefore does
not imply finite scale/angles, a nonsingular matrix or finite output. MatrixInverse
has no status result or singular check: tiny/infinite/nonfinite inputs and an
underflowed, zero or nonfinite determinant still follow the reviewed unconditional
reciprocal/cofactor and endpoint-dot sequences. This mapping does not equate
matrix inversion with the original transpose/direct-reciprocal formula, certify
NaN payloads or trap timing, or recertify general error continuation and the whole
loader. Nonuniform models, arbitrary input axes/overlap, a standalone helper ABI,
Linux x87 behavior and subsequent collision result/normal handling are outside
this consumed-operation claim.

One focused equal closure; no new defects, source changes or primary overrides.
All previous source and verdict decisions, including the loader's earlier error
diagnostic correction, are preserved. Independent review is recorded in
review-provenance-scaledlocal16.md; exact bodies, uniform-key string extraction
and review receipts remain in recon-provenance-scaledlocal16.noindex. Static
review only: no compilation, tests, CI, replay or runtime exercise.
