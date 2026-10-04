# WS15 — rendering parity

**The follow-up section at the end supersedes the initial status below.** It
contains matched Windows/native comparisons, the merged-tree colour regression,
and the subsequent shader, lighting, HUD and gameplay fixes.

2026-10-03. Worktree `~/Projects/cod2-native-wt/render-parity`, branch
`port/render-parity`, starting commit `4ddccfd`.

**Verified partial result; full rendering parity is not achieved.** The native
client loads the original Mac ARB programs and reflection, renders lit world
materials and the Enfield, accepts the stock MP FX flags, and no longer draws
empty talker slots. The real ARB draw path remains opt-in because firing can
still crash in an Apple OpenGL/Metal completion thread. Sky clouds, complete
lighting/post-processing parity, visible FX primitives, and the required i386
wavelet comparison remain unresolved.

No CMake files or shared `src/headers/*` files changed. No licensed assets,
executables, shader payloads, screenshots, or decompiler dumps were committed.
No packages were installed and no remotes, pushes, PRs, or issues were created.
All work and commits stayed in this worktree.

## Evidence locations

Use these abbreviations below; all game imagery and extracted data are outside git:

```text
H = ~/Library/Application Support/CoD2-native-ws15
E = H/evidence
S = H/main/screenshots
W = ~/Library/Application Support/CoD2x-Wine/highball/bottles/cod2x/drive_c/Games/CoD2/main
```

The read-only Mac reference was
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`; the accompanying reference
source was `~/Projects/cod2-native-refs/CoD2x/src/other/Call_of_Duty_2_Multiplayer_MAC_1.3.c`.
Stock data came from `~/Games/CoD2/main`. Native captures used a private home and
listen-server port 29015. The final Windows reference used port 29016. Owned
clients were stopped; listen-server `quit` still reaches the inherited script
cleanup hang and needs termination after joining.

## Shaders and lighting

The missing real shader path was an incomplete reconstruction of the Mac shader
loader, rather than a demonstrated D3D bytecode translator failure. The original
Mac `_D3DXCompileShader` contains a filename-indexed library of compiled ARB
programs and constant metadata. The native implementation instead selected
generic programs by inspecting HLSL text, with mostly empty reflection. Stock
IWDs supply HLSL/technique descriptions, but do not supply that Mac ARB library.

`tools/macos-port/extract_shaders.py` recovers the library from the user-owned
symbolized binary into an external directory: 208 `.vsa`, 209 `.pse`, 208 `.vc`,
209 `.pc` files, 834 total. It follows paired filename/value references inside
the compiler function, validates signatures and metadata, reconstructs the fixed
CTAB records, and writes a SHA-256 manifest. It refuses an output inside this
repository. NVIDIA-specific vertex programs are unnecessary for this ARB path.

The new native loader uses `COD2_MAC_SHADER_CACHE` and retains genuine register,
sampler, class, and type metadata. An explicitly selected cache with a missing
or invalid entry fails visibly; it does not quietly select a generic shader.
Without a cache the existing fallback remains available, with a diagnostic.

The D3D draw implementation was independently replacing the shaders with a
fixed-function approximation. `lp64_shader_draw.h` now binds the real programs,
the D3D vertex declaration (including tangent/normal/texture inputs), and all
16 samplers. The vertex-program handle is read through its native structure,
not the old `shader + 4` offset. Indexed reads are checked against the index
buffer length. This path requires both a cache and the existing `D3D_PROG`
opt-in; `D3D_PROG=0` also counts as enabled because the pre-existing switch
tests environment-variable presence. Omit it to keep the approximation.

Additional faults exposed by real reflection/programs:

* World code samplers still addressed `GfxWorld` and lightmap pointer groups
  using ILP32 offsets/strides. Static-model lighting, four lightmap images, and
  outdoor lighting now use typed native pointers.
* The native loader had been merging directional lightmaps into one approximate
  image. The Apple path now retains the four RGB/sun images required by the
  original shaders. This corrected the red world surfaces observed immediately
  after enabling the real shader library.
* OpenGL texture completeness was lost when mipmapped sampler states were
  applied to textures with fewer levels. Both 2D and cube constructors now set
  `GL_TEXTURE_MAX_LEVEL` to the actual level count minus one. The sky changed
  from white/black failures to blue, but the final sky still lacks the reference
  clouds.
* The reconstructed image-generator light-grid matrix and cubemap face-axis
  table were zero initialized. Their Mac constants are now populated. The
  lightmap weight generator also used an uninitialized direction component and
  complement, reversed `atan2` arguments, and incomplete weights. The native
  generator now follows the Mac arithmetic and shader-specific byte packing.
  The Enfield/hands changed from almost black to visibly lit.
* Bitmap format 2 was expanded into the old PowerPC byte order. Native BGR
  input now becomes BGRA with opaque alpha.

The eleven renderer sanitizer fixtures include reflection bounds/path rejection,
bitmap channel ordering, lighting weights that partition unity, distinct cube
faces, and the wavelet loader's lookahead. Runtime diagnostics confirmed real
program IDs, cube/2D bindings, and declared attributes during world draws. The
sky cube itself has a populated 512-pixel face with a varying red range
115–239, so its current flat appearance is not explained by an absent image.
No ARB compile-error messages occurred in the captured map logs.

The actual `Material_Marshal32To64` was also exercised on all **3,535 stock
binary materials**, under ASan/UBSan, with resource registration mocked. All
were accepted and their source bytes remained unchanged. Files under
`materials/shaders`, `techniques`, and `techniquesets` are text and were excluded
from the binary count. `E/materials/audit.json` records the inputs, count, and
zero binary rejections; `E/material_stock.c` is the external audit harness.
This validates marshalling, not every material's rendered appearance.

To reproduce the real shader path:

```sh
python3 tools/macos-port/extract_shaders.py \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  "$HOME/Library/Application Support/CoD2-native-ws15/shaders"

