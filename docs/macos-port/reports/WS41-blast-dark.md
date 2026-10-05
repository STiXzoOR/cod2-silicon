# WS41 — grenade screen feedback on native OpenGL

Date: 2026-10-05. Worktree: `blast-dark`. Branch: `port/blast-dark`, based on
`cbae9ed` (the supplied `port/int-0.2` release candidate). Work performed by
Codex, with no delegated agents or sibling checkout access.

## Result and merge scope

The near-frag blackout is caused by a missing **backbuffer-to-texture copy** in
`CDirect3DDevice_StretchRect`, not an immortal explosion particle or light.
Native OpenGL now captures the scene pixels used by grenade screen feedback.
Both stock and CoD2x show a visible world during the feedback and recover after
its four-second duration. Distant blasts, smoke and sustained Sten fire are
checked separately below.

Focused DCO-signed commits:

- `15093af`: restore the native backbuffer copy and add a production-function,
  synthetic CGL pixel regression to the existing shader raster suite.
- `244c4b7`: keep shellshock warning filenames stable and add a production ASan
  fixture to the existing game suite.

No shared headers, top-level CMake, launcher, packaging, Metal, generated typed
snapshot, ABI baseline or offset approval files changed. Nothing was pushed.
The likely merge touchpoints are `CDirect3DDevice_StretchRect`,
`tests/lp64/game/run.py` and `tests/lp64/renderer/run_shader_raster.sh`.
The final report is committed separately.

## Root cause and reference evidence

The initial stock run reproduced mean luminance **51.18 → 67.47 → 6.19**.
The HUD remained readable. Unlike the shorter WS40 captures, the longer run
also caught recovery at eight seconds after grenade release. The blackout is
therefore sustained feedback during the active effect, rather than permanent
world-light corruption in this reproduction.

Turning off `fx_enable` removed the visible blast but retained the blackout
(mean 6.02). Setting `r_dlightLimit 0` also retained it (approximately 6.01).
These are server grenade explosions with the client particle/light path
disabled, which narrows the fault to another client response.

Temporary instrumentation of the production screen-blend function recorded:

- start 16550, duration **4000 ms**;
- screen blend fade **1000 ms**, effect time **400 ms**;
- saved-screen state becomes active on the next frame.

The instrumentation was removed before the commits. The console
`cg_shellshock 2` experiment described in the task uses parameter slot zero;
`CG_DrawActiveFrame` instead selects the server snapshot's nonzero slot for
the grenade. The last loaded dvars can describe a different effect. Thus a
short console test does not exercise this grenade's sustained feedback.
The four-second gameplay effect itself is retained.

`RB_SaveScreenCmd` asks `StretchRect` to copy the backbuffer into render target
7, `R_RENDERTARGET_SAVED_SCREEN`. The reconstructed shim instead ignored
`pDestSurface` and tried to draw the source as a texture. A backbuffer has no
texture owner, so `CDirect3DSurface_GetGLBlitInfo` returns false and the function
returns without copying. `RB_BlendSavedScreenCmd` then samples an unwritten
feedback texture over the 3D view. The HUD is drawn afterward.

The read-only Mac 1.3 reference verifies the missing operation:

- `CDirect3DDevice::StretchRect`, at `0x17c74`, distinguishes its backbuffer
  source at `0x17c86`–`0x17c8b`.
- Its same-size copy branch starts at `0x18c77`, saves the active texture and
  binding, binds the destination, calls **`glCopyTexSubImage2D` at `0x18d32`**,
  and restores binding and active texture at `0x18d6d` and `0x18d78`.
- STABS identifies the tested i386 member at `0x1c` as `mBackBuffer`. The native
  implementation uses the existing typed `DeviceImpl.backBuffer` member.

```sh
binary="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
otool -arch i386 -tV -p __ZN15CDirect3DDevice11StretchRectEP17IDirect3DSurface9PK7tagRECTS1_S4_21_D3DTEXTUREFILTERTYPE "$binary"
otool -arch i386 -tV -p __Z22RB_BlendSavedScreenCmdP25GfxRenderCommandExecState "$binary"
```

The fix implements the full-surface, same-size backbuffer copy used here,
behind `COD2_X64`, and restores GL active-texture/binding state. The existing
quad path remains available for other calls. This is not a reconstruction of
all scaled/rectangular/offscreen blits or a Metal change.

The synthetic `saved_screen.c` fixture calls the actual production
`CDirect3DDevice_StretchRect` in a private CGL framebuffer. Two differently
colored halves prove copied content and orientation; assertions also prove
active texture, texture binding, blend, depth-test and depth-write state are
unchanged. It fails its first pixel assertion before the fix and passes with
ASan/UBSan afterward:

