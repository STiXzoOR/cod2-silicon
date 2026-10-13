# WS82: objective compass and location ownerdraw

Date: 2026-10-07. Base: `709e8b4`, branch `codex/ownerdraw-fidelity`.
Scope: FA09b from the full [WS44 audit](WS44-fidelity-audit.md).
The native `CG_OwnerDraw` dispatch now draws compass objective pointers (86)
and translated player locations (33). The i386 dispatch is preserved behind
`COD2_X64`; CoD2x compass visibility and position controls also apply to 86.

## Reference and reachability

The authority is the external owned Mac 1.3 STABS binary, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Its `CG_OwnerDraw` jump table selects `0x18d2ff` for 86 and `0x18e10c`
for 33. The entire objective branch through `0x18d71b` and its out-of-line
fade, interpolation, origin and material branches were reviewed before the
implementation. No binary, disassembly, extracted menu or game content is
included in this change.

| Contract | Reviewed reference | Native behavior |
| --- | --- | --- |
| Pointer eligibility | 16 next-snapshot objectives, state `OBJST_CURRENT`, valid next-snapshot client | Same; objective team does not add a filter |
| Position source | Objective origin for entity sentinel 1023; otherwise `centity_t.lerpOrigin` | Typed fields; reference entity stride 548 and origin offset `0x1ec` |
| Distance | Horizontal distance from `refdef.vieworg` | Height does not change radius or alpha |
| Radius | Minimum radius below minimum range; maximum at maximum range; linear interpolation between | Existing `CG_ApplyCompassPointerRadiusScale` converts the fraction to compass radius |
| Alpha | 1 within compass maximum range; objective minimum alpha beyond objective maximum range; linear interpolation between | Distance alpha replaces menu alpha and is capped by HUD fade |
| Size and orientation | 16 times compass size; yaw relative to updated pointer orientation | Same center and unrotated icon placement; double degree-to-radian conversion before float trigonometry |
| Material | Objective default, or server icon indices 1–127 with nonempty names of at most 63 bytes | Existing server-name validator and `CL_RegisterMaterialNoMip(name, 7)` |
| Location | Next-snapshot client; `infoValid`; direct `clientInfo_t.location`, offset `0x38` | Existing translation routine, drawing at `rect.x`, `rect.y + rect.h` with caller font/style/color |

The owned stock IWD menus contain an active `ui_mp/hud.menu` objective
pointer item using 86, as well as the single-player HUD item. No `.menu`
caller of 33 was found in that stock install. `Item_OwnerDraw_Paint` calls
`UI_OwnerDraw`, which forwards to `CG_OwnerDraw` when cgame is initialized.
Location therefore restores a generic/mod menu path; this report does not
claim a stock-visible location regression. Existing text offsets in the
native dispatch are retained.

The surrounding native compass cases 84, 85 and 89 already honor
`Cod2x_DrawCompass` and `Cod2x_CompassOffset`. Applying those same controls
to the restored 86 keeps the objective icons aligned with the compass and
hidden when its CoD2x visibility control is disabled. This is an intentional
extension of the stock reference behavior. No CoD2x defaults change.

Native snapshot-null and client-index guards avoid drawing before valid
snapshot state exists. Valid snapshot behavior follows the reference.

## Source-executing acceptance fixtures

[ownerdraw.py](../../../tests/hud/ownerdraw.py) compiles the complete production
translation unit with remapped engine compile flags, then executes the real
dispatch with ASan/UBSan and assertions enabled. It also executes production
fading, pointer orientation, radius scaling, angle/vector math, server
material-name validation and location translation routines. Synthetic external
services supply config strings, localization results and CoD2x settings;
the fixture captures the `UI_DrawHandlePic`/`UI_DrawText` arguments. Unrelated
dispatch cases abort if reached. It neither runs nor emulates the i386 engine.

The unchanged `709e8b4` baseline failed nine of the ten case groups in both
stock and CoD2x. Only the negative invalid-client case passed. The restored
source passes all ten groups in each build:

- Six objective states, all 16 slots and the last slot; no team filter.
- Cardinal positions and min/intermediate/max distances; distance alpha;
  vertical separation and coincident configured range boundaries.
- Entity-attached objectives use interpolated entity origins.
- Default, valid, out-of-range, empty and 63/64-byte material names.
- HUD enable/UI visibility, fade expiration, rounded fade duration and fade
  cap behavior; hidden compass paths preserve pointer orientation.
- Next-snapshot client validity, null/out-of-range snapshot state and isolation
  from current/predicted clients.
- Compass scaling, pointer yaw, caller alignments, RGB and untouched input color.
- CoD2x hide/offset controls and stock isolation from those settings.
- Next-snapshot location, translated text, placement/font/style and original
  location behavior independent of the compass HUD fade gates.
- Unknown location, untranslated text and localization-warning fallback.

Reproduce using each build's actual engine compile database:

```sh
taskpolicy -b nice -n 19 python3 tests/hud/ownerdraw.py \
  --compile-commands /path/to/build-macos/compile_commands.engine.json \
  --output /private/qa.noindex/ws82-stock
# Repeat with build-macos-codx. Add --baseline 709e8b4 for the failing baseline.
python3 tests/hud/ownerdraw-reference.py \
  --binary /path/to/owned/cod2mp_mac_1.3_i386 --data /path/to/owned/CoD2
python3 tools/abi/legacy.py --base 709e8b4
git diff --check
```

The compile database paths may belong to another checkout; the fixture remaps
their source/include roots to this checkout. Output must be outside the repo.
The reference validator checks jump-table identities, imported variables,
numeric constants and nested STABS field mappings in place, and optionally
inventories stock menu callers without extracting files.

The owned-reference validator passes. Legacy checks pass with zero mismatches
in five include-free configurations and the actual engine SDK preprocessing
command with `COD2_X64` removed. An initial SDK comparison caught moved
`__LINE__`-generated field-assert typedef names; the native declarations were
moved after those assertions, and the exact comparison then passed.
`git diff --check` also passes.

These are logical HUD draw-command checks, not rendering screenshots or
whole-game parity. Full linked builds, menu sweeps, online gameplay and
screenshots require the orchestrator's combined integration validation and
exclusive interactive game slot. The Linux i386 binary comparison is still
required where its multilib prerequisites are available; the local legacy
guard check establishes unchanged preprocessor bodies only.
