# Whole-game fidelity acceptance before 0.3

The user explicitly reaffirmed whole-game fidelity and regression prevention on 2026-10-07. The latest instruction authorizes autonomous checks and removes a required manual playtest. Engineering workers use Sol 6.1 xhigh; design workers use Astra high.

## Current scope override

Current authorized delivery is **0.2.1**: composed gameplay readiness plus
Config Studio foundation and remaining units, local match summaries and the
server ready check. Metal and other 0.3 renderer work remain parked. The
orchestrator owns commits, PR, merge, tagging and release publication. Player
directory publication remains unavailable until permissions and production
trust and hosting are established. Historical evidence below retains its named
revision; final composed build, runtime, UI, CI and package acceptance require
fresh receipts for the released source and archive.

Current integration `codex/current-readiness` includes WS45 harness/diagnostics,
WS47 registry/grammar/language/commands (trimmed profile storage scope), WS48
protocol masks, WS50 environment/reverb, WS52 capsule/stationary collision,
WS53 mantle weapons, WS54 fog/objectives, WS55 startup order/configstrings,
WS57 item-config bounds, WS58 MG sensitivity, WS59 script failure checks,
WS60 camera attachment, WS61 sight traversal/box culling, WS62 authoritative
grenade stance, WS63 client registration flags, WS64 snapshot intervals,
WS65 player broadcast and WS67 browser display sorting. Each report specifies
its actual verification limits. That checkout's completed audit at `0f07adc`
passes the full stock/CoD2x ABI, imports, raw offsets, inventory and tables;
the two default calibration checks still reject repaired historical controls.
`codex/readiness-composed` at `8e8d363` additionally composes WS66
shotgun spread, WS68/69 host lifecycle/admission, WS70 fonts, WS71 HUD, WS72
console/domain bounds, WS73 numeric fields, WS74 gameplay revision matrix and
mounted animation flags, WS75 water, WS76 script identity, WS77 master defaults,
WS78 calibration controls, WS79 UDP operations, WS83 challenge arithmetic,
WS80/84 compiler/parser and builtin operands, WS87 literal/local-name operands,
WS82 compass/location drawing, WS81 StopGameType lifecycle and WS88 player
skeleton bind translations.
Parent reruns pass for the affected bounded fixtures, including the composed
host lifecycle, UDP, admission and real callback/VM paths. Fresh stock and
CoD2x client/dedicated builds passed at `6e9a75f`, before WS81 integration.
WS88 completed private renderer overlays and released those objects.
Final builds and broad acceptance of the final combined source remain pending.

WS56 adds a current-source CoD2x client/host gap audit. Its CR03/CR07 host
lifecycle and admission now have production-path fixtures. WS80/84 and WS87
have passed independent review and production-path sanitizer fixtures.
WS81 independent review found and then verified fixes for error-unwind recovery
and queued-command approval identity. Its 15 live cases passed in a private
four-TU overlay of `8f17eaf`, including boolean arguments, intervening FIFO
commands and real VM error shutdown. Composed fail-fast ASan/UBSan callback
and VM checks pass. Dedicated shutdown removes the map command in the original
engine too; the recorded restart is a fresh engine, not a same-process restart.
WS82 owner-draw fixtures pass in both modes; live objective rendering remains
separate. WS86 records actual input, menus, focus, joins and gameplay runs of
frozen `8f17eaf` binaries. It also preserves the incomplete third-person model
failure. WS88 subsequently proved and repaired missing child bind translations:
matched stock before/after and corrected CoD2x captures show complete local
bodies. Independent code review and extended skeleton/CPU skinning sanitizer
fixtures pass; identified remote/bot visual proof remains open. WS89/WS91 retain
bounded original-codec comparisons and remaining PCM differences. WS110 restores
incoming playback without capture, WS112 restores the original 8192 Hz rate, and
WS119 records bounded relay/playback and spectator-routing evidence. Final
composed voice acceptance remains a separate current-release check; microphone
capture and universal original PCM parity are not established. WS90 records
its positional audio/mixer checks and their limits.
Additional GSC/server management breadth remains separately recorded.
Existing client checks do not certify server behavior. Host readiness remains
zero until the composed contracts and their validation support advancement.

## Authority and limits

Read WS44-fidelity-audit, WS43-faithful-move, WS42-faithful-ui, WS38-lp64-audit, WS38-core, WS12-abi-audit and `docs/macos-port/parity.md` alongside current source. Reports describe their recorded commit, not necessarily the integrated source. The Mac 1.3 reference is authoritative for stock behavior; actual CoD2x 1.4.6.8 code is authoritative for intentional extension behavior. Do not copy incompatible licensed reference code.

Structural score, a successful build, the existing stub baseline, and a menu smoke are not whole-game acceptance. WS44 found both false positives and confirmed defects with low/zero structural scores. Every restored behavior needs a production-path regression test with an independent expected result, and runtime proof where relevant. Do not clear a known difference by changing the test expectation or baseline.

## Current integrated evidence and ownership

