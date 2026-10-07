# WS64: native CoD2x host snapshot interval

2026-10-07. Current-release WS56 CR07 snapshot correction, based on `380bf2b`.

The pinned CoD2x `d8c54695a5239ac99d1dfe96212b809b1a54bfee` source implements
`SV_UserinfoChanged` with requested snapshots clamped to 1–40 before integer
`1000 / snaps` conversion (`src/shared/server.cpp:323–338`). An absent value
keeps the original 50 ms default. There is no negotiated-revision condition on
this server-side policy. Native hosting instead capped every request above 30
at 33 ms, including the native CoD2x client's own 40 request.

The native CoD2x path now caps requests above 40 at 25 ms, retaining the
existing division for 1–40, 1000 ms for nonpositive requests, and 50 ms for an
absent value. Stock and inactive legacy behavior remain unchanged. No server
FPS default, client 40/125 defaults, protocol/revision advertisement, admission,
identity, heartbeat or authentication behavior changes. A 25 ms requested
snapshot interval does not make a default 20 Hz server run at 40 Hz.

`tests/fixes13/server_snaps.c` executes the actual production
`SV_UserinfoChanged` with synthetic userinfo lookup and LAN service boundaries.
Its fixed expected table covers absent, -1, 0, 1, 20, 30, 31, 39, 40, 41 and
125 requests across LAN/non-LAN and listen/dedicated settings. It also confirms
that name, rate, voice and download metadata still update. The baseline fails
20 interval assertions (five values in four host settings); both stock and
CoD2x controls pass under ASan after the fix.

The affected server translation unit compiles with actual Release commands for
both client-host and dedicated targets in both modes (four objects). The one
changed source passes all ten inactive legacy configurations. Four independent
`unifdef` combinations confirm byte-identical inactive source, including native
stock, explicit CoD2x zero, and nonnative CoD2x. This does not establish Linux
i386 artifact identity or measured network cadence.

Evidence is private under
`~/Library/Application Support/CoD2-native-ws63/validation.noindex/` with
`snaps-`/`server-snaps-` prefixes. Full composed runtime validation must configure
40 server ticks and measure actual snapshot delivery; host lifecycle and the
other WS56 server contracts remain separate work. No live server, GUI,
benchmark, public service or installation was used for this unit.
