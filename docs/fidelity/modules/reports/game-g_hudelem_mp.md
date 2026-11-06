# game/g_hudelem_mp module reconciliation

Updated 2026-11-03; baseline `a890e30`.

Complete comparison: **55/55 entries — 41 equal, 14 fixed**. Four shared
corrections account for the fixed entries; forwarding wrappers are accepted
with their corrected callees. Build and review/replay acceptance are recorded
below. Mac 1.3 is authoritative; Linux is read-only behavioral reference.

## Corrections

- Method lookup now uses case-sensitive `strcmp`, matching the Mac import at
  0x1b30ce. The method table still canonicalizes recognized lowercase names.
- Enum field updates use unsigned 32-bit masking. This preserves the Mac shift
  semantics and eliminates undefined signed shifting for the normal font mask.
- Type-specific resets clear `value`, including text, map, shader, timer and
  clock commands. They preserve movement and fade timing as the Mac does.
- Default resets and reused allocation slots retain `moveStartTime` and
  `moveTime`. The Mac default blocks omit writes to offsets 0x60 and 0x64;
  the previous whole-element memset erased them.

All four changes are guarded by `COD2_X64`; inactive source behavior is retained.

## Complete function accounting

| Function | Status |
|---|---|
| GScr_AddFieldsForHudElems | equal |
| GScr_NewClientHudElem | fixed |
| GScr_NewHudElem | fixed |
| GScr_NewTeamHudElem | fixed |
| HECmd_Destroy | equal |
| HECmd_FadeOverTime | equal |
| HECmd_GetHudElem | equal |
| HECmd_MoveOverTime | equal |
| HECmd_Reset | fixed |
| HECmd_ScaleOverTime | equal |
| HECmd_SetClock | equal |
| HECmd_SetClockUp | equal |
| HECmd_SetClock_Internal | fixed |
| HECmd_SetGameTypeString | equal |
| HECmd_SetMapNameString | fixed |
| HECmd_SetPlayerNameString | equal |
| HECmd_SetShader | fixed |
| HECmd_SetTenthsTimer | equal |
| HECmd_SetTenthsTimerUp | equal |
| HECmd_SetText | fixed |
| HECmd_SetTimer | equal |
| HECmd_SetTimerUp | equal |
| HECmd_SetTimer_Internal | fixed |
| HECmd_SetValue | equal |
| HECmd_SetWaypoint | equal |
| HudElem_AddString | equal |
| HudElem_Alloc | fixed |
| HudElem_ClearTypeSettings | fixed |
| HudElem_ClientDisconnect | equal |
| HudElem_DestroyAll | equal |
| HudElem_Free | equal |
| HudElem_GetAlignX | equal |
| HudElem_GetAlignY | equal |
| HudElem_GetAlpha | equal |
| HudElem_GetColor | equal |
| HudElem_GetFont | equal |
| HudElem_GetHorzAlign | equal |
| HudElem_GetMethod | fixed |
| HudElem_GetVertAlign | equal |
| HudElem_SetAlignX | equal |
| HudElem_SetAlignY | equal |
| HudElem_SetAlpha | equal |
| HudElem_SetBoolean | equal |
| HudElem_SetColor | equal |
| HudElem_SetDefaults | fixed |
| HudElem_SetEnumString | fixed |
| HudElem_SetFont | equal |
| HudElem_SetFontScale | equal |
| HudElem_SetHorzAlign | equal |
| HudElem_SetLocalizedString | equal |
| HudElem_SetVertAlign | equal |
| HudElem_UpdateClient | equal |
| Scr_FreeHudElemConstStrings | equal |
| Scr_GetHudElemField | equal |
| Scr_SetHudElemField | equal |

All 49 exports and 6 Linux-only inline helpers were compared across their
complete bodies. Field descriptors, all 18 method records and all five enum
name tables were decoded directly from the binary. Float constants and libc
imports were resolved independently. The ledger records function-specific
addresses, mappings and evidence; detailed comparison notes remain private.

## Verification and limits

The complete production HUD translation unit is exercised through public script
field accessors, method lookup and allocation entry points under ASan/UBSan with
assertions enabled. Nine regression groups fail on the baseline and pass in
stock and CoD2x, alongside a passing control group. Coverage includes stale-value
clearing for five command families, reset/reuse movement timing, all three
allocation variants, exact method lookup, font masks and preserved alignment
bits. Type changes preserve unrelated movement and fade timing.

Both stock and CoD2x client/dedicated builds and changed-TU ABI audits pass.
Both source-specific legacy checks pass ten inactive configurations. No public
signature/layout changes or i386 binary-identity claims are made.

Equality uses valid registered field indices and VM entity references, initialized
globals, stable slot lifetimes, normal executing-script error behavior, and
representable timer conversions/nonoverflow game clocks. Extra native null guards
do not reproduce the original null dereference after a returning error callback;
the normal VM error path longjmps or fatals. Method lookup tests establish its
contract, not that the compiler necessarily passes mixed-case names. The existing
invalid-enum diagnostic buffer policy is unchanged. No renderer capture or
visual HUD acceptance is claimed from a dedicated-server replay.

Private evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-hudelem-20261103.noindex/`.

One fresh independent local review found no actionable defects. It verified the
four corrections against the Mac assembly, affected callers, tables and imports,
all 55 classifications and the final fixture extensions. Its receipt is retained
in the evidence directory. No external or additional per-fix review was used.

Both 2,000-record/two-client replays match the preceding native state/player
traces and each other exactly. Command SHA, seed and IWD sets match the validated
original trace. Existing original differences remain: velocity frame 102,
quantized state 116, movement flags 652, view angles 776 and movement time 1803.
Entity counts, movement type, health, weapon and weapon state match throughout.
Whole-project parity remains open.

## Math-helper follow-up — 2026-11-05

The complete `FastCeil` operation exposed one additional conversion defect in
`HECmd_SetTimer_Internal` and `HECmd_SetClock_Internal`. Mac 0x1b3d4f,
0x1b3f03 and 0x1b3f39 return `INT_MIN` from `CVTTSS2SI` for nonfinite or
out-of-int-range `ceilf` results. Direct native C casts did not preserve that
contract. A `COD2_X64` helper now handles that result before the existing
parameter validation at all three sites. The two owner rows remain fixed;
their original evidence is retained with this correction appended.

This source compiles with the configured stock/CoD2x arm64 Release client and
dedicated commands. All ten unchanged HUD module groups pass both modes under
ASan/UBSan. The exceptional inputs are compiled and reviewed, not specifically
exercised. Five inactive source configurations and byte-for-byte
`unifdef -UCOD2_X64` output match the base. This file has no `__LINE__` uses.
No tests were added; full integration validation and fresh review remain with
the parent.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
