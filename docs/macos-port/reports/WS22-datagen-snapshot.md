# WS22 — public LP64 typed-data snapshot

Branch: `port/datagen-snapshot`. Starting commit: `df08d50`.

## Implementation and merge notes

Five production files are tracked in `build/lp64_gen/`, following upstream's
existing generated-source convention. They preserve repository storage and
symbolic relocations, annotate LP64 types, and retain the three production
instances of the thirteen previously verified scalar recoveries. No private
binary, game asset, decompiler dump, coverage report or object is committed.

`cmake/datagen.cmake` now selects the snapshot when either private reference is
absent. With both references present it regenerates into the CMake build tree
(default `<build-dir>/x64_gen`), then compares all five files before compilation.
Mismatch is fatal and invalidates the primary output so the next build retries.
`COD2_REGENERATE_TYPED_DATA=ON` requires both references. The explicit
`COD2_UPDATE_TYPED_SNAPSHOT=ON` option refreshes only these five files; turn it
back OFF after reviewing the diff. It also requires both references. Standalone
generator scratch output remains ignored `build/x64_gen/`.

Only typed-data source debug paths are normalized, so the snapshot and
regenerated object files are byte-identical even across checkouts. Engine source,
headers, `CMakeLists.txt`, and original native/web/win/gfx generated artifacts
are unchanged. Both existing calls to this module remain inside `COD2_X64`
branches; the OFF source selection and flags are untouched. A complete i386
engine binary comparison is unavailable on this arm64 Mac (no i386 libc
sysroot/toolchain). The generator's actual i386 round-trip remains successful.

Shared-file edits are limited to this isolated CMake module and a short PLAN
paragraph. `cmake/macos-arm64.cmake` and `tools/cod2x/` are untouched for WS21.
The script storage fixture now finds its production BSS object through
`compile_commands.json`, supporting either source selection.

## Snapshot determinism and sizes

Run twice at background priority, into distinct directories:

```sh
taskpolicy -b nice -n 19 python3 tools/datagen/generate.py \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --output build/x64_gen/determinism-a
taskpolicy -b nice -n 19 python3 tools/datagen/generate.py \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --output build/x64_gen/determinism-b
for name in data_native.c literals_native.c import_pointers_native.c bss_native.c typed_types.h; do
  cmp "build/x64_gen/determinism-a/$name" "build/x64_gen/determinism-b/$name"
done
rg -n '/Users|stix|\$HOME|/home/|20[0-9][0-9][-/][0-9]{2}[-/][0-9]{2}|[0-9]{2}:[0-9]{2}:[0-9]{2}' build/lp64_gen
```

All five comparisons matched. The metadata scan returned no matches. Inspection
also found no input/output-directory interpolation in these five outputs; paths,
reference provenance and coverage remain in ignored diagnostic files.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `data_native.c` | 56,658 | `904ce5034bbe23d9bbb709c4099f0eecd4951f21171acf72f4c85efc6b2cbcd3` |
| `literals_native.c` | 154,534 | `fee0c3d3e1d46cddad4b1c637cbfde5d0d24cfd25e64ba86c07c19a814146b9a` |
| `import_pointers_native.c` | 285,678 | `1e94bd7895f51f363648c3a644390f4db5c070c33f39a05264a8bc7a91cf084c` |
| `bss_native.c` | 69,668 | `c09861ed4c756a5837b52f9531953350cdcf480e1997f0df0de474dd0b1dfe90` |
| `typed_types.h` | 393,411 | `7512ac6472733645dd99576288fca9aae25ad8fadfb7094eb070d3dfc9700f45` |
| Total | **959,949** | |

The standalone round-trip passed all twelve grammar/header/recovery tests,
i386 bytes/relocations/symbols, arm64 full/native/BSS probes, sixteen compiled
scalar recovery instances, LP64 address points, and fourteen deterministic
text files. All four actual snapshot and regenerated build objects passed
alignment checks: 26 + 30 + 880 + 0 aligned LP64 pointer relocations.

## Fresh clone without private inputs

Scratch clone and initially empty HOME live inside ignored worktree scratch:

```sh
scratch="$PWD/build/x64_gen/fresh"
mkdir -p "$scratch/home"
git clone --no-hardlinks --single-branch --branch port/datagen-snapshot \
  "$PWD" "$scratch/repo"
cd "$scratch/repo"
HOME="$scratch/home" taskpolicy -b nice -n 19 cmake -S . -B build-macos \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCOD2_STABS_BINARY=/nonexistent -DCOD2_VALUES_BINARY=/nonexistent
HOME="$scratch/home" taskpolicy -b nice -n 19 cmake --build build-macos \
  --target cod2_macos -j2
HOME="$scratch/home" taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 \
  -DCOD2_STABS_BINARY=/nonexistent -DCOD2_VALUES_BINARY=/nonexistent
HOME="$scratch/home" taskpolicy -b nice -n 19 cmake --build build-macos-codx \
  --target cod2_macos_app -j2
```

