# Native client and demo timing

The native client now separates ordinary demo freezing from timedemo benchmark
advancement, as Mac 1.3 does. `demo` follows the ordinary client clock;
`cl_freezeDemo 1` holds that clock. `timedemo` advances 50 ms per rendered frame,
with either freeze value. At 60 FPS that benchmark plays approximately three
seconds of recorded action per wall-clock second. This change preserves the server simulation rate and benchmark step size;
it also corrects live client-clock smoothing.

`CL_SetCGameTime` also marks extrapolation when the unclamped client clock is
at or beyond five milliseconds before the latest snapshot. The reconstruction
had reversed that comparison. The real `CL_AdjustTimeDelta` then consumes that
flag and subtracts two milliseconds while smoothing a live connection. Its
extrapolation diagnostic now uses the original literal rather than a floating
format conversion without an argument.

## Source comparison

The complete Mac 1.3 bodies of `CL_SetCGameTime` (`0x17ba76`),
`CL_AdjustTimeDelta` (`0x17b544`) and `CL_PlayDemo_f` (`0x14c288`) were compared:
536 instructions, 2,182 bytes, checked against the original Mach-O bytes.

- `0x17bb95`–`0x17bba6`: only demo playback checks `cl_freezeDemo`; a true
  value skips ordinary clock advancement and continues at snapshot processing.
  The import cell at `0x1acd671` points to `_cl_freezeDemo` at `0x17e06a8`.
- `0x17bbb6` and `0x17bdce`: adjust the clock for new snapshots before demo
  benchmark processing. `0x17bbcb` returns for a live connection;
  `0x17bbd1`–`0x17bbd9` checks `isTimeDemo` before logging and fixed advancement.
- `0x17bc1a`–`0x17bc40`: increment the benchmark frame count and calculate
  base time plus frame count times 50. The existing logging body is retained
  in a native helper at the original point in the call sequence.
- `0x17bcb7`–`0x17bcbf`: a signed less-than branch skips setting extrapolation;
  equality sets it. The comparison uses realtime plus delta before the
  monotonic old-time clamp. It does not clear an already-set flag.
- `0x17bcda` references `Extrapolating snapshot!\n`. No floating argument is
  passed by the original call.

Priming, first-frame skip, pause gates, localhost restart, first-snapshot
initialization and demo-message consumption otherwise correspond. `CL_PlayDemo_f`
already selects `isTimeDemo` from the command name. Its native short-filename
safety check and CoD2x demo callback remain unchanged. No intentional CoD2x
clock override was found in its implementation source.

`CL_AdjustTimeDelta` otherwise corresponds for ordinary valid values. Its
existing widened average intentionally avoids signed overflow at large epochs.
The existing test for that behavior is preserved. Its reset/average diagnostic
prefixes and the invalid-snapshot error wording still differ from the original;
this unit does not change them or claim complete diagnostic-text parity.

The engine changes are native-only. No headers, layouts, clock-rate dvars or
original blobs change. The old benchmark body remains behind the legacy gate
to preserve the exact `COD2_X64=OFF` source. The benchmark wrapper now sets
freeze to zero and selects benchmarking explicitly with `timedemo`; its README
explains accelerated playback and removes the obsolete reconstruction quirk.

## Verification

`tests/online/run_client_time.py` includes the full production translation unit
and executes both real time functions. Synthetic services supply dvars, time,
demo messages and filesystem operations. There is no extracted or rewritten
clock implementation in the fixture.

Both stock and CoD2x pass all 13 groups with AddressSanitizer and
UndefinedBehaviorSanitizer:

- live monotonic clock, ordinary demo clock and frozen ordinary demo;
- timedemo with both freeze values, logging order, frame/counter progression,
  zero first timestamp and retry after a failed log open;
- extrapolation below/at/above the threshold, unclamped clock comparison,
  preservation of an existing flag, actual producer/consumer smoothing,
  non-unit timescale and the exact debug literal;
- demo-message consumption/state exit, priming and skipped first frame,
  pause combinations, inactive state, invalid snapshot and remote backwards
  time errors, and localhost restart.

Before changing production source, the same fixture failed seven groups and
passed six controls in each mode. The failures cover the corrected gates,
ordering, comparison, smoothing and diagnostic. The ordinary live-clock control
passed before and after the fix.

