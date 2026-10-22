# JPEG guarded-reload provenance candidate (R3)

A private decompiler candidate preserves the original JPEG switch and both
no-write positives while refusing all four known unsafe memory-write cases.
All four refusals occur at the genuine indirect site in complete, well-formed
XML. This accepts bounded candidate evidence; installed tools and accepted
generation 15 remain unchanged.

The corrected baseline loses direct memory writes during ActionDeadCode before
jump-table recognition. A new Funcdata-owned snapshot preserves typed raw P-code
effects and CFG edges immediately before that simplification. The two added
reload-recognition paths query it using operation address plus creation-time
identity, LOAD space and width. A write between the guard and reload in an
overlapping address space, an unknown effect or unavailable identity declines
recognition. Traversal terminates on finite cyclic graphs and conservatively
includes syntactic paths without proving branch feasibility.

Ownership is local to each Funcdata; snapshots are replaced on capture and freed
on clear/destruction. The candidate changes four private files: funcdata.hh,
funcdata.cc, funcdata_block.cc and the corrected jumptable.cc. Its class layout
change required all 94 candidate objects to be freshly compiled. The diagnostic
baseline separately rebuilt one translation unit and reused 93 hash-bound
objects. Upstream MULTIEQUAL handling and the existing surviving-write helper
remain unchanged; the rejected temporary-refusal guards are not carried forward.

Both worker and parent fresh campaigns passed:

- Seven focused cases, preserving JPEG's six ordered slots and duplicates.
- Fifteen retained cases, nine switch contracts, 52 upstream assertions and
  seven existing helper checks.
- Two hundred finite original synthetic i386 paths.
- Sixteen typed path/effect checks and eight ownership/lifecycle checks.
- Three ablations that lose required positives or reproduce unsafe tables when
  the corresponding capture/check is removed.

Parent verification independently parsed full XML, located actual indirect sites,
checked reachable targets from original instruction traces, and matched captured
write identities to actual baseline deletion events. All four negative fixtures
have an independently reachable target omitted by the old false-complete table.
Candidate focused XML and source hashes match the worker's sealed packet.

Three historical malformed full documents and two expected parser exits retain
their explicit qualifications; they are not counted as valid refusal documents.
Two failed lifecycle harness runs remain preserved before the corrected harness
passed. Candidate mechanism source did not change during those corrections.
The finite synthetic domain, conservative CFG analysis and private tests do not
prove full alias analysis, original JPEG process equivalence or general closure.

Private phase-r3 evidence:

- `20261023-jpeg-preaction-write-provenance`, manifest
  `2e509d449920c79ae19146246c6238842fcd326aee322aa0451c4a42ab460a4d`.
- `20261023-jpeg-preaction-parent-fresh`, seal
  `3980f4e51fd96ad3ce511699523e859dddabc7eafd8d63984e951abcf73f0289`.
- `20261023-fifteenth-wave-parent-verification`, independent classification and
  fresh replay receipts. Parent JPEG replay completed in 155.4 seconds.

No native production behavior or canonical tests changed; native game tests were
not rerun for this report. Installed adoption and generation-wide verification
remain separate work.