Both configurations printed the snapshot STATUS message and built successfully.
The app target built its CoD2x client and created an app under scratch HOME's
`Applications/`; `codesign --verify --deep --strict` passed. No licensed input
was available through either configured path or HOME. Python, Apple Clang/Xcode,
CMake, SDL2 compatibility and timeout were already installed; none was installed.
There is no stock app target in this base branch; the app target is feature gated.

The clone initially tested `ffad726`, then was fast-forwarded from this branch
to include the script fixture correction. Both normal Release builds regenerated
from real references and verified the snapshot. Every one of the four complete
Mach-O data objects matched its fresh-clone counterpart byte for byte, including
DWARF, in both stock and CoD2x builds.

Whole executables differ: stock 3,735,368 vs 3,753,768 bytes; CoD2x 3,809,000 vs
3,827,992 bytes. The ordinary engine still includes checkout paths in debug
symbols and a compiled shader-setup source path, plus `__DATE__`/`__TIME__` build
stamps. Comparing CString sequences after normalizing checkout paths found only
the build-time stamp differing (6,199 stock / 6,366 CoD2x strings). The one path
string is 25 bytes longer in the clone, changing later layout and linker fixups;
UUID/signature and symbol metadata also differ. This port does not promise a
reproducible entire executable; the typed payload objects are identical.

## Private dependency audit and skips

Searched `CMakeLists.txt`, `cmake/`, `tools/`, and `tests/` for
`cod2-native-refs`, `CoD2-mac-bin`, `Games/CoD2`, `COD2_VALUES_BINARY` and
`COD2_STABS_BINARY`. `scripts/` and `scripts/install.sh` are absent in this base
(WS20 is not merged). There is no other build-time private input. Packaging's
game-directory default describes runtime setup and does not read/copy assets
when making an app. Wine/macOS launch/combat helpers and stock rendering/asset
audits require licensed runtime data, outside the configure/build path.

The following reference checks now exit 0 with a specific SKIP message when
inputs are missing, and fail when an existing reference is invalid:

| Check | Missing input |
| --- | --- |
| `tools/datagen/roundtrip_test.sh` | Either STABS or scalar-value binary |
| `tests/fixes13/reference.py` | Either STABS or Steam binary |
| `tests/lp64/game/check_layouts.py` | STABS binary |
| `tools/abi/imports.py` | STABS binary |
| Retail-import stage of `tools/abi/check.sh` | STABS binary; source/callback audits still run |

All skips were observed with `/nonexistent` or empty HOME. Each direct check
also rejected a present text file masquerading as a Mach-O reference. Required
regeneration and snapshot-update configurations both failed clearly on missing
references; ordinary snapshot configuration and `cod2_datagen` succeeded.

## Verification suites and observations

