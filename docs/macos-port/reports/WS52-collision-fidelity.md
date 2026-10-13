# WS52: temporary capsule and stationary collision fidelity

Date: 2026-10-07. Starting revision:
`0dab850021da929bfc0a36727d02daf54a4fb1b8`, clean
`codex/collision-fidelity`. This is the bounded FA15a follow-up to WS44.
Production ownership is `src/PC/qcommon/cm_trace.c` plus one coordinated
native fraction correction in `src/PC/qcommon/cm_mesh.c`; movement,
shared headers, layouts, generated data and build configuration are unchanged.

The change restores rounded temporary-model traces and dedicated stationary
brush/mesh position testing under `COD2_X64`. The original WS44 five-ray probe
now agrees with the analytic Mac geometry for all five rays in each build.
It previously differed at `(9,9)` and `(-9,9)` because an AABB was used for
the radius-10 capsule. This does not establish whole-game collision parity.

## Authority and implementation contracts

Authority: the external owned Mac 1.3 STABS binary
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The original i386 virtual addresses below identify independently inspected
control flow. No original binary was executed, and no third-party collision
implementation was copied. Temporary disassembly remained outside the
repository and was removed after inspection.

| Mac fact | Native restoration |
| --- | --- |
| `CM_Trace` tests equality of all three start/end components (`0x646ae..0x646c2`, `0x64b04..0x64b24`) and enters its position branch before swept tracing. | Equal endpoints use dedicated position testing for temporary models, submodels and the world. |
| World position enumeration uses centered full box extents expanded by one unit (`0x646f7..0x647af`), up to 1024 leaves. | The same bounds and list capacity are passed to `CM_BoxLeafnums`; leaves stop after all-solid. |
| Leaf brush bounds use strict overlap; position calls occur at `0x6488e` and `0x64abf`. Brush node traversal selects children by bounds relative to the split, and a negative count owns an immediate child (`0x626ce..0x62749`). | Dedicated leaf/node traversal uses those contracts with typed node indexing. It retains a depth-64 guard, consistent with the existing native traversal's defensive limit. |
| Position brush tests cull axial bounds, then expand each nonaxial plane by `radius + abs(offsetZ * normal.z)` (`0x6277d..0x628ba`); overlap sets contents and solid flags (`0x628c4..0x628d5`). | Stationary brush support uses the capsule geometry instead of the previous generic box support. Solid brush results preserve the reference's lack of normal/material writes. |
| Stationary terrain calls `CM_MeshTestInLeaf` (`0x64afa`, `0x651a3`); its material propagation is visible at `0x683af..0x683bd`. | Position traces call the existing production mesh tester, with contents filtering and brush all-solid early exit. |
| Triangle-interior position overlap clears the fraction register at `0x6688c`, branches at `0x668ab` and writes zero with both solid flags at `0x66a60..0x66a6b`. | The existing mesh helper's `s0+t0` result is replaced by zero for native builds only. Exact-zero assertions cover submodel and world stationary terrain hits. |
| Temporary model `0x3ff` uses model-bound center, radius `min(halfWidthX, halfHeightZ)` and axial offset `halfHeightZ-radius` (`0x64c46..0x64ca0`, `0x64f8c..0x64feb`). | Stationary and swept temporary models use that rounded capsule geometry. Moving capsule radius and axial offset contribute to the combined contact shape. |
| Temporary position endpoint-sphere and middle-cylinder tests are strict (`0x64d0a`, `0x64d33`, `0x64d6b`, `0x64d94`, `0x64e0d`). | Position overlap uses strict squared-distance comparison to the combined capsule; exact tangency clears. Like the reference, temporary position overlap sets flags/fraction without writing contents. |
| Temporary sweep chooses sphere/cylinder branches from the two axial segments (`0x65060..0x652aa`); moving sphere/cylinder start overlap is inclusive (`0x63c61`, `0x650da`). | Rounded endcaps, middle-cylinder sweeps and start overlap replace the generic temporary brush sweep. |
| Sphere contact backs the quadratic root up by `separationLength*0.125/projection`, uses the initial separation normal and writes temporary brush contents (`0x63cec..0x63d5b`). Cylinder uses horizontal separation, horizontal delta squared, and validates the axial span at the unshifted root (`0x6537c..0x654bc`). | Sphere/cylinder fractions, initial normals, zero fraction clamp and contents use those contracts. Normalization calls the existing `Vec3NormalizeTo` API. |

Two non-ideal reference behaviors are deliberate. Temporary stationary tangency
clears while swept start overlap is inclusive. Also, when the swept start lies
inside the middle cylinder, Mac sets `allsolid` from the end's axial containment
alone (`0x652b1..0x65377`, particularly `0x65362..0x65373`). It does not retest
the end's radial distance. The fixture asserts the observed result for
`(9,0,0)` to `(20,0,0)`, preserving that behavior rather than substituting an
idealized capsule escape test.

