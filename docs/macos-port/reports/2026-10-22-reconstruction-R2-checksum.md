# Original/native checksum comparison

Production unkeyed and keyed block checksums match the original Mac i386 instructions across 1,200 declared cases in each native stock and CoD2x build. Return bytes, complete initialized input allocations and the padding table compare exactly. No production change was needed.

The corpus combines 40 lengths, five byte patterns and six operations: unkeyed plus five keys. It includes valid non-null zero-length inputs, block and padding boundaries, and the keyed four-byte prefix. Every case retains the full allocation and both 16-byte guards. The original and native checksum contexts are independently checked as 88 bytes, with state/count/buffer offsets of 0/16/24.

Both variants compile the complete production md4.c and com_shared.c from their real build entries, retaining optimization and disabled floating-point contraction while adding assertions, ASan and UBSan. The fixture does not replace the production memory wrappers or extract the checksum algorithm into test code. Native execution completed without sanitizer diagnostics.

Raw section-qualified symbols and paired function records establish seven original root/helper bodies. LLVM and Capstone agree on all 723 instructions. The cases execute 711; all 12 count-overflow-path instructions remain unexecuted and in the denominator. Both original padding symbols remain recorded, and the actual instruction operand identifies the table used. Original production memory wrappers execute with two explicitly bounded imported-service models. Neither original libc internals nor the import placeholders are claimed executed.

Original evidence retains ordered memory reads/writes, initialized stack effects, complete allocation snapshots, return state and local calling-convention observations. Native evidence combines sanitizers, complete allocation/table snapshots and an assembly observer for preserved registers, stack alignment and floating-point controls. It does not establish hardware memory-access tracing or unobserved native internal-stack zeroing.

Twelve semantic controls and six controls that alter actual original execution reject as expected. The separately authored parent audit first accepts unchanged observations, then requires the specific expected rejection for each mutation. Its stronger event-origin check catches the extra write in the executed import-effect control before stored-effect comparison. Two audit regressions ensure that an ineffective mutation or an unrelated error cannot count as successful detection. An earlier control-setup indexing failure after a successful comparison run remains preserved.

Worker and parent fresh replays rebuilt both variants and repeated original execution with zero mismatches. The parent independently parsed original Mach-O symbol, section and indirect-import records; checked layout and instruction bytes; replayed every recorded memory event; bound imported-service arguments, source reads and destination effects; and recomputed coverage. All deterministic original evidence files match across the three runs. Complete native text sections match for production objects, fixture, ABI observer and linked probes. Whole object files match; complete linked executable files differ and their hashes remain recorded. This establishes exact native text-section replay, not whole-executable identity or identity between x86 and ARM64 executables.

The original run records 718 consumed input paths. Each fresh replay records exactly one additional path, the sealed finalizer created after that run. The parent checked all 719 actual input identities, complete sealed evidence inventories, and output-write refusal controls. Existing CommonCrypto tests were inspected but were not used as the original-execution oracle.

Private evidence identifiers:

- Worker seal: `122ea1ddfe4a4398e30fdd0447f2f24ecc17565207fe9b5d7b3b9009a350f225`.
- Worker output manifest: `dd77b72660955f2ba04caba7923df838d6caae681d3e85296823db6254c6cac5`.
- Parent fresh replay seal: `0d99d1cbac09c2ecbde8153791df12caba961fb75162c86425162967dd024825`.
- Parent acceptance: `52034cb53c37dd5140c52be50d7f0c023aba6594acb25fb81d62148c03aa6793`.

Invalid pointers or lengths, overlap, intentional misalignment, enormous counts, arbitrary helper or landing-pad entry states, concurrent table mutation and whole-game/libc equivalence remain excluded. No game or renderer was launched. This is finite differential evidence; R2 and full reconstruction remain incomplete.
