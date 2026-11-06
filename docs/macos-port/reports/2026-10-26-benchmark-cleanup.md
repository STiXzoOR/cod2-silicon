# Benchmark cleanup: Darwin zombie groups

2026-10-26; base `204fb19`. This repairs the tooling failure encountered while
running the faithful-decompilation merge gates; no engine behavior changes.

## Cause and evidence

The unchanged `benchmark.stop()` occasionally failed with `PermissionError`
after terminating its wrapper. A captured failure showed `killpg(pgid, 0)`
returning EPERM while the group contained only its same-user wrapper in zombie
state. The wrapper had exited between the preceding `process.poll()` and the
group probe. Repeating the probe without reaping still returned EPERM.

A separate five-case process probe reproduced the transition deterministically:
each unreaped group leader returned EPERM; after `process.poll()` reaped it,
the same group probe returned ESRCH. The actual benchmark-completion fixture
also reproduced the original failure before the repair. Evidence remains in
`~/Library/Application Support/CoD2-native-decomp/F2-benchmark-cleanup.noindex/`.

Apple's published [XNU group-signalling implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_sig.c#L1580-L1624)
explains this result: group iteration excludes zombies, and finding no eligible
member in an existing group returns EPERM. Once the leader is reaped, its empty
group disappears and lookup returns ESRCH. This was observed on Darwin 27.0.0;
the published source supports the mechanism rather than proving the exact
running kernel's identity. No prior matching issue or PR was found.

## Repair

Each group signal now has one narrow retry. On PermissionError, poll the known
wrapper again. If it remains live, propagate the failure immediately. If it
has exited, retry the same group signal once. Existing ESRCH handling confirms
that the group disappeared; a repeated permission failure still propagates.
TERM, the signal-zero probe and the final KILL use this same rule. The grace
period and descendant cleanup behavior are unchanged.

Permission failure alone never counts as successful cleanup. This repair does
not claim to handle every possible zombie-descendant lifetime or process-group
permission policy; failures that persist after the single retry stay visible.

## Verification

Three regressions were added to the existing verification suite: a real
unreaped group leader using `waitid(..., WNOWAIT)`; a deterministic exit between
poll and group probe; and persistent permission failure with either a live or
exited wrapper. Before the repair, the first two failed with EPERM and the
persistent-error control passed. Afterwards the verification suite passed
**17/17**, including the existing hung-engine completion, timeout-must-fail and
surviving-descendant termination controls. The full tools suite passed **59/59**.

```sh
taskpolicy -b nice -n 19 python3 -m unittest tools.tests.test_verification -v
taskpolicy -b nice -n 19 python3 -m unittest discover -s tools/tests -v
git diff --check
```

The full suite includes both original benchmark scenarios; their assertions
were preserved. All observations used synthetic processes, without game data,
game execution or VM access. No full-game performance claim follows from this
cleanup repair.

## Review resolution

All five simplify lenses completed without changes. Focused correctness and
independent Claude Opus 5.5 review completed at
`/tmp/compound-engineering-501/ce-code-review/20261026-095542-4e456e6a/`.
Both findings were applied: #1 capability-gates the real zombie fixture because
macOS Python before 3.13 lacks `os.waitid`; the portable race/error fixtures
stay enabled. #2 asserts exactly one signal attempt for a live wrapper and
two for an exited wrapper with persistent denial.

After those test changes the canonical full tools suite passes 59/59
(`review-tools.log`). A separate process with `os.waitid` removed verifies that
only the real-process fixture skips while both portable controls still pass.
This simulates the missing capability, not an executed older Python runtime.
No actionable review finding remains.
