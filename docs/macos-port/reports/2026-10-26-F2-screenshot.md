# F2 native TGA screenshot channels

Ordinary native TGA screenshots now serialize BGR pixels. The framebuffer
readback continues to supply RGB to the native JPEG backend. This fixes the
red/blue reversal found while comparing the saved reference viewpoints.

## Cause and Mac evidence

`R_GetFrontBufferData` reads `GL_RGBA` / `GL_UNSIGNED_BYTE`, reverses the row
order, and copies channels 0, 1, 2 into its three-byte output. The native
`R_SaveJpg` passes that RGB data to `MacJpeg_Encode`, so JPEG is correct.
`R_CaptureScreenshotNow` previously copied the same RGB bytes directly into
an uncompressed 24-bit TGA. TGA readers interpret those bytes as BGR.

The original Mac 1.3 `R_GetFrontBufferData` at `0x000e87f4` instead requests
`GL_BGRA` (`0x80e1`) with `GL_UNSIGNED_INT_8_8_8_8` (`0x8035`), set at
`0x000e899b`–`0x000e89a3`. On little-endian i386 that packed readback has
A, R, G, B bytes. Its three-byte copy at `0x000e8ad0`–`0x000e8ae1` reads
source offsets 3, 2, 1, producing BGR. `R_ScreenshotCommand` at `0x000e9d96`
sets 24-bit pixels and the top-origin descriptor at `0x000ea0f1`–`0x000ea0f5`
and writes that readback directly. The private Ghidra instruction export and
fresh `otool` output from the full-STABS Mac binary agree on these operations.
No reference source was copied.

The fix converts RGB to BGR immediately before ordinary TGA serialization,
behind `COD2_X64`. Changing shared readback would instead break the native
JPEG input contract. The source mismatch was already present in the initial
repository reconstruction; no previous screenshot regression test covered it.

## Verification

The new fixture includes the entire unchanged production translation unit
apart from the fix, with synthetic GL readback and allocation/file/JPEG output
boundaries. Its asymmetric 2×2 pixels check channels and vertical orientation,
the complete TGA header and size, unchanged JPEG input, and no file write on
both back/front readback failures. No replacement screenshot algorithm is
compiled in the fixture. Unused engine paths are discarded by the linker.

From the unit checkout, with the canonical integration compile databases:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_screenshot.py \
  --compile-db /Users/stix/.codex/worktrees/metal-u1-integration/cod2-native/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-screenshot.noindex/stock"
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_screenshot.py \
  --compile-db /Users/stix/.codex/worktrees/metal-u1-integration/cod2-native/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-screenshot.noindex/codx"
python3 tests/fixes13/legacy.py --base 4d51bcf
git diff --check
```

Both modes: **3/3 ASan/UBSan cases pass**. Against `4d51bcf`, both modes fail
the TGA red-channel assertion; JPEG and failed-readback controls pass.
`unifdef -UCOD2_X64` output compared with `cmp` is byte-identical to the
pre-change source. Ten legacy preprocessor configurations report zero
mismatches. These checks do not establish Linux i386 object identity.

No game, VM, or actual GL context was launched by this unit. Fresh integration
captures and complete builds remain the orchestrator's acceptance checks.
Private evidence is under `F2-screenshot.noindex`; no game images are committed.

## Remaining screenshot module work

This is an ordinary TGA fix, not a completed reconciliation of `r_screenshot.c`.
Inspection found additional paths that need their own reference-backed tests:

- `R_LevelShot` writes the same RGB readback to TGA without a channel conversion,
  and its header has bottom origin despite readback's top-first rows.
- `R_TakeResampledScreenshot` allocates using the smaller source/destination
  extents, then reads the entire source image into that buffer. A normal
  1280×720 to 128×128 levelshot therefore needs further investigation of the
  original allocation and resampling rules before this command is safe.
- Cubemap state is still declared as a single `int cubeShotGlob`, then treated
  as an array of pointers in `R_EndCubemapShot` and `R_SaveCubemapShot`.
  The end-shot readback also passes the pixel border as image height. Its
  four-channel output and TGA serialization need reconciliation together with
  that state and crop contract. These are source findings, not runtime proofs.

## Integrated acceptance and review

Both full client and dedicated builds pass in stock and CoD2x after integration,
including the reliable-command review correction. A first build invocation
requested the nonexistent `cod2_dedicated` target after building each client;
the corrected invocation built `cod2_macos` and `cod2_macos_ded` successfully.
Fresh CoD2x captures from two separate client runs now decode correctly as TGA
without channel adjustment. The courtyard TGA/JPEG mean absolute difference
is 1.176 on the 0–255 RGB scale, consistent with separate lossy JPEG encoding.
See the [capture comparison](2026-10-26-F2-captures.md) for reference metrics.

The five-lens simplify pass completed with no changes. Focused correctness and
independent Claude Opus 5.5 review completed with zero actionable findings:
`/tmp/compound-engineering-501/ce-code-review/20261026-093455-b6d9cdf1/`.
The final fixture is registered in CI for both modes. No review fix remains.
