# F1 shared wave5 integration

Baseline `1c8fc45`. This batch corrects three mover behaviors, two FX scheduler
filters and three missile-impact branches. Module reports describe Mac evidence
and targeted regressions. It remains a bounded part of the faithful MP plan.

Stock and CoD2x client/dedicated builds pass. Mover, FX primitive/scheduler and
all thirteen missile groups pass under ASan/UBSan. The existing grenade
integration fixture also passes in both configurations after its shared
instrumentation changed. Final mover checks follow the equivalent named-mask
cleanup. All three changed engine sources are byte-identical to baseline under
`unifdef -UCOD2_X64`; whitespace checks pass. No whole-tree known-red gate or
unavailable Linux binary check is claimed green.

Live checks used read-only data, separate private homes and unique localhost
ports, with only one game process at a time:

- Stock Toujane replay: 2000 frames, 3776 live player rows starting at frame116,
  zero script errors. Entire quantized state and raw player files match wave4.
- CoD2x Carentan: 600 frames, 784 live player rows starting at frame112,
  zero script errors. This was a live smoke, not a cross-build replay oracle.

The stock corpus does not exercise every corrected impact/mover branch; focused
fixtures establish those outcomes. FX tests exercise real creation/update and
spawn filtering, but live renderer captures remain future F2 work. The earlier
native/Linux oracle divergence remains unresolved; this batch does not claim
full original-server parity.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/` under
`F1-shared-wave5.noindex`, `F1-missile-impact.noindex`, `F1-mover.noindex` and
`F1-fx-scheduler.noindex`. Review receipt is recorded below before integration.

Review completed with no retained findings: correctness plus an independent
Claude adversarial pass, receipt `20261024-wave5-w0zia41t`. The peer raised
unknown HINT_NOICON behavior; adjudication rejected that concern because the
Mac authority has the same behavior and the valid inherited token is the
intentional correction. No review fixes or unresolved batch findings remain.
Review was static; the final execution checks above were caller-run.
