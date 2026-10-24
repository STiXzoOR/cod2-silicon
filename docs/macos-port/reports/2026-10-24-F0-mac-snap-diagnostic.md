# F0 — Mac snapping diagnostic profile

This tooling unit adds an explicit `mac-snap-diagnostic` profile to the private
reference generator. `linux-stock` remains the default and preserves Linux
engine behavior. No production engine source or existing oracle evidence changes.

Mac 1.3 `__Z14Sys_SnapVectorPf`, `0xaed28–0xaed57`, uses `cvttss2si` then
`cvtsi2ss` for each vector component. Linux's `rint` rounds differently.
The independently written diagnostic body truncates finite int32-range values,
turns negative zero into positive zero, and produces float `INT_MIN` for invalid
or out-of-range conversion, matching the masked-invalid instruction result.
It does not emulate floating-point exception flags or unmasked traps.

The generator rejects unknown profiles and missing, duplicate or unclosed
snap functions before copying. Function discovery ignores comments and string
literals. Compared with default instrumentation, diagnostic generation changes
only the snap body and adds its profile metadata; no compiler flags or global
rounding controls change. The generated `oracle-profile.json` identifies source
generation and leaves the executable SHA null until an actual binary exists.
The README documents binding it to the built executable SHA and retaining both
profile and identity in every diagnostic run's `reference.json`. The existing
recorder hashes its container adapter, not the underlying executable.

## Verification

Seven new tests were observed failing before implementation: the profile API,
CLI choice and metadata were absent. They then passed:

```sh
python3 -m unittest discover -s tools/tests -p test_oracle_profiles.py -v
python3 -m unittest discover -s tools/tests -v
git diff --check
```

The full tools suite passes all 56 tests, including the seven profile tests;
`git diff --check` passes.

Tests use independently written synthetic source. They verify default snap
bytes are unchanged, only the intended diagnostic body changes, source remains
untouched, metadata distinguishes profiles, and malformed profile/function
inputs fail before copying. A generated-body C++98 fixture compiled with
`-fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all` checks 24
bit-pattern cases on all three vector components: signed fractions/ties/zero,
subnormals, int32 boundaries, infinities and quiet/signaling NaNs. Its host
compiler runs sequentially under `taskpolicy -b nice -n 19`.

No guest build, engine build or game process ran in this unit. The existing
VM started, but its configured SSH-agent authentication was rejected. Guest
compilation and replay await restoration of that access. No reference code,
game assets or disassembly entered the repository.

The existing native/reference raw spectator discrepancy at frame 102 remains
unresolved. Whether this profile removes the frame-116 quantized divergence is
unmeasured. Diagnostic agreement cannot be represented as original-server or
Mac-runtime agreement; stock evidence remains the independent reference.

## Integration review

Root reran the complete tools suite: 56 tests passed. Five simplify lenses
found two removable narrative comments, which were removed; proof tests were
retained. The focused ce-code-review completed with an independent Claude
review and one P2 documentation finding: reusing the stock recording block
could silently select the wrong executable for diagnostic traces.

Finding #1 was fixed with separate diagnostic replay/fetch commands, explicit
binary selection, profile/path/SHA checks before and after recording, and
retained reference.json. Follow-up checked all five diagnostic shell blocks
with bash -n and sh -n, accepted a synthetic matching identity and rejected
wrong profile/path/SHA. Whitespace checks pass. Review evidence is outside the
repository at /tmp/compound-engineering-501/ce-code-review/20261024-oracle-profile-ibrn1c76.
No actionable review finding remains; guest runtime verification remains pending.