The restored terrain call exposed the existing interior-fraction mistake in
`cm_mesh.c`: a stationary all-solid triangle hit returned its barycentric sum
(0.2 in the fixture). Strengthening the fixture to require Mac's exact zero
first reproduced two failures in the otherwise corrected stock implementation.
The parent approved the single native-only mesh correction after the independent
reference check; no other mesh algorithm was changed.

## Focused regressions and evidence

The [fixture README](../../../tests/collision/README.md) documents reproduction
and mocked boundaries. `tests/collision/run.py` inherits the actual native
client collision compile command, enables assertions and ASan/UBSan, and
compiles complete production `cm_trace.c`, `cm_mesh.c` and `cm_tracebox.c`.
It halts on UBSan errors. All compilation used `taskpolicy -b nice -n 19`;
fixture compilation is serial and engine compilation used at most three jobs.

Expected values are independently specified geometric equations and state
contracts from the Mac branches above. Coverage includes the five original
rays, both endcaps, axial and oblique sweep fractions/normals, point/sphere/
capsule shapes, stationary exact tangency, contents rejection, lateral escape,
shifted model bounds, asymmetric moving bounds, diagonal/sloped stationary
brushes, split and immediate brush children, strict leaf boundary culling,
world enumeration bounds, and stationary terrain material/solid results.

| Acceptance run | Stock | CoD2x |
| --- | ---: | ---: |
| Same assertions against starting production revision | 63 failures, exit 1 | 63 failures, exit 1 |
| Corrected production fixture, assertions + ASan/UBSan | 0 failures, exit 0 | 0 failures, exit 0 |
| WS44 five-ray analytic capsule spotcheck | 0/5 differences | 0/5 differences |

The WS44 spotcheck gained only synthetic stationary service and normalization
definitions needed by the restored production call graph; its five rays and
analytic oracle are unchanged. Those service stubs do not provide position
coverage. The new fixture exercises actual mesh culling, partition checks,
triangle overlap and material propagation; its thread storage and leaf
enumeration remain synthetic. It checks a triangle interior, not every edge or
vertex case. The tests execute native production source with current unchanged
headers, not original linked i386 artifacts or an i386 emulator.

Local evidence: `output/ws52-stock` and `output/ws52-codx` contain selected real
engine databases, scoped audit results and isolated Release objects. Fixture
copies/executables are under the existing ignored `output/ws47-stock` and
`output/ws47-codx` build directories. External summaries/logs use the prefix
`~/Library/Application Support/CoD2-native-ws52-`. No game data or proprietary
dump was added to git.

## Verification and limits

- Both changed engine translation units compile with actual stock and CoD2x
  Release commands, including `-ffp-contract=off`; both exit 0.
- Scoped function ABI audits include the trace, mesh and tracebox production
  translation units in each mode: three TUs, zero compiler errors, zero
  mismatches/new signatures and zero unprototyped floating calls per mode.
- Raw-offset audits in both modes: three sources, two selected sites, zero
  unsafe/unreviewed entries. Import audits in both modes: three native selected
  sites, zero proven missing/extra dereferences. Strict bit-conversion audits
  report zero violations in both modes. No raw-offset approval was added.
- Inactive source output is byte-identical to the starting source after
  `unifdef -UCOD2_X64`: `cm_trace.c` 28,924 bytes, `cm_mesh.c` 38,861 bytes.
  The include-free legacy checker passes both files across five configurations;
  actual native client SDK commands with the native definition removed pass
  two comparisons per mode. `tests/fixes13/legacy.py` passes both files across
  ten legacy configurations. These are supplemental source-preservation
  checks. Actual Linux multilib i386 object/binary comparison was unavailable
  and was not run; no original artifact preservation claim is made from
  preprocessing alone.
- `git diff --check` passes. No full engine link/build, game process, live
  collision/playtest, benchmark, install, publication or shared configuration
  change was performed.

FA15b remains open and untouched: `CM_BoxSightTrace` still needs independent
contracts and fixtures for its old-hit hint, specialized leaf/tree traversal
and rounded sphere behavior. Moving world/submodel brush traversal and support
calculations remain the pre-existing implementation; this wave does not claim
their fidelity. Transformed traces, broader terrain geometry, deep BSP trees,
contents combinations, live player contacts and prediction/server parity need
separate coverage or release validation. WS43 movement files were not edited;
its collision boundary remains a synthetic movement service, so its frame
tests do not establish the real map-collision behavior restored here.
