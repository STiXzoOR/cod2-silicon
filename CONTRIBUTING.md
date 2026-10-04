# Contributing to CoD2 Silicon

Start with the [player README](README.md), [roadmap](docs/ROADMAP.md) and
[development log](docs/macos-port/README.md). Report a reproducible problem or
propose a focused change. Follow the [Code of Conduct](CODE_OF_CONDUCT.md).
Report vulnerabilities privately as described in [SECURITY.md](SECURITY.md).

## Preserve the original build

With **`COD2_X64=OFF`, the i386 objects and binaries must remain byte-for-byte
unchanged** from opencod2 `410342a`. A successful compile or an include-free
preprocessor comparison does not prove this; the Linux multilib comparison
gate below does. Do not alter original data blobs, legacy flags or shared
layouts to make arm64 compile.

- Use `COD2_X64` for pointer-width/layout changes. Existing source frequently
  tests `defined(COD2_X64)`, so CMake OFF must omit the definition, not define
  `COD2_X64=0` as a substitute.
- `COD2_APPLE_SDK` selects real Darwin SDK types only with Apple + `COD2_X64`.
  It prevents the reconstructed STABS declarations from shadowing SDK types.
- Use `__APPLE__` and `__aarch64__` / `__arm64__` where a change depends on the
  platform or instruction set. Architecture-neutral fixes must still pass the
  legacy comparison. CoD2x hooks need both native and `COD2_CODX` gates.
- Keep `-ffp-contract=off`: the Mac i386 reference used SSE scalar semantics,
  not x87 extended precision. Preserve fixed-width disk/wire records and
  handle/bytecode offsets; widening every integer is not an LP64 fix.

Verify behavior against actual commands, code and the original reference.
Compare other projects to understand intent, then independently implement;
do not copy unlicensed, GPL or AGPL reference code into the MIT port changes.
Record evidence and uncertainties. Preserve third-party notices.

## Keep private and proprietary material out

Never commit game data/IWDs, Activision binaries, extracted shaders or shader
caches, demos, screenshots containing game content, CD keys, key digests,
HWIDs, credentials, tokens or decompiler dumps. Keep them outside the repo;
`.gitignore` is a convenience, not permission to add them. Generated
game-derived data also stays local. Redact logs before sharing.

Use your own licensed install as a read-only `fs_basepath` and a separate
writable `fs_homepath`. Do not install system packages or change global
configuration during autonomous port work. If an existing tool is missing,
record it rather than silently installing it. Use isolated branches/worktrees
and never touch another contributor's checkout. Publication/push authority
belongs to the maintainer/orchestrator.

## Style and commits

Use the repository's [.clang-format](.clang-format): four spaces, no tabs,
function braces on their own line, attached control-flow braces, pointer stars
beside the name, no automatic line wrapping or include sorting. Match nearby
names and comment density. Avoid drive-by reformatting and unrelated changes
to shared headers or the top-level CMake file.

Make small commits with an imperative description of the concrete change,
for example `Fix native snapshot pointer width`. Explain why in the body and
include verification when it helps review. Every contribution needs a
[DCO 1.1](DCO.txt) sign-off:

```sh
git commit -s
```

This adds `Signed-off-by: Your Name <your-email>` and certifies your right to
submit the contribution. AI assistance does not replace that responsibility:
review the output, verify it and describe substantial AI use in the PR.

