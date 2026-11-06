# bgame/bg_weapons module reconciliation

Updated 2026-11-04.

**136/136 reconciled: 118 equal, 10 fixed, 8 intentional.** Mac 1.3 i386 is the authority. The legacy source is unchanged.

The 56 Mac exports were compared with the native source. Nontrivial bodies were read by hand, including the complete
`PM_Weapon` dispatcher at 0x7cbb2. Its 23-case jump table at 0x3292d0 was decoded from the binary. Linux helper names
were mapped to the corresponding Mac inline blocks and native helpers; the ledger records those ranges.

## Corrections

**Weapon and scoped-view bob rounded too early.** Mac 0x7f97d–0x7f9bd and 0x806df–0x8071f divide in float, evaluate
the cycle and phase additions in double, then convert back to float. Native used a float pi constant and float additions.
The roll-phase subtraction at 0x7fb12–0x7fb1f and 0x80874–0x80885 also uses double before conversion.

**ADS view bob changed the product grouping.** The cycle at Mac 0x80a67–0x80a93 uses the same float-to-double sequence.
At 0x80b55–0x80b6b the bob result is multiplied by the separately computed ADS fraction times view-bob multiplier.
Native chained those factors and multiplied amplitude before the vertical bob's 0.75 factor. It now uses the existing
bob routines and the Mac grouping.

**Idle timers multiplied in a different order before integer conversion.** Mac 0x7f83e–0x7f84e computes frame time
times 1000 before idle speed. Mac 0x80558–0x8057e computes breath scale times frame time times 1000 before idle speed.
Both native timers now preserve that order.

**An empty clip-only weapon could suppress its no-ammo event.** Mac 0x7ddc2–0x7ddd1 emits event 0x92 after calling
`BG_TakePlayerWeapon`, regardless of its result. Native conditioned the event on a successful return.

## Intentional entries

Eight Linux-only helpers are unused: the bulk item rebuild, ammo/clip name lookups, ammo/clip name and count getters,
and shared-cap name getter. They have no Mac symbol or native stub, and the Linux tree has no callers. The lookup
diagnostic strings are also absent from the Mac binary. They were not added as unused native APIs. The shared-cap
size getter is used and was compared in its Mac/native inline location instead.

## Verification and limits

The Release ARM64 `cod2_macos_ded` target built successfully with `taskpolicy -b nice -n 19` and two jobs. The changed
translation unit compiled without warnings. Selecting the `COD2_X64`-undefined source with `unifdef` produces exactly
the same 93,642 bytes before and after the change.

Call/string triage and the heuristic constant checker ran before the manual comparison. No new tests were written,
and no CI or replays were run in this worktree. All corrections are compiled and reviewed, not exercised. Nothing is
left unchecked. No Windows-specific behavior was encountered.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-bg-weapons.noindex/`.

## Integrated batch verification

Integrated source through `e92ca5f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The legacy guard check differs only for the three intended `linux-x86_64-port-on` source paths. An independent reviewer checked the changed source, nearby callers and Mac binary evidence and found no actionable defects; this was not a second whole-module audit.

Both 2,000-frame stock and CoD2x replays are byte-identical to the fresh `d873794` baseline for player, entity and state records. Against the Linux original, all 3,806 player records agree on the six logic fields; maximum origin drift remains 0.11792 units and velocity drift 2.0. The recording does not demonstrate the corrected branches, so those corrections remain compiled and reviewed rather than individually runtime-verified. No new per-fix tests were added.
