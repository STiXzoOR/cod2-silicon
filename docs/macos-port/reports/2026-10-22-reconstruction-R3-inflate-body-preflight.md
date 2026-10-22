# Inflate function-body preflight

The proposed correction assigns 510 already-decoded instructions, spanning 1,772 bytes, to their two existing inflate functions. The preflight applies only those body unions to disposable copies of accepted generation 14. It creates no final generation and changes no production code.

`inflate_blocks` gains 494 instructions and 1,720 bytes; its recorded body grows from 2,089 to 3,809 bytes. `inflate_codes` gains 16 instructions and 52 bytes; its recorded body grows from 2,533 to 2,585 bytes. The earlier ownership proof remains conditional for 58 records on callbacks returning normally with the required frame, registers and memory intact. This does not establish dynamic feasibility of every path or equivalence to the SDK zlib replacement.

All 619,351 complete instruction records, 389,849 references, 108,649 DATA references, 8,115 function identities and 100,589 symbols remain preserved. Every stored function field except the two intended bodies remains exact. The 529 affected-owner probes preserve C, high Pcode, control-flow graphs, switch records, results and debug XML; only the two input ownership records change. Unknown coverage stays at 10,384 bytes in 4,431 intervals. No unknown bytes were removed from the denominator.

The expanded data audit records all 169,281 definitions, including initialization state, actual bytes where initialized, undefined status, nested types, settings and persistent identifiers. The first attempt incorrectly requested bytes from uninitialized storage; that failure and its source are retained. The corrected exporter records uninitialized storage explicitly, without fabricating zero bytes.

Before/after raw data payloads are identical within each process. Across processes, Ghidra assigns a new identifier to its built-in one-byte undefined type. Installed source and bytecode establish this process-local behavior. The comparison permits only that exact singleton identifier difference at 24,518 recorded paths, requires one value per process, and keeps every other field, value, container type and ordering exact. Raw hashes and identifiers remain recorded separately from the qualified fingerprint. Fresh verification performs one exhaustive baseline comparison and derives after equality from exact same-process payload hashes; it does not claim a second recursive scan.

Existing recovered switch mappings and the unresolved JPEG contract remain unchanged. The inferred void-return discrepancy in the inflate functions is retained; no signature is changed. Decompiler warnings remain diagnostics rather than semantic proof.

Worker and independent parent fresh replays reproduced the exact effect contract. Sixteen semantic mutation controls, one consistent-ID positive control and an ineffective-mutation harness control passed in both fresh replays; the worker also verified two write refusals. Separate parent comparators checked all global records, the two body changes, complete probe outputs and every nested data field. A removed-body-range negative control was rejected. The parent verified all 102,687 sealed worker files and confirmed the frozen source and disposable project copies remained unchanged after each read-only run.

Private evidence identifiers:

- Preflight packet seal: `0758f614b95e50ead48c240d970c66d9e5721d94bd1ce4014f7212b943d70beb`.
- Exact effect contract: `696f85afb95f31a898e5d2eb8419eeed56ae2105b7137be1a904eb16d7178fab`.
- Parent acceptance: `4dad9c385c015de75bc5d246da02afff4994fd4b249223c4957d238ac708353d`.

This accepts only the bounded preflight. Applying a new immutable generation remains a separate step. It makes no original/native or whole-function equivalence claim, and full reconstruction remains incomplete.
