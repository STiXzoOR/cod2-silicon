# WS61: bounded sight-trace fidelity

Date: 2026-10-07. Starting revision:
`380bf2b08f85beabfd04897e292bd3a9fce12457`, clean
`codex/sight-fidelity`. This follows the completed [WS52 FA15a
restoration](WS52-collision-fidelity.md) and addresses the FA15b contracts
identified in [WS44](WS44-fidelity-audit.md).

Native `CM_BoxSightTrace` now returns the original brush/terrain hit hints,
uses dedicated leaf and BSP sight traversal, and tests rounded temporary
models. The change also corrects the shared native `CM_TraceBox` interval
cull needed by the restored leaf path. Production ownership is
`src/PC/qcommon/cm_trace.c` and the separately approved native-only correction
in `src/PC/qcommon/cm_tracebox.c`. The FA15a swept/position helpers and
`cm_mesh.c` are unchanged. This is bounded source-fixture fidelity, not
whole-game collision parity.

## Independent authority and contracts

Authority is the external owned Mac 1.3 STABS binary
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The SHA, sight symbol addresses and scalar constants were independently
read from the actual Mach-O sections. Private disassembly was inspected
outside the repository. No original binary was executed or emulated, no
third-party collision implementation was copied, and no disassembly or
game-derived payload was added to git.

| Mac fact | Native restoration |
| --- | --- |
| `CM_BoxSightTrace` is at `0x6551e`; it calls specialized leaf/tree sight functions at `0x65836`/`0x65899`. | World sight traces walk production BSP nodes; submodels call the dedicated leaf function. They do not enumerate leaves or derive their result from ordinary trace fractions. |
| Brush sight clipping at `0x628fe..0x62b97` uses radius offsets for axial bounds, `radius + abs(offsetZ * normal.z)` for nonaxial support, an interval initially `[0,1]`, and no swept-plane epsilon. It returns brush index plus one. | Dedicated brush clipping preserves exact entry/exit comparisons and stable brush hints. Contents filtering belongs to traversal callers. |
| A positive world hint whose index is below `numBrushes` is tried first at `0x6585d..0x65876`/`0x65c25..0x65c44`. This direct call has no contents-mask check. | A still-blocking valid brush hint wins before tree traversal, including the observed contents-mask bypass. Clear/stale, negative and terrain hints fall back to traversal. Submodel and temporary-model paths ignore hints. |
| Leaf brush nodes at `0x62b9a..0x62e8e` filter node/brush contents, process a negative-count immediate child first, then split using `size[axis] + 0.125 - range`. A gap can select neither child. | Typed node indexing preserves immediate-child priority, split/gap selection, near/far order and early hit return. |
| Leaf sight at `0x62e94..0x62fda` first culls brushes using leaf bounds expanded by the full moving size; terrain calls `CM_SightTraceThroughAabbTree`. Terrain hits return `numBrushes + treeIndex + 1`. | Dedicated leaf sight preserves brush-before-terrain priority, production mesh calls, leaf/tree material masks and the terrain hint range. |
| BSP sight at `0x637a4..0x63adc` uses axial `size + 0.125`, diagonal point offset `0.125`, diagonal nonpoint offset `2048`, and parallel threshold `2^-21`. It visits the near child first and stops on any hit. | The sight tree carries three-component segment endpoints and those split contracts, independently of the ordinary trace tree and its fraction pruning. |
| Setup at `0x65713..0x65771` sets `axialCullOnly` when `size.x * size.y * size.z * 16 * deltaLen` exceeds swept capsule-bounds volume. | Native sight setup restores that mesh-culling flag. The Mac constants `0.5`, `0.125`, `16`, `2048` and `2^-21` were checked directly. |
| Temporary sight at `0x658e1..0x65e47` filters contents, culls against model bounds plus one, derives center/radius/axial offset from model bounds, and tests the selected upper/lower sphere or middle cylinder. A block returns `-1`. | Rounded temporary sight supports moving points, spheres and capsules, including translated/asymmetric bounds and inclusive start tangency. It uses the existing FA15a geometry derivation without reusing its trace fractions. |
| Sphere sight at `0x63dfc..0x63f12` adds `projection * 0.125 / initialSeparationLength` to the quadratic root. The cylinder at `0x65cb6..0x65d9c` uses full `deltaLenSq`, including Z, and checks the axial span at the root before the bias. | Separate sight sphere/cylinder calculations preserve both non-ideal reference behaviors. |
| `CM_TraceBox`, `0xbb3f6..0xbb4cf`, switches min/max plane sign from `-1` to `+1`, maximizes entry, minimizes exit, rejects entry at/after exit, and returns overlap after both passes. Exit need not equal the requested fraction. | The approved native helper correction restores both signs and interval comparisons. Its independent slab oracle exercises the actual production function. |

