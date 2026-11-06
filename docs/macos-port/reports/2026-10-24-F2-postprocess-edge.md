# F2 post-processing target edge — 2026-10-24

The remaining dark bottom strip under strong blur came from an uncovered row
in each intermediate filter target. Native projection adds a half-pixel shift;
the filter quad still began at `(0,0)`, putting its upper GL boundary exactly on
the last pixel center. Rasterization excluded that row. The first pass copied
black into the target texture, the next pass sampled it, and final composition
exposed it at the bottom of the screen.

## Change and authority

`rb_imagefilter.c` now starts native Apple filter quads at `(-0.5,-0.5)` in both
Gaussian and glow pass loops. The existing projection shift then places their
bounds on whole-pixel edges, covering the complete target. All source UV
extents, weights, pass ordering and final-composition orientation remain the
same. Global projection, 3D sample alignment and HUD geometry are unchanged.

Mac 1.3 `RB_DrawStretchPic` at `0xd8148` writes the supplied corner positions and
adds the requested width and height (first corner at `0xd824d`, second corner
at `0xd82cd`). `RB_Set2D` at `0xd5d72` constructs the viewport-sized transform
and calls the original projection converter. The converter at `0x254f6`
uses small negative device-specific offsets (`-0.15/-0.075`, or `-0.3/-0.3`),
not the native `+1/-1` clip-coordinate numerator. The original full-target
filter geometry therefore does not put its upper boundary exactly on the
excluded sample centers. This fix compensates the intentional native projection
adaptation; it does not restore an old GPU-specific offset globally.

## Reproduction and verification

Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/F2-postprocess-edge.noindex/`.
The worktree began clean at `dfa30ec` on `decomp/f2-postprocess-edge`.

- A production projection CGL probe showed a completely unpainted top row at
  320x180, 321x181, 640x360 and 1280x720. The bottom row was painted.
- A private observational device build recorded framebuffer pixels immediately
  before saved-screen and auxiliary-target copies. It changed no rendering
  behavior and restored pixel-pack state. At 1280x720 with `r_blur 2`, the saved
  scene had nonzero edge rows. The first 320x180 filter target had top-row mean
  RGB 0.0; the second had rows 178/179 at 19.35/0.0. This located the first bad
  boundary before texture copy or final composition. Samplers already selected
  clamp-to-edge, so adding UV clamps would not address the cause.
- Strengthened `imagefilter.c` records draw origins and checks compensation in
  the existing native-extent case. Before the production edit, that case failed
  while the other seven cases passed. The new `imagefilter_raster.c` uses actual
  production filter draws and the production projection converter in a real
  CGL context. Its baseline failed at pixel `(0,179)` in the first 320x180 blur
  pass. The fixture uses synthetic solid-color geometry; it proves raster
  coverage, not licensed shader sampling or original runtime parity.
- After the fix, all eight image-filter cases and raster coverage for blur/glow
  at the four even/odd extents pass under ASan/UBSan in stock and CoD2x builds.
  The optional `--raster` check is local because hosted CI need not provide CGL;
  the ordinary runner continues to check draw origins without a GL context.
- The live fixed client on `mp_toujane` resolves the first target top row at
  mean RGB 39.23 and the second target rows 178/179 at 39.07/39.23. The final
  screenshot bottom-center row rises from 0.07 to 37.64 under strong blur;
  clear and glow-control rows remain about 38. Weak blur also loses the dark
  strip. The inspected fixed image is full-size, upright, blurred through the
  bottom edge, with sharp HUD. Baseline port 29576 and fixed port 29577 both
  exit normally with code 0 and release the game lease. No combat cycle was
  repeated for this focused reproduction.
- `unifdef -UCOD2_X64` plus `cmp` against `dfa30ec` is exact for the only changed
  engine file, `rb_imagefilter.c`. `git diff --check` passes. This source check
  does not substitute for the unavailable Linux object-identity gate.

Run from this worktree, substituting stock or CoD2x compile database:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_imagefilter.py \
  --raster \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-postprocess-edge.noindex/green-stock"
```

Use `build-macos-codx` and a distinct output directory for CoD2x. The baseline
check can be reproduced with `--baseline dfa30ec` and a fresh output directory.
Private `probe-build.py`, `fixed-build.py`, `run.py`, and the results in
`baseline/`, `fixed/`, `readbacks/`, and `fixed-readbacks/` preserve the live
reproduction. Root build objects were read-only at `dfa30ec` during private
linking and released before integration rebuilt them.

## Limits

This is a bounded native filter-target coverage fix. No original Mac runtime
capture is available, and whole-wrapper or whole-renderer parity is not
claimed. The general pixel-centers fixture and saved-screen/final-post-effect
fixtures remain unchanged. No UV clamp, sampler fallback, shader modification,
new runtime option, shared-header change or allocation policy was added.
Only task-private probe code contains observation switches. No proprietary
captures, binaries, shaders or disassembly are included in Git.
