# R3 exception-path and compression discovery checkpoint

This checkpoint improves the private Mac multiplayer analysis. It does not
complete reconstruction or establish native runtime equivalence. Metal
integration remains paused while decompilation work proceeds.

## Discovery changes

Five independently checked proposals add 3,245 stored instructions covering
11,005 bytes, two tables covering 48 bytes, and 193 exact function-body unions.
The proposals cover exception cleanup and catch paths, partial inflate bodies,
and the final two bounded JPEG switch contracts. No original instructions,
function identities, or inherited region identities are removed.

| Measure | Previous generation | This generation |
| --- | ---: | ---: |
| Stored instructions | 615,100 | 618,345 |
| Unknown executable bytes | 24,874 | 13,821 |
| Unknown bytes in instruction sections | 21,765 | 10,712 |
| Physical unknown intervals | 4,618 | 4,508 |
| Preserved function identities | 8,115 | 8,115 |
| Preserved original region identities | 4,621 | 4,621 |

The exact reference contract adds 1,036 control references and 69 separately
checked absolute memory-read references. Local metadata marks 201 newly
recovered exception calls as non-returning. These are call-site changes;
global callee behavior is not inferred from them.

## Export and diagnostic scope

The full export retains 7,481 internal pseudocode records and 634 external
records. Fourteen C outputs change; 7,467 remain byte-identical. Two changes
occur within the initial scope. Twelve additional outputs change because
addresses previously presented as data are now classified as code. Their
stored function evidence and complete original instruction metadata remain
identical. Two network outputs also change inferred types and local-variable
coalescing; these are decompiler inference effects, not compiled-C equivalence
results.

The initial 415-function probe set remains intact. A separate 12-function
extension covers those additional effects, bringing the total to 427. All
427 probes match the full export; none are truncated or limited.

Ten original switch contracts retain 81 ordered slots and ten defaults.
Ghidra recovers nine jump-table mappings, including all eight from the previous
checkpoint. One new JPEG switch still lacks a native jump-table object despite
its independently verified byte-level contract. This limitation remains open.

For the same original 94-function scope, diagnostic sites outside their owned
bodies fall from 560 to 324, and sites in still-unknown bytes fall from 285 to
49. The earlier 72-function subset has the same before/after counts. The larger
427-function set has 501 outside-body sites, including 449 already-stored
instructions belonging outside the probed function, 49 still-unknown sites,
and three competing instruction boundaries. These populations are reported
separately to keep the comparison honest.

Warning counts remain 1,261 functions and 1,621 comment lines. One inherited
unowned call site is retained without inventing a function owner. The three
competing instruction boundaries remain unresolved.

## Independent verification

The parent separately reran all six discovery, export, C-effect, impact, ledger,
and frozen-manifest checks. Each passed. A fresh read-only Ghidra replay matched
all 7,497 evidence files, with nine project files and 157,527 input files
unchanged. The final post-replay freeze check passed across 33,368 frozen items.
The parent accepted this generation on 2026-10-18.

## Validation limits and next work

Exception-path evidence is static and conditional on the recorded ABI and
memory assumptions; actual exception execution and arbitrary callees have not
been validated. The inflate proposals have 84 bounded original-instruction
cases, with 70 returns and 14 stops before unmodeled imports. The final JPEG
proposals have 42 such cases, with 32 returns and ten unmodeled stops. These
finite cases do not certify whole-function or whole-game semantics.

Next work applies already checked cleanup and orphan-boundary proposals against
the newly accepted baseline, then addresses the remaining ownership and
exception-metadata gaps. Private binaries, decompiled C, detailed proofs,
traces, and project files remain outside Git.
