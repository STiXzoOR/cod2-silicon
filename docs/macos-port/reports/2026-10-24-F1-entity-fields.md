# F1 entity-field callbacks

Baseline: `6c6edd0`. Native `g_spawn_mp.c` registered all ten entity fields
with null callbacks. Restore the six callbacks present in the Mac 1.3 table:
origin and angles update trajectories, health updates player health or the
nonplayer maximum, and classname/model/spawnflags reject script assignment.
Generic field parsing and the legacy source are unchanged.

Also restore the generic getter's absent-value behavior: a zero string ID or
null entity pointer produces no pushed value, allowing the caller to supply
undefined. Native previously pushed an empty string or explicit undefined.
Mac `Scr_GetGenericField` branches directly to its return at `0x1a0ccf`
(string) and `0x1a0c99` (entity). The extended fixture failed on the zero
string before this correction and passes after it; nonzero values still push.

Mac authority: `_fields` at `0x36dee0`, ten 16-byte records followed by a
terminator. Callback slots resolve to `Scr_ReadOnlyField` (`0x1a0614`) in
records 0/2/3, `Scr_SetOrigin` (`0x19b24a`) in record 1, `Scr_SetHealth`
(`0x19b2bc`) in record 7 and `Scr_SetAngles` (`0x19b28c`) in record 9.
`Scr_SetObjectField` at `0x1a14f3–0x1a150c` dispatches these callbacks.
The existing native callback bodies match the inspected Mac instructions;
the missing table bindings prevented their execution.

The new production-source `entity_fields` fixture first failed because a
script origin assignment left `s.pos.trBase` unchanged. With the bindings
restored it passes for both stock and CoD2x with ASan. It checks networked
origin/angles, conditional relinking, health with and without a client,
generic count assignment and all three read-only fields:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/game/run.py --build build-macos --test entity_fields
taskpolicy -b nice -n 19 python3 tests/lp64/game/run.py --build build-macos-codx --test entity_fields
```

Both client and dedicated targets build in both configurations. An initial
Toujane DM frame, seed 12345 and 50 ms ticks, now places entities 617/618 at
the reference positions and angles. All 168 entities present in the reference
have matching quantized entity hashes. Thirteen still have small raw floating
point differences. Native still has 37 additional entities (205 versus 168),
so that intermediate run still failed whole-world replay parity. Combined
with the subsequent startup-order fix in `g_main_mp.c`, `initial-startup-stock`
matches the reference's 168 entities and quantized hash `de316057` at frame 4.
This is a callback/getter fix,
not a claim that all game/spawn functions are reconciled.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/F1-shared-wave2.noindex/`.
`initial-stock` contains the bounded engine run; `initial-comparison.json`
lists row differences. `unifdef -UCOD2_X64` output is byte-identical to the
baseline for the changed engine file. No reference source or game assets
were copied into the repository.
