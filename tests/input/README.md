# Real SDL input QA

This suite injects into the installed SDL event queue and consumes events through
the game's production `SDL_PumpInputEvents`. It requires macOS, the existing SDL2
and SDL3 installs, a native compile database, and the user's licensed game data.
It does not use Accessibility, OS event synthesis, console mouse commands, or
direct UI key/mouse callbacks. Menu geometry and state come from runtime structs;
no extracted menus are stored in the repository.

The library is test-only. It is built under the requested evidence directory and
loaded with `DYLD_INSERT_LIBRARIES` for an owned test process. Without the four
`COD2_TEST_INPUT_*` paths, it performs no injection and writes no diagnostics.
The normal game binary has no test library dependency.

## Checks without a game window

Use a fresh output directory for each command:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/input -p 'test_*.py' -v
python3 tests/input/run.py --self-test \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-selftest-stock"
python3 tests/input/run.py --self-test \
  --compile-db build-macos-codx/compile_commands.json \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-selftest-codx"
```

The standalone executable opens no window. The test checks motion fields, timed
button edges, key translation, UTF-8 decoding, fractional wheel accumulation and
direction metadata at both the SDL bridge and production engine pump. It also
checks that inserting the library without opt-in is inert. The Python tests use
real loopback UDP sockets to verify browse responses and proxy transport.

SDL2-compat's SDL2-to-SDL3 conversion discards precise wheel fields and rejects
text input. The helper uses the **already loaded** SDL3 library (`RTLD_NOLOAD`) for
those two event types, and SDL2 `SDL_PushEvent` for motion, buttons and keys.
Text backing memory remains alive until the full SDL queue is drained. Text is
sent one UTF-8 character at a time to fit SDL2's 32-byte field.

`--build-only <game-binary>` additionally proves that the injector and the game
have the same SDL2 install name. Override `--sdl3` if the game's SDL3 library is
installed elsewhere; loading a second SDL3 instance is rejected.

## Live checks

Coordinate these runs with the person at the Mac and other agents using the
display. The driver checks for an existing `cod2_macos` and waits up to 180 seconds
for it to exit. It uses a private home and net port, a read-only shader
cache, a windowed timeout, and only terminates its own process groups.

```sh
python3 tests/input/run.py build-macos/cod2_macos --scenario inventory \
  --port 29942 \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-inventory"
python3 tests/input/run.py build-macos/cod2_macos --scenario all \
  --server-binary /path/to/cod2_macos_ded --server-port 29952 --port 29942 \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-live-stock"
python3 tests/input/run.py build-macos-codx/cod2_macos --scenario all \
  --compile-db build-macos-codx/compile_commands.json \
  --server-binary /path/to/cod2_macos_ded --server-port 29953 --port 29943 \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-live-codx"
python3 tests/input/run.py build-macos/cod2_macos --scenario browser --fullscreen \
  --port 29944 \
  --output "$HOME/Library/Application Support/CoD2-native-ws42/input-fullscreen"
```

An existing owned localhost server can be supplied with `--server-address`
instead of `--server-binary`. `--scenario browser` covers the browser without
joining; `menus` adds option, bind and dialog assertions; `join` adds both
connection paths; `all` also starts a local listen game through its UI button.
`options`, `controls`, `dialogs` and `maps` isolate their respective checks;
`maps` also launches the selected listen map before confirming Quit.
Fullscreen runs are limited to the inventory or browser scenario and capped at
60 seconds. They keep raw preferences enabled and assert that relative SDL
motion reaches the menu cursor without extra scaling, recording rendered and
Cocoa point gains for comparison with the desktop pointer.
Fullscreen uses `com_maxfps 333`, `in_rawmouse 1` and `m_rinput 1`; windowed
interaction runs use 60 fps and finish within the 180-second client timeout.

The suite fills a long Local/LAN browser using localhost UDP responder identities
so scrolling can be tested without unrelated public servers. With a real server
configured, getinfo/getstatus and connection packets are forwarded to it; join
acceptance requires `CA_ACTIVE`, not merely a connection attempt. Browse filler
identities remain simulated. Join uses a dedicated fixture; Start New Server
separately exercises a listen server. This does not prove broadcast discovery
across a physical LAN, raw hardware delivery, or macOS gesture synthesis.
The setup permits the non-pure private server in the browser filter and clicks
Quick Refresh after LAN rows arrive, since newly discovered stock slots start
without a ping. Advertised proxy ports stay below 32768 to avoid the stock
address formatter's signed-port output. Selectable stock lists move selection
on wheel input and scroll their viewport when the selection crosses an edge.

Runtime assertions cover fractional scroll/reversal, selected row N, timed
double-click and Join button connections, Favorites add/remove, map selection,
Start New Server choices, Options tabs and yes/no/multi/slider values, Controls
bind/clear, localized dialog labels, password text input, Quit cancellation and
a final SDL confirm press that must exit the owned game with status zero.
Screenshots accompany label checks; pixel correctness still needs visual review
or the production opt-in text trace. Failing coverage is an error, while controls
with a single supported choice are recorded as skipped.
Language choices additionally use the runtime installed-language flags: with
English-only data, the test clicks an unavailable choice and checks that English
remains selected. This is skipped for changed multi coverage.

Production tracing is unset for live checks by default. `openmenu`/`closemenu`
provide scenario setup; all tested activation uses SDL input. Add `--native-trace`
only when production trace evidence is wanted. Snapshots are kept at checkpoints
and failures; intermediate polling snapshots are removed to bound disk usage.
Options panels are reached through their runtime parent actions. Every tested
mouse motion requires the UI catcher, and bind tests require the pending state
to begin and finish; a visible menu alone cannot satisfy input ownership.
Named dvar values and bindings are restored through console commands in the
private home. UI callback state can remain changed until the owned process
exits; every run starts a fresh process.

`results.json` records checks and any failure. `inventory.json`, atomic UI
snapshots, input acknowledgments, private server/client logs, and screenshots
stay under `~/Library/Application Support/CoD2-native-ws42/`. Credential/HWID dvar
values are redacted; password assertions compare length. The observer avoids
engine feeder callbacks because they can change server selection and caches.
`processes.log` retains the `pgrep -fl` checks before waiting and before launch.

## FIFO protocol

Each line is `<sequence> <command>`. Acknowledgments in `input.jsonl` include the
sequence, status and SDL millisecond clock. Commands run on the main input pump;
`wait` schedules following commands without sleeping the game thread.

```text
1 motion 100 80 0 0
2 button 1 1 1
3 wait 35
4 button 1 0 1
5 wheel 0 -0.25 1
6 key F8 1
7 key F8 0
8 text e282ac
9 wait 80
10 snapshot
11 barrier
```

Motion coordinates use SDL logical window points, with explicit relative deltas.
Button arguments are button/down/click-count. Wheel arguments are X/Y/flipped.
Text is hexadecimal UTF-8. `snapshot` writes `snapshot-<sequence>.json` under the
fixed evidence directory. The driver uses 35 ms presses and 120 ms between
double-click downs, then verifies that the observed interval is at most 300 ms.
