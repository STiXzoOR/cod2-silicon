# WS40 — native grenade collision and effects

Date: 2026-10-05. Worktree: `grenades`. Branch: `port/grenades`, based on
`88044c8` (`port/int-0.2`). Work performed by the autonomous Codex agent.

## Result and merge scope

Native frags and smoke grenades now bounce, stop on Toujane's ground, and
produce events 188 and 191 there. Smoke grows visibly between two and ten
seconds. Orange frag blasts, bullet impact particles, and wall decals were
inspected in screenshots on stock and CoD2x with OpenGL. Metal verification is
unavailable on the supplied base because it lacks a Metal backend; details below.

There were three reconstruction errors on the investigated path:

1. Grenade movement checked bit 16 instead of the spawned grenade's bit 24.
2. The FX parser treated RGB curves as scalar curves and ignored channel scale.
3. Ordinary projectile world impacts were skipped, and both splash calls
   exchanged inner damage with radius.

The functional changes are guarded by `COD2_X64`, retaining the original
expressions in the legacy path. Opt-in diagnostics additionally require Apple
arm64. No headers, top-level CMake, launcher, packaging, Metal implementation,
typed-data snapshot, or ABI approval lists changed. No sibling worktrees were
accessed. WS39's requested report and trace commits were read through this
repository's git objects, rather than its checkout.

Focused DCO-signed commits, in order:

| Commit | Change |
| --- | --- |
| `81e8498` | Server missile and client event receipts |
| `17cba8a` | Correct grenade bounce/landing flag and production ASan fixture |
| `d0783ed` | Self-contained WS39 FX receipts, without golden-pose dependencies |
| `cb2f319` | RGB curve dimensions and authored channel scales, with ASan fixture |
| `84f9f43` | Native-only stubs for the isolated platform FX call test |
| `100e489` | Ordinary projectile impacts and both splash damage calls |

Merge the whole branch: the bounce repair alone produces the smoke event but
leaves tiny black particles. `tests/lp64/game/run.py` gains two suite entries
and a copied production `g_utils_mp.c` include for the missile fixture. Those
are the likely merge conflict points. During this run `port/int-0.2` advanced
to `0b3fc0c` with only a CHANGELOG edit; this branch remains based on `88044c8`
and has not changed that file. Merge the commits normally to retain that update. The new tests belong to the existing
game suite, so no new suite or CONTRIBUTING edit was needed. Nothing was pushed.

## Collision root cause and reference evidence

`fire_grenade` already sets `s.eFlags = 0x1000000`, `TR_GRAVITY`, a valid
spawn base/time, owner zero, and clip mask `0x2802891`. In the broken live run,
the movement trace found world entity 1022, with a nonzero wall normal and
fraction below one. The code then skipped the bounce response, leaving the
original gravity trajectory intact. Subsequent evaluations advanced through
the contacted wall/floor and ultimately thousands of units below the map.
This disproves a missing broadphase candidate or a trace that never hits.

The Mac 1.3 reference's `G_RunMissile` and `G_BounceMissile` test byte 11 of
the entity for bit zero. With `eFlags` at byte eight, this is bit 24,
`0x1000000`, rather than `0x10000`. The native path now uses the named local
constant `GMISSILE_EF_BOUNCE` for support tracing, damping/rest, and collision
dispatch. The separate `0x10000` used to mark an entity's explosion effect
was preserved; it has a different purpose.

