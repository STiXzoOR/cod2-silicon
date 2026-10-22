# R2 clipping characterization

The current native `PM_ClipVelocity` matches the original Mac MP 1.3 function
for all 360 frozen finite cases in both stock and CODX. No production correction
or public test change was needed. This is bounded execution evidence, not a
claim of complete numerical or gameplay equivalence.

The domain combines 12 velocities, ten normals and three actual pointer
arrangements: distinct output, output sharing the input object, and output
sharing the normal object. It retains signed zeros, six axial normals, two
rounded nominal-unit normals and two non-unit normals. Each comparison covers
all three guarded physical objects, including the unused distinct output in
alias cases.

## Execution and independent checks

Raw debug records independently establish the original function boundary.
Capstone and LLVM agree on all 32 original instructions, which execute in every
case. No unwind FDE covers the function; its zero-valued absolute symbol is not
treated as unwind evidence. Original execution checks the actual calling
convention, registers, floating-point controls, stack, return and memory-access
allowlists against the immutable original binary.

Both native variants compile the complete production translation unit using
their actual Release commands, including disabled floating-point contraction.
The harness calls the linked production symbol with real pointer aliases and
captures preserved registers, stack and floating-point state. An inspection of
the entire linked body checks native memory accesses; native accesses are not
dynamically hooked.

The parent repeated all original and native cases in a fresh output root.
Both variants again matched 360/360 complete object images. A separately
authored audit reconstructed the domain, parsed original and native binary
records, checked complete observations and confirmed identical original
observations between worker and parent. All native object results also agree.
The fresh run passed seven controls, including an effective mutation of the
original mapped constant, rejection of an ineffective mutation, and detection
of output, coverage, guard, floating-point-control and register corruption.

The worker's first attempt completed the original cases but stopped before
native execution on an unnormalized dependency path. Its continuation reused
explicitly hash-bound original observations. The parent replay used no such
reuse. Failed harness versions and parent audit assumptions are retained with
the corrected scripts and results.

Existing movement/collision tests and two levels of callers were inspected.
Static call evidence does not establish live collision reachability. No broad
native suite was rerun for this report-only unit. Exceptional floats, other
floating-point modes, the complete input domain and live gameplay remain
outside this result. The full macOS shared-cache image is not pinned.

## Private evidence

Original bytes and complete observations remain outside the repository.
The worker manifest is
`bbc505fd09e9ed84eae7cca9d2839ca535674fa2a1abbf95b9ca11d45f1c984d`.
The fresh parent execution seal is
`0473e3eeeea5f5e11b3bafc0e8564098b2fbc629cfa7e3b13cc4ed21c960ef00`;
the independent parent audit seal is
`f644c28e10555e41cce581f7cab295f6d21744ea4d2276abcf7a133501e789d4`.
R2 and the full validated reconstruction remain ongoing.