```sh
sh tests/lp64/renderer/run_shader_raster.sh
```

No game pixels, shaders or assets are used in the fixture.

## Shellshock warning: verified table, stable filename

The proposed missing table entry is contradicted by the reference. Reading
`_cg_shock_dvar_names` at `0x34d680` gives exactly the current native **29**
entries, with `cg_shock_sound` at index 4 and `cg_shock_mouse_fadeTime` at 28.
Both Mac save/load callers pass `0x1d` (29). They omit
`cg_shock_viewKickFadeTime`. `CG_SetShellShockParmsFromDvars` stores immediate
`0xbb8` (3000 ms) at `0x1d6f96`. The native table/count and three-second value
are consequently preserved; no value or extra reference-table entry was
invented. The licensed shock files contain the extra name, so the benign
unknown-name warning remains.

The garbage filename was real: the loader retained the pointer returned by
`va`, whose two thread-local buffers are reused during file services and dvar
parsing. The native loader now copies that path before either can reuse it.
Live warnings correctly name `shock/default.shock` and
`shock/hold_breath.shock`, instead of `0.05` or `0`.

```sh
otool -arch i386 -tV -p __Z22CG_LoadShellShockDvarsPKc "$binary"
otool -arch i386 -tV -p __Z22CG_SaveShellShockDvarsPKc "$binary"
otool -arch i386 -tV -p __Z30CG_SetShellShockParmsFromDvarsP18shellshock_parms_t "$binary"
python3 tests/lp64/game/run.py --build build-macos --baseline 15093af --test shellshock_file
python3 tests/lp64/game/run.py --build build-macos --test shellshock_file
python3 tests/lp64/game/run.py --build build-macos-codx --test shellshock_file
```

The baseline command intentionally fails the filename assertion after the
synthetic services overwrite `va` storage. Both repaired fixtures pass and
assert the verified table identity/count. Reference reads and disassembly
were not committed.

## Live evidence

Private drivers, logs, configs, binaries and screenshots are under
`~/Library/Application Support/CoD2-native-ws41/` (`$E` below), with a before-copy
control executable at `/tmp/ws41-before/cod2_macos`. That control substitutes
only the original `cbae9ed` device object into the current stock link. The first
near/FX/light controls were run from the actual pre-fix engine build.

Every launch checks `pgrep -fl cod2_macos` and waits if a game is running. Runs
are windowed, use distinct private homes/ports 30041–30085 and
`timeout -k 5 180`; only the launched process group can be cleaned up. The real
owned shader cache is selected:

```sh
E="$HOME/Library/Application Support/CoD2-native-ws41"
export COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2 Silicon/shaders"
python3 "$E/drivers/blast.py" build-macos/cod2_macos "$E/new-near" 30086 gl baseline
python3 "$E/drivers/blast.py" build-macos-codx/cod2_macos "$E/new-far" 30087 gl distant
magick "$E/final-stock-near/home/main/screenshots/after-7.jpg" \
  -colorspace gray -format "%[fx:mean*255]" info:
```

The driver enables `COD2_MAC_COMBAT_TRACE=1`, joins British allies with
`sten_mp`, uses god mode and `setviewpos 2569 2274 181 180 25`, hides the gun,
then holds `+frag` for 0.6 seconds. Screenshot bursts are followed by longer
captures. Engine receipts prove event 188 at approximately
`(2433,2203,71)` with `explosions/grenadeexp_concrete`. Capture filenames/timing
and means are retained in the private JSON evidence.

### Near frag

| Run | Before | Blast frame | During feedback | Release +7 s | Release +8 s | Release +12 s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Before stock (`before-stock-near`) | 51.18 | 67.47 (`blast-50`) | 6.09 (`blast-75`) | — | 50.07 | 50.07 |
| Fixed stock (`final-stock-near`) | 51.20 | 63.26 (`blast-50`) | 39.08 (`blast-75`) | 50.31 | 50.32 | 50.33 |
| Fixed CoD2x (`final-codx-near`) | 51.18 | 69.49 (`blast-50`) | 36.49 (`blast-75`) | 48.73 | 50.11 | 50.10 |

The grenade detonates about three seconds after release. The +7-second capture
is at the end of the four-second feedback; the CoD2x value is slightly below
its settled +8-second value, consistent with residual FX/camera variation. The fixed
world remains visible throughout, and the authored feedback ends normally.
`before-stock-nofx` and `before-stock-nolight` separately reproduce the blackout.
Inspected before `blast-75`, both fixed `blast-75`, and the recovery captures.

