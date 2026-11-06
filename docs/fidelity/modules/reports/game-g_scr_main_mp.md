# game/g_scr_main_mp module reconciliation

Updated 2026-11-04; baseline `9b7c4ec`.

**242/242 reconciled: 225 equal, 11 fixed, 6 intentional.** All 229 Mac functions were
compared with the live native bodies and their helpers, 36,592 bytes in total. The Linux-only
helpers are inlined on Mac and were compared at their inlined sites, apart from one Linux
placeholder with no Mac counterpart. Both builtin tables were compared mechanically against
the binary: all 145 function rows and 59 method rows. Mac 1.3 is the authority; Linux 1.3 was
read only. The legacy source is unchanged.

## Corrections

**Script `radiusDamage()` did nothing.** `GScr_RadiusDamage` passed the world entity as inflictor
and NULL as attacker. Mac 0x1994d1-0x199514 and Linux do the reverse, and `G_RadiusDamage`
returns at once on a NULL attacker (0x1c45ca), so every script call was silently ignored.

**A mislabelled builtin was hidden by a workaround.** Mac's table row 127 binds `endparty` to
`GScr_EndXboxLiveLobby`. Native had a second `issplitscreen` row bound to that function, so
`endparty()` in stock scripts was an unknown function. An earlier fallback in
`Scr_GetFunction` hid this with a stub and also made every builtin lookup case-insensitive.
The row is now correct, the fallback is gone and lookup uses `strcmp` as at 0x19b16d.

**Script trigonometry rounded differently.** `sin`, `cos`, `asin`, `acos` and `atan` multiplied
two floats. Mac multiplies in double and narrows only the result (0x1989dd, 0x198a1d,
0x198b39, 0x198bb5, 0x198bef), so scripts received different low bits. `tan` was already
correct.

**`playFx` always normalized the cross product.** Mac rescales it and rebuilds up only when its
length is below 0.999 (0x19fd41-0x19fe2e). Native also normalized lengths from 0.999 upward,
which changed the axes passed to `AxisToAngles`.

**The spawn point warning tested the wrong flag.** Mac 0x19f695 tests `allsolid`, as Linux does.
Native tested `startsolid`.

**Smaller differences.** `precacheShader` restores the text at 0x2dd4b4. `ScrCmd_PlayLoopSound`
resolves the alias before storing entity fields, as at 0x19e631-0x19e643, which matters only
when the lookup raises a script error.

## Retained differences

These rows are `intentional`. None changes successful behavior.

| Function | Mac | Native |
|---|---|---|
| `Scr_PlayFXOnTag` | passes the numeric tag id to `%s` in the error at 0x1999a7, which would fault | passes the tag text |
| `Scr_PlayLoopedFX` | passes an int to `%f` in the error at 0x19c9a4 | passes a double, defined on arm64 |
| `Scr_ParseGameTypeList` | leaves the description buffer unterminated; no minimum name length | terminates it; guards short names |
| `GScr_SetDvar` | cleans up to 0x2000 characters into a 0x400 buffer, result discarded | bounds the loop to the buffer |
| `GScr_LoadScripts` | calls `Scr_BeginLoadScripts` and `Scr_EndLoadScripts` itself; always loads the level script | callers make those calls in the same net order; an empty map name skips the level script |
| `GScr_Stub` | no counterpart | not present; Mac binds table rows to distinct empty functions |

Two original quirks remain as on Mac. Script file index 1 passes the `> 1` check although only
one slot exists, so it reads past the slot. Native CoD2x paths and opt-in trace output are
deliberate and outside this comparison.

## Verification and limits

`tests/lp64/game/scr_main.c` is a new test of the production translation unit with seven groups:
`table`, `trig`, `shader`, `spawn`, `playfx`, `loopsound` and `radius`. All seven fail on `9b7c4ec`
and pass in stock and CoD2x under ASan and UBSan with assertions on. `table` also resolves all 145
Mac builtin names in order. Compile-time assertions tie entity
and script-constant offsets used by these builtins to the Mac values. The harness binds symbols
it does not provide to inert storage, so an unexpected call faults at once.

The local CI steps 02 to 12 passed on this source. The source-based fixture step passed on a
rerun: its first run failed because a new fixture from later work had overwritten
`tests/lp64/game/spawn.c`, which was restored from Git.

Both 2,000-frame replays match a trace from the previous commit on state, player records and
entity counts, and stock matches CoD2x. So that recording reaches none of the corrected paths,
including `radiusDamage()`. Original parity is unchanged: the first state difference is at frame
116 and the first player difference at frame 102.

One fresh local review checked every source hunk and both tables against the binary and
spot-checked about 40 functions marked equal. It found `GScr_RadiusDamage`, which the first
comparison had wrongly passed, so an individual `equal` row here rests on one careful reading and
can still hide a similar slip. A search of this file for other NULL or world-entity arguments found
no further case. The review also noted that the first `table` test covered few rows; it now covers
every name, but not every handler.

Private evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-scr-main-20261104.noindex/`.
