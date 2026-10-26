# F2 client input: held stance and jump commands

2026-10-26; base `6737ada`; `src/PC/client_mp/cl_input.c`.

Two native command-construction omissions are repaired. Holding `+movedown`
now requests crouch, holding `+prone` takes priority over it, and releasing
those controls restores the selected persistent stance. Holding `+stance`
through `cl_stanceHoldTime` now changes to prone (or standing when started
prone). Raising a crouched/prone player's stance with `+moveup` no longer adds
the flying-mode jump bit to normal player commands.

## Original behavior and reachability

The authority is the owned Mac MP 1.3 i386 binary. Ghidra instruction bytes were
checked directly against its Mach-O segments: all 2,884 exported instructions
for the module's 85 functions matched. That byte check validates the evidence;
it is not a claim that every function has been reconciled.

- `CL_KeyMove` at `0x18996e`: `0x18997d–0x1899b9` tests held prone before
  held crouch, clears the opposite stance bit, and sets `0x2000` for these
  temporary overrides. `0x189c8a–0x189ce9` handles the persistent stance and
  clears `0x2000`; `0x189de1–0x189e22` compares elapsed frame time against
  `cl_stanceHoldTime`, updates the stance, and consumes the held-state timer.
  The starting-prone branch at `0x189ece` chooses standing. The prior native
  body only ORed persistent crouch/prone bits into the command.
- `CL_CmdButtons` at `0x188ee2`: `0x189048–0x189064` subtracts two from
  `pm_type`, compares against two, and uses unsigned `JA` to skip the extra
  move-up button outside modes 2–4. The native signed comparison also accepted
  ordinary modes 0 and 1. The repair casts before subtraction so the range
  calculation itself has defined unsigned arithmetic.

These are ordinary bindings registered by `CL_InitInput`. The real handlers
`IN_DownDown`, `IN_Prone_Down`, `IN_Stance_Down`, `IN_Stance_Up`, and
`IN_UpDown` already produce the necessary state. The latter raises a persistent
stance before it activates the regular jump button; the erroneous range check
bypassed that distinction. `PM_CheckDuck` consumes the requested stance and
uses `0x2000` to suppress forced-stance events when a temporary stance is
blocked. Its behavior is unchanged.

All stance handlers, their key-down/up helpers, and the command-construction
path were compared with the original instructions, including timer/release
interactions and the existing ADS/binoculars condition. Native mouse finite
value guards and the intentional CoD2x MG sensitivity remain unchanged.

## Verification

`tests/input/run_stance.py` inherits the real client compilation flags, enables
assertions and ASan/UBSan, and extracts complete active production bodies.
It exercises actual input handlers, `CL_CmdButtons`, `CL_KeyMove`,
`CL_CreateCmd`, mouse/angle helpers, `ClampChar`, and `PM_IsBinocularsADS`.
Synthetic boundaries provide command arguments and engine globals. The runner
also compiles the entire changed translation unit with those sanitizer flags.

Both stock and CoD2x pass nine groups:

- Held crouch/prone priority, release restoration, and temporary-state tracking.
- Before/at the hold threshold, all three initial stances, and configurable
  zero/1,000 ms thresholds.
- Timer deferral while a temporary override is active, including ignored release.
- Short stance taps and existing stance-control handlers.
- Press/release within one frame and clearing incompatible command bits.
- Normal stance-raising without jump and the exact modes 2–4 range.
- Dual-key movement, fractional timing, ADS/binoculars, UI/shellshock flags,
  command timestamp cap, weapon indices, and angle encoding.

Against unchanged `--baseline 6737ada`, the same fixture has six failing groups
(40 assertions) and three passing control groups in each build. These are
behavioral failures, not compiler or extraction errors. The existing
`tests/cod2x/run_mouse_sensitivity.py` suite also passes in both builds.

```sh
# Use the matching stock or CoD2x engine database; run serially.
taskpolicy -b nice -n 19 python3 tests/input/run_stance.py \
  --compile-db <build>/compile_commands.engine.json --output <private-output>
taskpolicy -b nice -n 19 python3 tests/input/run_stance.py \
  --compile-db <build>/compile_commands.engine.json --output <private-baseline> \
  --baseline 6737ada
taskpolicy -b nice -n 19 python3 tests/cod2x/run_mouse_sensitivity.py \
  --compile-db <build>/compile_commands.engine.json --output <private-mouse>
python3 tests/fixes13/legacy.py --base 6737ada
python3 tools/abi/legacy.py --base 6737ada
```

`unifdef -UCOD2_X64 -U__aarch64__ -U__arm64__` produces byte-identical old/new
source. The two legacy tools report ten and five inactive configurations with
zero mismatches. The ABI legacy invocation used an external compile database
and matched zero SDK entries; it does not claim SDK preprocessing coverage.
`git diff --check` passes. Evidence is private under
`~/Library/Application Support/CoD2-native-decomp/F2-client-input.noindex/`.

## Remaining scope

This is a bounded stance repair, not full client-input parity. A further
comparison found `CL_SendCmdInternal` testing demo waiting where the original
reads `cls.downloadName[0]` for its 50 ms download throttle (`0x18a072`,
STABS offset `0x2b4838`). Its LAN test also uses `serverAddress` instead of the
original `netchan.remoteAddress`. Those network-scheduling paths are unchanged
and need their own producer/behavior tests. The inline key-time calculation
also differs for an active key with zero downtime and previously accumulated
milliseconds; normal timestamped input was covered, but that unusual mixed
console/timestamp state was not repaired here. Packet/voice serialization and
registration/shutdown were not fully reconciled in this unit.

No game, VM, physical-input run, full linked build, network replay, or Linux
x86 artifact comparison ran in this unit. Live stance acceptance, full
integration gates, reviews and publication belong to the coordinating task.
No game-derived material or reference implementation was copied into Git.

## Integration and review

Full stock and CoD2x client/dedicated builds pass. A private live Toujane test
passes eight transitions in each build: standing, held crouch, held prone,
release to crouch, release to standing, long-stance prone, raise to crouch,
and raise to standing. Draw origins show the expected 20-unit stand/crouch
and 29-unit crouch/prone differences with unchanged horizontal position.
Captures were inspected and both clients exit zero. The first attempt at the
ordinary spawn displayed “Prone Blocked” in a narrow corridor and was retained
as failed full-stance coverage; the accepted runs use an open courtyard.
Console commands exercise real registered input handlers, not physical SDL
keyboard delivery. Evidence is `live-open-stock`, `live-open-codx`, and
`live-open-results.json` under the unit evidence root. VM running state was
restored afterward, with no guest commands or authentication change.

All five simplify lenses completed without code changes. Correctness and
independent Claude Opus 5.5 review completed with zero actionable findings:
`/tmp/compound-engineering-501/ce-code-review/20261026-095224-68f09adc/`.
The root added matching stock/CoD2x CI commands using the verified runner flags.
No review fix remains.