### Distant frag

After the grenade lands, the accepted distance driver enables noclip for a
stationary observation camera at `setviewpos 2569 2700 400 270 25`. `viewpos`
receipts verify `(2569,2700,401)` throughout. This is **611.7 units** from the
landed explosion, with visible above-roof world geometry. The explosion event
still arrives. The old-copy control also stays bright here.

| Run | Before blast (`blast-0`) | After blast (`blast-75`) | Release +12 s |
| --- | ---: | ---: | ---: |
| `before-distant-high` | 96.63 | 96.74 | 96.63 |
| `final-stock-distant-high` | 96.64 | 96.56 | 96.63 |
| `final-codx-distant-high` | 96.64 | 96.71 | 96.63 |

Early `*-distant` and `*-distant-stable` runs used a lower camera and showed
underside/void geometry. They are rejected as acceptance evidence. The
accepted `*-distant-high` screenshots were inspected. The separate
`distant-before` screenshot request was superseded by the burst's first queued
request; `blast-0` provides the accepted pre-detonation camera image instead.

### Smoke and sustained SMG fire

Event 191 is grounded near `(2432,2201,71)`. Both smoke sequences show an
expanding gray cloud, never a black world. The licensed smoke definition's
maximum delay plus particle life is **8000 + 40000 = 48000 ms**. The first
42-second captures still contained smoke and were not treated as expiry proof.
Final expiry runs capture through that authored bound:

| Run | Before | Release +8 s | Release +20 s | Release +48 s | Release +49 s |
| --- | ---: | ---: | ---: | ---: | ---: |
| `final-stock-smoke-expiry` | 51.18 | 90.19 | 108.95 | 51.18 | 51.19 |
| `final-codx-smoke-expiry` | 51.17 | 90.34 | 108.97 | 51.18 | 51.18 |

Both `after-48.jpg` images were inspected: the cloud has cleared and the world
is visible again. The means agree with their pre-smoke images within 0.01.
Measurements are retained in `$E/smoke-expiry-luminance.json`.

Sten runs `final-stock-bullets` and `final-codx-bullets` fire three two-second
bursts, replenishing ammunition between bursts. Inspected `bullet-23.jpg`
shows the world and impact particles/decals, with mean **57.11 / 58.89** versus
pre-fire **46.85 / 46.76**. The screenshot burst catches ammunition consumption.
The immediate final `after` screenshot was superseded by quit, so that filename
is not claimed as evidence; the last sustained-fire captures are present.
All accepted combat drivers exit zero.

These are visual correctness checks, not performance measurements. `ioreg`
reported `IOConsoleLocked = Yes` during the run. Screenshots are internal GL
readbacks; background compiler/fixture/ABI jobs used `taskpolicy -b nice -n 19`.
No competing game was observed before launches. No FPS claim or unlocked-screen
benchmark result is made, and no benchmark lock was needed for these checks.

## Merge gate

The actual engine compile databases were saved as
`build-macos{,-codx}/compile_commands.engine.json`; fixture copies use
`-UNDEBUG` so Release does not disable assertions. Logs are ignored local files
under `output/ws41/`, or private live output under `$E`.