D3D_PROG=1 \
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2-native-ws15/shaders" \
build-macos/cod2_macos \
  +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$HOME/Library/Application Support/CoD2-native-ws15" \
  +set r_mode 1280x720 +set r_fullscreen 0 +set com_introPlayed 1 \
  +set sv_pure 0 +set g_gametype dm +devmap mp_toujane
```

This is a diagnostic mode, not a claim of safe gameplay. Optional
`COD2_MAC_SHADER_DIAGNOSTICS=1` reports the first bindings and GL errors.

## FX

The reconstructed flag names/masks did not match Mac 1.3. Native private tables
now contain the verified 24 attribute entries and 13 spawn entries. Legacy
tables/counts remain on the OFF path. Token parsing now handles leading and
repeated whitespace and allocates room for the terminating NUL.

A full-width `FxBoltFramePtr.value` is also retained through acquisition,
scheduler dispatch, and cleanup. The old `_placeholder` truncated a live bolt
frame and caused a first-shot CPU fault in `FX_GetBoltingFrame`. The fixture
checks a pointer above `UINT32_MAX`, repeated acquisition, and balanced releases.

The dedicated fixture gained an optional stock parser audit. It clears the
original 256-template per-map cache between assets, mocks media/model
registration, and preserves normal missing-file/parser diagnostics. The stock
audit covers 375 `.efx` files and 558 dependency reads: **372 accepted, three
rejected, zero `flags` errors**. The rejections are:

| Template | Reason |
| --- | --- |
| `fx/fire/fireheavysmoke.efx` | References absent `fx/smoke/heavysmoke.efx` |
| `fx/impacts/minefield.efx` | References absent `fx/impacts/fluff1.efx` |
| `fx/impacts/wall_impact_small_dirt.efx` | Uses `rgbComponentInterpolation`, absent from the verified Mac spawn table |

These are reported, not silently ignored or assigned invented masks.
`E/fx-stock/manifest.json` and `parser.log` retain the audit evidence. Stock
Toujane no longer reports Emitter `flags` failures.

**Visible effects are still blocked by inherited native code.**
`FxScheduler_CreateEffect` explicitly jumps to cleanup under `COD2_X64` because
`FxPrimitives.c` still contains x86 C++ vtable addresses and four-byte virtual
dispatch. Removing that guard would reintroduce invalid native calls. Muzzle
flashes, smoke, explosions, and impacts are therefore not restored merely by
the parser/bolt fixes. Porting the effect classes and their virtual dispatch is
required before visual FX parity can be claimed.

```sh
ASAN_OPTIONS=symbolize=0 sh tests/platform/run_dedicated_fx.sh
ASAN_OPTIONS=symbolize=0 python3 tools/macos-port/check_fx.py \
  "$HOME/Games/CoD2/main" \
  "$HOME/Library/Application Support/CoD2-native-ws15/evidence/fx-stock" \
  --parser build-macos/headless-fx-parser