| Area | Evidence already present | Remaining acceptance and next owner |
| --- | --- | --- |
| ABI and pointer widths | WS12 plus WS38 declarations/imports/raw-offset gates | WS45 reruns actual stock/CoD2x compile databases, private imports, typed-data round trip, legacy source guards; preserve separate unavailable Linux object gate |
| Menus/input/focus | WS42 real SDL input tests, 87 checks/build, fullscreen subset | WS45 reruns clicks, scroll, double-click joins, option labels/binds, Cmd-Tab, default quiet diagnostics and console version; subjective hardware feel is not claimed |
| Movement/prediction | WS43 reference comparison; WS74 real revision selection 0/3/4/6 and hosted0, 100,000 prediction/server state comparisons per build, plus independent guide20,000 each | Final composed runtime; collision worlds and animation definitions in the matrix are synthetic, so separate rows below remain |
| Dvars/grammar/profiles | Source research has concrete reference differences | WS47 verifies/fixes cheat flags, registration ranges/owner bits, commands, tokenizer and writable active-profile paths; ISO mapping must follow evidence, not literal §→F1 collision |
| FA01 network key masks | WS48 fixes all32 widths; reference spotchecks and fuzz pass | Preserve original i386 initializer; final composed protocol/runtime checks |
| FA15 collision | WS52 rounded/position paths and WS61 sight/culling pass independent geometry fixtures | Deep real BSP/terrain and gameplay geometry coverage remains separate |
| FA02 mantle weapons | WS53 restores inactivity dispatch;28 transitions and92 timing cases pass, retained by WS74 | Final live mantle/fire/reload/change behavior |
| FA04 level state | WS54 fog/objective sentinels and WS55 script/config initialization ordering pass | Final listen/dedicated map startup/restart checks |
| FA06 reverb | WS50 reference wet-level and command cases pass | Verify actual audible consumer path; numeric output is not acoustic capture |
| FA07 sound command | WS50 restores argument/priority/value behavior and bounded validation | Final runtime environment-effect checks |
| FA03 view attachments | WS60 controlled tag/mounted-camera fixtures pass | Cross-check live turret aiming and real skeletal tags |
| FA09 HUD | WS71 draw fixtures and WS82 all-objective compass/location cases pass; independent review passed | Live objective rendering and resolution coverage remain |
| FA10 browser | WS67 display-name comparator cases pass both builds | Final live server-browser input/sorting |
| Numeric fields and FA16 fonts | WS70 locale asset-path and WS73 localized float-field fixtures pass | Glyph runtime checks with installed licensed locales |
| FA11 console hints | WS72 domain/default/latched draw fixtures pass; enum formatter overflow reproduced and bounded | Final console UI rendering; approved branding retained |
| FA12 water/buffer lifetime | WS75 actual water data/mips/sampler/coordinate fixtures pass both builds | Live rendered water comparison; original GPU lifetime remains unproved by native CPU buffer fixtures |
| FA13 script analysis | WS80/84 loop scope, booleans, parser stack and builtin operands restored; WS87 packed literals/local names pass byte oracles at all alignments | Final combined suite; other packed backpatch stores remain outside these bounded repairs |
| FA14 script failures | WS59 required script/entrypoint failures now checked and covered | Final live script loading and ordinary map controls |
| FA08 voice codec | WS89/WS91 retain original-codec comparisons and remaining PCM differences; WS110/112/119 record incoming playback, rate and relay repairs | Verify the composed voice contracts and retain the bounded historical runtime evidence; microphone capture and universal original PCM parity are not established |
| Third-person models/legs | WS88 matched before/after and stock/CoD2x movement captures show complete local bodies; independent review and extended skeleton/skinning fixtures pass | Identified remote-player visual proof and final composed builds remain |
| Footsteps | WS43 events and alias calls observed | Capture acoustic output/call-chain behavior across running/walking/surfaces; an emitted event alone does not prove audible sound |
| Weapons/damage/hit registration | WS44 did not establish a specific new defect | Deterministic recoil/spread/damage/hit-location/trace result cases and gameplay capture; retain seeds and compare full relevant state rather than coarse hash only |
| Metal (parked) | WS39 evidence preserved on its older branch | No merge or implementation under the current user scope |

## Scheduling

1. Finish WS45 full 0.2.1 gate and autonomous runtime checks. Record known fidelity failures separately from integration regressions.
2. Finish WS47 documented registry/grammar/profile corrections; do not mix them into WS45 baseline while it runs.
3. Run independent confirmed-fix units in isolated worktrees: protocol tables; collision; weapon/level state; audio; HUD/view/UI/script. Shared source files have one owner, and live-game/benchmark use is serialized with the benchmark lock.
4. Resolve plausible findings with characterization before implementation. Reconcile older reports against current source, including intentionally replaced platform behavior and active unmatched identities. Never turn all 1,386 unmatched names into implementation tasks without resolving identity and callers.
5. Integrate tested current-release fidelity and CoD2x units. Include the authorized Config Studio units in 0.2.1; keep Metal parked. Re-run behavior tests for changed subsystems and the complete merge gate on the composed commit. Review actual code separately from worker claims.
6. Keep Metal work parked. Finish the current renderer and gameplay acceptance. A failure is fixed or reported with exact evidence; it is never relabeled a pass to meet a version number.

## Regression contract

- Keep each discovered failure reproducible in a committed synthetic fixture; private original binaries/assets/corpus and generated captures stay local.
- Preserve stock and CoD2x coverage and original COD2_X64-off source. Changes affecting storage get typed layout and bounds tests; changes affecting behavior get state/output oracle checks.
- Verify runtime events, data state and visible/audible output where these are distinct. Avoid assertions against values derived from the function under test.
- Record unavailable prerequisites explicitly. Full original-i386 differential execution and Linux multilib binary equality require compatible execution environments; access to this Mac alone does not establish them.
- Full audit runs preserve unresolved identities, pointer-bearing table gaps, indirect-call uncertainty and unreviewed sample cases. No claim of universal perfection follows from finite tests.
