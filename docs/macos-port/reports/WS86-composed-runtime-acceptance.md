# WS86: composed runtime acceptance

Checked on 2026-10-07, from `codex/readiness-composed` at
`8f17eafa2b24ad1f2d2a61d1e494831167c14c33`. The completed stock and CoD2x
client/dedicated binaries were frozen throughout. This unit changes no engine
code and runs no engine rebuild. Private test injectors and passive window-state
utilities are the harness's existing compilation steps.

Both modes pass the current complete SDL input suite, menu sweep, bounded map
smoke and local movement/gameplay characterization. Full third-person torso/legs
remain unproved: direct engine images still show fragmentary geometry. The
canonical focus screenshot command remains failed; separately recorded state
checks pass. These results do not establish original-engine rendering equality,
acoustic fidelity, external-server interoperability, or readiness of hosted
CoD2x revision 6. WS81's StopGameType callback is not integrated into this head
and receives no acceptance claim here.

## Frozen artifacts and evidence

Worktree: `/Users/stix/.codex/worktrees/config-catalog/cod2-native`.

Private runtime evidence (`E`):
`/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex`.

Private input evidence (`I`):
`/Users/stix/Library/Application Support/CoD2-native-ws42/WS86-composed-8f17eaf.noindex`.

The installed `/Users/stix/Games/CoD2` remains the read-only data source. Every
run has its own private writable home. No user keys/profiles, TCC permissions,
global settings, future Config or Metal work, publication, or installed game data
were changed. The runs held the shared `_agents/bench.lock` and proceeded
sequentially, without competing game or benchmark processes.

| Frozen relative binary path | SHA-256 |
| --- | --- |
| `build-macos/cod2_macos` | `2aa108a293afed7c91d34cf3a1dd181828a875263c6995383fbe58afcd194091` |
| `build-macos/cod2_macos_ded` | `af249995b92ff4e4cb278a4fa627e633aea3430532579698d3959b150bc00e23` |
| `build-macos-codx/cod2_macos` | `127c0551bc28a8c0048f558fa9e776ee7e192b711e56e3dd224a32b6f5a81cad` |
| `build-macos-codx/cod2_macos_ded` | `f70d35b56de1cda2b48adfa8f9a5f8ea86b5c993ddd3b4be0149369a5fb914e0` |

`E/frozen-manifest.json` records these artifacts and current harness/compile
database hashes. `E/final-state.json` independently confirms all four hashes
unchanged, HEAD unchanged, `pgrep -fl cod2_macos` exit 1 with empty output, and
the shared lock absent after cleanup. The runtime slot was released to the
parent before report preparation.

`E/all-command-results.json` preserves every underlying command, working
directory, elapsed time, exit code and log path. The per-phase receipts remain
`live-results.json`, `input-runtime-results.json`, and
`extra-runtime-results.json`. Times below describe bounded run lifetimes and
are not performance measurements.

The private runner is copied from
`CoD2-native-current-readiness/validation.noindex/run_gates.py`. Adaptations
point to this checkout and new evidence, remove the dedicated rebuild, pair
each client with its own mode's dedicated binary, and run the smoke in both
modes. Extra phases call narrowly adapted existing WS45 private helpers.
`E/helper-provenance.json` records those copies. Production input/menu/parity
assertions were not weakened.

## Current command results

| Check | Stock | CoD2x |
| --- | --- | --- |
| Complete windowed SDL input, own dedicated server | PASS, 87 checks; command 106.118 s, client 104.532 s | PASS, 87 checks; command 107.176 s, client 105.402 s |
| Menus and safe UI scripts | PASS, 59 front-end + 73 in-game menus, 46 safe scripts; 49.266 s | PASS, same coverage; 49.923 s |
| Toujane bounded deterministic smoke | PASS, 100 nonempty-world frames + final entity dump; 8.133 s | PASS, same coverage; 7.796 s |
| Bare fullscreen SDL browser | PASS, 8 checks, 30.502 s command / 29.345 s client | Not separately selected; activated copy below covers this mode |
| Explicit own-app fullscreen activation, unchanged browser assertions | PASS, 8 checks; 30.631 s | PASS, 8 checks; 31.706 s |
| Private borderless focus state | PASS, 3 Finder cycles + Mission Control and desktop restoration; 10.820 s | PASS, same assertions; 14.408 s |
| Private exclusive focus state | PASS, 3 Finder cycles + Mission Control, actual 1920x1080 selected mode and desktop restoration; 14.621 s | PASS, same assertions; 14.631 s |
| Local spawned movement/bot/render characterization | PASS runtime assertions and quit 0; 33.763 s; visual torso/legs remain unresolved | PASS runtime assertions and quit 0; 34.175 s; same visual limit |

