# F1b: Mac 1.3 collision traversal

Branch: `decomp/f1-cmtrace`. Starting commit:
`4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7`.

Restored native brush-node descent and the brush clipper it calls. The wider
`cm_*.c` comparison also found and fixed disabled partition construction,
terrain edge/border errors, a point-contents sentinel omission, and entity
sight-hint truncation. All engine changes are inside `COD2_X64` guards.
The existing WS52/WS61 rounded temporary-model and dedicated sight algorithms
remain in place. There are no CoD2x-specific collision overrides in this batch.

## Authority and changes

Authority was the owned Mac 1.3 i386 binary, inspected with `nm`, `otool`, and
its private Ghidra export as a reading aid. Linux 1.3 source helped identify
inlined functions and understand the data flow. Implementation was written
from the Mac instructions and geometric reasoning; no reference source,
disassembly, decompiler text, game data, or generated game content is committed.

| Mac evidence | Result |
| --- | --- |
| `CM_TraceThroughLeafBrushNode_r`, `0x62fe0..0x632fc` | Contents filtering, immediate child for negative counts, split distance/range, empty gaps, near-first four-component interpolation, parallel threshold, `fraction <= p1.w` pruning, iterative far descent. Removed the depth-64 cutoff. |
| Brush clipper inlined at `0x632fc..0x637a2` | Min XYZ before max XYZ; cached axial reciprocal products; entry epsilon without an exit epsilon; incoming result fraction as the exit limit; delayed sloped-plane division; capsule support; first equal entry plane wins. Preserve startsolid/allsolid, contents, and material writes in their actual order. |
| `CM_TraceThroughLeafBrushNode`, `0x63ae2`; leaf code in `CM_TraceThroughTree`, `0x63f90` | Expanded-leaf slab cull, original endpoint fraction values, fraction-zero early-out, and brush-before-terrain call structure. BSP fraction pruning uses `<=`. |
| `CM_TestInLeafBrushNode_r`, `0x626b4..0x628fc` | Original iterative one-sided position descent, immediate child, split equality, and allsolid early-out. Removed the depth cap. Sloped support keeps the Mac addition/subtraction order. |
| `CM_Trace`, `0x64587..0x64662`, `0x646e7`, `0x64a4c`, `0x64b43` | Restore scalar Z-bound order, volume-based `axialCullOnly`, stationary `isPoint = 0`, existing-allsolid position early-out, and the incoming fraction in the world segment's fourth component. |
| `CMod_PartionLeafBrushes_r`, `0x77f0a..0x784cc` | Keep the selected axis instead of forcing `-1`; score wholly-below brushes correctly; initialize each node; partition above/below into the proper children; carry the minimum range across both children. |
| `CM_LoadMapFromBsp`, including `0x78f23..0x78f7d` | Model radius uses the furthest absolute bound. Zero-brush submodels still accumulate terrain contents. CollisionPartition allocation uses the native structure size (24 bytes here); the disk record and Mac i386 structure remain 12 bytes. |
| `CM_PositionTestCapsuleInTriangle`, `0x668e2..0x669d2`; `CM_TraceCapsuleThroughTriangle`, `0x66eec..0x67298` | Finite-edge position/projection uses `axis[2]`. Swept edge motion uses a dot product rather than a determinant. Overlap returns immediately, and the successful axial segment overlap writes its result. |
| `CM_TraceThroughAabbTree_r`, `0x677c7..0x67c9c`, especially `0x67ae0` | Test the second cap for negative triangle Z normals. Process borders after triangles, including mixed partitions. Borders require horizontal movement and a nonzero capsule axial segment. Restore endpoint quadratics, full delta length, strict positive roots, and the original-root Z test when the reported fraction clamps to zero. |
| `CM_PointContents`, `0x69223..0x69228` | Return zero for absent brush-node index zero. The apparent recursive stub is unused; the live `_impl` already performs the traversal. |
| `CM_PointSightTraceToEntities_r`, `0xaf888..0xaf8cf`; `CM_ClipSightTraceToEntities_r`, `0xafcd0`; split arithmetic at `0xafe64` / `0xb0664` | Propagate the full signed entity/child hit hint, including `-1`, instead of returning boolean `1`. Swept entity split fractions use reciprocal-then-multiply. |
| `CM_SortNode`, `0xb07b4..0xb07ef` and corresponding static-model branches | Allocate a child using the caller's bounds, matching the Mac sector split and size threshold. |

Mac `__literal4` values were read directly: `0.125` at `0x3261c8`,
`2^-21` at `0x326258`, `16` at `0x326260`, and terrain barycentric tolerances
at `0x326228`/`0x32622c`. No x87 literals were substituted.

