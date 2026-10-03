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