```

## HUD and missing beep

The native HUD now uses the Mac owner-draw IDs: record 265, local talking 266,
talker slots 267–270. Empty slots return before drawing a speaker or indexing
`playerNames[-1]`. The previous generic range drew icons for empty slots and
misclassified other owner draws. Before captures show repeated speaker icons;
the corrected captures show none in this one-player, non-talking test.
`Voice_IsClientTalking` itself already uses the Mac's 300 ms talk-time test;
no speculative changes were made to voice state arrays. Real remote voice
recording, reception, and timing remain untested.

All **28 IWDs**, including localized packs, were searched case insensitively.
`sound/misc/beep.wav` is absent. The only beep WAV is
`sound/vehicles/horn_beep.wav` in `iw_06.iwd`. The Windows 1.3 reference reports
the same missing misc beep. This is a stock-data diagnostic, not evidence of a
native path/case/IWD-search regression. No replacement sound was added.
See `E/beep-audit.json` and the reference `W/console_mp.log`.

## Wavelet IWI validation

All **25 stock format 6/7 images** were decoded through every mip level by the
actual engine decoder: 13 format 6, 12 format 7. ASan/UBSan reported no errors;
format 7 alpha is opaque throughout. Input and decoded BGRA mip-chain hashes
are in `E/wavelets-arm64/manifest.json`. The three 4096-entry Huffman tables
also match the Mac binary byte for byte; their SHA-256 values are:

```text
alpha     c2a276ead30ca8c78ea09ac719d6e94c1b7dc84e29eb5ac00fab273acdaee346
blue      eb0c8f4bab629e9aa33f153affa7862a151b6c55ce69fd15ed3e7729bda30ad9
red/green b00e631c4d070f71e48ef71ef752b62c3ff46b89a816964bf65c1021da21b297
```

`E/wavelet-table-comparison.json` records all three 16,384-byte comparisons.

The stock audit demonstrated the decoder's two-byte reservoir lead plus dword
refill, including final input positions one/two bytes beyond the file length.
`FS_ReadFile` guarantees only one NUL. The native image loader now makes a
zero-padded six-byte lookahead copy for wavelet images, and uses the native
`mapType` for its cube decision. A separate sanitizer fixture exercises that
actual loader-to-upload path. The decoder algorithm was not changed.

```sh
clang -g -arch arm64 -std=c11 -ffp-contract=off -DCOD2_X64=1 \
  -Isrc -Isrc/headers -Wno-typedef-redefinition \
  -Wno-duplicate-decl-specifier -fsanitize=address,undefined \
  tests/lp64/renderer/wavelet_stock.c src/PC/gfx_d3d/r_image_wavelet.c \
  -o build-macos/ws6-tests/wavelet_stock
ASAN_OPTIONS=symbolize=0 python3 tools/macos-port/check_wavelets.py \
  "$HOME/Games/CoD2/main" \
  "$HOME/Library/Application Support/CoD2-native-ws15/evidence/wavelets-arm64" \
  --decoder build-macos/ws6-tests/wavelet_stock
```

**The requested i386-vs-arm64 output comparison is not completed.** macOS cannot
execute the original i386 program. No i686 Windows cross compiler, `lld-link`,
`ld.lld`, QEMU, or comparable executable i386 harness is available here.
No tool was installed. Two owned x86_64/Rosetta helper probes, PIDs 56864 and
60406, entered kernel `U` state with SIGKILL pending. Their parent audit
processes were stopped; both helpers had exited by the final process check.
The ARM decoder/sanitizer results and table comparison
are useful evidence, but do not substitute for differential i386 output.

## Screenshot comparisons and their limits

All captures used `screenshotJPEG` at 1280×720 on local Toujane with cheats.
Before executable `E/cod2-before` is the starting native build. External driver
scripts and logs in E preserve the commands, menu responses, and filenames.

The comparison also exposed a camera bug: `SetClientViewAngle` was applying
prone constraints to standing players and skipping them for prone players.
The native condition now matches Mac 1.3. A sanitizer fixture proves standing
yaw 210 is retained, prone yaw is constrained, and mounted state is exempt.
Cgame command name/registration loops also used eight-byte records; they now
use the native record size. `viewpos` can run with a current snapshot when no
future snapshot exists.

Standing-camera height and noclip behavior still differ. The native command
`setviewpos 2569 2274 181 0` settled at printed `(2568 2274 130) : 0`.
For the closest reference comparison, Windows used
`setviewpos 2569 2274 129 0`, printing `(2568 2274 129) : 0`. Thus the final
corner view has matched XY/yaw and a one-unit height difference, not an exact
camera proof. The Wine reference itself shows incorrect occlusion/transparent
overdraw in this view. The other requested roof/high-sky positions could not
be held reliably natively and are recorded as failed camera comparisons.

| Evidence | Path |
| --- | --- |
| Before, corner/courtyard filename | `S/ws15-before-matched-courtyard.jpg` |
| After, same native command | `S/ws15-after-matched-courtyard.jpg` |
| Final build after wavelet padding | `S/ws15-padded-final-courtyard.jpg` |
| Closest Windows reference | `E/ws15-reference-matched.jpg` (also under W/screenshots) |
| Before repeated speakers | `S/ws15-before-hud.jpg` |
| After lit weapon and empty talkers | `S/ws15-lookup-combat-hud.jpg` |
| After wider world/flat blue sky | `S/ws15-lookup-combat-courtyard.jpg` |
| Before/after difference panel | `E/before-after-courtyard.png` |
| After/Windows difference panel | `E/native-windows-courtyard.png` |
| Before/Windows difference panel | `E/before-windows-courtyard.png` |

`tools/macos-port/compare_images.swift` uses only macOS SDK ImageIO/CoreGraphics.
Panels show A, absolute RGB difference amplified 3×, then B. For the corner
before/after comparison, RGB MAE is **20.58/255**, RMSE **22.69/255**; 14.94% of
pixels have a channel difference above 32. Before versus Windows, MAE is
**52.93/255**. After versus the closest Windows reference, MAE is **34.95/255**,
RMSE **61.38/255**; 30.14% exceed 32. These
measure the remaining visible difference; they are not a parity threshold,
especially given the reference overdraw and camera limitation.

```sh
swift tools/macos-port/compare_images.swift \
  "$HOME/Library/Application Support/CoD2-native-ws15/main/screenshots/ws15-before-matched-courtyard.jpg" \
  "$HOME/Library/Application Support/CoD2-native-ws15/main/screenshots/ws15-after-matched-courtyard.jpg" \
  "$HOME/Library/Application Support/CoD2-native-ws15/evidence/before-after-courtyard.png"
