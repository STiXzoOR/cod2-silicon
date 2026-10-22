# R2 native friction correction

Native `PM_Friction` now follows the measured Mac MP 1.3 flag precedence:
flag `0x200` applies the fixed ground-friction factor and bypasses
`Jump_ReduceFriction`, including that helper's clear-state side effect. An
initialized type-4 walking state also receives ground friction before flight
drag. The legacy `COD2_X64`-off branch retains its previous implementation.

The preceding [characterization](2026-10-22-reconstruction-R2-friction-characterization.md)
found 61 differences among 800 finite initialized states in each native variant:
33 combined-flag velocity differences, 20 combined-flag clear-state differences,
and eight type-4 walking velocity differences. This correction resolves all 61
within that same frozen domain. It does not establish every possible input or
whole-game equivalence.

## Independent execution and regression evidence

The worker executed all 800 original-instruction cases and both real native
variants. The parent then independently repeated that execution in a new output
root. Both runs matched all 800 original results in stock and all 800 in CODX,
with no stock/CODX differences. Complete selected observation artifacts matched
between the runs, including guarded before/after state, floating-point controls,
original ABI checks, ten controls and the modeled real native `PmoveSingle`
caller. The original observations also remained byte-for-byte identical to the
accepted pre-correction parent observations.

An independently authored audit reconstructed the complete case domain and
checked raw original instructions, every complete state record and the result
partition. The original `PM_Friction` and `Jump_ReduceFriction` instruction
coverage remained 103/103 and 32/32 respectively. This is original instruction
execution with bounded initialized objects, not original whole-process play.
The actual native caller agrees with replay of the original friction root;
original `PmoveSingle` itself was not executed. Type-4 walking caller
reachability remains unproved.

Six public regressions use literal original observations and the real jump
helper. The worker captured failures in the combined-flag and type-4 tests
before changing production code. The parent independently checked their
expected values against fresh original results. No production test hook,
export, shared-layout change or renderer change was introduced.

## Build and compatibility checks

Final stock and CODX client and dedicated builds passed. Both full movement and
weapon revision matrices passed with the existing sanitizer configuration.
Existing integer-precision compiler warnings were retained; no sanitizer
runtime failure occurred.

The first parent legacy source guard rejected a whitespace-layout difference
introduced by splitting the condition across preprocessor branches. The parent
preserved the full legacy condition, then passed all five legacy-off guard
configurations and four actual SDK preprocessing comparisons. Four native SDK
comparisons independently confirmed that the final compiler input is unchanged
from the source used in the 800-case replay. Native builds and both revision
matrices passed again after that formatting correction.

These are source-preprocessing checks. The full Linux legacy artifact comparison
remains blocked by the previously recorded upstream baseline compiler errors;
this report does not claim legacy object or executable byte identity. It also
does not claim ARM64 executable identity with the original i386 executable.

## Private evidence

All original bytes and complete execution records remain private. The worker
packet seal is `69695799f77eb9f43edf809cc6239d8dd63ac4397a8e5a57f7649583a3d03a21`.
The fresh parent execution seal is
`f540056e21963c825d55b41dae460a962439bb8b89e83989555f6071e221cb39`;
the parent verification seal is
`23f0f83dfb4899cb6b8c802df6da08887c1ea74701697211c6b2a85668246329`.
They retain the failed guard, exact source versions, input identities, commands,
controls and passing final checks. R2 remains ongoing and the full validated
reconstruction contract remains incomplete.
