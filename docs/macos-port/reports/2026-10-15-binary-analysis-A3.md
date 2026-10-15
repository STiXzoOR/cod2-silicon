# A3 — full private export against the frozen discovery inventory

Date: 2026-10-15. Scope: the three owned Mac i386 targets from
[A0](2026-10-15-binary-analysis-A0.md), using the final
[A2 inventory](2026-10-15-binary-analysis-A2.md). A3 is complete for this declared
static inventory: every function/import and every Rizin candidate has a durable
outcome. This is not a claim of original-source recovery or complete runtime
behavior, and it does not include unverified Windows/server variants.

| Target | Inventory records | Decompiled bodies | No decoded body | External imports | Successful retries | Pseudocode with warnings, including no-body records |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Mac MP 1.3 reference | 8,111 | 7,477 | 0 | 634 | 0 | 1,257 |
| Steam Mac MP | 8,653 | 7,950 | 1 | 702 | 1 | 1,146 |
| Steam Mac SP | 7,971 | 7,350 | 1 | 620 | 1 | 1,011 |

Totals: 24,735 inventory records; 22,777 decompiled bodies, two synthetic
records with no decoded instructions, and 1,956 external imports. Counts are
across three binaries, not unique functions deduplicated across versions.
There are no remaining provider timeout/failure outcomes for accepted bodies.
One function in each Steam build timed out at 30 seconds and succeeded on its
single 90-second retry; both attempts remain recorded. The two no-body records
retain provider-emitted text and the original provider result, but are excluded
from the successful-body denominator.

Export contract and private deliverables:

- Pinned Ghidra 12.1.4, i386 language, persistent projects; bulk export is offline
  and reads the frozen analyzed programs without changing them.
- Paths keyed by binary SHA-256, architecture and image-base-relative entry
  address; external records use a separate namespace.
- Per-function pseudocode, original-byte assembly, CFGs, outgoing xrefs and
  incoming entry xrefs. Global xrefs retain references to function interiors.
  Aliases, thunk targets, computed-flow sites and discovery provenance remain
  attached to the inventory/evidence.
- Atomic per-function results, checkpoint every 100 records, 30/90-second retry
  budgets, 64 MiB provider payload limit, explicit diagnostic and truncation
  fields. No truncation was reported. Warnings are retained verbatim privately;
  emitted pseudocode does not certify correct types or semantics.
- The initial export was resumed after recovering 348 additional boundaries.
  All existing function-evidence hashes were unchanged; prior pseudocode and
  evidence hashes were checked before reuse. Initial inventories and transition
  receipts remain archived.
- All 48,301 Rizin candidates have separate disposition records. The reference
  build retains 5,045 unresolved candidates outside accepted function bodies
  (5,043 in unknown executable bytes and two inside decoded instructions).
  Candidate starts inside existing bodies, data labels and non-executable
  locations are classified separately rather than counted as decompilations.
- A private SQLite/FTS5 index contains all 24,735 records and searchable emitted
  pseudocode. The local search helper supplements full-text search with literal
  name substrings so familiar names still match mangled symbols.

Validation passed: exact inventory-ID/ledger equality; external/internal
classification; nonempty emitted text; every pseudocode and assembly/CFG/xref
artifact hash; all original and private-copy SHA-256 identities; SQLite
integrity and three search smoke checks. All six earlier REA pilot functions
retain identical body sizes and pseudocode after whitespace trimming. A2's
independent byte check matched all 1,700,685 instructions to input-file bytes.
The successful resume exercised the checkpoint path against the expanded
inventory. No game process or runtime build was needed for this analysis-only
work.

All binary-derived material remains under the owner-only external workspace
(`0700`) declared in the plan. `exports/full/` contains the artifacts and search
index; `receipts/A3-validation.json`, `A3-search-validation.json`,
`A3-pilot-crosscheck.json` and `A3-output-manifest.json` retain the audit.
The output manifest binds 92,915 private files, including scripts and receipts.
Its SHA-256 is:

2900cc5cc9e122e73c98d5dbee6bc922e2641d58a0d054cf85a0590d9e9ce2d2

Only this sanitized report and the plan update are committed. No binary,
pseudocode, disassembly, symbol dump, private database or diagnostic log is
published. Native source and main are unchanged.

Remaining work: the 401,761 unclassified instruction-section bytes, unresolved
indirect targets/candidate boundaries, warning review, types, source mapping
and behavioral validation remain visible in A2 and belong to A4–A6. This export
provides a searchable reference for that work; it does not certify a fully
validated reconstruction or Metal renderer behavior.
