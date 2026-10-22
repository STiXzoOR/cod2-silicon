# JPEG switch candidate review

The private decompiler candidate requires revision before tool acceptance. Independent review found that its guard-to-reload check can miss writes represented as a memory-output COPY. Ghidra can convert an explicit STORE into that form. The candidate checks STORE, calls and several other effects but does not reject this nonvolatile memory write. A transferred guard range could therefore exclude a feasible switch index.

This is a source-level finding with a concrete counterexample design. It has not yet been reproduced by executing the new fixture. The next private experiment must show the actual normalization and recovered range, then verify a correction against unchanged positive cases. Both added guard-recognition paths need coverage. STORE-origin flags alone are insufficient without checking actual output storage and the transformations that preserve or lose those flags.

The parent verified the review's complete sealed inventory and read-only modes, inspected the relevant source, and reran its evidence checker. All eight checks passed, reproducing the saved results exactly. They cover 712 candidate-capsule files, 237 pinned source copies, nine accepted switch contracts in eight owners, four synthetic fixtures, two barrier-mutant runs and the retained results of 52 upstream assertions per tool. Neither the independent review nor this parent replay reran the decompiler or upstream test binaries.

The existing write fixtures modify an adjacent field: they demonstrate a conservative any-store barrier, not mutation of the guarded field. Three retained result documents have existing serialization errors; switch fragments parse and paired tool outputs remain byte-identical. Two inflate inputs require the previously recorded type-definition order adaptation. These limitations remain part of the evidence.

Private evidence identifiers:

- Independent review: `1da3fcb13218ea5e363737e292efe3a53582e713a5003405f20e8571f0b40b2a`.
- Independent review seal: `b454438de15dfaab89d58d452f96ee8d4ba9e1634beae78f5cc3641ae1743370`.
- Parent replay seal: `f3f61f667e56b5451b03c3e7da793123550d90192804bbd30db26eaff9150969`.
- Parent adjudication: `626664981f7e04dfda9c2514c7c1d7dd7d37b6770afd533e8dac3eb42c5467ed`.

No installed tool, accepted project or native game code changed. No original JPEG write is asserted by the synthetic counterexample. The JPEG switch remains unresolved in accepted reconstruction; this review accepts neither the candidate nor a new generation.
