# universal/q_shared module reconciliation

Updated 2026-11-04.

**63/90 reconciled: 54 equal, 6 fixed, 3 intentional; 27 unchecked.** Mac 1.3 is the authority. Every assigned row and every live shared-source function was inspected. Linux definitions were read only for identity and intent.

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
