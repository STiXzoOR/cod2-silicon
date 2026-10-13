# WS77 — CoD2x dedicated master defaults

2026-10-07. Base `9ac08ef`, composing WS68 and WS69. Reference: CoD2x
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`, `src/shared/server.cpp:1293–1295`
and `src/shared/shared.h:32–37`.

The native dedicated server already supports five configured masters, cached
DNS lookup, periodic heartbeat/status packets and shutdown completion. Its
second default was empty. When the authoritative hosted revision reaches the
supported CoD2x revision, registration now defaults `sv_master2` to
`master.cod2x.me:20710`. Master1 retains Activision, and remaining masters stay
optional. The current incomplete host revision remains zero; this change does
not raise it or advertise protocol 120.

Native's existing operator-configurable master flags remain zero. This differs
intentionally from upstream's ROM/changeable-reset flags and forced first two
masters: WS69's approved native contract preserves operator overrides and empty
entries. The existing 180-second heartbeat and 600-second status periods,
public-dedicated-only dispatch, explicit ports, bare-name default port, failed
DNS cache and name-change refresh are preserved. Registration is lazy; the
host revision is an immutable readiness policy during a process lifetime.

## Evidence

The platform fixture runs the actual `macos_server.c` with synthetic DNS and
packet sinks. It does not contact a public directory. Three ASan/UBSan CTest
cases cover stock, CoD2x hosted revision zero, and an explicit candidate
revision six. Before the production change, the candidate case failed the
second-master default assertion; the stock and hosted-zero controls passed.
All three now pass, including configured first/third masters, LAN silence,
heartbeat/status cadence, failed lookup caching, changed names, explicit empty
entries, bare-name port 20710, flatline, completion and deferred SIGTERM.

The stock and CoD2x dedicated translation units compile with the actual Release
engine flags. Each scoped function ABI audit reports one translation unit,
zero errors, zero mismatches and zero new findings. Production-only inactive
guard comparisons pass. The general CoD2x inactive checker reports 17
differences exclusively in the deliberately expanded platform **test** file;
the production file has zero differences. No checker is weakened or baseline
updated. `git diff --check` passes.

Private evidence is under `CoD2-native-protocol/validation.noindex/ws77-masters`
and `masters-*.log`, with the fixture build under `CoD2-native/ws77-masters`
in the user's Application Support directory. Reproduce with:

```sh
cmake -S tests/platform -B /private/output/ws77-masters \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build /private/output/ws77-masters \
  --target macos_server_control_test macos_server_control_codx_test --parallel 3
ctest --test-dir /private/output/ws77-masters -R '^server_control' --output-on-failure
```

No live public listing, backend acceptance or authenticated join is claimed.
This bounded change does not implement the separate UDP limiter/statistics,
public-IP convenience query or script callbacks. Config Studio and Metal remain
parked.
