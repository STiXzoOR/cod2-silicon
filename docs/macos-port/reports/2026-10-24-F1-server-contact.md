# F1 server contact and friendly-visibility corrections

Native stock and CoD2x behavior changes in `sv_game_mp.c`, based on `bd18d2f`.

`SV_EntityContact` now accepts points strictly inside the expanded cylinder
radius and at/outside the disk threshold. Both comparisons were reversed.
Cylinder vertical exclusion and ordinary brush trace delegation are unchanged.
The query is used by trigger touches, player use and script `istouching`.

`SV_inSnapshot` now continues overflow PVS traversal from the last explicit
cluster through `lastCluster` inclusively. It preserves Mac's unusual final
endpoint-equality rejection: an empty overflow PVS can reach fog acceptance,
while visibility only at the endpoint is rejected. This is original behavior,
not a redesign of visibility. The caller supplies offscreen friendly information.

Mac authority, independently interpreted:

- `__Z16SV_EntityContactPKfS0_PK9gentity_s`: `0x14e3fa–0x14e3fe`
  compares radius squared to distance squared and uses strict `seta`;
  `0x14e32d–0x14e330` compares distance squared to radius squared and uses
  inclusive `setae`. Both exclude unordered floating-point comparisons.
- `__Z13SV_inSnapshotPKfi`: `0x14ea66` branches to `0x14e9aa` retaining
  the last explicit cluster in EDX. `0x14e9b7–0x14e9d7` scans inclusively;
  `0x14e9d9–0x14e9db` retains the equality rejection.

Linux 1.3 was a read-only guide. No reference implementation or disassembly
was copied into the repository. These are bounded corrections, not full
reconciliation of the server module.

`tests/lp64/server/f1_contact.c/.py` links the production server queries and
production vector-distance/fog math. Synthetic services provide brush trace
results and PVS bytes. It checks cylinder/disk inside/equal/outside boundaries,
vertical rejection, brush delegation, explicit and overflow cluster visibility,
empty PVS, endpoint-only and lower-plus-endpoint visibility, fog, and entity
flags. The preexisting use-list test stubs contact and cannot catch this bug.

Before engine edits the fixture reported seven failed assertions. An initial
link failure from duplicate fixture/production fog functions was resolved by
using production fog math before collecting the behavioral red. After edits,
stock and CoD2x both pass under ASan/UBSan with nonrecovering sanitizer checks:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/server/f1_contact.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-server-contact.noindex/green-stock"
taskpolicy -b nice -n 19 python3 tests/lp64/server/f1_contact.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-server-contact.noindex/green-codx"
```

`unifdef -UCOD2_X64` is byte-identical to the baseline; whitespace checks pass.
This does not substitute for the unavailable Linux object/binary comparison.
Integration builds, review and live checks are recorded below when completed.
