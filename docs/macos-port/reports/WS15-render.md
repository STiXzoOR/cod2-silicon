# WS15 — rendering parity

2026-10-03. Worktree `/Users/stix/Projects/cod2-native-wt/render-parity`, branch
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