| Check | Result | Evidence |
| --- | --- | --- |
| Release arm64 stock and CoD2x builds | Both pass | `/tmp/ws41-{stock,codx}-final-build.log` |
| ABI function audit | 622 / 639 translation units; zero errors, mismatches or new entries | `output/ws41/abi-final-{stock,codx}/` |
| Renderer callback audit | 218 bindings each; zero table/cast/floating-call mismatches | Same final ABI directories |
| Import audit | 514 symbols, 185 placeholders; zero proven missing/extra dereferences | Same final ABI directories |
| Raw-offset gate | 390 / 403 sources, 1196 / 1202 reviewed sites; zero unreviewed sites | `output/ws41/raw-{stock,codx}.json`, final ABI directories |
| Tools, datagen, ABI checker, shader manifest and Wine trace unit suites | All pass | `output/ws41/units-00` through `units-06.log` |
| CoD2x full sanitizer and native suites | Both pass | `output/ws41/units-{07,08}.log` |
| SDK identity and performance source fixtures | Both pass | `output/ws41/units-{09,10}.log` |
| Private datagen round trip, game layouts and 1.3 reference facts | All pass | `output/ws41/units-{11,12,13}.log` |
| Online, fixes13 and game production fixtures | Pass on both build databases | `output/ws41/fixtures-00` through `fixtures-05.log` |
| Script compiler/VM and UI fixtures | Script passes; UI passes on both databases | `output/ws41/fixtures-{06,07,08}.log` |
| Renderer buffers/commands, raster, saved screen, texture mip and volume fixtures | All pass | `output/ws41/fixtures-09` through `fixtures-12.log` |
| HUD, impact marks, FX events/primitives/cloud and dedicated FX fixtures | All pass | `output/ws41/fixtures-13` through `fixtures-18.log` |
| Platform CTest, ASan/UBSan Debug | 28/28 pass | `output/ws41/platform-02.log` |
| Platform CTest, plain Debug | 30/30 pass, including diagnostics/crash-report tests | `output/ws41/platform-05.log` |
| Stock and CoD2x menu sweeps | Each passes 59 front-end menus, 73 in-game menus and 46 safe scripts | `$E/menu-{stock,codx}/results.json`, driver logs |
| CoD2x deterministic local state capture | Exit zero; 100 statehash rows, 206 entities | `$E/parity/{run.json,statehash.txt,entities.txt}` |
| fixes13 legacy guard against `port/int-0.2` | Two production files, 10 configurations; zero mismatches | Command below |
| ABI legacy guard against `port/int-0.2` | Two files, five COD2_X64-off configurations; zero mismatches | Command below |
| Exact `unifdef -UCOD2_X64` + `cmp` | Both changed production files byte-identical to `cbae9ed` | `/tmp/ws41-{device,shock}-{before,after}.i386` |
| Whitespace check | `git diff --check` passes | Final checkout |

`output/ws41/{units,fixtures,platform}.json` retain every command, exit status
and log filename. Every command in those three groups exited zero. The build
commands use the CONTRIBUTING Release configurations, arm64 architecture,
`COD2_X64=ON`, and `-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1` for CoD2x. Both use
`cmake --build <directory> --target cod2_macos --parallel 3`. Existing linker
warnings about duplicate libc++, the local SDL deployment target and section
alignment remain. Typed-data regeneration agreed with the committed snapshot.

The parity recorder used the existing engine through a private wrapper which
checks for other games, sets windowed mode and the shader cache, and supplies
`timeout -k 5 120`. It records local deterministic server state; no x86 parity
comparison or render-performance result is inferred. Menu sweeps ran
sequentially with private output directories and ports 30081 and 30082.

```sh
python3 tests/rendering/menu_sweep.py build-macos/cod2_macos \
  --output "$E/menu-stock" --port 30081
python3 tests/rendering/menu_sweep.py build-macos-codx/cod2_macos \
  --output "$E/menu-codx" --port 30082
python3 tools/parity/record.py --binary "$E/drivers/parity.sh" \
  --data "$HOME/Games/CoD2" --output "$E/parity" --frames 100 --port 30085
```

Commands for the ABI/legacy portion:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws41/abi-final-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws41/abi-final-codx
python3 tools/abi/raw_offsets.py build-macos/compile_commands.engine.json --output output/ws41/raw-stock.json
python3 tools/abi/raw_offsets.py build-macos-codx/compile_commands.engine.json --output output/ws41/raw-codx.json
python3 tests/fixes13/legacy.py --base port/int-0.2
python3 tools/abi/legacy.py --base port/int-0.2
```

For each changed production source, `git show cbae9ed:<path>` and the working
file were separately passed through `unifdef -UCOD2_X64`; `cmp` reported exact
byte identity. No original i386 expressions were altered.

Two limitations remain explicit:

- `bash tools/ci/compare-x86.sh 410342a HEAD output/ws41/x86-reference` cannot
  run here: it requires x86_64 Linux and multilib. This Mac cannot prove i386
  object/binary identity. The exact preprocessor and required legacy gates pass;
  Linux artifact comparison remains an orchestrator/CI responsibility. No
  packages were installed.
- `tests/cod2x/check_inactive_gates.py --base port/int-0.2` reports **14
  mismatches across two files and 17 configurations**. Its native
  `COD2_CODX=0` cases require a native stock repair to disappear, and its
  `COD2_X64=0` cases define a macro which CMake OFF must omit. This is not a
  CoD2x-only patch and that additional gate does not pass. The required
  COD2_X64-off gates both report zero; no gate or baseline was weakened.

## Remaining limitations

The extra shock-file name still produces the reference-consistent warning,
now with a stable filename. Adding it to the table or loading its authored
value would change verified Mac behavior. Sparse Toujane visuals, the existing
GL renderer's other incomplete blit variants, and Metal remain outside this
workstream. Live CoD2x here means the CoD2x-enabled native build on a local
listen server; this does not certify an internet competitive-server session.
