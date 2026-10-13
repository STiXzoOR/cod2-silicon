# WS94: read map lighting from its actual fields

2026-10-07. The user reported yellow-green tint on models in Toujane. A private
one-translation-unit rendering comparison confirms that restoring the map
lighting field reads removes the visible patches on barrels and sandbags.
The scoreboard rectangles are a separate WS93 defect.

`R_LoadWorld` first advanced its world pointer to `sunParse`, then cast that
interior pointer back to `GfxWorld` and applied the field offset again. Its
subsequent offsets into `sunParse.name` also did not correspond to the actual
lighting fields. The routine consequently fed unrelated values into the seven
lighting dvars before rebuilding sunlight.

The owned Mac 1.3 `R_LoadWorld` at `0xe1d3c` forms one sun-parse base. It reads
ambient scale at relative 0x40, diffuse fraction at 0x50, sunlight at 0x54,
ambient RGB at 0x44/48/4c, sun RGB at 0x58/5c/60, diffuse RGB at 0x64/68/6c,
and direction at 0x74/78/7c. These correspond to the existing named
`SunLightParseParams` fields. The native repair reads those fields directly;
the legacy path is unchanged. No lighting constants, shader instructions,
layouts, dvar defaults, or color transforms are altered.

`git log -S` places the faulty name-relative expression at initial commit
`63de35a`, predating this readiness wave.

## Verification

The new `tests/fixes13/world_lighting.c` passes distinct synthetic values
through the actual extracted map-to-dvar production block and records all
seven setter destinations. Before the fix, 15 scalar/color/direction components
disagree with independent literals. Stock and CoD2x ASan fixtures pass after
the repair. All five native-off source comparisons against `3c6bd89` pass;
this is supporting preprocessing evidence, not i386 binary equivalence.

Private stock and CoD2x overlays replace only `r_bsp.c` in frozen `f5b83ad`
builds, use a fresh home and read-only licensed data/cache, and run the same
map, view, and graphics settings as the baseline. Both complete with exit zero.
Stock before/after images show the yellow-green model patches disappear.
Both corrected runs report sunlight RGB approximately (1, 0.902, 0.761) and
ambient scale 0.02, compared with the faulty stock values (0, 0, 0.502) and 1.
These are observations of this map, not new hard-coded defaults.

The matching capture and overlay manifests are machine-local under
`~/Library/Application Support/CoD2-native-current-readiness/ws94-map-lighting.noindex`.
The baseline is the sibling `ws93-render-state.noindex/stock-scoreboard` run.
Synthetic tests contain no original binary or map payload. Screenshots,
licensed data, original disassembly, and runtime logs remain outside Git.

Independent review confirmed the original field offsets and all seven calls,
reran both sanitizer fixtures, and reproduced the 15 baseline failures. It
reported no blocking findings. Its machine-local receipt is under
`/tmp/compound-engineering-501/ce-code-review/ws94-map-lighting-independent`.

This bounded repair does not establish complete visual parity or close all
maps, weather, shadow, bloom, lightgrid, or material behavior. Independent
integration verification remains separate from these receipts.
