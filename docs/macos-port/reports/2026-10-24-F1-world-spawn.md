# F1 world spawn field initialization

The native game initialized map entities before loading the script field
registry. Its temporary empty field buffer made `Scr_FindField` return zero
for every dynamic BSP key. This discarded `script_gameobjectname`, which the
installed deathmatch script uses to delete entities reserved for other modes.

The F0 startup comparison had 205 native entities versus 168 reference
entities. The native-only turret origins correspond to BSP entities tagged
for other gametypes; the bomb triggers correspond to `bombzone` tags. This
localizes the discrepancy to the missing tags rather than entity allocation.
The installed Toujane BSP contains exactly 37 tagged entities not allowed by
deathmatch: seven `tdm`, seven `hq`, twenty `bombzone`, one `sd`, and two `ctf`.
Their classes comprise seven turrets, twenty-five script movers, and five
triggers, matching the native-only entity type counts exactly.
The separate entity-field callback correction already made the quantized
hashes of all 168 common entities agree.

## Change and authority

`G_InitGame` now loads scripts and their Radiant field registry before spawning
entities in the native build. It no longer substitutes an empty field buffer
before spawning. The original OFF source remains unchanged.

Mac 1.3 `__Z10G_InitGameiiii` branches from `0x1adf73` to its cold-load setup
at `0x1aea47`. It calls `GScr_LoadScripts` at `0x1aea7d`, then `BG_LoadAnim`
at `0x1aea82`, and returns through `0x1aeae3` to `GScr_LoadConsts` at
`0x1adf79`. Entity initialization follows, including
`G_SpawnEntitiesFromString` at `0x1ae0ac`. The Linux 1.3 source guide agrees
with this ordering. Neither reference source nor game-script text is included
in this change.

This focused change retains the native build's existing script reload behavior
on map restart. Mac 1.3 only loads scripts on a cold load; full restart lifecycle
reconciliation remains separate work.

`G_SetupWeaponDef` still runs before script compilation. `GScr_LoadScripts`
allocates fresh class maps and entity arrays before spawning; the subsequent
`Scr_InitSystem` initializes VM globals without clearing those entity fields.

## Verification

- `unifdef -UCOD2_X64` output for `g_main_mp.c` matches commit `6c6edd0`
  byte for byte.
- `git diff --check -- src/PC/game_mp/g_main_mp.c` passes.
- Stock and CoD2x client/dedicated targets build with the combined wave changes.
- The stock initial replay now has 168 entities and quantized hash `de316057`
  at frame 4, exactly matching the F0 reference. Before this reorder it had
  205 entities, with all 168 common entity hashes already matching after the
  callback fix. Raw hashes still differ; the comparison uses the established
  oracle's quantization, not bitwise float equality.

The map's 37 non-DM tagged records consist of seven turrets, 25 script movers
and five generic triggers, matching the types of the 37 former native extras.
Private evidence is in `F1-shared-wave2.noindex/initial-startup-stock`:

```sh
python3 tools/parity/compare.py \
  "$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex/original-f0c-initial/statehash.txt" \
  "$HOME/Library/Application Support/CoD2-native-decomp/F1-shared-wave2.noindex/initial-startup-stock/statehash.txt"
```

Result: `PASS: 1 frames match (frame, simt, active entities, q)`.

A private diagnostic driver under `F1-world-extras.noindex` attempted to log
gametype tags around the installed filtering script. Its 20-frame run completed
without script errors but produced no diagnostic log lines; it does not prove
tag values or filtering. The initial-entity replay is the required behavioral
check.