The x86 byte-for-byte CI comparison is temporarily non-blocking because
upstream `410342a` does not compile with GCC 14 on Linux; see
[the tracking issue](https://github.com/STiXzoOR/cod2-silicon/issues/10).

## Native builds and merge gate

Use Xcode Command Line Tools, CMake, Python 3.9+ and SDL2-compatible headers/
libraries; development used SDL3 + sdl2-compat (`scripts/build-sdl.sh` builds
the pinned release versions without Homebrew). A clean checkout needs no
proprietary input: arm64 builds compile the committed typed-data snapshot in
`build/lp64_gen/`. If both reference binaries described in
[tools/datagen/README.md](tools/datagen/README.md) are present, CMake
regenerates the data and fails if it differs from the snapshot. Refresh the
snapshot only with `-DCOD2_UPDATE_TYPED_SNAPSHOT=ON`, and commit the result
with the change that required it.

Run from the repository root. On a shared benchmarking Mac, prefix builds
and fixture compilation with `taskpolicy -b nice -n 19`. Do not run competing
game/benchmark processes.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos --parallel 3

# Keep a copy of the actual engine compile database. Source fixtures inherit
# these flags, so enable their assertions after the Release build is complete.
python3 - <<'PYTHON'
import json
from pathlib import Path
for build in ['build-macos', 'build-macos-codx']:
    path = Path(build) / 'compile_commands.json'
    path.with_name('compile_commands.engine.json').write_bytes(path.read_bytes())
    rows = json.loads(path.read_text())
    for row in rows:
        if 'command' in row:
            row['command'] = row['command'].replace(' -DNDEBUG', ' -UNDEBUG')
        if 'arguments' in row:
            row['arguments'] = ['-UNDEBUG' if x == '-DNDEBUG' else x for x in row['arguments']]
    path.write_text(json.dumps(rows))
PYTHON

python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/datagen -p test_datagen.py -v
python3 tools/abi/test_audit.py
python3 tools/abi/test_imports.py
python3 tests/rendering/shader_setup.py
python3 -m unittest discover -s tests/lp64/renderer -p test_wine_draw_trace.py -v
COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh
sh tests/cod2x/run_native.sh
sh tests/online/sdk_identity.sh
python3 tests/online/run.py build-macos/compile_commands.json
python3 tests/online/run.py build-macos-codx/compile_commands.json
python3 tests/fixes13/run.py --build build-macos
python3 tests/fixes13/run.py --build build-macos-codx
python3 tests/perf/run.py
python3 tests/lp64/game/run.py --build build-macos
python3 tests/lp64/script/run.py
sh tests/lp64/renderer/run.sh
sh tests/lp64/renderer/run_shader_raster.sh
sh tests/lp64/renderer/run_texture_mips.sh
sh tests/lp64/renderer/run_volume_upload.sh
python3 tests/platform/run_hud_matrices.py
python3 tests/platform/run_impact_marks.py
python3 tests/platform/run_fx_events.py
python3 tests/platform/run_fx_primitives.py
python3 tests/platform/run_fx_cloud.py
sh tests/platform/run_dedicated_fx.sh

cmake -S tests/platform -B build-macos/platform-tests \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build build-macos/platform-tests --parallel 3
ctest --test-dir build-macos/platform-tests --output-on-failure
# Repeat without sanitizers to include diagnostics and crash-report tests.
cmake -S tests/platform -B build-macos/platform-tests-plain \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=OFF
cmake --build build-macos/platform-tests-plain --parallel 3
ctest --test-dir build-macos/platform-tests-plain --output-on-failure

# Private reference checks: these consume external owned Mac binaries.
sh tools/datagen/roundtrip_test.sh
python3 tests/lp64/game/check_layouts.py --build build-macos
python3 tests/fixes13/reference.py

sh tools/abi/check.sh build-macos/compile_commands.engine.json output/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/abi-codx
git diff --check
```

The full ABI check must report **zero mismatches**; it includes function,
renderer callback and import-indirection audits. It reads the private Mac
reference binary. Public CI runs the function and callback portions and
synthetic checker tests; it cannot run the binary-dependent import audit.

On x86_64 Linux with the multilib dependencies listed in the
[upstream README](docs/upstream-opencod2-README.md):

```sh
bash tools/ci/compare-x86.sh 410342a HEAD output/x86-reference
```

Use a fresh output directory. The script fixes metadata/path differences and
compares every game object and both binaries. Its result must be byte-identical.
Keep the original Windows build path buildable too. Preprocessor checks against
the pre-change commit supplement this gate:

```sh
python3 tests/fixes13/legacy.py --base <pre-change-commit>
python3 tests/cod2x/check_inactive_gates.py --base <pre-change-commit>
python3 tools/abi/legacy.py --base <pre-change-commit>
```

Finally, on a Mac with licensed data and an interactive session, record a
local smoke run and check online stock/CoD2x gameplay after relevant runtime
changes. Neither fixture success nor frame-cap registration proves live
stability or performance:

```sh
python3 tools/parity/record.py --binary build-macos-codx/cod2_macos \
  --data "$HOME/Games/CoD2" --output output/smoke-new --frames 100
```

## Which checks need game data?

“No” below describes the test's inputs. A suite that needs an engine compile
database or generated object still inherits the current build's private
reference prerequisite. Interactive tests need a real display/audio session
even when they use synthetic content.

| Suite/check | Licensed input needed? | Other requirements |
| --- | --- | --- |
| `tools/tests`, `tools/datagen/test_datagen.py`, shader manifest tests, Wine trace parser tests | No | Python standard library; synthetic files only |
| `tools/abi/test_audit.py`, `test_imports.py` | No | Clang for synthetic independent translation units |
| `tools/abi/audit.py`, `callback_tables.py` | No | Real native compile database; no game executable required |
| `tools/abi/check.sh`, `imports.py` | **Mac binary** | Full-STABS reference plus compile database |
| `tests/cod2x/run.sh`, `run_full.sh`, `run_native.sh` | No | Clang/SDK, SDL headers; synthetic archives, URL probe app, local HTTPS fixtures (openssl); native watchdog/crash fixtures |
| `tests/online/run.py` | No | Compile database; synthetic production-function tests, no internet play |
| `tests/online/sdk_identity.sh` | No | IOKit/CoreFoundation and legacy-stub fixture |
| `tests/fixes13/run.py`, `tests/lp64/game/run.py` | No | Compile database; generated fixture labels are empty |
| `tests/lp64/script/run.py` | No | `build-macos` compile database and generated `bss_native.c.o`; includes compiler and VM suites |
| `tests/perf/run.py` | No | Clang; source fixtures, **not a benchmark** |
| `tests/lp64/renderer/run.sh` | No | Clang + ASan/UBSan; synthetic buffers/materials/commands, no GL context |
| Renderer `run_shader_raster.sh`, `run_texture_mips.sh`, `run_volume_upload.sh` | No | Synthetic content in private real CGL contexts; OpenGL-capable session |
| `tests/platform` CTest, `run_dedicated_fx.sh`, `run_hud_matrices.py`, `run_impact_marks.py`, `run_fx_*.py` | No | Synthetic FX/HUD/network/audio; full CTest also exercises real window/fullscreen/audio APIs |
| Legacy/preprocessor guards and x86 artifact comparison | No | Git history and compatible compiler; x86 comparison requires Linux multilib |
| Datagen round trip, arm64 generated-data checks and determinism | **Mac reference binaries** | `tools/datagen/roundtrip_test.sh`; generation uses full STABS + Steam scalar-value binary |
| `tests/lp64/game/check_layouts.py`, `tests/fixes13/reference.py` | **Mac binaries** | Compare layouts/facts to reference STABS/disassembly data |
| `tools/macos-port/check_wavelets.py`, material/shader extraction validation | **IWDs and/or Mac binary** | External owned inputs; retain results outside git |
| `tests/rendering/toujane_rgb.py`, parity recording, combat drivers, demos, live play and benchmarks | **Game data** | Built engine, private writable home; some checks also need owned shaders, CD key, server or demo |

Diagnostic replay tools (`tests/lp64/game/replay.py`, renderer diagnostics,
LP64 inventory) inspect compiler output; they are not live acceptance tests.
Run focused checks for the changed subsystem while developing, then complete
the full gate before integration. Record unavailable prerequisites and failures
honestly; do not convert a failing check into an expected pass.
