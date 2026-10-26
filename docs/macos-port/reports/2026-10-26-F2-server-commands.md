# F2 — Reliable server commands

2026-10-26; base `4d51bcf`. Two native client repairs derived from the owned
Mac 1.3 MP assembly. The inactive original source remains exact.

## Repairs

Scripted reverb now reads numeric arguments before retrieving the room name.
`CG_Argv` returns one shared 1,024-byte buffer. Previously the arm64 build saved
argument 2's pointer and then overwrote its contents with arguments 3–5, so a
normal `r 1 room 0.8 0.9 0.125` command selected room `0.125`, which the sound
consumer rejected and replaced with the generic room. Existing stock and
CoD2x object disassembly confirmed this evaluation order. Mac
`CG_ServerCommand` reads argument 2 last at `0x1e38bf`–`0x1e38cb`, preserves
that pointer and passes it to `SND_SetEnvironmentEffects` at `0x1e393e`.
`PlayerCmd_SetReverb` produces this reliable command during ordinary script
execution. The previous WS50 sound fixes covered other entry points and did
not repair this dispatcher.

Reliable command `C` now requires a nonzero weapon's `offhandClass`, rather
than accepting any weapon definition. Mac `CG_ServerCommand` calls
`BG_GetWeaponDef` at `0x1e3048`, loads `offhandClass` at `+0x84`, and returns
without changing state when it is zero (`0x1e304d`–`0x1e3055`). Index zero
still clears the selection. This path is reachable through
`PlayerCmd_switchToOffhand`: both native source and Mac
`0x1c2855`–`0x1c287d` check weapon ownership before sending the command,
without checking its offhand class. A script selecting an owned rifle must
leave the grenade selection and its HUD notification unchanged.

## Verification

The new source fixture includes the complete production command translation
unit, the actual shared-buffer `CG_Argv` body, the actual offhand setter and
HUD notification path, and the actual `SND_SetEnvironmentEffects` body and
room table. Only external inputs and outputs are synthetic. Unrelated
command destinations abort if reached. Expected room indices, fade outcomes
and weapon classes are independently specified in the fixture.

Before the repairs, stock and CoD2x each failed the three intended groups:
room selection, fade cases using the room, and rejection of an ordinary
weapon. Wrong argument counts, valid/zero offhands and command-sequence
controls passed. After the repairs, **6/6 groups pass with ASan/UBSan in both
modes**. Cases include three room names, seven fade values around zero and
millisecond rounding, seven wrong argument counts, both offhand classes,
zero selection, no state/HUD mutation for an ordinary weapon, and no repeated
dispatch of an already-consumed sequence. The runner compiles the real
production translation unit with the supplied engine flags before the fixture.

Run from the checkout with private output directories:

```sh
taskpolicy -b nice -n 19 python3 tests/camera/run_server_commands.py \
  --compile-db build-macos/compile_commands.engine.json --output "$E/stock"
taskpolicy -b nice -n 19 python3 tests/camera/run_server_commands.py \
  --compile-db build-macos-codx/compile_commands.engine.json --output "$E/codx"
# --baseline 4d51bcf reproduces the three intended failing groups.
```

The worker used the canonical integration checkout's two engine databases;
the runner remaps source/include paths to its own checkout. Evidence is under
`~/Library/Application Support/CoD2-native-decomp/F2-server-commands.noindex/`.
`unifdef -UCOD2_X64` plus `cmp` is exact against `4d51bcf` for the changed
engine file, and `git diff --check` passes. These are focused checks, not a
Linux i386 object comparison, whole-game audio capture or new full build.

## Bounded module reconciliation

All sixteen Mac bodies in `cg_servercmds_mp` were compared with current source
and full assembly: `CG_ParseServerinfo`, `CG_ParseCodinfo`, `CG_AddToTeamChat`,
`CG_OpenScriptMenu`, `CG_CheckOpenWaitingScriptMenu`, `CG_CloseScriptMenu`,
`CG_MenuShowNotify`, `CG_SetClientDvarFromServer`, `CG_ParseFog`,
`CG_DeactivateChannelVolCmd`, `CG_SetChannelVolCmd`, `CG_DeactivateReverbCmd`,
`CG_SetConfigValues`, `CG_MapRestart`, `CG_ServerCommand` and
`CG_ExecuteNewServerCommands`. Exported instruction bytes were checked against
the owned binary; command/menu jump tables and floating constants were read
from that binary. This verifies the reference used, not native equivalence
by itself.

No further normal-input mismatch was found in the fifteen bodies outside
`CG_ServerCommand`. The dispatcher and its extracted helpers were inspected
across all branches. It is not marked wholly equal: score parsing writes
`clientInfo.score` only for valid client info, whereas the original writes it
before that check; vote substitution differs for empty custom display names
or adjacent map-name matches. No normal producer causing a visible issue was
established for those differences. Native bounds checks remain intact. The
reverb fade helper also omits an intermediate integer-to-float conversion at
large millisecond values, although the sound consumer itself converts the
result to float. These observations remain recorded rather than being
silently counted as reconciled.

Existing disabled tracing and intentional CoD2x dvar handling are retained.
Called subsystems, CoD2x hook internals, the Emscripten alternative and invalid
pointer inputs were outside this bounded comparison. No reference captures,
full-round acceptance or complete F2 parity are claimed.

## Integration and review resolution

Full stock and CoD2x client and dedicated builds passed after integration.
The focused review completed with one P2 finding:
`/tmp/compound-engineering-501/ce-code-review/20261026-092155-50242d36/`.
Correctness and independent Claude Opus 5.5 review identified that the class
check had removed the pre-existing NULL-definition guard. Finding #1 is fixed:
zero still resets, a missing definition is ignored, and registered non-offhand
weapons are rejected. The definition is looked up only once.

The new `C 100` fixture reproduces a UBSan NULL-member-access failure before
repair. All seven groups pass ASan/UBSan in both modes afterward, including
unchanged selection, fade and HUD state for that missing definition. Exact
COD2_X64-OFF source comparison still passes. Evidence is in `review-null-*`.
No actionable review finding remains. The five-lens simplify pass found no
necessary code change. Its proposed removal of the shared-argument-buffer
comment was rejected because the comment documents why room retrieval is last.
