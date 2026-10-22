# R2: original movement-direction characterization

Stock and actual CoD2x revisions 0, 1 and 2 each match all 1,050 frozen
`PM_SetMovementDir` cases. Production code is unchanged. Of those cases, 524
execute without an import model; 526 are conditional on the declared host
binary64 `atan2` model. This does not establish historical original-libm
equivalence or whole-game equivalence.

The original campaign executes the original function and its five actual math
helpers. Raw text symbols, paired N_FUN records, instruction bytes and two
disassemblers bind their boundaries. A separate raw CIE/FDE decode corroborates
the root's 423-byte extent. The root reaches all 117 instructions in the frozen
corpus itself, excluding the additional adversarial controls. Some helper
instructions remain unexercised.

The corpus covers prone and flag precedence, ladders, absent commands, airborne
state, zero displacement, adjacent speed-threshold values, forward/backward
movement, quadrants, angle wrapping, truncation order and the degree clamp. It
also includes explicit direct-call zero/negative frame cases; their presence
does not prove those inputs arise in a live caller.

Native comparison compiles the complete production movement translation unit
and actual math/runtime dependencies using the recorded Release flags plus
declared fixture, assertion and ASan/UBSan flags. It checks complete initialized
objects and guards, normalized pointer storage, callee-saved registers, stack
balance and FP control state. Original reads/writes and executed instructions
are recorded; the largest observed original stack depth is 208 bytes. Native
arbitrary reads and hardware FP exception behavior are not fully characterized.

CoD2x revisions 3, 4 and 6 produce the same 236 separately classified policy
differences: 116 ladder cases, 78 conditional backward rotations, 26 truncation
order cases and 16 positive-frame guards. These are not counted as original
matches. The unchanged public direction test passes in all seven configurations.
Ten controls include an unchanged positive, effective state/result corruption,
an executed original threshold-constant mutation and ineffective-mutation
detection; they are not ten distinct negative inputs.

The worker completed one original campaign, then preserved a failed native setup
that expected a stock compile entry for a runtime excluded by preprocessing.
Its corrected native run reused the original observations byte-for-byte. A later
metadata-only FDE parser correction and supplemental raw decode are retained.
The parent independently executed a fresh complete original/native campaign;
all original observations match the worker byte-for-byte, and full native
object/result records agree after accounting for runtime addresses. Parent
checks independently bind raw symbols, instructions, constants, FDE and state
effects. No production repair or broader native rebuild was needed.

Private evidence is under `reconstruction/phase-r2/`:

- Worker: `20261023-movement-direction-characterization`, inventory SHA-256
  `c0d21a63bceeb9375667b0d8dac51c032ea21336d2ca8d446fe1f1f3090b4f04`.
- Parent: `20261023-movement-direction-parent-fresh`, inventory SHA-256
  `7937717caa902340627e023ccbf886b82a83929c2bcc04c7e46c5b20dda86dcf`.
- Parent independent checks and complete command receipts:
  `phase-r3/20261023-seventeenth-wave-parent-verification`.

This completes the bounded characterization slice of R2. The full
[reconstruction plan](../../plans/2026-10-15-validated-reconstruction.md) remains
incomplete. Accepted generation 15 is unchanged.
