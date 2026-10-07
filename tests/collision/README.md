# Native collision reference fixtures

The fixture executes the complete active `cm_trace.c`, `cm_mesh.c` and
`cm_tracebox.c` production sources with synthetic models, brushes and terrain.
Its expectations are independently established Mac 1.3 geometry and state
contracts documented in [WS52](../../docs/macos-port/reports/WS52-collision-fidelity.md).
It does not launch the game or require game data.

Use an existing native stock or CoD2x engine compile database:

```sh
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos-codx
taskpolicy -b nice -n 19 python3 tests/collision/run.py --build build-macos \
  --baseline 0dab850021da929bfc0a36727d02daf54a4fb1b8
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

Thread-local storage and leaf enumeration are synthetic services.
`CM_BoxLeafnums` returns fixture leaves and captures the requested bounds;
the fixture verifies the reference's full box plus one-unit enumeration
expansion, but does not validate the real BSP enumeration routine. The math
normalization service is ordinary Euclidean normalization. Mesh culling,
partition checks, triangle overlap and material propagation run in production.
The fixture uses a triangle interior and does not establish terrain edge or
vertex fidelity. No sight-trace, transformed trace, live player collision,
network prediction or map coverage is claimed.
