# R3: metadata serialization completes under the original heap limit

A minimal UTF-8 Writer serializer resolves the
[reproduced export failure](2026-10-23-reconstruction-R3-server-metadata-failure.md)
without increasing the configured 2 GiB heap. Both the frozen worker campaign
and fresh parent replay complete all five full metadata snapshots. The
unchanged strict audit rejects seven annotation consequences in both runs;
serialization success does not establish metadata acceptance.

The only serialization change replaces construction of a complete JSON string
with Gson writing to a buffered UTF-8 Writer. Record construction, extraction,
annotation APIs, comparator and frozen logical contract remain unchanged.
Both runs record `-Xmx2G`, `Runtime.maxMemory()` of 2,075,918,336 bytes and one
active processor before the workload. Peak live heap was not sampled.

Eight focused cases compare exact old/new output bytes using the pinned Gson
jar and verify CREATE_NEW preservation. They cover representative nested,
null, escaped, Unicode and numeric values; they do not exhaust every encoding
or numeric value. All 16 inherited static comparator controls pass, including
14 negative controls, a positive and a separately identified ineffective
mutation. These controls are not live JVM mutation experiments.

The completed snapshots cover 8,115 functions. Recursive namespace symbols
increase from 39,880 to 39,883; complete public label-history records from
77,728 to 77,731; datatype records from 181 to 190. Original and final stored
snapshots match the frozen expectations and the worker exactly. Each pair of
no-op snapshots is byte-identical. Accepted and private closed-project files
remain unchanged.

The seven strict failures are two unpredicted parameter-reference sets and
five named-type source timestamps. The new connectionless-packet parameters
expose 30 and three stack references already present in the original stored
instruction projection; the contract requires empty sets. Five named types
report nonzero source timestamps equal to their last-change timestamps within
the annotation window; the contract requires zero. No allowance was changed
after seeing these results. Headless execution exits 0, while audit and
effective exits remain 1.

The parent independently checks the exact changed function set, preservation
of old namespace/variable/type/history records, new type definitions and the
33 pre-existing references. It verifies the serializer delta and unchanged
contract/comparator. The next evidence task is to derive prospective rules
for the newly observed metadata from the implementation and original inputs;
this rejected run cannot be relabeled as accepted.

Private evidence is under `reconstruction/phase-r3/`:

- `20261023-server-metadata-streaming`, inventory SHA-256
  `038b26ba782873318d2ae7e5e98b368d59f38e8208b71536e44b5e00f699bb1a`.
- `20261023-server-metadata-streaming-parent-source` and
  `20261023-server-metadata-streaming-parent-fresh` retain the fresh replay.
- `20261023-twentieth-wave-parent-verification` contains independent checks
  and command receipts.

Both prior OOM runs remain sealed. One postrun reporting assertion omitted
intentionally changed dispatch/replay documents from its expected file list;
its failure and corrected static check are preserved, without a JVM retry.
No native source, installed tool or authoritative generation changes. Full
native suites were not repeated for this private serializer/report slice.
