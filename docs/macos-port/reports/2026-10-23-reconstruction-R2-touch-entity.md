# R2: original touch-entity correspondence

`PM_AddTouchEnt` matches the original i386 routine on 855 frozen cases in stock
and actual CoD2x revisions 0, 1, 2, 3, 4 and 6. A fresh parent campaign reproduces
all seven results. No production code changes in this slice.

Raw paired N_FUN and text symbols bind the import-free original at
`0x6b65e..0x6b6ac`: 78 bytes and 38 instructions. Its prologue establishes cdecl
stack arguments. The absolute-zero `.eh` alias is not FDE evidence; the complete
frame decoder finds no FDE starting here. Raw STABS and native layout probes
separately establish the layouts: original `pmove_t` is 248 bytes with count/list
offsets 64/68; native is 256 bytes with offsets 68/72.

The corpus covers every valid count from 0 through 32, eight representative
signed entity values, sentinel 1022, every active duplicate position, duplicates
only in the inactive tail, and repeated active duplicates. The unmutated corpus
reaches every original instruction and both outcomes of all six conditional
branches. These direct-call inputs do not establish live caller reachability.

Original results come from executing the bound instructions. Native results come
from whole production translation units using actual Release compile commands,
declared fixture options, and ASan/UBSan. Logical count/list results match exactly.
Complete guarded object images independently constrain all other bytes in each
layout; process-local native pointers are normalized and separately checked.
Callee-saved registers, stack return and floating-point state are checked.

The separate footprint audit and independently authored parent checker verify
all 16,261 original reads and 3,932 writes. Accesses stay within the active
count/list fields or the arguments and 16-byte saved-register stack frame.
The original makes no player-state or movement-local object accesses. Native
arbitrary reads are not exhaustively traced; sanitizers and whole-object checks
provide the stated native bounds.

Five raw instruction mutations change append, count, duplicate, sentinel and
capacity behavior, with changed-byte execution records. Five corresponding
native record corruptions test the comparator. A separate actual native source
mutation changes sentinel 1022 to 1021, rebuilds the whole translation unit and
produces two expected differences plus an unchanged positive. These control
types are distinct. Unchanged and ineffective controls are recorded separately.

The first worker campaign completed its original and native observations but
failed at a mutation control because Unicorn reused translated instructions.
That run and harness remain failed evidence. A corrected continuation explicitly
invalidates the translation cache and reuses the completed observations by hash.
The parent starts fresh and runs the corrected campaign end to end. Its original
records match the retained observations after requiring and removing only the
new empty `executed_mutations` field.

Private evidence is under `reconstruction/phase-r2/`:

- Worker: `20261023-touch-entity-characterization`, inventory SHA-256
  `b9c7cdbe863d874948c6d7310af19eb4b856be1cdd0aafdbb9e5cb852b38d741`.
- Parent: `20261023-touch-entity-parent-fresh`, inventory SHA-256
  `52a4798ca369442a0cb3f1f2df99d48d0d746bdbcd72c0145bb091a36fe55cac`.
- Independent checks and command receipts are in phase-r3's
  `20261023-eighteenth-wave-parent-verification`.

This accepts finite correspondence for valid counts, not invalid-count safety,
physical i386 execution or complete movement reconstruction. Native full suites
were not repeated for this report-only change.
