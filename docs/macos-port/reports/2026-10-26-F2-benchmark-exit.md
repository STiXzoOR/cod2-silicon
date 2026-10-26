# Benchmark cleanup during process-group exit

The benchmark wrapper could finish its timedemo successfully and then fail with
`Operation not permitted` while stopping its private process group. The failure
was reproduced with the synthetic engine used by `test_benchmark_completion_and_shutdown`;
no game, VM, renderer or external network was involved.

## Cause and evidence

`run.sh` launches the engine and `tee` as a pipeline. `benchmark.stop()` sends
SIGTERM to that entire group, then probes the group with signal zero. On this
Mac, the probe can report EPERM while group members are exiting. The previous
repair (`f494bf9`) reaped the known leader and immediately retried once. It
assumed both that an exiting leader was already reapable and that reaping it
finished the whole group's teardown. Neither assumption holds.

Private instrumentation captured both orderings:

- `repro7.log`: SIGTERM succeeded; the first poll and the poll after EPERM both
  returned `None`. A subsequent process listing showed only the leader as a
  same-user zombie. The error handler had raised before its exit was reapable.
- `repro68.log`: after successful SIGTERM, direct `proc_listpids(PROC_PGRP_ONLY)`
  enumeration at the first EPERM returned leader 1827 and descendants 1836/1837.
  Poll then reaped 1827 with status -15. The immediate retry still returned EPERM,
  and direct enumeration still returned descendant 1836. The group was gone by
  the later `ps` call. `proc_pidinfo` returned no record during this transition;
  the evidence establishes remaining group membership, not a recovered process
  status or command name for that descendant.

The first attempted correction waited only for the leader. Its deterministic
leader regression passed, but the real completion test still failed and the
second trace exposed the incomplete assumption. That attempt was replaced,
not accepted as a successful fix.

## Correction

`stop()` now establishes one grace deadline before the first group signal.
An EPERM causes it to poll/reap its known child and wait at most 50 ms before
retrying the same group operation, within that deadline. It does not treat
EPERM as success: the operation must succeed or the group must disappear
(ESRCH). Persistent EPERM is re-raised when the deadline expires, with one immediate
post-poll retry retained if an operation starts after the deadline. Signal targets,
SIGTERM/SIGKILL escalation and whole-group cleanup remain unchanged.

The timeout begins before signaling instead of after signaling, so the retry
cannot create an additional unbounded permission-error loop. As before, waiting
for the known child after SIGKILL is not given a separate timeout. No changes
were made to `run.sh`, benchmark timing arguments or engine code.

## Validation

Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F2-benchmark-eperm.noindex/`.

- The new exiting-leader test failed against the prior implementation
  (`baseline-transition.log`).
- The new remaining-group test failed against the incomplete leader-only fix
  (`baseline-descendant.log`).
- The two transition tests and persistent-permission test pass. Mocked monotonic
  time and sleep verify the deadline without real waiting; persistent permission
  errors propagate as the original exception with both live and reaped leaders
  (`deterministic-fixed.log`).
- `taskpolicy -b nice -n 19 python3 -m unittest discover -s tools/tests -p 'test_*.py'`
  passed all 61 tests (`tools-fixed.log`). This includes real synthetic completion,
  timeout rejection, unreaped-leader cleanup and termination of a surviving child
  which ignores SIGTERM. The subsequent test-only change replaced wall-clock
  waiting with mocked time in the new regressions; those three tests were rerun.
- Five additional executions of the exact synthetic completion test passed,
  recorded in `exact-fixed-0.log` through `exact-fixed-4.log`.
- `git diff --check` passes.

The worktree started clean at `60b94db` on `decomp/f2-benchmark-exit`. Only
`tools/macos/benchmark.py`, `tools/tests/test_verification.py` and this report
belong to the change. Engine fidelity, live animation speed and game benchmark
performance are outside this repair. Root integration owns fresh simplification
and code review before shipping.

## Review correction

Review `20261026-124807-ed974b23` found that the shared deadline had removed
the earlier immediate retry when SIGKILL escalation began after grace expired.
The exact mocked sequence (grace .2, monotonic 0/0/.3/.3, reaped leader,
SIGTERM/probe success followed by EPERM then ESRCH on SIGKILL) failed before
the correction (`review-escalation-before.log`). This is a demonstrated control-
flow regression; its frequency in real Darwin teardown was not measured.

Each group operation now retains one immediate post-poll retry when its first
EPERM arrives after the deadline. Later retries remain bounded by the same
deadline. The new escalation regressions require the successful transient case
to reach `wait()` and the persistent case to raise the original exception after
exactly two SIGKILL attempts, without sleeping after the deadline. Existing
transition and persistent-denial proof tests remain unchanged.


After the review correction, all 21 verification tests pass
(`review-verification-after.log`) and the full tools suite passes all 63 tests
(`review-tools-after.log`). `git diff --check` passes. The earlier five repeated
completion runs preceded this review correction; both final suites also include
the real synthetic completion scenario. No additional game run was needed for
this process-cleanup correction.

## Integration review and verification

The integrator applied all five simplification rubrics without changing code or
proof fixtures. Completed local and independent Claude review
`20261026-124807-ed974b23` retained one P2 escalation regression, repaired above.
Claude Opus 5.5 was confirmed; high effort was requested but actual effort was
not independently reported. The final integrated tools suite passed all 63 tests
in 29.558 seconds, including real synthetic process-group cleanup.