The two complete input runs include real localhost UDP getinfo/status and both
Join-button and double-click connections reaching client state 8 (`CA_ACTIVE`).
They also start the selected listen `mp_burgundy`, change Options and bindings,
exercise password text/cancel/confirm, and quit with status 0. Browser filler
identities are simulated; the upstream dedicated servers are real and match
their clients' stock/CoD2x build modes. A single installed language is an
explicit retained-value skip, not changed-multi coverage. Input screenshots
remain under `I/input-live-{stock,codx}/home/main/screenshots`.

The bare fullscreen run records injected SDL `[10,-6]` and UI `[10,-6]`, render
`[30,-13.5]`, Cocoa content points `[47,-21.15]`, and gains `[4.7,3.525]` at
1920x1080 rendering / 3008x1692 window points / backing scale 2. Preferences
remain raw mouse enabled and `com_maxfps 333`. This verifies injected event and
coordinate delivery, not physical trackpad feel or HID polling.

`E/quiet-default.json` records zero movement/view/mantle records, zero lines
matching the current scanner's diagnostic tags, zero native trace files and
zero fatal records in both default complete-input logs. Test-only opt-in
injector telemetry is excluded. Menu sweeps intentionally opt into tracing;
their six existing destructive/external skips remain intact.

Direct review of the current stock Join Server and CoD2x Filter engine JPEGs
shows Connection, Mods, Friendly Fire, Killcam and PunkBuster labels present.
The review concerns those selected images, not all 264 menu captures or
comparison with retail rendering.

## Retained failures and baseline distinction

The initial three input commands pointed at `E/input.noindex`; all exited 2
before game launch with the unchanged harness message:

```text
run.py: error: Evidence must be under /Users/stix/Library/Application Support/CoD2-native-ws42
```

Those logs remain `E/input-live-stock.log`, `input-live-codx.log` and
`input-fullscreen.log`. Fresh retries under `I` pass, with separately named
`input-live-permitted-*` logs/receipts. This is a runner setup correction and
does not turn the initial commands into passes.

The current canonical stock borderless focus command exits 1 before engine
launch at the desktop screenshot:

```text
could not create image from display
subprocess.CalledProcessError: Command '['screencapture', '-x', '-C', '/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex/focus-canonical-borderless-stock/desktop.png']' returned non-zero exit status 1.
```

`E/focus-canonical-borderless-stock.log` retains the complete failure. The
private state-only derivative omits this OS screenshot and changes its ROOT to
the composed checkout, retaining the original active-window, Finder/Mission
Control, display-mode, quit and desktop-restoration assertions. Its four
commands pass; those are state proofs, not successful canonical screenshots.
No permissions were changed.

