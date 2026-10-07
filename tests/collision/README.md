# Native collision reference fixtures

The fixture executes the complete active `cm_trace.c`, `cm_mesh.c` and
`cm_tracebox.c` production sources with synthetic models, brushes and terrain.
Its expectations are independently established Mac 1.3 geometry and state
contracts documented in [WS52](../../docs/macos-port/reports/WS52-collision-fidelity.md)
and [WS61](../../docs/macos-port/reports/WS61-sight-trace-fidelity.md).
It does not launch the game or require game data.

Use an existing native stock or CoD2x engine compile database:

```sh
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos-codx
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos \
  --baseline 0dab850021da929bfc0a36727d02daf54a4fb1b8
# Sight-trace follow-up baseline, after the FA15a restoration:
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos \
  --baseline 380bf2b08f85beabfd04897e292bd3a9fce12457
```

The runner prefers `compile_commands.engine.json`, falls back to
`compile_commands.json`, remaps its source root to this checkout, enables
assertions and ASan/UBSan, and halts on UBSan errors. Compilation is serial.
Generated copies and executables stay under the chosen build directory.
`--baseline` applies the same assertions to historical production sources
using the current unchanged headers and compile flags. It must fail on the
pre-fix revision; it does not execute the original i386 binary.

Coverage includes the original five middle-cylinder rays; stationary and
swept rounded corners/endcaps; point, sphere and capsule shapes; asymmetric
and translated bounds; exact stationary tangency; axial and oblique sweep
fractions/normals; start-solid/all-solid semantics; contents masks; stationary
diagonal/sloped brush capsule support; leaf boundary/split selection; and
stationary world/submodel terrain overlap with actual production mesh tests.
Sight coverage includes rounded temporary point/sphere/capsule contacts,
inclusive sight tangency, translated/asymmetric bounds, separate sight contact
bias and cylinder discriminant behavior, axial/nonaxial capsule brush support,
entry/exit endpoint comparisons, brush hit identifiers, contents masks,
world old-hit hints and fallback, axial/diagonal BSP near/far traversal,
leaf brush splits/gaps/immediate children, leaf bounds culling, and world/
submodel terrain hit identifiers. The shared production `CM_TraceBox` helper
is tested against 2,000 independent double-precision slab intersections plus
explicit clipped-fraction and reversed-ray cases.

Thread-local storage and leaf enumeration are synthetic services.
`CM_BoxLeafnums` returns fixture leaves and captures the requested bounds;
the fixture verifies the reference's full box plus one-unit enumeration
expansion, but does not validate the real BSP enumeration routine. The math
normalization service is ordinary Euclidean normalization. Mesh culling,
partition checks, triangle overlap and material propagation run in production.
The fixture uses a triangle interior and does not establish terrain edge or
vertex fidelity. Sight's BSP traversal executes in production on shallow
synthetic trees. The world sight checks require zero leaf-enumeration calls.
No transformed trace, live player collision, network prediction or map
coverage is claimed. The Mac binary is independently inspected as authority;
the tests do not execute or emulate its i386 instructions.