Reference input, used read-only:
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`.

```sh
binary="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
nm -a "$binary"
otool -arch i386 -tV -p __Z12fire_grenadeP9gentity_sPfS1_ii "$binary"
otool -arch i386 -tV -p __Z12G_RunMissileP9gentity_s "$binary"
otool -arch i386 -tV -p __Z15G_BounceMissileP9gentity_sP7trace_t "$binary"
otool -arch i386 -tV -p __Z16G_ExplodeMissileP9gentity_s "$binary"
```

Function addresses are `0x1c9796`, `0x1ca0e4`, and `0x1c9db0` for spawn,
run, and bounce respectively. The spawn flag store is at `0x1c983d`.
Disassembly was inspected locally and was not committed.

The extended projectile fixture exposed the reconstruction's outer collision
gate: a normal projectile hitting a non-damageable world surface did nothing.
At `0x1ca41a`–`0x1ca42d`, the reference instead chooses bounce only when the
hit is non-damageable **and** bit 24 is set; everything else follows impact
handling. Native dispatch now has that shape. The synthetic ordinary
projectile emits impact event 190 and does not emit grenade bounce event 187.

Both reference splash calls check inner damage and pass inner damage, outer
damage, then radius. STABS places those WeaponDef fields at `0x37c`, `0x380`,
and `0x378`. The timed explosion call is at `0x1c9710`, and the impact call
at `0x1cad2c`. The original reconstruction checked radius and passed radius,
outer damage, inner damage. Both native calls and their conditions now match
the reference. The fixture verifies distinct values `250`, `15`, and `100`,
plus suppression of a non-damaging grenade's splash call.

## Effect root cause

After the collision fix, event 191 and the smoke effect receipt arrived, but
the cloud remained tiny and dark. A temporary particle diagnostic found a
live `gfx_smokepuff_atlas` particle at age 3937 ms, life 38879 ms, radius
`0.471003`, size scale `1`, and RGB `(0,0,0)`. The owned template's scale and
RGB curve had not survived parsing. That temporary diagnostic was removed.

`PrimitiveTemplate_ParseChannelCurve` always allocated two floats per key,
parsed only time/value, and created a one-dimensional curve. RGB consumers
expect time plus three values. `PrimitiveTemplate_ParseChannel` also ignored
`scale` pairs. Native parsing now allocates and reads four floats for the two
RGB channels and stores the authored scale range in the typed channel. Scalar
curves retain two floats per key. No proprietary template text was copied
into either fixture.

The Mac reference `PrimitiveTemplate::ParseChannel`, at `0x5e8d0`, dispatches
RGB channels to `ParseChannelRgbCurve`, at `0x5e0a8`. That helper allocates
16 bytes per key, parses four floats, and creates dimension-three curves.
The scale branch parses one or two floats, duplicating a single value.

```sh
otool -arch i386 -tV -p __ZN17PrimitiveTemplate12ParseChannelEP24BackCompatibleParametersP7GPGroup11FxChannelId "$binary"
otool -arch i386 -tV -p __ZN17PrimitiveTemplate20ParseChannelRgbCurveEP7GPValue11FxChannelId "$binary"
```

The smoke-cloud screenshots confirm the parser repair changes the visible
result. It also restores authored color and scale for the shared explosion,
impact, and ambient effect paths. This broader FX reach should be checked during integration; no renderer-specific
code changed.

## Trace and live reproduction

All game logs, drivers, homes, and screenshots are outside git, under:

`~/Library/Application Support/CoD2-native-ws40/` (called `$E` below).

`COD2_MAC_COMBAT_TRACE=1` enables `[combat-missile]`, `[combat-event]`, and
`[combat-fx]` receipts. Missile receipts include frame/move/down/bounce/explode
phases, origin, trajectory type/base/delta/time, ground, owner, flags, mask,
fuse deadline, and trace fraction/startsolid/contents/hit/normal. Frame-only
receipts use `-1` sentinels for an absent trace. The environment lookup is
cached. Tracing is silent when the variable is absent.

Private drivers in `$E/drivers/` use stdin console input, wait for the client
to become active, query `sv_serverId` and `configstrings`, then send `cmd mr`
responses for the server-info, British allies, and `sten_mp` class menus.
Runs use `mp_toujane`, `setviewpos 2569 2274 181 180 25`, a fresh `fs_homepath`,
unique ports 29962–29992, `r_fullscreen 0`, 1280×720, `com_maxfps 125`, and
`timeout -k 5 180`. Every game launch checks `pgrep -fl cod2_macos` and waits
for an existing game. Only a driver's own process group is eligible for its
cleanup. The real shader cache was selected for every gameplay and menu run:

```sh
E="$HOME/Library/Application Support/CoD2-native-ws40"
export COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2 Silicon/shaders"
python3 "$E/drivers/grenades-final.py" build-macos/cod2_macos "$E/new-run" 29993 gl smoke
```

The supplied source and both engine compile databases contain the OpenGL shim
only: there is no registered `r_renderer` dvar, MTL backend, or CAMetalLayer.
`r_renderer metal` therefore has no backend-switching effect on this base. All
runs whose directories contain `metal` actually used OpenGL. Earlier progress updates
incorrectly called those Metal results; this report corrects that verification
error. The local FX commit's verification wording was also corrected before
handoff, without changing any committed source tree. No Metal result is claimed. The orchestrator
needs to merge the common gameplay/FX repair onto its Metal branch and repeat
the same private driver there with a verified backend selector. Adding or
importing Metal here would violate this workstream's instruction to leave it
untouched.

The corrected-clock run uses normal game timing, without `fixedtime` or
benchmark mode. God mode is used to keep the test camera alive near a frag.
`cg_drawGun 0` is applied after landing so the effect is visible. The accepted
camera stays at the original spot; early experiments that moved it into a
roof/post or toggled noclip incorrectly were rejected as visual evidence.

### Before

`before-stock-gl-2/console.log`, trace-only build:

- Smoke at server time 12150: origin `(2481.995,2274.164,98.263)`, type 5,
  base `(2568.995,2274.164,130.763)`, delta `(-870,0,-285)`, trTime 12000.
  Fraction `0.047452`, startsolid 0, contents `0x8000001`, hit 1022,
  normal `(0.493,-0.870,0)`. No smoke event 191 arrived.
- Frag at 22750: fraction `0.093165` against the same wall; no bounce.
  At 26100 it exploded at `(-409,2274,-5738)`, ground 1023. Client event
  188's interpolated origin was `(-372.912,2274.164,-5607.167)`.
- Inspected `home/main/screenshots/smoke-10.jpg` and `frag-9.jpg`: neither
  shows the required ground cloud/explosion.

The first diagnostic launch (`before-stock-gl`) crashed because the newly
added variadic `Com_Printf` call lacked its declaration. The declaration was
added before committing the trace. That launch is rejected evidence.

### After

In `final-stock-gl-frag`, the missile stops at time 13200, origin
`(2433.314,2203.073,71.132)`, type 0, delta zero, ground 1022. At 15850 it
explodes at snapped `(2433,2203,71)`. Event 188 arrives, and the FX receipt is
`explosions/grenadeexp_concrete`, nine primitives, at the landed position.

In `final-stock-gl-smoke`, the missile stops at 13200, origin
`(2431.970,2201.213,71.048)`, ground 1022. At 13350 it emits the smoke event;
client event 191 is at time 13302. The cloud is visibly larger at ten seconds
than at two seconds.

Inspected screenshot evidence, relative to `$E`:

| Build / requested selector (actual renderer: GL) | Frag blast | Smoke growth |
| --- | --- | --- |
| Stock / GL | `final-stock-gl-frag/home/main/screenshots/frag-23.jpg` | `final-stock-gl-smoke/home/main/screenshots/smoke-{2,10}.jpg` |
| Stock / ignored `metal` request | `final-stock-metal-frag-fast/home/main/screenshots/frag-69.jpg` | `final-stock-metal-smoke/home/main/screenshots/smoke-{2,10}.jpg` |
| CoD2x / GL | `final-codx-gl-frag-fast/home/main/screenshots/frag-69.jpg` | `final-codx-gl-smoke/home/main/screenshots/smoke-{2,10}.jpg` |
| CoD2x / ignored `metal` request | `final-codx-metal-frag/home/main/screenshots/frag-23.jpg` | `final-codx-metal-smoke/home/main/screenshots/smoke-{2,10}.jpg` |

After the final shared-projectile repair, fresh `post-splash-stock-gl-frag`
and `post-splash-codx-metal-frag` runs again exited zero with grounded event
188 and visible blasts (`frag-73.jpg` in both screenshot folders).

Fresh `post-splash-stock-gl-smoke` and `post-splash-codx-metal-smoke` runs
also exited zero with grounded event 191 and an inspected dense cloud in
`smoke-10.jpg`.

The frag image sequences also show the small grenade at rest before the blast.
The shorter 0.1-second capture interval sometimes missed the brief bright
blast, so two runs were repeated at a 0.03-second interval. These are visual
checks, not performance measurements.

Wall runs `final-stock-gl-wall` and `final-codx-metal-wall` use yaw zero.
Their first hit has wall normal `(-0.866,-0.500,0)` and contents `0x8000001`.
Velocity reflects from `(940,0,120)` to `(-133.310,-230.900,9.757)`, then
reflects from a second wall and lands at approximately `(2612,2270,72.64)`.
Both emit grounded event 188. Fresh `final-stock-gl-bullets` and
`final-codx-metal-bullets` runs show concrete/wood impact receipts, gray impact
puffs, and black wall decals in `bullet-3.jpg` and `decals.jpg`; ammunition
falls from 32 to 11.

## Other projectiles and fuse timing

Read-only inspection of the licensed MP weapon definitions found 42 unique
definitions: 31 bullet, eight grenade, two projectile, and one binoculars.
There are no stock MP rifle grenades. The two projectile definitions are
`panzerfaust_mp` and `panzerschreck_mp`; both are rockets. Their shared ordinary
projectile world-impact behavior is now covered by the production fixture.
This is not a claim of a live rocket pickup test on Toujane.

All eight stock grenade definitions set `cookOffHold=0`. Frags have a
3.5-second fuse and smoke a one-second fuse. Holding `+frag` for 2.5 seconds
in `final-stock-gl-cooked` and `final-codx-metal-cooked` still produces the
authored 3500 ms spawn-to-explosion interval, with grounded event 188. The
production fixture separately supplies `grenadeTimeLeft=500`, verifies it
is consumed, and verifies no explosion at 450 ms and an explosion at 500 ms.
Thus the shortened-fuse path is exercised even though the stock definitions
do not enable holding to cook a grenade.

## Regression and gate evidence

The ASan `grenades` fixture links real missile, trajectory, math, and native
origin/angle functions against synthetic floor/wall and lifecycle services.
It tests spawn, gravity, damping/rest, floor/wall bounce, startsolid response,
frag/smoke events, shortened fuse, splash argument order, and an ordinary
projectile impact. The synthetic `fx_channels` fixture links the real template
parser and curve allocator and tests both RGB channels, scalar curves, and
single/range scales.

```sh
# Expected failures, proving the regressions precede their fixes:
python3 tests/lp64/game/run.py --build build-macos --baseline 81e8498 --test grenades
python3 tests/lp64/game/run.py --build build-macos-codx --baseline 17cba8a --test fx_channels
# Passing production fixtures, with Release assertions enabled per CONTRIBUTING:
python3 tests/lp64/game/run.py --build build-macos
python3 tests/lp64/game/run.py --build build-macos-codx
```

The pre-bounce fixture fails its stationary assertion; the pre-parser fixture
fails its dimension-three assertion. The splash extension failed its distinct
argument assertion before the repair, then exposed and caught the missing
ordinary projectile impact. Both full native game suites pass after repair.

Gate logs are local, ignored files under `output/ws40/`. Builds and fixture
compilation use `taskpolicy -b nice -n 19`. Both Release engine targets compile.
The real engine compile databases remain in `compile_commands.engine.json`;
only fixture copies use `-UNDEBUG`.

| Gate | Result / evidence |
| --- | --- |
| Stock and CoD2x Release builds | Pass, `build-{stock,codx}-final.log` |
| Tools, datagen, ABI checker unit tests, shader setup, Wine trace parser | Pass, `gate-units-00`–`06.log` |
| CoD2x full sanitizer and native protocol suites, SDK identity | Pass, `gate-units-07`–`09.log` |
| Online stock/CoD2x, fixes13 stock/CoD2x, perf fixtures | Pass, `gate-fixtures-00`–`03.log`, `gate-units-10.log` |
| Game stock/CoD2x, script, UI stock/CoD2x, renderer fixtures | Pass, `game-final-*.log`, `gate-fixtures-06`–`09.log` |
| Shader raster, texture mips, volume upload | Pass, `gate-visual-00`–`02.log` |
| HUD matrices, impact marks, FX events/primitives/cloud, dedicated FX | Pass, `gate-fixtures-10`–`15.log`; affected FX checks repeated in `fx-*-final.log` |
| Platform sanitizer / plain CTest | 28/28 and 30/30 pass, `gate-visual-05` / `08.log` |
| Typed-data roundtrip, layout checks, private fixes13 reference | Pass, `gate-units-11` / `12.log`, `gate-fixtures-16.log` |
| Menu sweeps stock / CoD2x | Both pass: 59 main, 73 in-game menus, 46 scripts; `$E/menu-*-final` |
| Parity recording | 100 nonempty-world frames, `$E/parity-final`; private windowed `timeout -k` wrapper, not a parity comparison |
| `tests/fixes13/legacy.py --base port/int-0.2` | 4 production source files, 10 configurations, 0 mismatches |
| `tools/abi/legacy.py --base port/int-0.2` | 5 source/test files, 5 COD2_X64-off configurations, 0 mismatches |
| `unifdef -UCOD2_X64` followed by `cmp` against `port/int-0.2` | All four changed production sources and the changed platform fixture byte-identical |
| `git diff --check` | Pass |
| Full ABI checks, stock / CoD2x engine databases | Both pass: 622 / 639 TUs, 0 errors/mismatches/new; 218 renderer bindings each at 0; logs `abi-final-*.log` |
| Raw offsets stock / CoD2x | 390 / 403 sources; 1196 / 1202 reviewed sites; 0 unreviewed/unsafe |

The full import audits each inspect 514 reference imports and 185 placeholders.
Stock has 1777 active native sites; CoD2x has 1779. Both report zero proven
extra or missing dereferences. The function audits were refreshed after the
last projectile repair using the tool's content-addressed cache, again with
zero errors/mismatches/new (`audit-post-splash-{stock,codx}.log`). No baseline
or approval file changed.

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws40/abi-final-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws40/abi-final-codx
python3 tools/abi/raw_offsets.py build-macos/compile_commands.engine.json --output output/ws40/raw-final-stock.json
python3 tools/abi/raw_offsets.py build-macos-codx/compile_commands.engine.json --output output/ws40/raw-final-codx.json
python3 tests/fixes13/legacy.py --base port/int-0.2
python3 tools/abi/legacy.py --base port/int-0.2
```