```

## Checks and remaining blockers

The native target builds. Eleven renderer fixtures pass ASan/UBSan; dedicated
FX and view-angle fixtures pass; the 25-image wavelet audit passes. The full ABI
check reports **620 TUs, zero errors/mismatches/new findings**, **218 renderer
bindings with zero table/cast/unprototyped-float mismatches**. Its import audit
has zero proven missing or extra dereferences. OFF source-body comparison
against `4ddccfd` reports **85 comparisons, zero mismatches**. This checks
guarded OFF preservation; an executable 32-bit binary comparison was not
possible on this host. `git diff --check` is clean.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos -j12 --target cod2_macos
ASAN_OPTIONS=symbolize=0 sh tests/lp64/renderer/run.sh
ASAN_OPTIONS=symbolize=0 sh tests/platform/run_dedicated_fx.sh
clang -arch arm64 -std=gnu99 -g -O0 -fcommon -ffp-contract=off \
  -DCOD2_X64=1 -fsanitize=address,undefined -Wl,-dead_strip \
  -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration \
  -Wno-error=int-conversion -Wno-error=implicit-int \
  -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Isrc -Isrc/headers \
  tests/platform/macos_view_angle.c src/PC/game_mp/g_client_mp.c \
  src/PC/qcommon/q_math.c src/PC/universal/com_math.c \
  -o build-macos/ws15-view-angle
ASAN_OPTIONS=symbolize=0 build-macos/ws15-view-angle
tools/abi/check.sh build-macos/compile_commands.json build-macos/ws15-abi-final
python3 tests/lp64/renderer/legacy_guards.py --base 4ddccfd
git diff --check
```

Blockers that prevent a full-parity or gameplay-ready claim:

1. Real-program firing intermittently ends in SIGTRAP/SIGSEGV. Owned reports
   `E/cod2_crash_25175.txt` and `E/cod2_crash_56835.txt` show
   `objc_destructInstance -> _Block_release -> MTLDispatchListApply` in an
   asynchronous completion thread. Recent `E/ws15-lookup-combat.log` records
   exit -5 after `+attack`; one later firing run survived, which is not proof
   of a fix. Forcing `glFinish` did not resolve it. The application-side cause
   is not established; blaming the driver alone would be premature.
2. Sky clouds are absent and world/model lighting still differs. Real ARB
   shaders, textures, reflection, and generated weights are now available for
   continuing the draw-state/projection/fog investigation. The final
   `E/ws15-padded-final.log` also reports Apple fallback to software vertex
   processing after `buildPipelineState failed` (`m_disable_code: 1000`).
   Loading a real program is not proof that it runs on the GPU.
3. 2D/post-processing remains on the existing approximation. Enabling the same
   ARB draw helper for every 2D pass caused the HUD to disappear, so that
   experiment was reverted. Full channel mixing/bloom parity is not verified.
4. Native FX primitive dispatch remains disabled for the concrete x86-vtable
   reason above. Flags/media parsing alone cannot make the effects visible.
5. Camera/noclip differences and the unavailable i386 execution harness limit
   the requested strict comparison. Remote voice and complete multiplayer
   gameplay were not validated. No 333 fps claim is made.

## Merge notes

Merge the focused commits after `4ddccfd` in order, followed by this report.
The main WS13 overlap is `CDirect3DDevice.c` (native sampler-array size,
Set/GetTexture stage limits, one include and early dispatch), plus the small
vertex-program getter and mip-completeness calls in the texture constructors.
The draw helper is a separate header. It currently binds 16 samplers and
attributes per draw; WS13 can optimize that once correctness is settled.
Preserve both the cache requirement and `D3D_PROG` opt-in while crashes remain.
The verified loader/reflection and image/parser fixes can be merged without
enabling that experimental rendering mode for normal users.

Other overlaps are guarded `rb_shade.c` sampler accesses, the Apple directional
lightmap loader condition, image generation/loading, FX parser/scheduler, UI
owner draws, and the two camera/command fixes. Preserve all OFF branches when
resolving conflicts. No additional frameworks or system packages are needed.
The external shader cache and evidence must remain outside the repository.