The private reconciliation table covers **111 native bodies** in all nine
`cm_*.c` files: 84 equal local contracts, 20 fixed, six intentional native
helpers/defensive paths or unused compatibility code, and one Mac/Linux
service difference. `CM_LoadMap` initializes one trace-thread record on Mac
(`0x7394d..0x73a0e`); Linux initializes MAIN and DATABASE records. The Mac
contract is retained. Compiler-inlined Linux functions are paired with their
owning Mac symbol. The table is at the evidence path below, not a committed
per-function report. “Equal” means reviewed geometry/control flow for valid
finite inputs, not an executable i386 equivalence claim.

## Differential corpus

`tests/collision/maps.py` uses the production BSP loader, real BSP leaf
enumeration, production math, brush and terrain collision. Licensed IWDs are
read-only; extracted BSPs and captures stay in its required private output.
No game process or writable game home is involved. The harness excludes
static XModels and dynamic entities; separate synthetic world fixtures cover
the changed sector and hint contracts.

Each map runs 120,000 deterministic queries: 20,000 each of point sweeps,
capsule-shaped `CM_BoxTrace` sweeps, point sight, capsule sight, point contents,
and capsule position tests. All result fields are compared exactly, with
material pointers normalized to indices. Both sides use the same corrected
loader so partition construction does not confound trace comparison.

| Map | Point sweep deltas | Capsule sweep deltas | Capsule sight deltas | Position deltas | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| mp_toujane | 2,397 | 7,951 | 63 | 17 | 10,428 |
| mp_carentan | 2,645 | 5,663 | 144 | 49 | 8,501 |
| mp_dawnville | 2,820 | 5,764 | 99 | 48 | 8,731 |
| Total | 7,862 | 19,378 | 306 | 114 | **27,660 / 360,000** |

Point sight and point contents have zero deltas. The entire corpus completes
under ASan/UBSan without a diagnostic. Intermediate production builds attribute
every differing query to these changes:

| Changed stages | Queries |
| --- | ---: |
| Brush clipping only | 15,111 |
| Traversal/setup only | 2,315 |
| Mesh/empty-contents only | 1,047 |
| Brush + traversal/setup | 8,358 |
| Brush + mesh | 129 |
| Traversal/setup + mesh | 87 |
| All three | 613 |

These stages correspond to the inspected Mac contracts above: capsule
support/intervals/reciprocal rounding and plane ties; visitation/fraction
pruning and contents overwrite order; terrain edge/cap/border behavior.
For example, a synthetic near brush that contains the start must write its
contents before the far brush's actual impact, so that the impact's contents
survives. The old traversal reverses that result. Axial reciprocal products
also explain single-ULP fraction deltas. Attribution identifies which change
caused each difference; it is not execution of the original binary.

The independent geometric regression fixtures retain near/far contents
priority, equal axial material ties, deep descent, sloped capsule support,
finite edges, both border endpoints, vertical endpoint motion, exact-zero
roots, overlap, and original-root Z after a fraction clamp. Existing WS52/WS61
fixtures remain in the combined gate. The old five-ray spotcheck needed only
its production `CM_TraceBox` link dependency added; its cases and analytic
expectations are unchanged.

Release elapsed query times were 0.561768 -> 0.338920 seconds (Toujane),
0.464283 -> 0.316965 (Carentan), and 0.542507 -> 0.362819 (Dawnville).
ASan/UBSan times were 1.583215 -> 0.769040, 1.111660 -> 0.870795, and
0.971369 -> 0.788217 respectively. Both runs produce the same delta counts.
The lease was held, but **the screen was locked** and other agents were active.
These compare tracers on the corrected partition tree, not the original
loader's flat-tree cost. They are exploratory, not a production performance claim.

## Verification

Evidence: `~/Library/Application Support/CoD2-native-decomp/F1b.noindex/`.
The map summaries are `maps-final-asan/summary.json` and
`maps-release/summary.json`; compressed private captures preserve all query
results. The Release run also writes per-query difference attribution CSVs. `reconciliation.md` contains the private comparison table.

Commands run with background task policy, nice 19, and at most two compiler
jobs. The focused collision runner now runs all five suites by default.

