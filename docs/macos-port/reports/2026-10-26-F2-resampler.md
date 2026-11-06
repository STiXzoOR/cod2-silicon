# F2 screenshot resampling

Native levelshot and savegame screenshots now allocate enough memory for the
source image and both resizing passes. Upsampling replicates the last source
pixel in its spare expansion space, and levelshots serialize BGR pixels.

## Reconciled behavior

All changes are in `r_screenshot.c`, behind `COD2_X64`:

- `R_TakeResampledScreenshot` allocates using the maximum source/destination
  extent on each axis. The reconstructed minimum comparisons underallocated
  normal 1280×720 captures resized to 128×128 levelshots or 512×512 savegame
  JPEGs. Mac 1.3 at `0x000e9022`–`0x000e902f` uses `CMOVNS` to select each
  larger extent, then multiplies both extents and bytes per pixel and adds the
  header size at `0x000e9033`–`0x000e9039`.
- `R_UpsamplePixelData` replicates the last source pixel immediately after the
  source span. The reconstruction wrote one pixel past the destination span
  instead. Mac `0x000e8c87`–`0x000e8ca5` writes through the source pointer plus
  sample stride. Its center-based interpolation and per-axis byte rounding
  remain unchanged.
- Native upsampling uses an integer index for the clamped-left sentinel,
  avoiding formation of a C pointer before the source allocation. The final
  reverse steps in the sample, row, and column loops are skipped once no
  iteration remains. Downsampling bounds source advances by sample count,
  including vertical-stride advances that would pass the allocation end, and
  skips the final source load when its weight is zero. Original
  `0x000e8edf`–`0x000e8efe` reads that sample even for a zero contribution,
  which can be one past the image. These changes are native memory-safety
  adaptations with unchanged pixel arithmetic, not claimed Mac discrepancies.
- `R_LevelShot` converts RGB readback to BGR before writing its TGA. Original
  Mac readback supplies BGR, while native readback intentionally supplies RGB
  to the platform JPEG encoder. This is the same serialization boundary
  mismatch fixed for ordinary TGA screenshots in `204fb19`.

Evidence comes from the full-STABS Mac binary and its private instruction
export, checked against fresh symbol-scoped `otool` output. No reference
source or images were copied into the repository.

Both commands are normally reachable: `R_ScreenshotCommand` dispatches
`levelshot` to 128×128 resampling and `savegame <name>` to 512×512 resampling.
The fixtures call that actual command dispatcher for these two cases.

## Verification

The existing screenshot fixture now includes the full production translation
unit and covers 12 cases. Synthetic GL pixels vary across both axes and all
channels. Expected resizing uses independent rational pixel-bin overlap for
shrinking and pixel-center coordinates for expansion, with Mac's float/byte
rounding after each axis. Direct small fixtures separately expose the invalid
upsample write and zero-weight downsample read. Existing ordinary TGA, JPEG,
and failed-readback assertions remain active.

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_screenshot.py \
  --compile-db /Users/stix/.codex/worktrees/metal-u1-integration/cod2-native/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-resampler.noindex/stock-final"
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_screenshot.py \
  --compile-db /Users/stix/.codex/worktrees/metal-u1-integration/cod2-native/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-resampler.noindex/codx-final"
python3 tests/fixes13/legacy.py --base 204fb19
git diff --check
```

Stock and CoD2x: **12/12 ASan/UBSan cases pass**, including 1280×720→128²,
1280×720→512², expansion, both mixed-axis directions, same-size capture,
allocation/header extent checks, and failed-readback cleanup. With
`--baseline 204fb19`, both modes produce eight expected failures and four
passing controls. Savegame hits a heap-buffer-overflow during readback;
levelshot's more distant out-of-allocation write produces an ASan write fault.
The direct endpoint fixtures independently report stack-buffer-overflow write
and read failures. No test expectation was weakened to obtain green results.

`unifdef -UCOD2_X64` plus `cmp` gives byte-identical source against `204fb19`;
ten legacy preprocessor configurations also report zero mismatches. This is
source preservation, not Linux i386 object identity. No full game build, live
capture, real GL context, or VM run was performed by this unit.

## Preserved quirks and remaining scope

Original `R_LevelShot` leaves the TGA descriptor at bottom origin despite
readback's top-first pixels. This patch preserves that descriptor and row
order; the fixture explicitly checks both. Original `R_TakeResampledScreenshot`
reads into the allocation base, then resamples from base plus header; native
already reads into base plus header. This patch preserves the existing native
header placement and does not introduce the original leading-pixel shift.
Neither quirk is mislabeled as newly established image parity.

Cubemap storage, square readback dimensions, and cubemap color interpretation
remain a separate bounded follow-up. This report does not close the entire
screenshot module. Evidence and all logs stay in `F2-resampler.noindex` and
the earlier `F2-screenshot.noindex/resampler-diagnosis` directory.

## Integration and review

Final full client and dedicated builds pass in both configurations. Live runs
execute the actual levelshot and savegame commands after the stance sequence,
exit zero, and produce decoded 128×128 TGA and 512×512 JPEG images. The JPEG
was visually inspected and is upright with correct colors. The TGA visibly
retains the original inverted descriptor/row relationship documented above;
this is not presented as an upright levelshot fix. Evidence is shared with
`F2-client-input.noindex/live-open-{stock,codx}` and
`resampled-live-dimensions.json`. No original-runtime image equality is claimed.

The five-lens simplify pass removed one redundant comment; proof assertions
were retained. Focused correctness and independent Claude Opus 5.5 review
completed with zero actionable findings:
`/tmp/compound-engineering-501/ce-code-review/20261026-095602-f447db7c/`.
Its two noted fixture gaps were then covered additively: 640×480→512²
(non-integer expansion on one axis) and 1×1→3×5. Existing expectations and
production source were unchanged. Both modes pass all 14 groups, recorded in
`review-extra-stock` and `review-extra-codx`. No actionable review fix remains.
