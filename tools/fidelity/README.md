# Fidelity audit

Run from this worktree's root with Python 3 and the existing Xcode tools. No
packages, network access or i386 execution are needed. The private Mac 1.3
binary is read at runtime; no disassembly, AST dumps or binary bytes are saved.
Output JSON contains function names, addresses, counts, literal references,
source locations and comparison signals. Keep output and caches outside git.
Strings containing code, formatting or long text are represented by hashes and
byte counts; short dvar names, localized keys and asset paths remain readable.

```sh
python3 -m unittest discover -s tools/fidelity -v
export FIDELITY_EVIDENCE="$HOME/Library/Application Support/CoD2-native-ws44"
python3 tools/fidelity/audit.py \
  --compile-commands build-macos/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/stock.json"
python3 tools/fidelity/audit.py \
  --compile-commands build-macos-codx/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/codx.json"
python3 tools/fidelity/tables.py "$FIDELITY_EVIDENCE/stock.json" \
  --output "$FIDELITY_EVIDENCE/tables-stock.json"
python3 tools/fidelity/calibrate.py "$FIDELITY_EVIDENCE/stock.json"
python3 tools/fidelity/spotcheck_reverb.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/reverb-stock.json" --expect-difference
python3 tools/fidelity/spotcheck_msg.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/message-stock.json" --expect-difference
python3 tools/fidelity/spotcheck_collision.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/collision-stock.json" --expect-difference
```

`--binary` overrides the reference path. `--jobs` defaults to two compiler
processes. AST caches contain summaries only and are keyed by compiler command,
source, headers/includes and extraction implementation. Use the actual Release
engine compile database, before test runners replace `-DNDEBUG` with `-UNDEBUG`.
An unreadable binary or failed TU is a failure, not an empty inventory.

The reference inventory includes every named `N_FUN`, checked against `nm -ap`.
Aliases/overloads can share an address. `otool -tV` supplies symbolic calls;
`-tv` avoids misleading symbolization of numeric immediates, and both variants
also inspect `__textcoal_nt`. C++ member names map to the reconstruction's
`Class_Method` spelling; constructors map to `Class_Class`. Unmatched or ambiguous
names remain visible. Import stubs normalize to API names. Direct native calls
come from clang ASTs with the real build flags; function arguments are not
mistaken for callees. Bounded native reachability (four call edges) accounts for
extracted helpers and original inlining. This can hide differences and still
produce boundary-related false positives. Indirect calls are counted, not
resolved.
`_impl` names, recognized `Reg` ABI bridges and encoded template callee names
normalize to their reference identity. An extracted implementation of the same
function contributes its literals and branches to its wrapper's comparison.

Scores of eight or more are triage flags. Missing engine callees carry the most
weight; absent floating literals and strings are weaker; reduced branch count
is weaker still. Tiny original empty functions do not gain a stub score simply
because ours is empty. No name in the required calibration list affects scoring.
Native-only helpers report mismatching reference ancestors, explicitly labeled
`no-standalone-reference`, rather than inventing a reference function address.

Dvar recovery tracks straight-line i386 cdecl stack/register assignments and
records unknown arguments as null. Branches invalidate the state. Names,
types, defaults, limits and flags are compared only where both sides have
resolved values. Dynamic names, indirect renderer registration, enum tables,
path-dependent registrations and computed defaults need manual review. Missing
recovery is not proof a dvar is absent. Literal differences also need review:
compiler folding, SSE negation masks and helper boundaries affect these sets.

`tables.py` compares referenced scalar arrays field by field using the STABS
layout and actual arm64 build objects. It preserves float bits and accounts for
widened `long` and struct padding. Missing/ambiguous storage, unsupported layouts
and pointer-bearing tables are explicit outcomes. Use the existing
`tools/datagen/roundtrip_test.sh` relocation gate for typed pointer tables;
that gate proves the generator preserves its inputs, not that all source
initializers reproduce Mac 1.3. Runtime-populated/BSS tables need differential
harnesses.

The seed-44 sample selects 30 flagged and 30 unflagged owned `src/PC` functions,
excluding bundled Speex/zlib/JPEG and functions outside `src/PC`. PC sound and
PunkBuster adapters remain eligible. For review, inspect the original
with `otool -arch i386 -tV -p <symbol> <binary>` and read the native function and
its helpers. A review JSON has `flagged` and `unflagged` arrays with exactly the
sample's `name`/`address` pairs, a `verdict` (`difference`, `equivalent`, or
`unresolved`) and a nonempty `note`. `calibrate.py --reviews <file>` validates the
sample and reports resolved precision plus conservative lower/upper bounds.
Unresolved cases never silently become true positives. “Equivalent” means no
difference found in this inspection, not a proof of all inputs.

## New-stub gate

```sh
python3 tools/fidelity/audit.py \
  --compile-commands build-macos/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/stub-stock.json" \
  --stub-gate tools/fidelity/stub-baseline.json
# Repeat with build-macos-codx/compile_commands.engine.json.
```

This mode needs no private binary. It fails on additional empty bodies,
unconditional zero returns, output zeroing or comments containing `stub`,
`simplified`, `TODO`, `approximate` or `placeholder` in active engine/platform
function bodies. Source paths, function names and pattern sets are counted;
line moves do not fail, and duplicate occurrences cannot borrow one exemption.
The initial baseline freezes existing debt and legitimate small functions; it
is not an approval of their fidelity. Removing existing candidates passes.
Review a new candidate's reference behavior and reachability before adding a
specific entry. Never regenerate the baseline to make a failing change pass.
`--write-stub-baseline` is only for the initial snapshot.

The reverb harness extracts the current production definitions, compiles them
with actual engine flags and types, and captures their AIL wet-level output.
Its expected cases come from manually verified Mac 1.3 branches at `0x56878`
and `0x568fa`. `--expect-difference` characterizes the current known regression;
omit it for a fidelity acceptance check (currently fails). It leaves only JSON
observations, removes its temporary source/executable, and needs no game data.

The message harness reads `kbitmask` from the reference binary and the actual
arm64 object, then compiles the production `MSG_ReadDeltaKeyBits` body with real
engine flags/types. It probes each field width with a differing key bit and a
wire value restricted to that field's width, as the production writer does.
Only JSON observations remain. Repeat both harnesses with `--build
build-macos-codx`; omit `--expect-difference` to require fidelity.

The collision harness compiles the production `cm_trace.c` and trace-extents
setup with actual engine flags/types. Its five point rays use a synthetic
temporary model; expected blocked/clear results follow the rounded model
geometry verified in Mac 1.3's `CM_Trace` branch at `0x6498c` to `0x64e2a`.
The rays stay in the middle cylinder and far from collision epsilon boundaries.
This is an analytic geometry oracle, not execution of the reference binary.
Unused world/terrain dependencies are isolated; this probe establishes no claim
about terrain contacts. Repeat it for both builds and omit `--expect-difference`
to make the current two corner-contact differences fail acceptance.
