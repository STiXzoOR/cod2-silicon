# game/g_items_mp module reconciliation

Updated 2026-11-03; baseline `be7cda9`.

Complete comparison: **29/29 entries — 21 equal, 8 fixed**. Mac 1.3 is the
behavioral authority; Linux is read-only mapping/reference material. Every
export and inline helper was compared in full; no reference source is copied.

## Corrections

- Health pickups use item quantity for the amount and double-health exception.
  The original cap/normalization branch structure is restored too; that branch
  difference alone has no demonstrated valid-input output discrepancy.
- Weapon pickups read and update `item.count2` for clip ammo. Initial oversized
  clips clamp without transferring the excess to reserve ammo. Generated
  nonpositive ammo counts clamp to zero.
- Partial ammo remains on the ground when weapon ammo pools are enabled.
  Pickup UI categories use `offhandClass`. No-slot weapons skip forced selection.
- Swapped dropped items inherit spawn flags and ground entity, preserving their
  own tag/parent fields. A failed selected drop returns quietly; exhausted slot
  searches still emit the denial. Weapon-bit tests use unsigned shifts.
- Tagged drops keep the source entity angles and consume angular random values
  in Z/Y/X order, matching the original even where that behavior is surprising.
- Corpse and item movement check `r.inuse` after think callbacks. Live entities
  proceed even with `active` clear; freed entities stop before further movement.
- Both weapon hint-registration errors restore the original leading byte 0x15.

All production changes are under `COD2_X64`; legacy source tokens are retained.
A suspected missing third orientation-axis row was ruled out: both actual
AxisToAngles implementations consume only the first two rows. No fix was added.

## Complete function accounting

| Function | Status |
|---|---|
| Add_Ammo | equal |
| ClearRegisteredItems | equal |
| Drop_Item | equal |
| Drop_Weapon | fixed |
| DroppedItemClearOwner | equal |
| Fill_Clip | equal |
| FinishSpawningItem | equal |
| G_BounceCorpse | equal |
| G_GetAnimDeltaForCorpse | equal |
| G_GetItemClassname | equal |
| G_ItemClipMask | equal |
| G_OrientItemToGround | equal |
| G_RegisterWeapon | fixed |
| G_RunCorpse | equal |
| G_RunCorpseAnimate | equal |
| G_RunCorpseMove | fixed |
| G_RunItem | fixed |
| G_SpawnItem | equal |
| GetFreeCueSpot | equal |
| IsItemRegistered | equal |
| LaunchItem | equal |
| Pickup_Ammo | fixed |
| Pickup_Health | fixed |
| Pickup_Weapon | fixed |
| RegisterItem | equal |
| SaveRegisteredItems | equal |
| SaveRegisteredWeapons | equal |
| Touch_Item | fixed |
| Touch_Item_Auto | equal |

Function-specific addresses, helper mappings and complete comparison evidence
are retained in the ledger. Private notes cover all branches, binary constants,
string/import identities and relevant caller contracts.

## Verification and limits

Four complete-production-TU fixtures pass in stock and CoD2x under ASan/UBSan
with assertions enabled: 3 registration groups, 5 physics groups, 4 drop groups
and 15 pickup groups. The 23 new regression groups fail on the baseline; the
four control groups pass. Actual production math is linked for drop, pickup
and corpse paths. The final pickup fixture uses a valid dropped-weapon queue
limit, and its replacement-field failure was reconfirmed on the baseline.

Both client and dedicated builds and scoped changed-TU ABI audits pass in both
modes. Both legacy checks pass ten inactive configurations. No public signatures
or layouts changed, and no i386 binary identity is claimed.

Domains are valid initialized entity/item/weapon/clip indices, bounded game-time
and ammo arithmetic, positive max health, stable metadata and entity slots, and
normal script error/notification contracts. The drop-queue dvar is restricted
to 1..32. An original nonclient clip-only null dereference is outside defined
behavior. Unchanged forwarders are accepted with their corrected callees.

Machine-local evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-items-20261103.noindex/`.
One fresh local batch review found no actionable defects. It checked all 29
classifications, affected Mac blocks, fixture evidence and compatibility guards;
its receipt is retained in the evidence directory. No external/per-fix review
or additional reviewer agents were used. Integrated replay results follow.

Both 2,000-record/two-client native replays complete and match each other in
state hashes, player fields and entity counts, with matching command SHA, seed
and IWD sets. Compared with the preceding native build, player records and
entity counts are identical, but state hashes change from frame 499. The final
entity dump differs only for corpse 64 and item 151: both now settle instead
of retaining their previous movement trajectories. This is consistent with
the corrected post-think liveness/settling paths and drop behavior; the fixtures
and assembly comparison support that attribution. The review accepted this
bounded explanation without claiming original parity.

The saved original lacks an entity dump, so those final entity changes have not
been compared directly to original entity records. Against its state/player
trace, first differences remain velocity frame 102, quantized state 116, flags
652, view angles 776 and movement time 1803. Entity counts, movement type,
health, weapon and weapon state still match. Whole-project parity remains open.
