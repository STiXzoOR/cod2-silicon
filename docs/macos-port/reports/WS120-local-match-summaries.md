# WS120 — Local match summaries (Config Studio A2)

## Implementation

Native clients record local observations in `fs_homepath/main/stats/matches.jsonl`.
The first valid non-demo snapshot starts a record. Intermission (`pm_type == 5`,
as assigned by `G_ClientEndFrameIntermission`), new gamestate, local map loading
and disconnect finish it once. A later non-intermission snapshot starts another
record. No telemetry is transmitted and the module does not alter gameplay state.

`cl_matchSummaries` defaults on. Startup `+set cl_matchSummaries 0` disables
recording for the process. The client registers it read-only and captures the
startup choice, so subsequent server/config assignments cannot enable writing.
The launcher persists `GameSettings.matchSummariesEnabled`, defaults missing
older settings to true and passes the setting for both migrated and older
profiles. It is a machine setting and cannot be overridden through Advanced.

Metrics come from these sources:

- Frame times are the common loop's unscaled `rawMsec`, before fixed time,
  timescale and gameplay clamping. The cap is captured at the frame limiter's
  decision, using `Cod2x_FrameFPS` for CoD2x. `framesUnderCap` counts frames
  whose wall time is at or below that frame's integer limiter budget;
  `capSampleFrames` counts frames with a positive cap. `capFPS` is the final
  sampled cap, or the configured effective cap if there were no frames.
- The static histogram has 10,001 buckets. Quantiles below 10,000 ms are exact
  integer millisecond ranks. An overflow quantile reports the observed maximum
  as an upper bound. Maximum frame time is always the observed maximum. The
  JSON includes `frameTimeHistogramLimitMS`; the reader exposes
  `frameQuantilesAreExact`. No frame or snapshot callback allocates memory.
- Ping uses valid parsed snapshots only when the command-time acknowledgement
  loop found an outgoing packet. The fallback 999 value without an
  acknowledgement is unmeasured, rather than an invented ping sample.
- Packet loss is accepted netchan message sequence gaps, accumulated once
  after `Netchan_Process` accepts a message. It is not lagometer misses or UDP
  datagram/fragment loss. `packetLossSource` identifies that distinction.
- Map, mode and advertised rule hints are copied from server/system config
  strings. Hints remain hints; the module does not infer a verified ruleset.
- Duration uses the monotonic clock; the ending timestamp uses UTC wall time.

Missing frame, ping or packet samples produce JSON nulls. Histograms/counters
are bounded state; file handling and retention occur only on finish. Retention
writes the latest 199 complete bounded lines plus the new record. The file is
mode 0600. Directory-relative descriptors reject symlinks and nonordinary files;
an exclusively created owned temporary file is renamed within the stats
directory. Hints are bounded to keep generated JSON lines below 8192 bytes.

`MatchSummaries.read(homePath:)` returns up to 200 records, oldest first. It
reads at most 1,638,400 bytes of a file's tail, drops an incomplete first/final
line, rejects symlinks/nonordinary files, unknown schemas, nonfinite values and
inconsistent metric totals. A missing/unusable stats path returns no records.
Check-up/Privacy UI wiring belongs to the composed launcher integration.

All engine additions are under `COD2_X64`; the new module is empty for dedicated
builds and is excluded from native-off source objects by CMake. No shared
layout, original byte path or renderer/shader work changed.

## Evidence

Private artifacts are under
`/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS120.noindex`.

Red evidence before the feature: the initial no-op lifecycle fixture exited 1
with `metrics: engine did not write a local match summary`. The strengthened
fixture using actual `CL_Disconnect` and `CL_MapLoading` from committed
`76102bc` exited 1 with the intermission/map/disconnect count assertion, while
the metric accumulator check passed. The first synthetic symlink-directory
check reproduced an out-of-directory write. The reader's newline/totals tests
also failed before the bounded reader correction.

Verification commands (one compiler at a time, all under
`taskpolicy -b nice -n 19` in the runners):

```sh
python3 tests/matches/run.py --verify-production \
  --compile-commands '/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS100.noindex/compile_commands-stock.engine.json' \
  --output '/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS120.noindex/stock-verified'
python3 tests/matches/run.py --verify-production \
  --compile-commands '/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS100.noindex/compile_commands-codx.engine.json' \
  --output '/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS120.noindex/codx-verified'
COD2_MATCH_TEST_OUTPUT='/Users/stix/Library/Application Support/CoD2-native-current-readiness/WS120.noindex/launcher-final' \
  sh tests/launcher/match_summaries.sh
```

Both engine modes pass deterministic frame/ping/loss/snapshot metrics, actual
disconnect/map-loading functions, once-only intermission/map/disconnect,
demoplayback exclusion, absent metrics, histogram overflow, 205-to-200 record
retention, process-frozen opt-out, and symlink file/directory/temporary cases
under AddressSanitizer and UndefinedBehaviorSanitizer. Their runners compile
all four affected production translation units with real engine flags. Existing
reconstruction warnings remain; no new module warning was observed.

For each existing touched engine source the runner executes
`unifdef -UCOD2_X64` on candidate and `git show 664f1a1:<path>`, saves both
results and runs `cmp`; all three are identical. The new module preprocesses
to zero bytes native-off. `nm -u` on its dedicated object is empty. These source
checks do not replace the full original i386 object/binary comparison gate.

The Swift test passes strict Swift 6 concurrency and warnings-as-errors on the
macOS 13 target. It checks default/legacy/persisted privacy, both launch modes,
Advanced opt-out protection, last-200 reading, malformed/incomplete records,
impossible metric totals, an oversized-file tail and symlink exclusion.

## Remaining integration verification

The parent owns canonical commits, launcher source/run-list integration,
catalog registration, Privacy/Check-up views, whole-project native/i386 gates,
and live sessions. This worker ran no game, GUI, benchmark or real user-home
write. A live verification worker has the stock/CoD2x isolated-home opt-in,
retention and opt-out contract; its results are separate evidence.