## Follow-up: merged renderer regression and matched parity (2026-10-03)

Started with `git merge port/main`, fast-forwarding this worktree to `88d2976`.
This includes WS13, WS14 and the orchestrator's colour-converter adaptation.
All subsequent work stayed on `port/render-parity`; no sibling worktree was
accessed. The evidence abbreviations H/E/S above still apply.

### Magenta regression: cause and automated check

The regression came from directional lightmaps, not WS13's indexed colour
conversion. The earlier Apple-specific image-loader change always retained four
packed lightmap planes. The default approximation then sampled plane zero as
RGB, although its channels encode directional coefficients. Keeping the four
planes only for real ARB rendering restores the original RGB synthesis for the
approximation. WS13's indexed-span conversion and NULL-for-RGBA contract remain
intact.

Built the merged baseline, a targeted rollback of WS13's colour conversion, and
the lightmap-path fix. Baseline and colour-conversion rollback screenshots were
pixel-identical (MAE 0); the lightmap-path change removed the pink surfaces.
These controlled builds and images are outside git. The fixed intro check uses
a stable world-only ROI and rejects excessive pixels whose red and blue both
exceed green, as well as implausible mean RGB. It requires only Python and the
macOS Swift SDK, not Pillow/NumPy or installed packages.

```sh
python3 tests/rendering/toujane_rgb.py build-macos-codx/cod2_macos \
  --home "$HOME/Library/Application Support/CoD2-native-ws15/rgb-codx-final"
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2-native-ws15/shaders" \
  python3 tests/rendering/toujane_rgb.py build-macos-codx/cod2_macos \
  --home "$HOME/Library/Application Support/CoD2-native-ws15/rgb-arb-final"
```

The merged baseline fails: magenta fraction 0.456774, mean RGB
`[41.72,27.55,40.93]`. Both final modes pass: default approximation fraction 0,
mean `[52.82,48.49,39.43]`; real ARB fraction 0, mean `[43.86,42.49,38.47]`.
Logs are `E/rgb-codx-final.log` and `E/rgb-arb-final.log`; each private test home
contains its game-produced `main/screenshots/ws15-rgb-regression.jpg`.

### Matched cameras, reference capture and measured result

Both games use Toujane DM, 1280x720, `cg_fov 80`, `r_gamma 1`,
`r_ignoreHwGamma 0`, `r_fog 1`, unchanged normal maps, `r_aaSamples 1`,
`r_picmip_manual 1`, and all three picmip values 0. HUD and gun are hidden for
the three comparisons. Noclip holds the pose; it is turned **off** for combat
validation. The native noclip dispatch previously used the wrong movement
case. Correct dispatch also makes the camera stable between capture frames.

| View | `setviewpos` | Captured eye | Pitch |
| --- | --- | --- | --- |
| Courtyard | `2569 2274 181 180` | `2569 2274 182` | 0 |
| Rooftop | `3051 2178 220 180` | `3051 2178 221` | 0 |
| Sky up | `3051 2178 350 0`, then hold/release `+lookup` | `3051 2178 351` | clamped 85 |

Sky projection/eye constants verify the matching pitch; an older Windows sky
capture was only 67.3 degrees and is explicitly excluded. Another intermediate
capture used Windows picmip 2 and is also excluded. The final files below use
the matched camera and picmip settings. Screenshot JPEGs read the pre-gamma
framebuffer on both paths, so gamma presentation has a separate GPU test.

The Windows API trace uses the existing WS8 Wine prefix and WineD3D with
`WINEDEBUG=fixme-all,+d3d9,+d3d_shader,+d3d`. WineD3D's GL presentation on this
machine produces black/incorrectly occluded images, so the clean visual
reference uses the same Windows 1.3 diagnostic executable and existing WS8
D9VK DLL. No Wine components were installed. The launcher now supports a
private `--desktop 1280x720`; this prevents host desktop sizing from silently
changing the reference framebuffer. The precise reference launch was:

```sh
COD2_WINE_D3D9="$HOME/Library/Application Support/CoD2x-Wine/downloads/d9vk-macOS-async-v1.10.3-20250511/x32/d3d9.dll" \
tools/wine/play-cod2x.sh --reference --renderer d9vk --desktop 1280x720 \
  --resolution 1280x720 --dx9 --local --fullscreen --fps 60 -- \
  +set in_mouse 0 +set net_port 29016 +set r_picmip 0 +set r_picmip_bump 0 \
  +set r_picmip_spec 0 +set r_picmip_manual 1 +set r_aaSamples 1 \
  +set r_gamma 1 +set r_ignoreHwGamma 0 +set cg_fov 80 +exec ws15-parity.cfg
```

`W/main/ws15-parity.cfg` contains menu responses, the poses above, released
movement/look buttons, and cvar queries. `E/final-parity-views.py` drives the
equivalent native console commands and traces. The final native images were
captured with `build-macos-codx/cod2_macos`. Comparison uses all RGB pixels with
no alignment, colour adjustment, crop or exclusion mask:

