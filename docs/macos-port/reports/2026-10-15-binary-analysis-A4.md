# A4 — types and source correspondence inventory

2026-10-15. The private catalog covers all 24,735 records in the declared
three-binary Mac corpus. This completes the catalog and its evidence audit;
complete type recovery and behavioral equivalence remain unproven.

Source identity: `95db9c12922d7460633eab3ef5c038ae8df88b9a`; source-tree object
`4ec6695155930f24b78a31fb389755e9085cd6f8`. No runtime source changed.

The existing STABS parser recovered 82,983 type nodes, 25,877 named references,
2,109 variables and 6,824 function-scope records (6,804 unique debug function
names). All 2,002 parse errors remain private and explicit, including unsupported
C++ access/method metadata. Types retain their i386 identity; inferred Ghidra
types were not substituted for debug types. The three selected Direct3D class
layouts are not recovered by this parser and remain opaque.

The existing data-generator round-trip passed its 12 unit tests and the full
i386 byte/relocation/symbol comparison, arm64 probes, pointer alignment and
deterministic regeneration. It reused 67 engine types and verified 17 recovered
LP64 scalar objects against the Steam value binary. The coverage report still
lists fallback objects, including 28 BSS objects; reproduced bytes alone do not
prove typed native semantics.

Vtable candidates were bounded by section and next-symbol extents. There are
64/61/61 tables in reference MP/Steam MP/Steam SP, with 672/670/670 words pointing
to known functions. Header/data words and secondary-table interpretation remain
explicitly unresolved; these are not certified native vtable layouts.

A fresh private CMake configuration and Clang audit covered 626 client/dedicated
translation units: zero compiler errors and zero cross-unit ABI mismatches.
Source correspondence uses only the 388 client units and 5,286 compiler-derived
non-static definitions inside `src/`; SDK and dedicated-only definitions are
excluded. Symbol spellings are compared before legacy-underscore and demangled
C-wrapper matching. Manual review resolved apparent ambiguities caused by SDK
overloads, dedicated FX code and C++ runtime import forwarders.

| Binary | Unique source candidate | No active non-static definition match | Import-forwarding thunk | External import |
| --- | ---: | ---: | ---: | ---: |
| Mac MP 1.3 reference | 4,861 | 1,983 | 633 | 634 |
| Steam Mac MP | 4,690 | 2,559 | 702 | 702 |
| Steam Mac SP | 3,972 | 2,759 | 620 | 620 |

Each record retains source commit/tree, matching method, candidate locations and
native signatures, debug-scope references when available, thunk destination,
export outcome, confidence and limitations. No ambiguous candidates remain in
this filtered catalog. A unique candidate is symbol correspondence, not proof
of equal behavior. Unmatched records can be static definitions, libraries,
variant-specific code or missing reconstruction. This repository is an MP port;
SP matches do not establish an SP implementation. No mapping was automatically
marked behaviorally validated.

Six targeted i386 field checks passed using parsed STABS: the world sun-parse
base, three lighting scalars, backend scroll and nested scene time. Fresh Clang
arm64 layout output keeps the width distinction concrete: backend scroll moves
from byte 1,244 to 1,288; world sun-parse from 52 to 96; nested scene time remains
956. Current named-field accesses and the existing water/lighting fixtures are
the native contracts. An initial lookup used the wrong backend aggregate name;
its failed observations and corrected type identity remain in the private audit.

Renderer callback verification passed 218 bindings with no table/cast mismatch
or unprototyped floating call. Retail import verification covered 514 symbols
and 1,779 active native sites, with zero proven extra or missing pointer loads.
The 185 placeholder declarations remain inventoried, not certified complete.

Private outputs live under the external analysis workspace's `analysis/`:
type/function-scope records, bounded vtable candidates, compiler audit,
per-binary correspondence and a 24,735-row SQLite index. Public output is this
sanitized report and plan status only. Remaining type/parser gaps, unmatched
source candidates and semantic review are carried into A6's disposition ledger.