```sh
cmake --build build-macos --target cod2_macos --parallel 2
cmake --build build-macos-codx --target cod2_macos --parallel 2
python3 tests/collision/run.py --build build-macos
python3 tests/collision/run.py --build build-macos-codx
python3 tests/collision/run.py --build build-macos \
  --baseline 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7
python3 tests/collision/maps.py --build build-macos \
  --baseline 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7 --queries 120000 --sanitize \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1b.noindex/maps-final-asan"
python3 tools/fidelity/spotcheck_collision.py --build build-macos \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1b.noindex/spotcheck-stock.json"
python3 tools/fidelity/spotcheck_collision.py --build build-macos-codx \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1b.noindex/spotcheck-codx.json"
python3 tests/fixes13/legacy.py --base 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/fidelity -v
git diff --check
```

Final results:

- Both complete native client builds: exit 0.
- Five assertion/ASan/UBSan collision suites per mode: zero failures. The same
  expected-value suites against the starting source fail all five suites,
  with 36 assertions (10 trace, six loading, 14 mesh, five world, one contents).
- Existing analytic capsule spotcheck: 0/5 differences in both modes.
- Function ABI: nine collision translation units per mode; zero compiler
  errors, mismatches, or unprototyped floating calls. Raw-offset audits: nine
  sources, 25 existing sites, zero unsafe/unreviewed entries. Import audits:
  14 native selected sites, zero proven missing/extra dereferences. Strict
  bit-conversion audit: zero violations in both modes.
- `unifdef -UCOD2_X64` followed by `cmp`: exact starting-source identity for
  `cm_trace.c` (28,924 bytes), `cm_mesh.c` (38,861), `cm_test.c` (8,948),
  `cm_world.c` (34,290), and `cm_load_obj.c` (44,871). The fixes13 comparison
  also passes ten legacy configurations. Include-free ABI preservation passes
  five inactive configurations; five actual SDK compile entries per mode
  also preprocess identically with the native definition removed.
- Tools tests: 32 pass. Fidelity tests: 35 pass. All four ABI checker unit
  suites and trace-guard check pass. `git diff --check` passes.

The exact scoped audit inputs are `{stock,codx}-collision-compile.json` in
private evidence, selected from the newly configured engine databases without
changing flags. Audit commands, each under `taskpolicy -b nice -n 19`:

```sh
ev="$HOME/Library/Application Support/CoD2-native-decomp/F1b.noindex"
for mode in stock codx; do
  python3 tools/abi/audit.py "$ev/$mode-collision-compile.json" --jobs 2 \
    --baseline tools/abi/baseline.json --output "$ev/abi-collision-$mode.json"
  python3 "$ev/limit_jobs.py" tools/abi/raw_offsets.py "$ev/$mode-collision-compile.json" \
    --output "$ev/raw-$mode.json"
  python3 "$ev/limit_jobs.py" tools/abi/imports.py \
    --compile-commands "$ev/$mode-collision-compile.json" \
    --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
    --json "$ev/imports-$mode.json" --check
  python3 tools/abi/astdump_strict.py "$ev/$mode-collision-compile.json" --jobs 2
  python3 "$ev/limit_jobs.py" tools/abi/legacy.py \
    --base 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7 \
    --compile-commands "$ev/$mode-collision-compile.json"
done
```

`limit_jobs.py` is a private runner that caps existing checker thread pools at
two workers; no checker rules or baselines were edited.

## Limits and merge notes

The original Mac i386 code was inspected, not executed. Original-server replay
from F0 remains required: player clipping at brush/bevel/terrain edges,
grenade bounce fractions and normals, bullet material/hit selection, sight
checks and hit-hint reuse, and moving submodel contacts. Static XModel asset
loading and dynamic-entity gameplay were not exercised on real maps.
The three-map seeded corpus is not an exhaustive map or floating-point oracle.

The integrated full-engine gate is not claimed green. A broad ABI audit was
stopped after 100/396 translation units; completed scoped audits cover all nine
collision translation units in each mode. The renderer callback-table checker
could not run on that scoped database because it requires `r_init.c`; its
attempts exit 1 with `StopIteration`, not a callback pass. No callback binding
was changed. No full movement/network/rendering
or GUI smoke suite was run. The known global `legacy-abi` ten differences and
CoD2x inactive-gate 436 differences remain release limitations. The inactive
CoD2x checker against this unit's starting commit reports 42 differences:
seven configurations per six modified C files (five engine files and the
existing collision fixture). These are shared native fixes with CoD2x off,
or configurations defining `COD2_X64=0` that still select `defined(COD2_X64)`;
they must not be represented as a passing CoD2x isolation gate.
Actual Linux i386 object/binary comparison is unavailable. Inactive source
identity supplements that missing comparison and does not replace it.

No remotes, user game home, game data, shader cache, or global settings were
changed. No GUI game or app bundle was launched. Merge the signed local commit;
run the orchestrator's integrated gates and F0 replay before claiming full
original-engine parity.
