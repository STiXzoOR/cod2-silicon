# Native CoD2x logic tests

From this worktree on macOS:

```sh
./tests/cod2x/run.sh
```

The runner uses plain Apple `clang` and SDK frameworks, creates temporary binaries
inside this directory, and removes them on exit. It does not need the engine,
game data, system package installation, or a server.

- `test_cod2x.c`: advertised 118/120 selection, stable 32-character HWID2,
  canonical UUID hash vector, actual IOKit identity lookup, seeded CD-key digest
  vectors, stock connect-packet framing and text-field decoding, FPS limits.
- `test_runtime.c`: actual policy adapter with a minimal dvar fixture; checks
  immutable connection identity, stock-server fallback, competitive settings,
  demo exemptions, legacy FPS restriction and restoration on disconnect.
- `test_animation.c`: actual controller helper with typed fixtures; checks
  expanded crouch classification including bit 39, first-leg initialization,
  stance blend durations, prone posture/reload/fire offsets, diagonal alignment,
  stance bounce, separate animation users and time-rewind reset.

The fixtures model only the fields needed for these calculations. They do not
verify engine layout, networking, rendering, real hitboxes, or admission to a
live authenticated server. The report at
`docs/macos-port/reports/WS4-cod2x.md` records the engine syntax-check boundary
and the live-server test procedure.

The HWID2 domain is `opencod2:cod2x:hwid2:v1:` followed by the lowercase canonical
UUID. The first 16 bytes of SHA-256 become lowercase hexadecimal. Changing this
rule changes this machine's server identity; keep it stable. Tests never print
or store the actual UUID or machine ID.

The CD-key hash uses the stock seeded MD5 initial state and uppercase alphanumeric
normalization of at most the first 32 source characters. The fixed vectors were
cross-checked with a separate MD5 round implementation. This legacy protocol
hash is distinct from the HWID2 hash.

For the full native-client workstream, run:

```sh
sh tests/cod2x/run_full.sh
COD2X_SANITIZERS=1 UBSAN_OPTIONS=halt_on_error=1 sh tests/cod2x/run_full.sh
python3 tests/cod2x/check_inactive_gates.py --base 231d6be5d1f49864862a98a3e72d7b6ddbe840fa
```

The full runner adds IWD/config selection, tokenizer and extraction fixtures;
4096 registrations and exhaustion through the production dvar allocator; the
production big-info helper; visual commands, colors, orbit and radar geometry;
production radar buffer ownership and cheat-protected pose controls; raw-input
statistics and production `IN_Frame` routing; safe URL parsing and actual native
AppleEvent/bundle metadata; demo playback restoration, protected recording,
persistent upload queues and deferred quit.

It also posts owned fixture bytes to an ephemeral local HTTPS server using the
production libcurl uploader. A test-only CA override trusts the fixture; a
hostname mismatch must deliver no bytes. Nothing changes the system trust store.
The extractor tests use synthetic archives. Licensed release validation, live
server admission and physical input/rendering are separate checks in
`docs/macos-port/reports/WS10-cod2x-full.md`.

The normal runner exercises a real 12-second watchdog stall, disable/heartbeat
cases and SIGABRT reports, taking about 45 seconds. Sanitizer mode excludes those
deliberate signal cases. Its added C fixtures use ASan/UBSan; the unchanged WS4
runner and standalone HTTPS subprocess retain their original compile flags.
The fixtures and temporary app bundles are created under the system temporary
directory and removed on exit. The inactive-gate comparison checks preprocessed
source bodies, including explicit zero-valued macros; it cannot prove byte-level
32-bit binary parity.