| View | Before MAE (0–255) | Final MAE (0–255) | Final RMSE (0–255) | Before / after / reference panel |
| --- | ---: | ---: | ---: | --- |
| Courtyard | 13.7801 | **2.0050** | 3.8825 | `E/courtyard-final-parity-panels.png` |
| Rooftop | 8.5242 | **1.8718** | 4.7761 | `E/rooftop-final-parity-panels.png` |
| Sky up | 13.4085 | **0.5856** | 0.9951 | `E/sky-final-parity-panels.png` |

Before: `S/ws15-volume-{courtyard,roof,sky}.jpg`, the first **matched ARB**
baseline in this follow-up, not the earlier magenta intro. After:
`S/ws15-parity-final-{courtyard,roof,sky}.jpg`. References:
`E/followup-d9vk-parity-{courtyard,rooftop,sky}.jpg`. Each panel has labelled
Before/After/Windows reference columns, with metrics saved beside it as
`E/{courtyard,rooftop,sky}-final-parity-metrics.json`.

```sh
swift tools/macos-port/compare_images.swift --before-after-reference \
  "$H/main/screenshots/ws15-volume-courtyard.jpg" \
  "$H/main/screenshots/ws15-parity-final-courtyard.jpg" \
  "$H/evidence/followup-d9vk-parity-courtyard.jpg" \
  "$H/evidence/courtyard-final-parity-panels.png"
```

### Draw-call evidence and fixes

`COD2_MAC_DRAW_TRACE` names a request file. Writing an external JSONL output
path into it captures one complete native frame: geometry, material and shader
labels, all float constants, D3D render/sampler/texture-stage states, texture
objects and actual GL texture target types. It never saves shader bytecode.
Final native traces are `E/native-parity-final-{courtyard,rooftop,sky}.jsonl`.
The Wine reducer records constants at the **inner** Wine draw after its buffered
state flush; recording at D3D9 API entry incorrectly reports stale constants.
The reducer's thread/flush behaviour has an automated test.

Wine raw log: `E/wine-parity-trace.log`. Reduced reference frames:
`E/wine-parity-frames/{courtyard,rooftop,sky}.jsonl`, frames 536/581/714 with
173/179/47 draws. Native has 117/118/4 draws. Windows performs a depth prepass,
and the Mac renderer orders sky earlier, so blindly pairing draw indices is
wrong. Correlating geometry gives 50/62/4 candidates; two model candidates are
ambiguous. `E/matched-draw-semantic-diff.json` records the candidates and
differences, including disabled-state exclusions and normalized projection
registers. Unobserved zero Wine sampler entries are unknown, not evidence of
an invalid driver state.

The first common shaded world draw is `toujane_decal_rug1`: native draw 1 versus
Windows 66 (courtyard) /67 (rooftop), not the repeated depth draw. Its native
fog row was NaN, while Windows supplies `[-1,1,-0.00015,0]`. Typed dvar reads
fix it; final fog and detail scale (`16,16,0,0`, Mac pixel c23 versus Windows
c0) match exactly. Mac WVP c23..26 maps to Windows c0..3 after GL depth and
pixel-centre normalization. Final courtyard/rooftop world matrix differences
are at most about 0.0003 from float rounding.

Other fixes are grounded in the Mac 1.3 binary and actual CPU/GPU tests:

- **Sky:** Apple compiles the original ARB program but unused primary/secondary
  colour OUTPUT aliases corrupt its cube texture coordinates to a constant.
  Pruning only unused aliases restores all cloud/sky faces; no shader
  instructions or licensed programs are committed. CGL checks reproduced the
  failure across VBO/client arrays, filtering modes and extra attribute state.
- **Lighting:** restore real volume `LockBox` dispatch (the COM vtable had six
  extra slots), upload/copy dirty 3D slices, preserve static atlas coordinates
  and BGRA lighting data, and bind typed 3D samplers. Real GL readback confirms
  the production upload path. Match the original persistent lightmap-weight
  row normalization and outdoor map dimensions/interpolation.
- **Model colours:** the actual cached-model producer stores BGRA at offset 24;
  skeletal offset-12 colours remain ARGB. Shared native order selection now
  respects that distinction and keeps RGBA input untouched. A fixture calls
  the real cached producer with distinct channels and alpha.
- **Rasterization:** apply ARB cull and separate blend equations. Convert D3D9
  integer pixel centres to GL half centres in projection; remove the legacy
  device's subpixel quirk for native hardware. The CGL raster fixture checks
  both windings, five blend operations and separate alpha.
- **State/layout:** restore DPVS plane comparisons; use native matrix sizes for
  2D HUD, typed shadow/lightmap settings, correctly sized render-target aliases
  and shutdown clearing, and actual native NPOT texture extents for screen UV
  constants. Every production change retains its old OFF path.
