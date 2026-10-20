# R2 additional original-instruction math comparisons

The parent accepted a finite comparison of `AnglesSubtract`, `AngleDelta` and
`Vec3MajorAxis` against the owned Mac multiplayer reference. Actual production
`com_math.c` was compiled for ARM64 with stock and CoD2x engine flags, optimization,
FP contraction disabled, AddressSanitizer and UndefinedBehaviorSanitizer.
No production code changed.

Each variant passes 7,272 cases: 5,876 for `AnglesSubtract`, 708 for `AngleDelta`
and 688 for `Vec3MajorAxis`. Across both variants, all 14,544 comparisons match
exactly for the observable return bits and the full 96-byte fixture arena.
The void function's return is a placeholder; its memory effects are compared.
Input and guard words outside intended output locations remain unchanged.

The domains include signed zero, subnormals, angle and quantization boundaries,
seeded finite inputs, vector ties and finite squared extremes. Thirteen buffer
layouts exercise disjoint, identical and overlapping inputs/outputs, preserving
the original sequential writes. Angle-to-integer conversions stay within the
representable range, and iterative angle reductions are bounded.

Original raw symbol records, function extents and all 98 original instruction
boundaries were checked independently. Every original instruction executes in
the case set; two additional instructions capture the floating return.
The math executions make no modeled import calls. Eleven negative controls
reject malformed observations and detect return/memory corruption, lane order,
alias preloading and tie-breaking changes.

Every original case starts with FPCW `0x037f` and MXCSR `0x1f80`; every native
case starts with FPCR/FPSR zero. The standalone native fixtures also inherited
zero FPCR/FPSR. Control modes remain unchanged by the functions. Exception status
is recorded, but cross-architecture status equivalence and actual engine FP
state are not established. The results apply to these explicit conditions.

The parent fresh replay passes 20 commands and all 15 artifact comparisons.
Behavior artifact bytes match; the consumed-input manifest matches as a complete
path/hash/size dictionary despite JSON key-order differences. All 421 consumed
input identities and 415 sealed files were checked. Failed harness attempts,
including the parent's missing output-directory setup, remain in private evidence.
Parent acceptance receipt SHA-256: `7b1749de48714c581782000e8bcd97e2174fd4209af7f8e9546b1e83163b178b`.

This adds bounded execution evidence. It does not establish behavior for every
float or alias arrangement, whole-function equivalence, live gameplay fidelity,
or completion of R3–R6. Original bytes and detailed traces remain outside Git.
