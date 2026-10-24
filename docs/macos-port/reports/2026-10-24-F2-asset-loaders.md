# F2 light and model surface loaders

Base: `06275b3`. Behavior changed: **yes**, for native light-load failure.

`R_LoadLightDef` now returns NULL after the requested file is missing, including
`default`. Previously it retried `light_dynamic` for that name. Fallback belongs
to light registration; the loader must report failure to its caller. Mac 1.3
`R_LoadLightDef` at `0xfb7e8` performs one file read; the negative-length branch
at `0xfb816` goes directly to the NULL return at `0xfb95d`. The zero-length
branch frees the buffer before returning NULL. The native bounds checks remain.

The same bounded comparison checked type and sampler bytes, both image names,
image registration arguments `(1, 5)`, native allocations, lowercased definition
names, buffer release, and return values. Those already match the Mac behavior
for valid records after accounting for pointer width.

`XModelReadSurface` (`0xc7318`) was compared through the end of its body at
`0xc7737`: allocation and memory accounting, rigid/skinned marker handling,
bone bitsets and 64-byte bone offsets, all vertex fields, base and extra blend
weights, stream advances, triangle allocation, and odd-triangle padding.
No change was needed. An odd triangle count deliberately duplicates the final
index three times and increments the surface triangle count. Index allocation
is not added to `memUsage`, matching the original. Native `sizeof(XSurface)`
allocation remains intentional.

`r_xmodelsurfs_load.c` supplies v60 adapter functions, not separately named Mac
1.3 entrypoints. Its alternate surface decoder has the same valid-record field
writes and stream layout as `XModelReadSurface`; its `GfxVertex` offset names
are misleading but resolve to the correct `XVertexInfo` byte positions. Its
import-copy and material-registration bridge retain the native pointer arrays
and callback slots. No claim is made that those adapters are original Mac
functions. No adapter code changed.

## Proof

Existing tests inspected: `tests/lp64/renderer/run.sh`, the native renderer
fixture runners, and the existing WS6 loader report. There was no focused
light-loader or surface-loader fixture in this base. Existing lighting fixtures
exercise different entrypoints and cannot prove the loader failure contract.

Added `light_loader.c` and `run_light_loader.py`. The runner includes the actual
production source, uses each engine compile database's flags, and enables
ASan/UBSan. Synthetic records exercise missing default, missing other name,
empty file, both image registrations and sampler fields, empty image names,
and the preserved rejection of a truncated record. Allocation, file release,
name normalization, and callback arguments are checked.

From this worktree, the exact commands were:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_light_loader.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-asset-loaders.noindex/red" \
  --baseline 06275b3
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_light_loader.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-asset-loaders.noindex/stock"
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_light_loader.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-asset-loaders.noindex/codx"
```

Baseline: `missing-default` aborts at `reads == 1` after the second read; the
other five cases pass. Final stock and CoD2x: all six cases pass under both
sanitizers. Logs are in the private output directories and their sibling
`red.log`, `stock.log`, and `codx.log` files. Existing two integer-narrowing
compiler warnings remain unchanged.

`unifdef -UCOD2_X64` output for the sole modified engine source,
`r_light_load_obj.c`, compares byte-identical with `06275b3` using `cmp`.
Private `before.off`, `after.off`, and `off.log` retain that result.
`git diff --check` passes. Neither surface source was modified.

## Limits

No full build, live game, VM, original executable run, or capture comparison
was performed in this unit. The Linux object gate remains unavailable; OFF
source comparison does not establish object identity. The unchanged surface
decoders were instruction-compared, not newly runtime-tested. Invalid surface
counts and bone indices remain outside the original valid-asset contract.

Considered and not built: another loader fallback, extra malformed-surface
policies, and refactoring the adapter's offset names. The first contradicts
the original failure path; the others are unnecessary for this bounded fix.
The registration change owned by the integration agent must remain in place
when integrating this loader correction.

Changed paths: `src/PC/gfx_d3d/r_light_load_obj.c`,
`tests/lp64/renderer/light_loader.c`,
`tests/lp64/renderer/run_light_loader.py`, and this report.
