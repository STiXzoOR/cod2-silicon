# R3 — Decompilation and source discovery checkpoint

Status: bounded discovery work complete; R3 and full reconstruction remain open.
The user prioritized decompilation before further Metal integration. Metal stays
paused after U2. Multiplayer remains the first reconstruction target.

All original binaries, decompiler output, execution traces and detailed recovery
specifications remain outside Git in the private analysis workspace. This report
contains aggregate results only. The original A0–A6 evidence is unchanged.

## Export integrity and discovery

The three original Mac exports were independently checked against their inventory,
per-record statuses and content hashes: 24,735 records, including 22,777 decompiled
bodies, 1,956 external entries and two explicit loader placeholders without decoded
bodies. These are export-accounting results, not equivalent reconstructed functions.

The corrected MP inventory has 8,115 entries, including four boundary discoveries
from the earlier R3 pass. The latest discovery generation preserves that inventory
and every preexisting instruction while applying two verified dispatch tables and
partial function-body repairs. A fresh full MP export contains 7,481 decompiled
bodies and 634 external entries; per-record status, assembly/CFG/xref hashes and
pseudocode hashes all pass. Its nine project files remain unchanged during the
read-only export.

The fresh pseudocode is byte-identical to the prior export, including the two
repaired functions. Their assembly, ownership and control-flow records improved,
but unresolved jump-table warnings remain in pseudocode. The next decompiler task
is to reconcile those warnings with the verified dispatch evidence; this checkpoint
does not claim improved pseudocode recovery.

| MP discovery measure | Before this checkpoint | Latest generation |
| --- | ---: | ---: |
| Inventory entries | 8,115 | 8,115 |
| Decoded instructions | 601,076 | 601,472 |
| Unknown executable bytes | 70,251 | 68,454 |
| Unknown instruction-section bytes | 67,142 | 65,345 |

The 1,797 newly classified bytes comprise 1,717 instruction bytes and 80 dispatch
table bytes. Original-entry execution, independent instruction decoding and
control-flow checks support the corrections. A separate ownership replay passed
138 bounded runs and 7,708 instruction/edge checks. It supports two partial bodies;
97 and eight unknown bytes respectively remain excluded, together with the tables.
No whole-function semantic equivalence is claimed.

Unknown intervals increase from 4,617 to 4,621 because resolving portions splits
existing intervals. A smaller byte count does not imply fewer unresolved regions.
There are still 2,690 owned computed sites without static targets and four additional
unowned computed sites. Having some static targets does not prove that an indirect
site's runtime target set is complete.

## Source correspondence

A fresh syntax-only index covers all 400 translation units in the active source
graph: 396 repository units and four generated units. It contains 7,793 main-file
function definitions, including 2,396 static definitions. Matching against the
original debug identities yields 712 unique static candidates: 141 retained from
the earlier bounded pass and 571 additions. The original 1,983 unmatched-function
denominator is retained; 1,271 still lack a candidate from this pass.

A macro-location attribution bug was found and corrected before accepting this
index. All 400 units were re-extracted; synthetic location regressions pass, all
2,717 inputs remain unchanged, and all 5,276 prior main-file nonstatic file/name
pairs are retained. The private evidence manifest independently verifies 4,068
files. Preliminary results are preserved and explicitly superseded.

These are source candidates, not signature, ABI or behavior certifications. The
index excludes header-inline definitions, Objective-C methods, C++ members and
templates, and aliases without bodies. The existing public ABI audit is unchanged.

## Remaining work

Continue executable-region and indirect-target discovery, complete partial bodies,
and corroborate source candidates against original instructions. Runtime objects,
initialization, required data layouts and behavioral comparisons remain separate
requirements in the [reconstruction plan](../../plans/2026-10-15-validated-reconstruction.md).
Successful pseudocode export does not close these requirements. No Metal code or
native gameplay implementation changes are included in this checkpoint.
