# Local ARB shader translation

This standalone native setup library prepares locally extracted programs for a
future Metal backend. It does not select a renderer or change game call sites.
No original or translated shader is distributed. Nothing in the legacy build
references these sources.

```sh
sh tools/metal/run.sh
# Explicit cache/destination (both licensed cache and derived output stay external):
sh tools/metal/run.sh \
  "$HOME/Library/Application Support/CoD2x Native/shaders" \
  "$HOME/Library/Application Support/CoD2-native-ws24/evidence/translated"
```

The cache has **417 ARB programs plus 417 CTAB metadata files**, not 834
programs. Setup verifies all manifest digests, metadata bounds and pairs,
translates each program, and uses `newLibraryWithSource` to compile both its
render entry and a compute entry for arithmetic testing. Missing cache prints
`SKIP` (library/CLI exit 77; test runner treats that as a skip). Invalid cache
or any translation/compilation failure returns failure. Setup refuses derived
outputs beneath a repository containing the port plan, including symlink paths.
It never installs the optional offline Metal Toolchain.

`src/Mac/Metal/arb_to_msl.{h,cpp}` separates parser, register IR and MSL emitter.
`shader_setup.{h,mm}` exposes a C-callable first-run/loading API independent of
Python. The app backend must call it before play and build linked pipelines;
this workstream deliberately does not wire it into the GL launcher. The current
cache uses `program.env` and `state.fog.color`; local parameters and address
registers also have synthetic parser coverage. State bindings are an ordered
IR list: supply each corresponding `float4` at `ARBConstants.state[slot]`.
CTAB remains authoritative for engine name/register reflection.
Setup releases its validation libraries after compilation and caches MSL source.
The backend must load/retain functions and create every pipeline during loading;
this source cache is not a persistent GPU pipeline archive.

MSL ABI: packed `ARBInput` at buffer 0; `ARBConstants` at buffer 1;
`ARBOutput` at buffer 2 for tests; individual typed texture and sampler at their
original stage index. The renderer will replace packed vertex input with
declaration-specific stream fetching. Vertex wrapper shifts original ARB clip
depth `(z + w)/2` to Metal's depth range; the compute test leaves outputs raw.
Fragment wrapper returns color and implements discard. Raster features such as
alpha test, depth output (unused in this cache), blend/fog/sRGB policy and linked
varying specialization belong to the backend workstreams.

Only the observed instruction family is a production coverage claim. The
precision hint is accepted, but full float arithmetic and disabled fast math
are intentional. `NRM` and the legacy undeclared `r*_abs` spelling translate to
normalization and absolute value; Apple GL rejects three representative original
programs using these (17 across the full cache), so their GL semantic equivalence
is **unverified**.
The 16 undeclared-absolute-value programs also use `DP2ADD`, which Apple GL
rejects independently. Synthetic oracles compare these three inferred operations
against valid ARB arithmetic sequences; those checks do not resolve the original
program rejections.
The original undefined-program behavior is not a visual parity target.
See [WS24 report](../../docs/macos-port/reports/WS24-metal-design.md).

GPU differential testing:

```sh
taskpolicy -b nice -n 19 clang++ -std=c++17 -fobjc-arc -ffp-contract=off \
  -Wno-deprecated-declarations -framework Foundation -framework Metal \
  -framework OpenGL tests/metal/semantic.mm -o build-macos/metal/semantic
python3 tools/metal/check_semantics.py --help
python3 tools/metal/check_semantics.py --self-test --limit 0
# The same original probes run with no licensed cache:
python3 tools/metal/check_semantics.py --self-test --cache /nonexistent
# Strict all-program comparison (currently returns failure for 17 GL rejections):
python3 tools/metal/check_semantics.py --all
```

The runner selects every distinct stage/instruction set from the local cache;
all generated MSL, diagnostics and results must remain in its external output
directory. A GL rejection or tolerance mismatch fails the strict run; it is not
silently skipped. Independently written GPU self-probes need no game data.
This verifies sampled shader arithmetic, not renderer/golden-image parity,
implicit derivatives or mip selection in a rasterized material.
