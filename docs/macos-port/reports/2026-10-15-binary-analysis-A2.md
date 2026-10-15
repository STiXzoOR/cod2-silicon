# A2 — whole-program discovery and range reconciliation

Date: 2026-10-15. Scope: the three owned Mac i386 targets identified by full
SHA-256 in the [A0 report](2026-10-15-binary-analysis-A0.md). Windows, dedicated
server and other unverified variants remain outside this run.

A2 is complete as an inventoried static discovery pass, with explicit unresolved
regions and candidate dispositions. It does not prove that every possible
runtime function or indirect target has been discovered.

Pinned Ghidra 12.1.4 analyzed each input into a persistent private project;
Rizin 0.9.1 independently ran `aaa;aflj` and exported strings. REA's earlier
Ghidra pilot remains the cross-check. All tools, binaries, projects, scripts,
symbols, assembly, pseudocode and diagnostic logs stay in the owner-only private
workspace outside Git. This commit contains only this report and plan status.

| Target | Internal functions | External imports | Internal thunks | Rizin candidates |
| --- | ---: | ---: | ---: | ---: |
| Mac MP 1.3 reference | 7,477 | 634 | 633 | 32,604 |
| Steam Mac MP | 7,951 | 702 | 703 | 8,144 |
| Steam Mac SP | 7,351 | 620 | 621 | 7,553 |

External imports are separate records without library implementations. Thunks
remain in the internal denominator and retain their target identity; aliases
share one entry-address record. Provider source classifications and the
explicit candidate-merge ledger distinguish imported names from heuristic
function-boundary recovery. Libraries linked by the original application were
not imported as additional binary targets.

The initial Ghidra pass left Rizin candidates in decoded instructions outside
function bodies. A bounded recovery pass attempted those candidates only at
existing instruction starts, rejected interior-byte starts, and retained
every attempt and its outcome. It created the following additional function
bodies without adding speculative disassembly:

- Mac MP 1.3 reference: 50 additional bodies from 62 candidates.
- Steam Mac MP: 168 additional bodies from 168 candidates.
- Steam Mac SP: 130 additional bodies from 130 candidates.

| Target | Instruction-section bytes | Decoded bytes | Defined data in those sections | Unclassified bytes | Decoded bytes outside functions |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mac MP 1.3 reference | 2,390,895 | 2,323,732 | 20 | 67,143 | 2,312 |
| Steam Mac MP | 2,436,010 | 2,263,816 | 588 | 171,606 | 1,590 |
| Steam Mac SP | 2,330,362 | 2,166,766 | 584 | 163,012 | 1,635 |

These are separate byte and function denominators. Unclassified bytes can
include alignment, jump tables, embedded data or undiscovered code; they are not
silently marked as padding or success. Every executable Mach-O segment has a
complete, nonoverlapping instruction/data/unknown interval partition, including
header and section gaps. Section-level coverage and Ghidra's executable-memory
block coverage are also retained separately because their boundaries differ.
No Ghidra function-body overlaps were found.

Private artifacts include the full symbol/alias inventory, defined strings
(including debug strings), imports, global xrefs, call edges, per-function
assembly and CFGs, computed flow sites and known targets. Computed jumps flag
possible switch/jump-table dispatch. Static target lists are not exhaustive
runtime proofs. Rizin-only candidates retain classifications for data,
non-executable locations, existing function interiors, decoded unowned code,
and unknown executable bytes. None is silently promoted to a successful export.

| Target | Global xrefs | Call edges | Computed calls | Computed jumps | Computed sites without a static target |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mac MP 1.3 reference | 382,808 | 40,141 | 2,516 | 343 | 2,692 |
| Steam Mac MP | 420,488 | 46,083 | 2,422 | 992 | 2,482 |
| Steam Mac SP | 394,464 | 43,109 | 1,985 | 900 | 2,037 |

Validation: original files and private copies retain their A0 SHA-256 values;
all 1,700,685 exported instructions (6,754,314 bytes) match their original
file-backed bytes exactly. Function IDs are unique, per-function evidence
matches the inventory, and section/segment byte partitions reconcile. All
three Ghidra analysis runs completed and saved their projects. Native runtime
build/tests are not relevant to this documentation and private-analysis change.

Limitations retained: Rizin emitted unsupported Mach-O relocation warnings and
skipped oversized string-search regions; its FUNC symbol labels alone are not
accepted as function evidence. Ghidra emitted VarnodeContext diagnostics on the
Steam inputs. Each Steam inventory contains one synthetic function record with
no decoded instructions; A3 must account for it separately. Unknown regions,
indirect destinations and provider disagreement remain A4–A6 work.

Private receipts: `A2-runs.json`, `A2-merge-runs.json`, `rizin-discovery.json`,
`A2-validation.json` and `instruction-byte-validation.json`, under the external
workspace declared in the analysis plan. A3 exports against this frozen final
inventory and must verify every outcome and artifact hash before completion.
