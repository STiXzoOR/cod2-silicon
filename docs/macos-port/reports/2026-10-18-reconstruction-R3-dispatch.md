# R3 — Compiler-helper and dispatch recovery

Status: this bounded MP discovery checkpoint is verified. Full decompilation and
reconstruction remain incomplete. Metal remains paused at the user's request.
This follows the [earlier R3 checkpoint](2026-10-18-reconstruction-R3.md).

## Discovery changes

Generation 06 adds 66 independently checked instructions, 68 bytes of dispatch
tables and three exact partial-body unions. Every preexisting instruction and
all 8,115 inventory entries are preserved. No binary bytes were changed.

| MP discovery measure | Generation 05 | Generation 06 |
| --- | ---: | ---: |
| Decoded instructions | 601,472 | 601,538 |
| Unknown executable bytes | 68,454 | 68,148 |
| Unknown instruction-section bytes | 65,345 | 65,039 |
| Physical unknown intervals | 4,621 | 4,618 |
| Owned computed sites without static targets | 2,690 | 2,688 |

The 306 newly classified bytes comprise 238 instruction bytes and 68 table bytes.
Four additional computed sites remain unowned. Known static targets do not imply
an exhaustive runtime target set. No unknown bytes were labeled as padding.

## Decompiler correction

The original compiler-helper symbol has an extra platform prefix that prevented
Ghidra from selecting its existing helper model. Applying that model to the one
verified helper restores four previously certified switch mappings in pseudocode:
37 ordered case slots, including duplicates, and four default paths agree with
the original evidence. The six callers of the separate helper remain unchanged.

All 68 direct callers were inspected. Their pseudocode changed; the only other
changed output is the helper itself, which gains an inlining notice without a
code change. Nonmetadata warning functions decrease from 1,208 to 1,204;
unrecovered-jump-table warning functions decrease from 179 to 175. Total warning
functions increase to 1,261 because Ghidra adds notices for the helper model.

The caller audit also records 1,187 intermediate-code sites outside their stored
function bodies: 913 in still-unknown bytes, 270 at existing instruction starts,
and four at competing boundaries inside existing instructions. These sites were
not accepted as new instructions or ownership. Five additional displayed switch
tables remain diagnostic. Full pseudocode remains diagnostic rather than a
certified reconstructed implementation.

## Verification and remaining work

Fresh export and independent parent verification pass all 8,115 records:
7,481 decompiled bodies and 634 external entries. Every assembly/CFG/xref and
pseudocode hash passes; the nine project files are unchanged by read-only export.
The discovery and caller-impact audits were independently replayed, preserving
76,930 frozen input files. Separate bounded original-instruction runs support
the added code and helper model; they do not certify whole-function behavior.

Next work resolves further compression dispatch, conflicting instruction
boundaries and exception cleanup regions. The original region/function ledgers
remain intact. All binaries, decompiled output, traces and detailed evidence stay
private and outside Git. This commit contains no native runtime or Metal changes;
the remaining requirements are in the
[reconstruction plan](../../plans/2026-10-15-validated-reconstruction.md).
