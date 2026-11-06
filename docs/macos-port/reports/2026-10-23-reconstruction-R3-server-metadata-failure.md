# R3: server metadata export stopped before annotation

The new audit of metadata omitted from the
[full server export](2026-10-23-reconstruction-R3-server-global-signatures.md)
fails before applying annotations. Both the frozen worker campaign and fresh
parent replay exit with `OutOfMemoryError: Java heap space` while serializing
the first BEFORE snapshot. No live metadata comparison completes, and the
missing parameter/local-symbol, label-history and datatype-identity evidence
remains unresolved.

The retained original stored snapshot contains 8,115 functions, 40,119 variable
records, 486 parameters, 181 datatypes and 1,069 selected instructions. The parent
snapshot equals the worker's exactly. Source and private closed-project files
remain unchanged. There is no first BEFORE file, AFTER file or completed driver
receipt; no annotation call is reached.

The new exporter constructs full Java collections and a Gson JSON tree, then
the inherited save helper requests a complete JSON string. Both stack traces
fail during the final `StringBuilder.toString` copy, before the file write.
The launcher-derived heap limit is 2 GiB; live JVM flags and heap occupancy were
not sampled. Expanded namespace/reference/history counts were not persisted.

Sixteen static comparator controls pass, including unchanged positives and a
separate ineffective control. They do not establish live metadata invariance.
No heap increase, retry of the worker campaign, annotation change or acceptance
relaxation follows the failure. A separately frozen streaming serializer under
the same budget is the next testable remediation hypothesis.

Private evidence is under `reconstruction/phase-r3/`:

- `20261023-server-metadata-characterization`, inventory SHA-256
  `414ea8e74c030cbabc086d01d033faf09fdfe303db1134a20bdb28d9fa3e606d`.
- `20261023-server-metadata-parent-source` and
  `20261023-server-metadata-parent-fresh` preserve the fresh reproduction.
- `20261023-nineteenth-wave-parent-verification` contains the independent
  failure check, diagnosis and command receipts.

No native source, installed tool or authoritative generation changes. Full
native suites were not repeated for this report-only failure record.
