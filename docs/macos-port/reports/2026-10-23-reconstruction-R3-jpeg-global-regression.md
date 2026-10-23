# R3: whole-export JPEG candidate regression

The frozen four-file JPEG recovery candidate passes a whole-generation export
contract in both the worker and fresh parent campaign: 38,715 checks each, zero
rejections, 8,115 status records and 7,481 complete C outputs. Exactly one C output
changes from accepted generation 15, for JPEG function `22a32b` at `0x22b32b`.
It matches the complete output frozen by the
[real-protocol preflight](2026-10-23-reconstruction-R3-jpeg-protocol.md).

The campaigns reuse the already rebuilt and verified parent `ghidra_opt` binary;
there is no source change or redundant rebuild. Each uses a separate complete
candidate distribution and a fresh copy of the closed accepted project. Actual
native-child process observations bind execution to that private candidate
binary, SHA-256
`81254bbc5381aea8c0778f7198fb85a4ec15c5473e73cd9b1cf0469446272226`.

The expected status order, diagnostics, complete C hashes and metadata surfaces
were frozen before execution. Only the known JPEG output was allowed to change.
All 7,481 functions complete on their first 30-second attempt; 634 external
entries have no body. No unknown recovery or difference is retroactively allowed.
The 90-second failure-only retry and 64 MiB payload limit remain unchanged.

The independent parent audit compares complete C and status records directly
against accepted generation 15, verifies every emitted C hash/size and the
actual child executable, and confirms the one exact changed output. It checks
7,503 other metadata files and all 169,281 recursive-data records. Only the
already qualified process-generated `/undefined` singleton UID differs in the
recursive-data comparison, at the same 24,518 occurrences. All other fields and
persistent identities remain exact. Full distributions, projects and JDK inputs
are unchanged before/after each campaign.

Ten static comparator controls pass, including the parent rerun: missing, extra
or incorrect C; status, diagnostic, timeout-budget and metadata drift; unchanged
positives; and a separately classified ineffective elapsed-time mutation. These
test the comparator, not mutated JVM implementations. Full attempt timings are
retained, but this campaign does not claim performance equivalence to a baseline
run under different system load.

Private evidence is under `reconstruction/phase-r3/`:

- `20261023-jpeg-global-candidate-regression`, inventory SHA-256
  `7fb88d229a023a31aacd99a878750536cdba9161005545fdfccd47df9074abc1`.
- `20261023-jpeg-global-parent-source` and
  `20261023-jpeg-global-parent-fresh` hold the fresh replay.
- `20261023-nineteenth-wave-parent-verification` holds independent audits,
  comparator controls and complete command receipts. Its first inventory read
  failed before any replay because the JPEG manifest omits the root directory
  from its subdirectory list; the corrected reader explicitly includes the root
  while preserving all file, hash, census and permission checks.

This accepts the measured whole-export regression contract. It does not adopt
the installed tool, promote generation 15, complete formal source-adoption
review, or establish full JPEG semantics. Existing exporter omissions for local
symbols and complete database history remain explicit. Native full suites were
not repeated for this report-only change.