All build/test/generator commands ran under `taskpolicy -b nice -n 19`.
Normal-build gate commands (logged under `/tmp/ws22-*`):

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --target cod2_macos -j2
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos -j2
python3 tests/fixes13/run.py --build build-macos-codx
sh tests/lp64/renderer/run.sh
python3 tests/lp64/game/run.py
python3 tests/lp64/script/run.py
python3 tests/lp64/script/vm_semantics.py
sh tests/cod2x/run.sh
python3 tests/perf/run.py
python3 tests/online/run.py build-macos/compile_commands.json
sh tools/abi/check.sh build-macos/compile_commands.json build-abi/ws22
sh tools/datagen/roundtrip_test.sh \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" build/x64_gen/roundtrip
```

The full fixes13 runner needs CoD2x compile flags: its timing test extracts
`Cod2x_LimitedFPS`, which is absent in stock builds. The stock run passed through
trajectory before stopping on that extraction; the full CoD2x run passed all ten
checks. This is an existing runner limitation, not a stock build failure.

Fresh-clone checks use empty HOME and the same background priority. Passing
suites include the gate's fixes13, 23 renderer fixtures, seven game fixtures,
script runtime/storage/compiler/VM, CoD2x core, performance and online fixtures.
Additional data-free checks passed: twelve datagen unit tests, fourteen
`tools/tests` harness tests, shader-cache manifest tests, Wine draw-trace parser
tests, IWD extraction tests, real local HTTPS demo upload tests, Release flags,
and three inactive-source guard checks against `df08d50` (zero changed source
files / zero mismatches).

The script runner's old hardcoded BSS path failed in both source selections;
the corrected compilation-database lookup passed in the normal and fresh clone.
The full CoD2x packaged URL probe previously invoked automatic licensed setup
and read actual Cocoa-home preferences (Cocoa home does not follow shell HOME).
Its synthetic fixture now disables automatic setup and re-signs the temporary
app, checking the explicit bundle game-directory metadata, path setter and raw
launch options. It no longer touches private first-launch state. The rest of
`tests/cod2x/run_full.sh`, including watchdog/crash/HTTPS tests, passes.

Under concurrent background builds, a first synthetic harness run intermittently
reported `Operation not permitted` during process-group cleanup; another
fresh-clone run missed its 0.3-second fake-program timeout. Both fourteen-test
suites passed on rerun. No harness timing policy was
changed. These remain possible load-sensitive test failures for WS20.

The ABI source selector previously used `'/build/' not in absolute_path`, so
running it inside this scratch clone silently selected zero translation units.
It now selects files under this checkout's `src/`, covering 620 engine TUs in
this clone and excluding generated data in either output location. Its four
real-Clang tooling tests pass. The 218-renderer-binding audit runs without
private input; only its retail import stage skips when STABS is absent.

Snapshot drift was exercised in the scratch clone's separate CMake tree, after
the data-free builds: an intentional comment change to `typed_types.h` failed
`cod2_datagen` with `Typed-data snapshot differs: typed_types.h`. Reconfiguring
with `COD2_UPDATE_TYPED_SNAPSHOT=ON` and rebuilding restored the original header
byte for byte. The scratch snapshot was restored; the main worktree snapshot
was never modified. Direct comparator checks also passed identical output,
rejected drift, invalidated failed generation and refreshed only the changed
file with `--update`.

Smoke: a background-priority Python driver checked `pgrep -fl cod2_macos` and
exact-name process inventory before starting its own client; it polls existing
games every 60 seconds when needed. It launched the CoD2x client under
`timeout -k 10 90`, with licensed `fs_basepath` read-only, a fresh ignored
`fs_homepath`, windowed 640×480 and `+devmap mp_toujane`. After CS_ACTIVE it sent
`screenshotJPEG ws22-snapshot`, waited for the JPEG, then sent `quit`. Result:
exit 0, map active, 640×480 JPEG. Driver/result/log/screenshot remain local under
`build/x64_gen/smoke.py` and `output/ws22/smoke/`. No other process was killed.

Ordinary client instruction/relocation disassembly also matched across all 385
engine objects between normal and fresh stock builds. The executable differences
described above are build stamps, checkout strings/debug symbols and resulting
link layout, not changed compiled engine instructions or typed data.

The first private ABI run used the old absolute-path selector and audited 628
TUs, including eight generated-storage instances after regeneration moved into
`build-macos/x64_gen`. It reported 19 mismatches, all generic `void(void)`
address-storage declarations in `typed_types.h` compared with real engine
function definitions. The old source-directory output had been excluded by
its `/build/` filter; snapshots were also excluded. Root-relative `src/`
selection restores consistent engine-call audit scope for both selections,
without changing either baseline or any engine prototype. Private verification
was rerun with the corrected source selector and the licensed STABS reference.

Final corrected ABI results: **620 TUs, 0 errors, 0 mismatches, 0 new**;
**218 renderer bindings, 0 table/named-cast/unprototyped-floating mismatches**;
**514 imported symbols, 1,776 active sites, 185 placeholders, 0 proven extra
and 0 proven missing dereferences**. Full private gate exit: **0**.

The corrected source/renderer gate first passed with empty HOME and skipped the
retail stage. It was then rerun in the same scratch checkout, using its warm
source-audit cache and an explicitly supplied licensed STABS reference:

```sh
reference="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
scratch="$PWD/build/x64_gen/fresh"
cd "$scratch/repo"
HOME="$scratch/home" taskpolicy -b nice -n 19 sh tools/abi/check.sh \
  build-macos/compile_commands.json build-abi/ws22-root-aware "$reference"
```

These are the same 385 distinct engine source files / 620 client+dedicated
compile entries as the normal build, verified byte for byte; the four data
objects were already verified identical to the private regenerations. The
retail-import stage is slow because it preprocesses all engine entries and
classifies line paths. Sampling our own audit process showed active Python
thread-pool/path/stat work rather than a stuck subprocess. This is existing
tooling cost, not a build dependency or a blocker; no timing shortcut or
baseline relaxation was applied.

## Remaining limits and handoff

No WS22 build blocker remains. Actual full i386 engine binary parity still needs
an external i386 toolchain/sysroot; this branch preserves its inputs and gates
and passes the standalone i386 data round-trip. Entire native executable bytes
remain sensitive to existing build stamps/debug paths. The stock fixes13 runner
needs CoD2x flags for its timing slice; the synthetic short-timeout harness can
fail under heavy background load and passes when rerun.

Merge the entire branch (implementation commits `80a4aaa`, `ffad726`, `2c46676`,
`ab2f5ea`, `0339d7c`, `201533c`, plus this report). Keep the five-file snapshot
and the root-relative ABI source selector together: generated-storage generic
prototypes were intentionally outside the original engine audit scope. WS21's
packaging/module files are untouched; its URL probe now explicitly exercises
the data-free bundle branch, while automatic first-launch setup needs separate
licensed-asset testing. WS20 can use the snapshot path with both private input
cache paths set to `/nonexistent` and run the documented suites with empty HOME.
No push, PR, issue, remote configuration change or system package installation
was performed.
