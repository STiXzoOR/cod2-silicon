# WS92: restore corpse animation ownership and vacant slots

2026-10-07, based on `f5b83ad`. Native stock and CoD2x share this repair.

A real spawned test client exposed a death-path crash: `PlayerCmd_ClonePlayer`
passed a null destination to `XAnimCloneAnimTree`. New levels allocated 64
player animation trees but omitted the eight corpse-owned trees. Restoring
that allocation then exposed a second defect: vacant corpse slots had entity
number zero, so the first clone selected and freed player entity zero. A later
spectator lookup dereferenced that player's cleared client pointer.

The owned Mac 1.3 reference establishes all three lifecycle operations:

- `G_InitGame` at `0x1aeac3..0x1aeae1` allocates eight corpse trees after the
  64 player trees, only when not restarting, using the same animation set and
  server hunk allocator.
- `0x1ae104..0x1ae120` resets all eight corpse entity numbers to -1 after
  gametype startup on every initialization, including restarts.
- `G_ShutdownGame` at `0x1af068..0x1af09c` frees the owned corpse tree when
  releasing scripts. An unused slot has no copied clientinfo tree, so freeing
  that alias misses unused allocations.

The native path restores these operations. No shared layouts, clone physics,
death timing, gameplay rules, or spectator lookup behavior change. Existing
native-off production operations remain intact.

## Verification

`tests/fixes13/corpse_trees.c` executes extracted production initialization
and teardown blocks with controlled allocation seams. Independent expected
behavior is 64 player trees plus eight distinct corpse-owned trees, vacancy
reset on cold load and restart, no tree reallocation on restart, and one free
per owned corpse tree whether the slot was used or unused. Repeated cleanup
does not free twice.

The baseline failed with 64 allocations instead of 72. Allocation alone then
failed to free unused slots. The additional vacancy test failed with entity
zero before the reset repair. Final stock and CoD2x ASan fixtures pass, as does
the existing level-script startup fixture. An independent reviewer reran both
fixtures in both modes and reported no blocking findings.

Private runtime overlays replace only `g_main_mp.c` in the frozen `f5b83ad`
build. The same private DM/test-client probe that exposed both crashes now
completes in stock and CoD2x, with spawned remote bodies, screenshots, and
clean exit. These bounded runs do not prove all deaths, corpse-pool eviction,
respawns, or map transitions. Full composed builds and gates remain pending.

The generic native-off checker reports five differences in the existing
`level_scripts.c` test adapter, which now exposes the script-data import to
the extracted reset block; production `g_main_mp.c` has zero differences.
The generic SDK pass selected no commands because its database belongs to
another worktree. The independent reviewer separately remapped the actual
stock command and verified identical native-off production preprocessing.
Neither result establishes Linux i386 object/binary equality.

Machine-local evidence is under
`~/Library/Application Support/CoD2-native-current-readiness/ws92-corpse.noindex`:
`stock-spawned` records the intermediate entity-zero failure;
`stock-slots-spawned` and `codx-slots-spawned` contain final runtime receipts;
overlay manifests identify replacement source hashes and binaries.
The earlier null-tree crash is under the adjacent
`remote-runtime-f5b83ad.noindex/stock-spawned` directory.
The independent review is
`/tmp/compound-engineering-501/ce-code-review/ws92-corpse-independent/report.md`.
Original disassembly, game content, screenshots, and runtime logs stay private.

The user subsequently reported yellow tint and scoreboard rectangles. Both
are tracked separately as WS93; a clean corpse run is not renderer acceptance.
