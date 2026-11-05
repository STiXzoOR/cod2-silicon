# universal/q_shared module reconciliation

Updated 2026-11-05.

**Current: 66/89 reconciled: 57 equal, 6 fixed, 3 intentional; 23 unchecked.**

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
