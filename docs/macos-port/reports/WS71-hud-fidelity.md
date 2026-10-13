# WS71: turret reticle and spectator control hints

Date: 2026-10-07. Branch: `codex/hud-fidelity`. Clean starting revision:
`0f07adc3cd91dbf11b70abc8c1a9552e57eb0d0c`.

This closes the two confirmed `cg_draw_mp.c` omissions in WS44 FA09. It does
not address the separately plausible owner-draw/compass finding in
`cg_newDraw_mp.c`. Only the native `COD2_X64` paths change. The original source
bodies remain in their legacy branches; no shared layout, blob, renderer,
font-selection or CoD2x feature implementation changes.

## Reference and causal chain

The authority is the externally owned Mac 1.3 STABS binary
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The original routines were inspected locally. No reference implementation,
assets or disassembly were copied into the repository. The new implementation
and synthetic fixtures were independently written from reviewed behavior.

`CG_DrawTurretCrossHair` (`0x1cf59c`–`0x1cf6f6`) checks the turret draw dvar and
pause state, obtains the mounted entity's **next** state weapon, gets its weapon
definition and uses that weapon's registered `hReticleCenter` material. It
returns for a zero weapon, missing material or crosshair alpha below 0.01.
`predictedPlayerState.pm_flags & 0x100000` changes the color to
`(0.25, 1, 0.25)`; otherwise the reticle is white. The target-color flag is
`pm_flags`, not `eFlags`. Its size comes from `iReticleCenterSize`, doubles in
split-screen, and draws through `CL_DrawStretchPic` at negative half size with
alignment `(2, 2)` and UV `(0, 0, 1, 1)`. The existing native implementation
ignored these inputs and issued an 8×8 white-material square at `(316, 236)`
with alpha 0.75, causing the wrong material, geometry, color and visibility.

`CG_DrawSpectatorMessage` (`0x1cfeba`–`0x1d0138`) is the control-hint routine.
It requires `cg_descriptiveText`, rejects key catcher bit 8, and reads
`nextSnap->ps.pm_flags & 0x3000000`. It refreshes controls, then builds these
localized instructions:

| Permission bit | Instruction | Command selection |
| --- | --- | --- |
| `0x1000000` | `PLATFORM_FOLLOWNEXTPLAYER` | `+attack` |
| `0x1000000` | `PLATFORM_FOLLOWPREVIOUSPLAYER` | `toggleads`, falling back to bound `+speed` |
| `0x2000000` | `PLATFORM_FOLLOWSTOP` | `+melee`, falling back to bound `+melee_breath` |

Each selected command is resolved through `GetKeyBindingLocalizedString`.
Unbound commands use localized `KEY_UNBOUND`; the selected key text is inserted
through `UI_ReplaceConversionString`. The font scale is exactly float bits
`0x3e555555`, equivalent to `5/24`. Text starts at x=240 and
y=`436 - 2 * UI_TextHeight(font, 5/24)`, advancing one font height per line;
alignment is `(0, 0)`, color is white and style is 3. This scale is constant
across split-screen states. The previous native routine checked `snap.pm_type
== 5` and drew only a spectator heading. `CG_Draw2D` already owns the spectator
heading and calls this routine when the scoreboard is absent and HUD menus
are visible; the omitted permission/binding paths prevented control hints.

The optional private-reference check validates the exact digest, reviewed
import identities and constants, and nested STABS offsets. It confirms the
mounted entity weapon at `centity.nextState.weapon`, weapon-info reticle field,
weapon reticle size field, predicted flags and view lock, and next-snapshot
flags. It is a metadata/literal check, not i386 execution or automatic proof of
the manually reviewed branch logic.

## CoD2x and implementation scope

The existing `CG_DrawFollow` option to suppress the spectated player name is
preserved and covered as a passing control in both configurations. Inspection
of the private CoD2x drawing hooks found no turret-specific override in this
scope. A red enemy turret tint in a separate Windows reconstruction is not the
Mac 1.3 contract and was not added. Stock and CoD2x therefore both restore the
Mac reference's white/friendly-green turret behavior.

