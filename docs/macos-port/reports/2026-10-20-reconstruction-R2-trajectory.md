# Original/native trajectory comparison

The production trajectory position and velocity calculations match the original Mac i386 instructions across 3,375 defined finite cases in each native stock and CoD2x build. Comparisons use exact result bits and every initialized byte in the input/output allocations, without numerical tolerances. No production change was needed.

The domain retains 270 earlier diagnostic cases. It covers position types 0, 1, 2, 3, 5, 6, 7 and 8, and velocity types 0, 1, 2, 3, 5, 7 and 8. There are 1,305 disjoint-buffer cases and 1,035 cases each with the result exactly overlapping the base or delta vector. Durations are positive; selected times cross start/stop boundaries and include negative values. Vectors include signed zeros, zero norm and seeded finite values. Integer arithmetic remains within its defined range.

The fixture compiles the actual production translation units with their stock/CoD2x options, optimization and disabled floating-point contraction, adding assertions and address/undefined-behavior sanitizers. It checks all 52 initialized input bytes and 32 output bytes, including guard and padding bytes. Original execution admits only the two roots and their original normalization helper; no modeled imports execute. Native memory checks use sanitizers and complete allocation snapshots, without claiming a native hardware trace of every access.

Raw symbols and paired function records establish the three original extents. LLVM and Capstone agree on all 336 instruction boundaries; the cases execute 295 instructions and leave 41 explicitly unexecuted. The position root has a matching unwind record. No unwind record starts at the velocity/helper entries; their absolute-zero unwind symbols are preserved as an evidence limitation.

Local calling-convention checks and floating-point controls pass. Exception status is a separate limitation: native hardware reports inexact in 1,325 cases, while this Unicorn build does not set the SSE precision flag in an independent synthetic diagnostic. The comparison therefore establishes no original-hardware exception-status parity.

Sixteen semantic/execution controls reject malformed or coherently changed results, altered guards, omitted cases, register/control corruption, invalid original bindings and disallowed execution. Coherent changes to both result and allocation bytes produce a differential mismatch in each buffer arrangement. Two additional controls reject replay writes into sealed evidence or an existing output directory. Tooling failures and superseded attempts remain preserved.

Independent worker and parent replays rebuilt both production variants and repeated every original case with unchanged semantic evidence. The parent separately parsed original symbol/function extents and checked every recorded result/allocation byte. All six production/ABI object files and both linked native text sections matched the final worker build; this does not assert identity between x86 and ARM64 code or between whole executable files.

The sealed package contains 486 outputs, with 447 primary and 846 supplemental consumed-input records covering source, headers, tools and runtime dependencies. Replay requires the dispatched committed source content rather than an unchanged current Git HEAD. Failed attempts and superseded fixture versions remain retained outside Git.

Private evidence identifiers:

- Worker seal: `8b7b91fb1dc44d33c9cdc0d336b6ca7ea84deb8dcb376ebb258189e569958026`.
- Output manifest: `d2a5a9d3832356e618fcd3087816906f2d65706fc80323834cc369c16fafd8ac`.
- Parent acceptance: `d36b32fd334988f09b8aac3baddcf96a4469d6c3c94b2babbd7f8f167729cf3d`.

Sine/default/error paths, velocity type 6, nonpositive durations, integer overflow, NaN/infinity, subnormal stress, partial overlap and misalignment remain excluded. This is finite differential evidence, not whole-function or game equivalence. R2 and full reconstruction remain incomplete.
