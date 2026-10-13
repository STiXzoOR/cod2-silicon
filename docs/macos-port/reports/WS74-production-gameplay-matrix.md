# WS74 — Production gameplay revision matrix

2026-10-07. Clean composed base `e4340a1` on branch
`codex/cod2x-gameplay-matrix`, isolated tree
`/Users/stix/.codex/worktrees/cod2x-client-fixes/cod2-native`. This composes
the current release integration with WS68 host ownership and WS66 shotgun work.
WS69 admission remains a separate implementation unit.

## Result

Production gameplay fixtures now exercise remote revisions **0, 3, 4 and 6**
with the real `cod2x_runtime.c` and `dvar.c` selection path, then repeat under
the actual **hosted revision-zero** lifetime. The matrix preserves the earlier
movement and WS53 mantle expectations. No host readiness constant, lifecycle,
admission, Config, Metal or 0.3 work was changed. `COD2X_HOST_REVISION` remains
zero; these fixtures do not certify or activate full hosted revision 6.

The expanded checks exposed and corrected one bounded native stock-fidelity
defect: animation's stock/revision-zero fallback used `eFlags & 0x3` for mounted
players, while the original Mac uses `eFlags & 0x300`. Ordinary low entity flags
could incorrectly neutralize prone yaw/controllers, and actual mounted flags
could miss their neutralization path. A native-only mask corrects four condition
sites: controller neutralization, initial angle swing flags, final torso/legs
yaw reset and torso pitch neutralization. Each has stock/CoD2x alternatives,
so eight source expressions use the mask. The mask expands to the original
`0x3` whenever `COD2_X64` is absent or zero.

## Independent reference contracts