WS45's earlier bare fullscreen failure (`Fullscreen menu did not retain SDL
relative input`) is historical baseline evidence and does not reproduce in
this session's current bare stock run. No production focus/input fix or general
elimination of activation prerequisites is inferred from this one success.

The minimal private app identities resign copies for LaunchServices activation.
Their whole-file hashes therefore differ from the frozen source executables:
stock `1ad02e90202091dbba6870882bc2ea87e503dea5ab1dbe0dc0eaa800c3e5bd09` and
CoD2x `8259918b262b44d4c924a827da118f175d34e3a2291d219d0b5483e068b168f3`.
`E/private-app-binaries.json` records exact paths and hashes.
`E/private-app-section-comparison.json` finds all 21 stock and 30 CoD2x Mach-O
sections equal in content, size and flags. This does not claim signed-file byte
identity. Both private identities were unregistered after their focus tests.

## Rendered gameplay and model handoff

Both private probes spawn the local British player with a Sten and enable two
test clients through the existing private `ws43qa` dm/team override. The copied
override comes from
`/Users/stix/Library/Application Support/CoD2-native-ws43/live-mantle-fixed/home/ws43qa`;
it is never installed in the licensed source or committed. `status`/telemetry
contains bot0 and bot1. The local player runs forward/right, then traverses
three camera positions and four orbit angles per position, fires, reloads and
leans. These gameplay commands use the existing console command path; the
separate menu suite provides real injected SDL input acceptance.

| Opt-in diagnostic observation | Stock | CoD2x |
| --- | --- | --- |
| Movement records | 1765 | 1763 |
| View records | 1025 | 1040 |
| Footstep event lines | 5 | 5 |
| Mantle records | 0, no mantle attempted | 0, no mantle attempted |
| Fatal diagnostic | false | false |
| Bot0/bot1 present | true/true | true/true |
| Quit status | 0 | 0 |

These counts establish consumed runtime paths, not acoustic quality, complete
animations, candidate hosted-6 gameplay, or remote snapshot parity. No acoustic
recording or identified remote/bot full-body visual acceptance was performed.

Directly reviewed clear outdoor third-person captures are:

- Stock: `/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex/movement-stock/qa.noindex/movement-trace-spawned/home/ws43qa/screenshots/ws86-box-approach-0.jpg`.
- CoD2x: `/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex/movement-codx/qa.noindex/movement-trace-spawned/home/ws43qa/screenshots/ws86-box-approach-0.jpg`.

They show fragmentary helmet/weapon geometry near the lower edge/ground, without
a complete local player's torso/legs. Stock box-approach 180 and ladder-court 90
were also reviewed. Twelve orbit/position images per mode are available, plus
the earlier wall-adjacent-style shot. The clear outdoor view reduces the prior
single wall-occlusion ambiguity, but does not isolate camera placement,
XModel/DObj loading, skeletal evaluation or draw submission as the cause.

The distinct first-person local viewmodel is visible after firing/reload:

- Stock: `/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex/movement-stock/qa.noindex/movement-trace-spawned/home/ws43qa/screenshots/ws86-firstperson-after-reload.jpg`.
- CoD2x: `/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws86-composed-8f17eaf.noindex/movement-codx/qa.noindex/movement-trace-spawned/home/ws43qa/screenshots/ws86-firstperson-after-reload.jpg`.

Both display local hands/weapon and ammo 32/188, following 32/192 in the earlier
third-person capture. This is not evidence that the local third-person body or
remote/bot whole-body models render. Bots were not spatially identified in the
reviewed frames, and the failure cannot be assigned to their models.

`E/visual-review.json` records selected image hashes and review scope. No engine
JPEG, extracted GSC, proprietary logs, or game-derived payload enters git.

### Minimal reproduction for the next private diagnosis

Use a fresh private home containing the two existing `ws43qa` override files
and the appropriate frozen executable. The exact launch vector is in each
`E/movement-{stock,codx}/qa.noindex/movement-trace-spawned/results.json`:
windowed 640x480, 60 fps, developer 1, non-pure dm, four clients,
`scr_testclients 0`, `fs_game ws43qa`, read-only licensed base, and
`devmap mp_toujane`. Stock port is 29966; CoD2x is 29970. The private helper
source is `E/movement-{stock,codx}/movement_trace_smoke.py`; its output directory
must be made new before any rerun, and the display slot must be coordinated.

After `CS_ACTIVE`, query `sv_serverId; configstrings`. Resolve the current
`serverinfo_dm`, British team and `weapon_british` configstring indexes, subtract
`0x4de`, and send the ordinary menu responses using the actual server id:

```text
cmd mr <server-id> <serverinfo-menu-index> close
cmd mr <server-id> <british-team-menu-index> allies
cmd mr <server-id> <weapon-british-menu-index> sten_mp
```

The captured helper waits 0.7 seconds per response, then 2 seconds to spawn.
Enable the bots, wait 6 seconds, and query status:

```text
god
set scr_testclients 2
setviewpos 1363 609 89 135 0
status
cg_thirdPerson 1
cg_dumpAnims 0
cg_showmiss 1
+forward
```

After 3 seconds send `-forward; +moveright`, wait 3 seconds, then
`-moveright`. The clear outdoor camera can then be reproduced independently of
the prior movement endpoints:

```text
cg_dumpAnims -1
cg_showmiss 0
cg_thirdPerson 1
cg_thirdPersonRange 100
setviewpos 2286 2110 140 90 0
```

Wait 1 second, send `cg_thirdPersonAngle 0; screenshotJPEG <new-private-label>`,
and allow at least 0.3 seconds. Other captured coordinates are
`1363 609 89 135 0` and `850 900 70 270 0`, each with angles 0/90/180/270.
For the captured first-person endpoint, use `setviewpos 850 900 70 270 0`,
return to `cg_thirdPerson 0`, hold
`+attack` for 0.5 seconds, send `-attack; +reload`, wait 2 seconds, then
`-reload; +leanleft`, wait 0.6 seconds, release lean and request a new engine
JPEG. End with `quit` and verify status 0 and an empty game process inventory.

The partial third-person geometry was already documented in WS43 and WS45;
this unit reproduces the unresolved appearance in both current modes. No retail
runtime comparison or new-versus-old regression conclusion is claimed.
