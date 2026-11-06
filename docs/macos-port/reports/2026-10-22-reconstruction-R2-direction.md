# Original/native direction conversion comparison

Production ByteToDir and DirToByte match the original Mac i386 instructions across 967 declared finite cases in each native stock and CoD2x build. Returns and complete initialized allocations compare exactly, without numerical tolerances. No production change was needed.

The domain preserves all 883 pilot cases and adds 60 symmetric boundary cases plus 24 perturbations that cross 12 maximum-score ties. There are 260 ByteToDir cases and 707 DirToByte cases per variant. No case was removed after observing results. Raw section-qualified symbols and paired function records establish the two original bodies. LLVM and Capstone agree on all 54 instructions, and the cases execute all 54. Neither entry has a matching unwind record; the original absolute-zero unwind aliases remain recorded.

Both variants compile the complete production com_math.c with the real stock/CoD2x options, optimization and disabled floating-point contraction, adding assertions and address/undefined-behavior sanitizers. The fixture checks the defined low-byte return, every byte of distinct aligned 32-byte input/output objects, and the entire 1,944-byte direction table before and after each case. The original table and both compiled/linked tables match. The table remains mutable; this evidence does not establish immutability.

Original execution admits only the two original bodies and uses no helper or modeled import. It records executed instructions, all memory reads/writes, initialized stack changes and local calling-convention state. Native assembly checks preserved registers, stack alignment and floating-point controls. Native memory evidence uses sanitizers and complete allocation snapshots, without claiming a hardware trace of every access.

Floating-point exception status remains qualified: native hardware reports inexact in 680 cases per variant, while the emulator reports no SSE precision status. The earlier independent parent diagnostic established that emulator limitation. This result therefore makes no original-hardware exception-status parity claim.

Sixteen semantic/execution controls reject coherent result changes, omitted cases, altered table/allocation bytes, ABI/control corruption, invalid original bindings and disallowed execution. Parent review caught an audit exception-handling bug that could count a missed mutation as rejection. The corrected audit requires exactly the mutated case and the expected rejection reason. Two regression controls verify that unchanged candidates and wrong-reason failures are rejected; the earlier sources and failed attempts remain preserved.

Worker and parent fresh replays rebuilt both variants and repeated all original cases with zero mismatches. The parent also used a separately authored parser/comparator to verify original symbol/function extents, instruction bytes, complete allocations, table contents and local ABI observations. Complete native text sections matched the reference build for production, fixture and ABI objects and linked executables. This does not assert identity between x86 and ARM64 code or whole executable files.

The source evidence manifest covers 1,145 outputs, and the replay verifies 747 consumed input identities, including committed source/header content, toolchain, SDK, emulator and runtime files. Current Git HEAD is observational; the original committed source content is the replay authority. Sealed source and external replay records remain private.

Private evidence identifiers:

- Worker seal: `9d1497d5030ca33aa632f72855cbcb532c0cb36e278f7b3afa532f98012b518a`.
- Output manifest: `eedc2faa781696c1771163e75c30d6ce331eb2e9ef8242c92b7fd70f7788aeed`.
- Parent acceptance: `918771f6e064c9a800bd2759122b08031276fb8e369ce33feb7526af4708bce3`.

Misalignment, partial overlap, NaN/infinity, subnormal stress, other floating-point modes, concurrent table mutation, landing-pad entries and unbounded inputs remain excluded. This is finite differential evidence, not whole-function or game equivalence. R2 and full reconstruction remain incomplete.
