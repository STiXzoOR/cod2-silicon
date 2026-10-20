# Reconstruction generation 13

Generation 13 applies the exact reviewed preflight packet to a new copy of the
accepted generation 12 project. It creates 268 instruction records covering
880 bytes, replacing five old records and one typed pointer. This accounts for
862 previously unknown bytes. The ledger now records 619,324 instructions,
10,473 unknown bytes and 4,436 physical intervals. No region or byte was removed
from the denominator.

All 619,056 untouched instruction records, 389,760 previous references,
8,115 function identities and 4,621 original region histories are preserved.
There are 79 new references and nine precisely scoped function-body extensions.
Pointer identity and its data references survive; only the two authorized VAO
callsites receive return overrides. The four additional typed conflicts remain
unchanged and require their own application preflight.

Only two of 7,481 C exports change: OpenGL texture-type constants lose default
labels, and one snapshot word-offset expression is reassociated. The parent
checked both complete diffs against their original operands and all 1,302
instruction records. This qualifies those rendering changes; it is not a proof
that recompiling the exported C preserves behavior. All nine repaired owners
retain identical C output.

A fresh parent replay reproduces all 8,115 export records, 7,481 C hashes,
514 impact probes and the complete discovery/audit/query metadata. Seven
semantic verifiers and frozen-input checks pass. Only measured decompiler
elapsed time is normalized. The copied and sealed project files are unchanged.

The warning census remains 1,261 functions and 1,621 lines, including 175
unrecovered-jump-table warnings. All ten original switch contracts are retained;
nine tables are recovered and the existing JPEG table remains unresolved.
Actual unwinding, binding, native equivalence and whole-game fidelity remain
outside this evidence. Native runtime behavior is unchanged; R3–R6 and Metal
implementation are incomplete.

Private evidence: `phase-r3/corrected-generation-13`.
Freeze SHA256: `46836b743600c919ba9149868faecd91c82936f6436f253cf252e4d9d34809c9`.
Parent acceptance SHA256: `7dea03601587396574330c9798712818b8ed3e40125f1aeceec192b59a6c2645`.
