# Function reconciliation ledger

`functions.json` contains a function list for every indexed source module.
`modules.csv` gives its four status counts. The active module is
`game/g_active_mp`; complete that module before moving to another.

Each row retains the Mac address/symbol and Linux source/line when available.
Only unique names are paired automatically. Cross-module name pairs are
explicitly provisional, and overloads or unresolved identities stay separate.
Every input definition remains represented. Row counts are reconciliation
work items, not a claim that unmatched rows are distinct runtime functions.

The starting index accounts for 7,477 exported Mac functions and four further
STABS functions. The Linux source index contains 4,442 AST-confirmed definitions
after resolving paths and removing repeated header inclusions. All 194 source
and header files were parsed. Diagnostics occurred in 158 files; an independent
ctags name comparison leaves seven conditional zlib candidates, which are
retained in the ledger. Inactive preprocessor variants and overload completeness
still require checking during each module's reconciliation. This is a tracked
baseline, not a claim that parsing proved the complete source universe.

Statuses mean:

- `equal`: the complete function has been reconciled with the authoritative
  behavior, with no required correction.
- `fixed`: the complete function has been reconciled and its required fixes
  implemented and validated.
- `intentional`: a documented Mac/CoD2x/platform difference, or a bundled-library
  or compiler-runtime exclusion explicitly permitted by the plan. An excluded
  upstream function is not being claimed semantically equal.
- `unchecked`: reconciliation or required fixes remain. A known partial fix
  belongs in its evidence and does not by itself close the function.

The initial metadata import does not automatically accept older partial audit
claims. Existing reports and private function tables remain evidence to consume
while completing the active module. This avoids silently treating missing
coverage as completed work, but makes the initial unchecked count conservative.

Use one report per module under `reports/`. Record completed functions, unresolved
differences, the batch's changes, tests and one review. Follow the current plan:
no cross-module sweeps, five-lens cleanup, or separate Claude review per fix.

`tools/fidelity/module_inventory.py` joins private Mac/Linux indexes without
copying code, assembly or assets. It writes a fresh baseline; supply reviewed
status overrides through `--decisions` when regenerating. Generate into a
temporary directory and inspect the diff before replacing the maintained ledger.
Private extraction inputs and scripts are in the machine-local
`CoD2-native-decomp/F-inventory.noindex` evidence directory.

An ETA must state the unchecked count, the measured completion rate and its
coverage limits. No full module has yet completed under this workflow, so the
first throughput estimate is provisional; timings for a few small helpers do
not establish a reliable whole-project completion date.

## Current workload and estimate

After the first module checkpoint, the ledger has 8,083 unchecked entries:
7,306 assigned engine entries and 777 unassigned entries. Another 1,360 entries
are intentional bundled-library exclusions. There are 13 equal functions and
one fixed function; the active module has 10 of its 24 functions unchecked.
These counts deliberately retain uncertain pairings and prior partial checks.

The timed sample closed five additional, mostly small functions in 380 seconds
(about 47 per hour), with substantial earlier evidence reused. Large functions
and their repairs remain open. Using a lower planning assumption of 20–40
entries per active hour gives approximately **200–400 active hours**, before
final cross-module runtime acceptance. This is a low-confidence workload range,
not a calendar promise or a measured end-to-end rate. Recalculate after the
first complete module and revise the count as duplicate identities and actual
compiler/runtime exclusions are resolved.

One local inventory-batch review found two classification defects: botlib had
been excluded despite the plan, and SDK-header provenance incorrectly excluded
game functions. Both are corrected. All botlib rows are engine/unchecked; SDK
header candidates remain unchecked until their symbol identity is established.
The review confirmed source-record accounting and matching CSV/JSON counts,
not parser completeness or all function semantics. No external peer or
five-lens cleanup was run.