The first platform build found the FX receipt hook's new console/helper
dependencies absent from its isolated fixture. Native-only stubs corrected
that; both builds and complete CTest runs above are the successful reruns.

Two supplemental limitations are explicit:

- `tools/ci/compare-x86.sh 410342a HEAD ...` exits 2: it requires x86_64 Linux
  and multilib. This Mac cannot prove i386 object/binary identity. The exact
  preprocessor comparison and both required legacy gates pass; Linux artifact
  comparison remains the orchestrator/CI's responsibility. No packages were installed.
- `tests/cod2x/check_inactive_gates.py --base port/int-0.2` reports mismatches
  because this is intentionally a stock-native repair. Its `COD2_CODX=0`
  native comparisons require every change to disappear, and it also defines
  `COD2_X64=0` in configurations where CMake OFF must omit the definition.
  It reports 17 mismatches across five modified sources/tests and 17
  configurations. This is not a passing CoD2x-only-change gate. The proper COD2_X64-off gates
  above have zero mismatches. The tool and baselines were not weakened.

## Remaining observations

After the visible near-camera frag blast, the world view becomes very dark
while HUD and input remain active. Examples: `final-stock-gl-frag/frag-24.jpg`
and `final-stock-metal-frag-fast/frag-70.jpg` under their screenshot folders.
This also occurred before the FX parser repair, and appears in both stock and CoD2x OpenGL runs.
It is not evidence of a grenade below the map: the event and effect origins
are at the verified resting position, and the prior frame shows the blast.
The cause of this post-blast rendering/feedback behavior is not established
here; it needs separate investigation during integration. No renderer or
shellshock change was made on an assumption. Metal remains untested on this
base, which implements only OpenGL.

The port still has the inherited sparse Toujane visuals and existing build
warnings about duplicate libc++, the installed SDL compatibility library's
newer deployment target, and common-section alignment. This report makes no
retail visual-parity, live rocket, or performance claim.