- **Gamma:** `MacDisplay_SetGamma` stores the device ramp rather than modifying
  desktop transfer tables. A 256-entry RGB16 LUT is applied in final FBO
  presentation. Identity retains the blit; nonidentity uses a GLSL 1.20 pass
  that preserves draw state. CGL ASan readback verifies independent R/G/B
  ramps, gamma mapping and `r_ignoreHwGamma` identity.

The real ARB path is now selected by default whenever a valid local
`COD2_MAC_SHADER_CACHE` is configured. `D3D_PROG=0` explicitly selects the
approximation; `D3D_PROG=1` is no longer needed. A missing licensed cache still
uses the approximation and reports why. The 834-file local cache remains in
`H/shaders`, outside git; the orchestrator must configure/extract it for users.

### Gameplay, FX and HUD follow-up

Restored native layouts, scene submissions and dispatch for nine FX primitive
classes. Fixed delayed-effect template entries, bolt/origin/axis fields and
local viewmodel camera origin. Particle clouds now initialize four corners,
all 6,144 indices and real packet counts (4,096 vertices, 2,048 triangles);
previous zero counts/UVs produced no smoke geometry. Dedicated FX continues
to avoid client graphics work. Fixtures exercise actual create/play/update,
delayed world/bolted scheduling, full primitive lifecycle and cloud buffers.

Noclip deliberately skips weapon updates in the original Mac. Earlier runs
that pressed attack while noclip was active therefore did **not** validate
firing, and are superseded by the grounded checks. The first grounded shot
reproduced the crash in `MacShader_DrawIndexed`: declaration pointers contained
normal-vector float bits. `nm` locates `markVerts` at `0x102151db8`, followed
only 16 bytes later by `materialGlobals`. The scratch buffer's reconstructed
type was an `int`, although impacts advertised 1,024 `GfxWorldVertex` entries.
A real native array and contiguous three-axis orientation fix both the global
overwrite and a separately reproduced stack overread. The regression fixture
fills all 1,024 vertices, copies the final nine, recycles the full mark pool
through 1,100 impacts and checks actual scene submissions under ASan/UBSan.

The zPAM white box is its mostly transparent 512x512 DXT5 compass face with
only mip 0. Unrestricted GL mip levels made it incomplete and sampled white;
the already-merged constructor fix bounds `GL_TEXTURE_MAX_LEVEL`. Production
CGL tests cover one-level, two-level and full mip chains. The actual zPAM image
was temporarily placed in the private home's **main** image search directory,
then removed. `S/ws15-pam-final-hud.jpg` confirms transparent compass corners
with real ARB HUD rendering; this is a local asset check, not a repeated online
zPAM session. A raw-directory override sits below stock IWDs and is insufficient.

### Final integration (2026-10-03)

Merged `port/main` at `64bb6ad` in `da2e4bd`, including WS14, WS16 and WS17.
The four conflicts preserve both sides' safety intent: WS15's typed scheduled
FX fields, widened renderer DObj storage and native matrix assignments cover
WS16's overlapping layout repairs; platform CMake retains gamma, frame-wait
and fullscreen tests. Other changes merged automatically and were reviewed.

Pending-change review: committed the native nonuniform bone-bounds fix and
four-pose corner comparison in `4fb4adb`, and the earlier follow-up evidence in
`1e0af39`. Removed the unused, never-called device-capability helper and its
unregistered test, plus temporary material-pointer diagnostics. The two source
experiments are preserved in a named git stash; the unused fixture is outside
git at `E/integration-final/unused-device-caps.c`. They are not merge inputs.

Integration exposed stale test fixtures, repaired in `8f8e4bc`: script strings
now test final-reference reclamation, VM mocks use the verified void returns,
delayed FX tests retain the production full-width link accessor, HUD expectations
use D3D9 pixel centers, and renderer options test automatic cache selection and
explicit fallback. No engine behavior was changed for these fixture repairs.

