# Reference-contract fixtures

These fixtures characterize the WS47 dvar, command, tokenizer, language and
profile corrections without running the game or opening a display. Expected
values come from independently inspected Mac 1.3 branches/table bytes,
CoD2x 1.4.6.8 source contracts and the native writable-home contract, recorded in
[WS47-fidelity-followup.md](../../docs/macos-port/reports/WS47-fidelity-followup.md).
They are assertions about behavior, not expected output generated from the
implementation under test.

The current-release integration keeps CoD2x’s existing shared config mapping.
Per-profile storage and migration are parked with Config Studio; writable-home
profile directory creation and deletion are included here.

Configure separate native stock and CoD2x builds with exported compile commands:

```sh
taskpolicy -b nice -n 19 cmake -S . -B output/ws47-stock \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake -S . -B output/ws47-codx \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 python3 tests/fidelity/run.py --build output/ws47-stock
taskpolicy -b nice -n 19 python3 tests/fidelity/run.py --build output/ws47-codx
```

`run.py` takes the real client `cmd.c` compile command, enables assertions and
ASan/UBSan, and preprocesses the active engine sources. It extracts the selected
functions/tables without editing them. Dvar fixtures link the entire production
`dvar.c`; tokenizer/profile fixtures link the entire production
`cod2x_policy.c`. Fixture executables, copied engine sources and generated
headers stay in the selected ignored output directory. Compilation is serial.

Use `--test tokenizer` (repeatable) to select fixtures. `--baseline <local Git
revision>` applies the same independently specified assertions to historical
production sources, using the current unchanged headers and build flags:

```sh
taskpolicy -b nice -n 19 python3 tests/fidelity/run.py --build output/ws47-stock \
  --baseline 0e46885152b77b5bd380c1b356400fc7d3878f17
```

The baseline is expected to fail. It is a source characterization, not a
comparison of original linked i386 artifacts.

| Fixture | Production behavior exercised |
| --- | --- |
| `tokenizer.c` | Escaped quotes, signed-byte whitespace including all 128 high bytes, BOM, adjacent/empty quoted tokens, overlapping block-comment terminator, verbatim limited-token remainder, exact/trailing/excess 512-token boundary, stock comment versus CoD2x URL handling, existing CoD2x capacity rejection |
| `dvars.c` | Unknown external string promotion by each typed registration command, engine-owned ROM string rejection, 3005-byte joined value and whole-argument cutoff at 4093 argument bytes |
| `registrations.c` | Actual selected registration expressions, real dvar flag/domain merge and external setting, cheat protection including vector content, preserved stock/CoD2x LOD and network defaults, existing config strings before registration, missing client dvars |
| `language.c` | Real language table/name/index/initialization/refresh/selection functions, Czech index 14 and fallback when only Czech assets exist |
| `profiles.c` | Actual profile path/init/change/create/delete functions and OS path builder, separate base/home ownership, active-file first token, profile reset/exec, stock mod paths and the retained CoD2x shared `main/config_mp.cfg` mapping |
| `commands.c` | Actual cgame table/registration/name lookup/dispatch and QuickMessage handler, case-insensitive `mp_QuickMessage`, `mr`, existing snapshot/pm-flag popup condition |

Boundaries deliberately isolated by the fixtures include allocation/string
services, FS listing/read/write/create/delete, localized asset availability,
popup dispatch and unrelated cgame command handlers. Registration fixtures
select relevant actual registration expressions and replace renderer import
dispatch with the same direct dvar API; they do not simulate the full renderer
or client initialization sequence. The language fixture separately tests the
first real `loc_language` registration so a later registration cannot mask its
domain. Profile fixtures observe OS paths and requests without touching real
player data. These tests do not establish linked-game startup, UI layout,
physical keyboard behavior, disk round trips or whole-game fidelity.

The broken baseline typed registration handlers also add EXTERNAL to a borrowed
ROM dvar name. A first characterization run exposed an invalid free during
shutdown. The fixture records the wrong flags, then restores the fixture-owned
flags before cleanup so all historical assertions can run. This cleanup is
outside production and does not hide the failed assertion.