The read-only CoD2x reference was verified at
`/Users/stix/Projects/cod2-native-refs/CoD2x`, pin
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`. No reference implementation or assets
were copied into the repository. Stock authority was the owned Mac 1.3 STABS
binary `/Users/stix/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`.
Private disassembly/layout receipts stay outside git.

- Mac `PM_SetMovementDir` at `0x6ceb8` applies backward-command rotation after
  converting displacement yaw to an integer; CoD2x
  `src/shared/animation.cpp:1331–1339` restricts that rotation to actual backward
  displacement at revision >=3. Literal direction cases include forward travel
  while holding back, diagonals and exact +/-90 degrees. Speed, airborne and
  prone controls remain separately asserted.
- Stock `PM_CheckDuck` contains the four script-event calls at `0x6e700`
  (event 16, prone→stand), `0x6e99b` (13, stand→crouch), `0x6e9c4`
  (12, prone→crouch) and `0x6ea87` (14, crouch→stand). CoD2x
  `animation.cpp:1389–1393` replaces these with controller transitions at >=3.
  The tests assert event calls and resulting effective stance independently.
- CoD2x `src/shared/server.cpp:1393–1413` documents removal of repeatedly
  generated unavailable smoke/frag events, which can overwrite shot events in
  the four-event ring. The fixture distinguishes this flood from the legitimate
  final-grenade event after an actual throw. Availability includes a missing
  inventory weapon, an owned but exhausted weapon and a different grenade class
  still available. One- and two-grenade valid sequences remain distinct.
- CoD2x `animation.cpp:979–984` ignores prone movement yaw only for revision 3;
  4 and 6 retain movement yaw. `1094–1268` defines stance classifications and
  the 200/232/400/200 ms stance blends. Controller reference paths define the
  prone forward/backward, reload and firing offsets. Expectations assert actual
  native angles, every controller/tag component and XAnim blend/rate/index
  arguments; they do not call production helpers to compute expected values.
- Fresh Mac inspection establishes the mounted mask: `BG_Player_DoControllers`
  at `0x182b94` tests `0x3` in the high flag byte; angle sites `0x183f4b`,
  `0x184034` and `0x18406f` do likewise. STABS identifies `entityState_s.eFlags`
  at byte 8. The Mac prone path `0x184019–0x18401e` assigns player yaw plus
  movement yaw to the legs. Stock 16-bit angle quantization remains explicitly
  checked: a torso destination of 18 degrees becomes `17.99560546875`.

## Implementation and verification

`tests/movement/policy_fixture.c` supplies synthetic allocation/string/log
services while linking the complete production runtime and typed dvars. The
matrix calls `Cod2x_ServerInit`, `Cod2x_SetClientDvar`, `Cod2x_ServerSpawn` and
`Cod2x_ServerShutdown`; it never substitutes a fixed game-version return.
Selector -1 denotes the real hosted-zero case. Movement/weapon-only fixtures
supply empty animation-reset endpoints; the animation matrix links the actual
production reset functions. Non-matrix runner behavior remains available.

Actual engine commands came from the parent's current integration tree:

`/Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json`
and `build-macos-codx/compile_commands.engine.json` at the same root. Paths were
remapped to the owned tree. Fixture assertions are enabled without changing the
engine databases. Compilation and checks use `taskpolicy -b nice -n 19`.

Evidence is under `/Users/stix/Library/Application Support/CoD2-native/ws74-*`.

| Check | Result |
| --- | --- |
| Stock and CoD2x full movement matrices | PASS; each repeats the preserved 20,000 whole-player-state comparisons at all five selections, **100,000 comparisons per build**, zero prediction/server differences. Independent full `PmoveSingle` expectations assert forward displacement, 183-unit snapped velocity and stock 90 versus CoD2x 0 yaw while back is held. Existing bob/walk/lean/timer/ladder/prone/collision/jump/mantle/event/sway checks also pass. |
| Independent readable 1.3 movement guide | PASS; 20,000 full-state comparisons in each build, zero differing bytes. The comparison explicitly selects the stock path with `-UCOD2_CODX`, as before; intentional nonzero-revision differences are tested by the independent matrix contracts. Reference bridge source/output remains private. |
| Stock and CoD2x weapon matrices | PASS at all five selections; WS53 sequence, 28 enable/flag/transition and 92 timing/state cases remain unchanged. Production `PM_Weapon`, event insertion, player/entity conversion and `CG_CheckEvents` prove repeated empty-offhand events replace a shot at revision 0 but preserve it at revisions 3/4/6. Actual final throws retain `0xa6` with the grenade index and one legitimate `0x94`; another grenade remaining emits no empty event. Both movement handlers are exercised. |
| Stock and CoD2x full animation matrices | PASS; each has 192 grenade-stance cases, mounted controls and 130 angle/controller/lerp cases across both animation users and all five selections. The CoD2x build also checks production resets. Cases include all 24 controller/origin components for prone forward/backward/reload/fire, revision 3-only yaw, low flags 1/2 versus mounted flags 0x100/0x200, four stance blends and displacement-driven animation rate/index output. |
| Mounted-mask red/green | Before the fix, stock production and `--baseline e4340a1` CoD2x revision 0 fail the independently expected prone yaw for `eFlags=8|1`: expected 45, actual 0. Final stock/CoD2x matrices pass unchanged mounted-mask expectations. |
| Existing non-matrix CoD2x runners | PASS; standalone animation, movement direction/stance and WS53 weapon fixtures retain their original runner mode. |
| Actual production animation objects | PASS; stock/client, stock/dedicated, CoD2x/client and CoD2x/dedicated with actual Release flags. |
| Focused production ABI | PASS; 2 TUs per mode, zero errors, zero mismatches, zero unprototyped floating-call failures. |
| Focused raw-offset audit | PASS; 1 source, 0 sites, 0 unreviewed/unsafe in each mode. |
| Native port-off preprocessing | PASS; 30 combinations of five legacy platforms, undefined/zero native gate and undefined/zero/enabled CoD2x gate, zero production animation differences against `e4340a1`. This is not the Linux i386 artifact comparison. |

The mounted correction deliberately affects native stock and negotiated0.
An arm64 `COD2_CODX=0` comparison must see this intended stock fix; an all-files
CoD2x-inactive checker is therefore not the appropriate disappearance proof for
this particular production hunk. The 30 native-off controls establish its
legacy gating. No engine/shared layout, disk/wire record or animation-table
duration was changed.

## Precise limits and handoff

Collision worlds, weapon/animation definitions, script services and XAnim/DObj
endpoints are synthetic. The complete production angle/lerp/controller code
executes, but no real clips, skeletal evaluation, renderer, game process, GSC
scripts, audio or hit geometry are used. Receiving-client events run through
actual player/entity conversion and `CG_CheckEvents` on in-memory entity state;
packet encoding, transport, second-client sound and snapshot delivery are not
exercised. The angle cases test final controller results, not every intermediate
stance curve or every weapon/animation combination.

Remote revision selection uses the real policy setter and dvars; actual
systeminfo/reliable command parsing and host lifecycle callers remain WS68's
independent fixtures. Hosted gameplay is checked only at the currently owned
zero revision. Dedicated source flags are compiled, not dedicated multiplayer
runtime acceptance. This unit does not establish WS69 authenticated admission,
real reconnect/restart behavior at candidate hosted6, WS65 sound/event visibility,
WS66 live pellet/damage dispatch, or configured40-tick authoritative/prediction
cadence. Parent owns composition, independent review and the full release gate.

Production ownership is limited to `src/PC/bgame/bg_animation_mp.c`.
Tests/docs are the movement runners/fixtures, policy utilities,
`tests/cod2x/animation_stance.c`, `animation_stance.py`, `animation_matrix.h`,
the CoD2x test README and this report. No commits, staging, game/benchmark,
packages, config migration or child agents were performed.