All requested suites pass, as do the extra HUD, FX primitive/cloud/event/impact
and CGL texture/raster/volume checks. Evidence is in `E/integration-final/`;
`suite-results.json` retains initial failures and successful rerun log names.
Both full builds exit 0. The complete ABI command exits 0: **620 translation
units, 0 errors, 0 mismatches; 218 renderer bindings, 0 table/cast mismatches;
0 proven extra or missing import dereferences**.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos -j12 --target cod2_macos
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx -j12 --target cod2_macos
python3 tests/fixes13/run.py --build build-macos-codx
sh tests/lp64/renderer/run.sh
python3 tests/lp64/game/run.py
python3 tests/lp64/script/run.py
bash tests/cod2x/run.sh
python3 tests/perf/run.py
python3 tests/online/run.py build-macos/compile_commands.json
python3 tests/lp64/script/vm_semantics.py
sh tools/abi/check.sh build-macos/compile_commands.json "$H/evidence/integration-final/abi"
python3 tests/lp64/renderer/legacy_guards.py --base port/main
git diff --check
```

The exact no-argument `fixes13/run.py` command also passed, using a temporary
`build/ws9/compile_commands.json` symlink to the CoD2x database, because its
legacy default needs CoD2x flags. Test output was then moved to ignored
`build-macos/ws15-fixes13`; the explicit `--build` form above reproduces it
without that setup. Logs are `build-{stock,codx}.log`, `fixes13-verified.log`,
`renderer.log`, `game.log`, `script-final.log`, `cod2x.log`, `perf.log`,
`online-rerun.log`, `vm_semantics.log` and `abi.log` in the evidence directory.

Inactive source-body checks show **175 comparisons, 0 mismatches** against
`port/main`, and 150 comparisons, 0 mismatches for the incoming merge against
`1e0af39`. The pending bounds change separately passed five comparisons against
`a2ed097`. The OFF source paths remain identical; an executable i386
byte-for-byte comparison is unavailable on this arm64 host.

### Merged-tree visual result and remaining gaps

Fresh CoD2x Toujane intro colour checks pass with **magenta fraction 0** in both
modes. Default approximation mean RGB is `[52.821,48.485,39.434]`; automatically
selected licensed ARB mean is `[43.858,42.489,38.468]`. Logs are
`E/integration-final/rgb-{default,arb}.log`; screenshots are in
`H/rgb-merged-{default,arb}/main/screenshots/ws15-rgb-regression.jpg`.

Re-captured the same courtyard, rooftop and clamped sky cameras on the merged
CoD2x tree, with the local shader cache and **no D3D_PROG opt-in or shader
overrides**; one-frame draw tracing recorded evidence. Other settings match
the table above. Comparison still uses all
1280x720 RGB pixels, without adjustments, against the existing matched Windows
references. Inspected all three new Before/After/Windows panels.

| View | Merged MAE (0–255) | Merged RMSE (0–255) | Evidence under `E/integration-final` |
| --- | ---: | ---: | --- |
| Courtyard | **2.0050** | 3.8825 | `courtyard-merged-panels.png`, `courtyard-merged-metrics.json` |
| Rooftop | **1.8726** | 4.7769 | `rooftop-merged-panels.png`, `rooftop-merged-metrics.json` |
| Sky | **0.5856** | 0.9951 | `sky-merged-panels.png`, `sky-merged-metrics.json` |

Native images: `S/ws15-merged-tree-{courtyard,roof,sky}.jpg`. New traces:
`E/native-merged-tree-{courtyard,rooftop,sky}.jsonl`. The driver log is
`E/integration-final/capture-driver.log`, engine log `E/ws15-merged-tree.log`.
This capture run reached all views and `quit` exited **0**, with the merged VM
shutdown fixes. The Windows reference images were reused, not re-generated.

| Earlier gap | Current verified status |
| --- | --- |
| ARB crash while firing | **Unresolved.** Fresh grounded Sten firing crashed on the first shot in `RB_EndSurface`, called by `RB_SetLightPropertiesCmd`; invalid address `0x3f8000003f800020`. Impact-buffer fixes remain useful but are insufficient. `E/integration-final/grounded-firing-crash.txt`, `combat-driver.log` and `E/ws15-merged-combat.log` preserve the failure. Pre-merge `H/cod2_crash_47601.txt` has the same fault/stack, so this failure predates the integration. No driver-only cause or safe-gameplay claim is made. |
| FX primitives | **Native dispatch/lifecycle restored:** nine classes, scheduled effects, cloud buffers and grenade/projectile events pass production ASan/UBSan fixtures. Logs `fx_primitives.log`, `fx_cloud.log`, `fx_events.log`, `impact_marks.log`. Current grounded smoke/frag validation could not proceed past the firing crash. Full live combat FX parity remains unverified. |
| Post-processing | **Partly repaired, full parity unverified:** native HUD matrices, ARB blending/rasterization, texture extents and gamma presentation are implemented. Complete channel mixing/bloom and sun/shadow parity remain outside the matched views and are not claimed resolved. |
| zPAM white HUD box | **Local texture incompleteness resolved.** Fresh production CGL mip tests pass one-level, partial and full chains (`texture_mips.log`). The earlier real zPAM screenshot `S/ws15-pam-final-hud.jpg` was inspected again and has transparent compass corners. A new online zPAM session was not run. |

The orchestration merge should take the full branch, including `da2e4bd` and
the fixture repairs. It already contains main through `64bb6ad`; no new shared
header or top-level CMake edits were introduced by this final step. Retain the
external licensed shader cache configuration for the matched ARB appearance.
The firing crash is the concrete remaining gameplay blocker; default ARB
selection must not be mistaken for stability certification. No 333 FPS claim.
No packages were missing or installed. No game data, binary, decompiler dump or
imagery was added to git; no sibling worktrees, remotes, pushes, PRs or issues
were touched.