Two reference behaviors deserve explicit examples. For the temporary radius-10
capsule, `(20,0,0)` to `(11,0,0)` stops one unit short of geometric contact,
but sight blocks: its quadratic root is `10/9` and its bias is `-1.125`.
The ordinary swept trace remains clear, as required by FA15a's different
bias. For `(20,0,-9)` to `(0,0,9)`, sight's full-length cylinder discriminant
is negative (`160000 - 724*300`), and neither cap fallback is selected;
sight clears. An idealized horizontal cylinder sweep would block this ray.
The fixture preserves these inspected Mac branches rather than replacing
them with ideal capsule intersections.

The restored leaf cull initially exposed the existing `CM_TraceBox` error:
both bounds used the min-plane sign, and an early exit was treated as a
cull. That affects direct entity culling consumers in `cm_world.c`,
`sv_world_mp.c` and `xmodel.c` as well as the new sight leaf path. Their
source call sites were inspected; no production consumer was edited.
They use the same overlap result and requested fraction contract. Live
entity/model coverage remains a release-validation requirement.

## Fixtures and verification

The [collision fixture](../../../tests/collision/README.md) executes full
active production `cm_trace.c`, `cm_mesh.c` and `cm_tracebox.c` with the real
engine compile flags, assertions and ASan/UBSan. Compilation is serial under
`taskpolicy -b nice -n 19`; UBSan halts on errors. Native fixture additions
are gated with `COD2_X64`, preserving their inactive source as well.

Coverage includes temporary rounded sides/endcaps, point/sphere/capsule
shapes, inclusive sight tangency, independent sphere/cylinder bias cases,
axial/nonaxial capsule brush support, entry/exit endpoint contacts, contents
mask filtering and the old-hint bypass, stable brush and terrain IDs,
world/submodel hint behavior, axial/diagonal BSP selection, near/far and
parallel traversal, brush-node ranges/gaps/immediate children, leaf bounds
culling, brush-before-terrain priority, and production terrain triangle
interior tracing. The shared box-cull fixture adds explicit clipped fractions
and reversed rays plus 2,000 exact comparisons against independent
double-precision slab intersections.

The initial expanded corpus reproduced **169 failures per mode** against
the starting production revision. The first reconstruction reduced that to
26 per mode. Of these, 24 came from my incorrect box-cull entry update,
which the unchanged independent slab oracle caught; the Mac SSE register
flow and a separate parent review confirmed the proper maximum-entry rule.
Two assertions came from an incorrect fixture expectation that overlooked
the independently inspected upper-cap fallback. Those expectations were
corrected from the reference branches. The final corpus adds further
nonaxial, moving-sphere, BSP boundary and terrain-priority cases.

| Final acceptance run | Stock | CoD2x |
| --- | ---: | ---: |
| Final assertions against starting production revision | 184 failures, exit 1 | 184 failures, exit 1 |
| Corrected production fixture, assertions + ASan/UBSan | 0 failures, exit 0 | 0 failures, exit 0 |
| Independent slab comparisons | 2,000/2,000 agree | 2,000/2,000 agree |
| Both changed TUs, actual client Release commands | exit 0 | exit 0 |

The function ABI audits cover three collision TUs per mode and report zero
compiler errors, mismatches/new signatures and unprototyped floating calls.
Raw-offset audits cover three sources and two existing sites per mode,
with zero unsafe/unreviewed entries. Import audits cover three selected
native sites per mode and report zero proven missing/extra dereferences.
Strict bit-conversion audits report zero violations. The raw gate initially
classified typed `node + 1` as a byte-offset candidate because a legacy
byte-pointer prototype shares the parameter name; the immediate child uses
the existing FA15a typed spelling `&node[1]`. No approval entry was added.

`unifdef -UCOD2_X64` output is byte-identical to the starting production
source: `cm_trace.c` 28,924 bytes and `cm_tracebox.c` 1,439 bytes. The
include-free legacy checker passes all three changed C files in five
inactive configurations; both actual production SDK commands with the
native definition removed pass per mode. The fixes13 legacy check passes
the two production files across ten configurations. `git diff --check`
passes. These focused results preserve the earlier FA15a fixture contracts.

Local evidence lives under
`~/Library/Application Support/CoD2-native-ws61-sight.noindex/`.
The original real databases are
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/{stock,codx}/compile_commands.engine.json`;
the private selected copies remap their checkout paths to this worktree.
Scoped audits select the three client collision translation units; their
scope must not be confused with the parent's composed engine gates.

## Limits

Thread storage and the FA15a leaf-enumeration boundary remain synthetic.
The sight traversal itself runs in production on shallow synthetic BSP and
brush trees, and all world sight cases require zero leaf-enumeration calls.
Mesh tracing runs production culling, partition checks and triangle-interior
tests; broader terrain edge/vertex/border geometry is unchanged and not
established by this wave. The corpus does not prove transformed sight,
deep/large real BSPs, malformed metadata, arbitrary contents combinations,
live prediction/server parity, rendering visibility, performance or map
coverage. No full engine link/build, game process, benchmark, install,
publication, staging or commit was performed by this worker.

Actual Linux multilib i386 object/binary comparison was unavailable and was
not run. Inactive source checks are supplemental preservation evidence,
not an i386 artifact-parity claim. Structural audit/approval manifests,
shared layouts, generated data, build flags and feature/configuration
surfaces are unchanged.