The implementation uses typed native `cg_weaponsArray` storage, the same
storage used by native weapon registration, rather than introducing i386 byte
offsets. Spectator control data uses bounded three-entry command/message
arrays and a 256-byte key buffer. A null `nextSnap` returns without drawing;
normal reference callers already provide a snapshot. Return values retain the
native function's integer convention and are ignored by `CG_Draw2D`.

## Verification

`tests/hud/run.py` includes the complete production HUD source, obtains the
active production client draw adapter, and links production screen placement.
Turret commands are captured at the renderer entry point. Spectator commands
are captured at `UI_DrawText`; localization, key bindings and font height are
independent synthetic inputs. The runner uses the engine compile database's
flags, compiles the current production translation unit unchanged, and enables
assertions plus ASan/UBSan for the fixture objects. It supports sibling compile
databases by rebasing source/include paths to this checkout.

| Check | Stock | CoD2x |
| --- | --- | --- |
| Baseline `0f07adc`: seven FA09 groups | All fail | All fail |
| Baseline: unchanged follow-policy control | Pass | Pass |
| Restored: all eight groups, ASan/UBSan | Pass | Pass |
| Scoped ABI audit: 9 production translation units | 0 errors, 0 mismatches | 0 errors, 0 mismatches |
| HUD raw-offset gate | 2 existing sites, 0 unreviewed | 2 existing sites, 0 unreviewed |
| HUD import-indirection audit | 0 proven missing/extra dereferences | 0 proven missing/extra dereferences |
| Five include-free `COD2_X64`-off configurations | 0 mismatches | 0 mismatches |
| Actual SDK command with `COD2_X64` removed | 0 mismatches | 0 mismatches |

The ten legacy patch/configuration source controls also report zero
mismatches. Python syntax compilation and `git diff --check` pass. The stock
compiler retains the pre-existing diagnostic `Com_Printf` implicit-declaration
warning, also present when compiling the baseline; no new warning was added.

`tests/cod2x/check_inactive_gates.py --base 0f07adc` is a CoD2x-only-change
guard and reports two intentional native stock differences
(`linux-x86_64-port-on-codx-zero`, `arm64-codx-off`); its other 15 configurations
are unchanged. Because this restoration deliberately changes native stock
behavior, that whole-script invariance requirement is inapplicable. Its failure
was retained and not relabeled as a pass or repaired by changing expectations.
The actual legacy and SDK checks above are the applicable inactive guards.

The fixtures cover material and next-state selection, pause/draw/alpha gates
including the 0.01 boundary, friendly-color flags distinguished from `eFlags`,
and centered geometry at 640×480, 1280×720 and 2560×1440 with full and half-height
viewports. Spectator cases cover permission combinations, next/snap disagreement,
descriptive-text and input-catcher gates, preferred and alternate controls,
unbound localized keys, multiple key names, refreshed bindings across draws,
exact font-scale bits and text layout in both split-screen states.

Evidence is outside git in
`~/Library/Application Support/CoD2-native-ws71/`: baseline/restored case logs
and JSON, scoped compile databases, ABI/raw-offset/import summaries and the
private-reference check log. No game data is needed to run the fixtures.
See [the fixture README](../../../tests/hud/README.md) for commands.

## Review and limits

The bounded diff was reviewed manually for reference dataflow, draw ordering,
array/buffer bounds, typed storage, inactive guards and CoD2x hooks. The
`ce-simplify-code` reuse, quality and efficiency rubrics were applied inline
under the assigned no-child-agent constraint. No worthwhile behavior-preserving
rewrite or actionable defect remained. Separate per-frame binding-selection
lookups preserve the original ordering and refresh behavior; caching or folding
them would change that contract.

This work does not execute the i386 binary, compare rendered screenshots,
exercise actual keyboard events, load real localization files or fonts, test
the complete `CG_Draw2D` pipeline, run a game/benchmark, or establish whole-game
fidelity. Renderer pixel output and live mounted-turret/spectator behavior
remain parent-owned integration checks. No commit, staging, publication or
external review was performed.