Example commands, using the integration checkout's real engine databases:

```sh
for build in build-macos build-macos-codx; do
  taskpolicy -b nice -n 19 python3 tests/online/run_client_time.py \
    --compile-db "$INTEGRATION/$build/compile_commands.engine.json" \
    --output "$EVIDENCE/$build"
done
```

The runner rebases source/include paths to its own checkout. `--baseline c2360e9`
selects the earlier engine source without changing the fixture. The engine
translation unit also compiles separately with each original production command.
The unchanged `tests/online/time_delta.c` large-epoch regression passes in both
modes through `tests/online/run.py ... time_delta`.

The updated benchmark-wrapper test first failed against the old freeze=1
command. After the wrapper fix, it verifies freeze=0, the explicit timedemo
command, completion and shutdown. One attempt reached completion but failed in
existing process-group cleanup with `Operation not permitted`; the targeted
retry passed. That transient cleanup failure is retained in private evidence,
not attributed to game timing and not hidden by changing process cleanup.

`unifdef -UCOD2_X64` output compares byte-for-byte with `c2360e9` for the engine
file. `tools/abi/legacy.py --base c2360e9` reports one file, five configurations,
zero mismatches. The optional actual arm64 SDK preprocessing check with
`COD2_X64` removed cannot run this legacy translation unit: its original
`mmintrin.h` rejects the arm64 target. This is not an i386 object-equivalence
claim. `git diff --check` passes.

Private audit, raw-byte verification and all logs are under
`~/Library/Application Support/CoD2-native-decomp/F2-client-time.noindex/`.
No gameplay, live demo, VM or network replay was run by this worker. The
integrator owns linked builds and live playback checks. These synthetic clock
checks do not establish full multiplayer or animation parity.

## Integration and live playback

The canonical integration passes all 13 timing groups with ASan/UBSan in both
configurations and links both Release clients and both dedicated servers.
CI runs the new fixture in both configurations. Exact OFF output remains
43,386 bytes, identical to `60b94db`; five guard and ten fixes13 configurations
pass. The integrated wrapper test reproduces the previously reported EPERM
cleanup failure; that separate tooling issue is under diagnosis. No wrapper
success is claimed for this integration run.

The completed independent review found no actionable production defect. Its
optional clarification distinguishing server rate from client smoothing is
applied in the opening paragraph. Receipt:
`/tmp/compound-engineering-501/ce-code-review/20261026-122646-8a93a7a8`.
The configured peer was Claude Opus 5.5; high effort was requested, actual effort
is unverified. All five simplification lenses completed without code changes;
five optional comment deletions were skipped to preserve the proof fixture.

Six actual client runs used the same private recordings of the earlier normal
speed walk/fire/reload scenario, with timescale 1 and fixedtime 0. Ordinary
`demo` completed in 9.906 seconds stock and 9.896 seconds CoD2x, measured from
the observed cgame initialization message to the demo-ending disconnect.
With initial freeze 1 released after about 3.05 seconds, completion took 9.915
and 10.131 seconds total. Freeze skips the ordinary clock update; realtime
continues, so resuming can catch up rather than extend the recording's duration.
The synthetic fixture directly verifies the frozen clock. These live samples
do not independently measure every animation or frozen frame.

With freeze 0, `timedemo` reports 193 frames / 3.9 seconds stock and 195 frames /
4.2 seconds CoD2x. Both generated CSVs; ordinary/frozen playback generated none.
These are completion checks that cross real snapshot boundaries, not controlled
performance comparisons or proof of reference-identical frame counts. All six
runs exited zero. The existing demo-ending disconnect and missing beep warnings
remain in logs. The native graphics lease was held throughout and the reference
VM was suspended and restored to its started state; no guest command was used.
Evidence: `F2-client-time.noindex/integration-*.log`, `live-results.json`,
and the six `live-*/console.log` files.

After integration, the cached full function ABI audits also pass all 634 stock
and 651 CoD2x translation units with zero errors, mismatches or new findings.
The preceding complete callback/strict-AST/import checkpoint is recorded in
the wave report; those broader results predate this timing-only change.
