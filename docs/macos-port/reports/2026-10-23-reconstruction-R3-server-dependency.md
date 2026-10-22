# R3: exact server-signature dependency preflight

A newly frozen bounded contract passes with the exact unused `/undefined *32`
dependency admitted. The [earlier strict rejection](2026-10-23-reconstruction-R3-server-signature-preflight.md)
remains a failure under its original contract. No native code, installed tool
or accepted reconstruction generation changes in this slice.

The allowance is one complete type record: `PointerDB`, length 4, alignment 4
and the exact definition. It is checked immediately after annotation and again
after decompilation. Every prior type must remain unchanged; stored functions
and composite fields must not reference the placeholder. The eight intended
signature-related types are otherwise unchanged. Extra types, wrong pointer
width and placeholder use in either location are rejected by effective controls.

The same two original-backed aggregate signatures are applied to private,
disposable copies of the closed generation-15 project. The known tail-call
override is measured separately. Three functions are decompiled in each of
three stages, with the unchanged 30-second first attempt, 90-second failure-only
retry and 64 MiB policy. All nine complete on their first attempt without
diagnostics; all seven driver stages finish successfully.

Both the worker and fresh parent replay verify full C, high Pcode, result
records and all five stored-metadata snapshots against the frozen prior
observations. The two signature changes preserve every old variable in exact
order and preserve the remaining function properties across all 8,115 records.
The complete three-function instruction/CFG snapshots are unchanged by signature
annotation, and all 1,069 instructions bind to original bytes. The inherited
driver's 995 label describes its earlier prefix binding scope, not that complete
denominator. Full debug documents are retained and parse successfully.

The first parent replay failed before any probe because the materializer omitted
`static/prototype-evidence.json`, which the driver reads. The failed source and
run are sealed. A new source/output pair adds that already sealed, hash-checked
input; the driver and acceptance contract are unchanged. The fresh replay and
independently authored parent metadata/instruction checks pass. The worker's
earlier bootstrap failure is also retained, with its malformed inline source
available only in the tool conversation.

A static prospective global contract records the actual caller chain and 1,560
reference edges. It allows only the three already measured complete C changes;
unknown wider inference or symbol/history changes remain unaccepted. No global
554-probe or 8,115-function campaign has run under that future contract.

Private evidence is under `reconstruction/phase-r3/`:

- Worker: `20261023-server-signature-dependency-preflight`, inventory SHA-256
  `624d20ea20b7728d7b2e8e98ce689715530c28d2431fe597b45e12eea8b8b359`.
- Successful parent: `20261023-server-dependency-parent-fresh-v02`, inventory
  SHA-256 `23679aa9381b9cb6e8f9673aa27efe95328b87fc47d75039192e0bc06990384d`.
- Parent audit: `20261023-server-dependency-parent-audit`, inventory SHA-256
  `b7108b2aea1f9025f90d9ce71c82fc42edbec464cccfd502b6448debee41d1f5`.
- Independent checks and full command receipts:
  `20261023-seventeenth-wave-parent-verification`.

This report accepts the bounded preflight only. A separately frozen global
application and parent verification are required before generation promotion.
Native tests were not repeated for this report-only change.
