# Hosted player broadcast fixture

`run_player_broadcast.py` extracts the active production game dvar registration,
the complete `G_RunFrame` caller, the player broadcast policy when active, and
the existing `SV_EntityIsVisibleToClientLocal` predicate. It uses the supplied
real engine compile database for both client and dedicated target flags, remaps
the original source root to this checkout, enables assertions, and builds with
ASan and UBSan. Outputs must be in a new private directory.

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/run_player_broadcast.py \
  --compile-db /absolute/private/build/compile_commands.engine.json \
  --output /absolute/private/new-output
```

Repeat with stock and CoD2x databases. `--baseline COMMIT` extracts production
code from that commit while retaining current fixture expectations and actual
engine flags. Against `380bf2b`, CoD2x fails for the absent dvar and frame
refresh; stock passes.

The registration mock records name/default/domain/flags from real production
registration, including listen and dedicated `dedicated` values. It does not
prove dvar allocation or setter clamping. Other game simulation, script, HUD
and end-frame dependencies are stubs. The fixture calls the entire production
frame function and changes health at `ClientEndFrame` to prove the refresh
observes final frame state. It also deliberately gives the frame a simulation
time different from `svs.time` to verify the snapshot clock owns the expiry.

Cases cover 0/15/16/64 players, zero/default/max limits, a 65th entity at the
end of the 1024-entry table, living/dead players, an ET_PLAYER spectator,
inactive/clientless/non-player entities, prior script broadcast values,
repeated frame refresh, hidden PVS, visibility masks/flags, self exclusion,
unlinked entities, exact expiry and the first observation after expiry.

The existing visibility predicate clears an expired broadcast time and still
returns visible on that first observation. A subsequent observation uses the
ordinary visibility tests. The fixture characterizes this behavior; it does
not change the predicate. Disabled or over-limit policy stops refreshing and
leaves existing broadcast times to that consumer.

These are synthetic source fixtures. They do not send snapshots, traverse a
real map portal, or render audio. A second live client must separately verify
fire/footstep delivery and zero/over-limit controls on the composed binary.
