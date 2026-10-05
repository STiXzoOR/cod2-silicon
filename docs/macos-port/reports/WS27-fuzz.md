# WS27 — fuzzing the network parsers

Date: 2026-10-04. Branch: `port/fuzz`. Worktree:
`~/Projects/cod2-native-wt/fuzz`. Base: `port/main` at `3ef770b`.

Defensive robustness testing of our own engine's parsers before anyone hosts a
Mac server. All work is native (`COD2_X64`); the i386 build is untouched. No
system packages were installed, nothing was pushed, and no capture of a real
server or player was used — every seed is constructed from the protocol framing
in `tests/fuzz/make_seeds.py`.

## Result

Five memory-safety bugs in the code that parses untrusted packets, all fixable
without weakening a check and without changing behaviour for valid input, and
all present in the original reconstruction (and most in retail 1.3). Each has a
regression input and a small guarded commit. Four libFuzzer-style targets plus
a portable driver cover connectionless dispatch, netchan reassembly, Huffman
and the `MSG_Read*` family, and command/userinfo tokenisation.

| # | Bug | Reach | Fix |
| - | --- | ----- | --- |
| 1 | `MSG_ReadBitsCompress` decoded past the input and past the 128 KB decompress buffer (2-bit codes expand each byte up to 4×) | any connected client; also client-side from a server | `57d15f1` |
| 2 | `MSG_ReadDeltaPlayerstate` walked past the 105-entry field table on a count byte of 106–255 | any snapshot from the server | `11eca99` |
| 3 | `MSG_ReadDeltaStruct` read `stateFields[-1]` and wrote a wild offset when a delta ended right before its count byte (count read back as −1) | any entity/client/archived snapshot | `6c21cbb` |
| 4 | `MSG_ReadDeltaHudElems` cleared `to[-1]` when the element count was truncated to −1 | any snapshot with a HUD delta | `4a4c09a` |
| 5 | `Info_SetValueForKey` one-byte stack overflow: the length guard allowed the sum to equal the buffer size, then `strcat` wrote the NUL past it | unauthenticated `getinfo` (SVC_Info's 1024-byte infostring) | `535bd32` |

Bugs 1–3 and 5 were found by the fuzzer; bug 4 was found by review on the same
path while fixing bug 2 (its write stays inside `playerState_t`, so ASan cannot
flag it and there is no crashing input — the `msg` target still exercises it).

## Targets

Each target is a libFuzzer entry point (`int LLVMFuzzerTestOneInput(const
uint8_t *, size_t)`) under `tests/fuzz/targets/`, built by `tests/fuzz/build.py`
by compiling the real engine translation units with the flags CMake recorded in
`compile_commands.json`, adding `-fsanitize=address,undefined` and
SanitizerCoverage, and dead-stripping them against the harness and driver — the
same "extract the production function" approach as `tests/online` and
`tests/lp64`. UBSan `alignment` and `shift-base` are disabled because the wire
format is deliberately unaligned (arm64 tolerates it) and `-1 << bits` is the
engine's sign-extension idiom; every other UBSan and all of ASan is on.

- **oob** (`sv_main_mp.c`, `sv_main_pc_mp.c`, `sv_voice_mp.c`, netchan, msg,
  huffman, cmd, net_hardening, q_shared) — drives `SV_ConnectionlessPacket`:
  `getstatus`, `getinfo`, `getchallenge`, `connect`, `rcon`, `ipAuthorize`,
  voice. The tokeniser, info-string builders, rcon command assembly and voice
  reader are the real code; the game, filesystem, challenge and direct-connect
  layers are stubbed at the harness boundary. `svs.clients` is a small real
  array so the status/info client loops run.
- **netchan** (`net_chan_mp.c`, `sv_net_chan_mp.c`, msg, huffman, q_shared) —
  feeds framed packets to `Netchan_Process` on one server channel, as
  `SV_PacketEvent` does, with the receive buffer sized to the MAX_MSGLEN
  LargeLocal the engine uses, then runs a completed message through
  `SV_Netchan_Decode`. Exercises fragment reassembly and the XOR decode.
- **msg** (`msg_mp.c`, `huffman.c`, `q_shared.c`) — decodes arbitrary
  compressed input into a MAX_MSG_DECOMPRESS_BYTES buffer (as
  `SV_ExecuteClientMessage`/`CL_ParseServerMessage` do), checks a Huffman
  encode/decode round trip, and drives the `MSG_Read*` family: strings,
  entity/client/archived-entity/playerstate deltas and usercmds, over
  exact-size heap structures so overruns are caught.
- **tokenize** (`cmd.c`, `q_shared.c`, `com_shared.c`) — runs
  `SV_Cmd_TokenizeString`/`Cmd_TokenizeString2` the way a client's reliable
  commands and connectionless packets are tokenised, reads every argument
  accessor back, and drives `Info_SetValueForKey`/`RemoveKey`/`ValueForKey`/
  `NextPair` and the `_Big` forms over 1 KB and 8 KB info strings.

CoD2x policy code (`cod2x_policy.c`, the `Cod2x_Tokenize` path) is compiled into
the oob, netchan and tokenize targets when the build's feature flags enable it.

## Driver

Apple clang on this Mac compiles SanitizerCoverage but ships no libFuzzer
runtime, and Homebrew LLVM is not installed (and must not be). `tests/fuzz/
driver.c` is a portable, coverage-guided mutation driver that links with any of
the targets:

- loads a seed corpus directory, keeps inputs that reach a new
  `(counter, hit-bucket)` feature of the inline 8-bit counters;
- mutates with byte/bit flips, inserts, deletes, repeated runs, splices,
  interesting integers, ASCII-integer edits, a dictionary and
  comparison-operand substitution (from `-fsanitize-coverage=trace-cmp` and the
  libc `__sanitizer_weak_hook_*` compare hooks);
- is reproducible: every run records its seed, and `-seed=N` replays it;
- bounds a run with `-runs` or `-max_total_time`, with a wall-clock
  `-timeout` watchdog;
- on a crash, a sanitizer report or a timeout, writes the input under
  `-artifact_prefix` and exits non-zero;
- replays file arguments once (regression mode) and shrinks a crash with
  `-minimize_crash=1`, in forked children, preserving the sanitizer summary.

The options use libFuzzer spelling, so the same seeds and regression inputs run
unchanged under a real libFuzzer build (see "How to run").

## Campaigns

Builds and runs under `taskpolicy -b nice -n 19`; other agents were using the
machine, so these are correctness campaigns, not timed. Seeds are the
synthetic corpora from `make_seeds.py`.

| Target | Seed | Budget | Runs | Outcome |
| ------ | ---- | ------ | ---- | ------- |
| msg | 11, 12 | ~3 min each | — | found bugs 1, 2, 3 (then fixed) |
| msg | 31 | 420 s | 79 104 | clean after fixes (2 742 features, corpus 602) |
| netchan | 41 | 420 s | 607 744 | clean (reassembly bounds held, corpus 62) |
| tokenize | 21 | ~1 min | — | found bug 5 (then fixed) |
| tokenize | 52 | 480 s | 1 098 240 | clean after fix (corpus 281) |
| oob | 51 | 480 s | 295 424 | clean (692 features, corpus 190) |

Each fuzzer-found crash was confirmed to reproduce on the pre-fix source
(`git stash` of the fix, rebuild, replay) and to pass afterwards.

## Fixes

All engine fixes are gated by `COD2_NET_BOUNDS` (`net_hardening.h`), defined for
native (`COD2_X64`) builds and wherever `COD2_FEATURE_NET_HARDENING` is on, so
the i386 configurations compile the bounds out and stay byte-for-byte. The
legacy guard checks confirm this (`check_legacy_guards.py`, `fixes13/legacy.py`:
0 mismatches across the i386/wasm/mingw/clang configurations).

1. **Huffman (`57d15f1`).** `MSG_ReadBitsCompress` looped until its bit cursor
   passed `size*8`, completing the last code from the bytes after the packet,
   and wrote with no output limit. Shortest codes are two bits, so a peer makes
   each byte decode to four: 32769 bytes of `0xaa` overrun the 128 KB
   `MAX_MSG_DECOMPRESS_BYTES` stack buffer in `SV_ExecuteClientMessage` (and the
   client's `CL_ParseServerMessage`). Native builds decode with a new
   `Huff_offsetReceiveLimit` that reads no bit at or past the input end and
   stop at the input end or the output cap. Valid messages are identical: the
   encoder always writes a complete code, so only trailing padding could form a
   partial one. Regressions: `huffman-overread`, `huffman-expansion`.
2. **Playerstate field count (`11eca99`).** `MSG_ReadDeltaPlayerstate` took a
   count byte and walked that many of the 105 `playerStateFields`; 106–255 read
   past the table and stored wire values at out-of-table offsets of the
   snapshot's `playerState_t`. Native builds treat a count above the table size
   as malformed (as `MSG_ReadDeltaStruct` already does for entities).
   Regression: `playerstate-field-count`.
3. **Truncated delta count (`6c21cbb`).** When an entity/client/archived delta
   ended right before its count byte, `MSG_ReadByte_core` returned −1; the check
   only caught values above the table, so −1 passed and the fill-unchanged loop
   ran from `i = -1`, reading `stateFields[-1].offset` and writing near the
   snapshot. Native builds compare the count unsigned, rejecting −1 and the
   too-large case together. Regression: `delta-truncated-fieldcount`.
4. **Truncated HUD count (`4a4c09a`).** `MSG_ReadDeltaHudElems` used the 5-bit
   count directly; a short read made it −1 and the closing `memset` cleared
   `to[-1]` onward (one `hudelem_t` before `hud.archival`, and `deltaTime`/
   `objective[15]` before `hud.current`). Native builds clamp a negative count
   to zero. Found by review; stays inside `playerState_t`, so no ASan crash and
   no regression file — the `msg` target covers the path.
5. **`Info_SetValueForKey` (`535bd32`).** The guard allowed
   `strlen(s) + strlen(newi) == 1024`, then `strcat` wrote the 1025th byte past
   a 1024-byte destination. `SVC_Info` builds a 1024-byte `infostring` that
   starts with the challenge string from an unauthenticated `getinfo`, then
   appends fixed keys, so a crafted `getinfo` reached it; `SVC_GameCompleteStatus`
   shares the size. Native builds reject the concatenation when it would not
   leave room for the terminating NUL. Regression: `infostring-setkey-overflow`.

## Remaining risks

- **Game/script/filesystem layers behind the parsers are stubbed.**
  `SV_DirectConnect` → `ClientConnect`, the pure-IWD checks, the download path
  (`SV_BeginDownload_f`/`SV_WriteDownloadToClient`), and the script VM that
  client commands ultimately reach are not exercised here; `ClientCommand`
  dispatch and the game's own `userinfo` consumers are out of scope. WS16
  covered the script VM and game-state integrity under a live sanitizer soak.
- **iwd/zip central-directory parsing (`PC/zlib/unzip.c`, task item e) was not
  fuzzed** — time went to a–d. It runs when loading downloaded mods and is a
  worthwhile next target; a harness over `unzOpen`/`unzGoToNextFile`/
  `unzGetCurrentFileInfo` on a synthetic archive would fit the same driver.
- **The campaigns were bounded and unprioritised** (shared machine, no libFuzzer
  value-profile). A longer run on a toolchain with real libFuzzer, and corpus
  minimisation across targets, would reach deeper states.
- **`Info_SetValueForKey_Big`'s non-CoD2x branch keeps a `> 1024` cap on an
  8192-byte buffer** — a truncation quirk, not an overflow (the buffer is far
  larger); left as-is because changing it alters behaviour for valid 1–8 KB
  info strings. Noted for review.
- The `msg` and `netchan` reassembly campaigns did not reach the exact
  `fragmentLength == MAX_MSGLEN` boundary in `Netchan_Process` (final `memcpy`
  to `msg->data + 4`); the `fragmentStart + fragmentLength > MAX_MSGLEN` and
  `> msg->maxsize` checks bound it, but a targeted seed would confirm the +4.

## Merge gate

Run from this worktree after an incremental rebuild of both engine binaries
with the fixes (which succeeded):

- **Both builds:** `cod2_macos` links from `build-macos` and `build-macos-codx`
  with the fixes, zero errors.
- **Legacy guards:** `tools/macos-port/check_legacy_guards.py --base <base>` and
  `tests/fixes13/legacy.py --base <base>` — 0 mismatches across the
  i386/apple-i386/mingw/wasm/clang configurations. The `COD2_NET_BOUNDS` gate
  keeps the 32-bit build byte-for-byte.
- **ABI:** `tools/abi/check.sh build-macos/compile_commands.engine.json <out>`
  against the private Mac 1.3 reference — 0 mismatches (functions, renderer
  callbacks, import indirection).
- **online (stock):** all 17 production-function suites pass under ASan/UBSan,
  including `infostring` (exercises the `q_shared.c` fix); on the CoD2x build
  `infostring` also passes.
- **fixes13:** passes on `build-macos-codx`; `lp64/game` `wire` (msg_mp.c) passes.
- **Fuzz:** `tests/fuzz/run.sh` builds all four targets, replays every
  regression input, and runs a smoke campaign each.

Two **pre-existing** suite failures are unrelated to this branch (they also
fail on the base commit and touch files WS27 did not change):

- `tests/online/run.py` on the **CoD2x** build fails to link the `timeout`
  suite: `CL_Frame` compiled with `-DCOD2_CODX=1` references `Cod2x_Frame` and
  `Cod2x_DemoClientFrame`, which `tests/online/timeout.c` does not stub. The
  stock build links it. WS14 only ran this suite on the stock build.
- `tests/fixes13/run.py --build build-macos` (stock) aborts in the `timing`
  check: it extracts `Cod2x_LimitedFPS` from `cod2x_protocol.c`, which is
  entirely behind `COD2_CODX`, so under stock flags the source preprocesses to
  empty. Confirmed identical on base (`--baseline 3ef770b`). The check passes on
  `build-macos-codx`.

The renderer/platform/packaging suites and the private round-trip checks were
out of this subsystem's scope; the orchestrator runs the full gate before
integration. The Toujane release smoke (`tests/packaging/release_smoke.py`) and
the x86 comparison need a packaged app and an x86_64 Linux host respectively and
were not run here.

## How to run

From the worktree with a native build present (see `CONTRIBUTING.md`):

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 3
taskpolicy -b nice -n 19 sh tests/fuzz/run.sh          # build, replay regressions, 30 s smoke each
```

`run.sh [BUILD_DIR] [SMOKE_SECONDS]` builds every target, regenerates the
synthetic seeds, replays all `tests/fuzz/regressions/<target>/*`, and runs a
short campaign each; `SMOKE_SECONDS=0` replays regressions only. Build one
target by hand with `tests/fuzz/build.py --target <name>`.

Longer campaign, reproducible from its seed, and crash triage:

```sh
build-macos/fuzz/oob -seed=1 -max_total_time=3600 -max_len=8192 \
  -artifact_prefix=build-macos/fuzz/crashes/ build-macos/fuzz/seeds/oob
build-macos/fuzz/oob -minimize_crash=1 \
  -artifact_prefix=build-macos/fuzz/crashes/ <crashing-input>
# then copy the minimised input to tests/fuzz/regressions/<target>/
```

On a machine whose clang has libFuzzer (Homebrew LLVM; not Apple clang, and do
not install it on this Mac), build the same targets against the real runtime and
fuzz with the upstream engine — the options and seeds are unchanged:

```sh
tests/fuzz/build.py --libfuzzer
build-macos/fuzz/oob -max_total_time=3600 build-macos/fuzz/seeds/oob
```

## Files

- `tests/fuzz/driver.c` — portable coverage-guided driver.
- `tests/fuzz/build.py` — builds targets from the compile database (`--libfuzzer`
  links real libFuzzer instead).
- `tests/fuzz/make_seeds.py` — synthetic seed corpora.
- `tests/fuzz/fuzz.h` — harness helpers (dvar storage, structured reader,
  Com_Error policy).
- `tests/fuzz/targets/{oob,netchan,msg,tokenize}.c` — the harnesses.
- `tests/fuzz/regressions/<target>/*` — minimised crashing inputs.
- `tests/fuzz/run.sh` — build + regressions + smoke campaign.
