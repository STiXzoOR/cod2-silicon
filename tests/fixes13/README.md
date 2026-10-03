# WS9 native correctness tests

Run from this worktree on macOS arm64. No engine link or game data is required.

```sh
cmake -S . -B build/ws9 -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release
python3 tests/fixes13/run.py
python3 tests/fixes13/build_flags.py
python3 tests/fixes13/run.py --test debug_enabled
python3 tests/fixes13/reference.py
python3 tests/fixes13/legacy.py
```

`run.py` preprocesses production code with the configured native compiler flags,
extracts the functions/declarations under test, and builds arm64 AddressSanitizer
executables. Engine services outside the tested functions are small mocks. The
ABI checks compile eleven complete translation units with system libc headers.
The hot-path checks compile seven complete translation units at `-O0` and inspect
their symbols and code relocations, so elimination of diagnostics does not depend
on optimization. Renderer surface counters, volatile crash-trace writes, and
calls into disabled trace helpers must also be absent.

The switch test extracts the actual duplicate-case validation block and comparator,
not the whole parser. The timing test runs the actual frame loop, CoD2x FPS policy,
and command/packet scheduling functions against a simulated millisecond clock;
it checks 1,000 frames each at 125, 250, 333, and competitively limited 333 fps.
The renderer option test runs with its option both absent and present.

`--test NAME` selects individual tests. `--baseline COMMIT` extracts old production
fragments from this worktree's local git history to reproduce a regression; complete
translation-unit ABI/hot-path/debug checks always use current source. Baseline
artifacts use a separate directory so they cannot replace current test artifacts.
For example:

```sh
python3 tests/fixes13/run.py --baseline 4802e924 --test snapshot
python3 tests/fixes13/run.py --baseline 4802e924 --test switch
python3 tests/fixes13/run.py --baseline 4802e924 --test timing
```

The first two fail on the starting commit; timing already agrees with Windows 1.3.
`reference.py` reads the local 2006 retail STABS binary and the licensed Steam 2013
binary. It writes no extracted binary data. Its BSS audit excludes ambiguous or
unavailable debug types. `legacy.py` compares preprocessed source tokens in ten
legacy configurations. Actual i386 executable byte identity still needs the
project's 32-bit build/comparison toolchain.
