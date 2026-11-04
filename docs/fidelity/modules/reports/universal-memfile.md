# universal/memfile module reconciliation

Updated 2026-11-04.

**8/12 reconciled: 8 equal, 4 unchecked.** Mac 1.3 is the authority; Linux 1.3 was read only. No source changes were needed.

All seven Mac bodies were read from exact symbol entries. The full-binary disassembly misaligns `MemFile_InitForWriting` at 0x9fb98, so the comparison used `otool -p` for every function. Triage and constant checks were also run; their call differences are compiler inlining, and their extra constants are addresses, stack sizes or the string-length scan.

Both initializers match their field stores and archive callbacks. The five common initialization stores are visible in Mac 0x9fb98 and 0x9fd34 and match the native inline implementation of `MemFile_CommonInit`. Read and write data functions match the zero-length and sticky-overflow returns, signed capacity comparison, error arguments, copy direction and cursor update. The archive writer matches the ordinary writer. `MemFile_WriteCString` matches the string-length calculation and the writer inlined at Mac 0x9fdde. `MemFile_ReadCString` matches the byte scan, cursor updates, terminator consumption and overflow path at 0x9fcc8.

## Storage contract

`MemoryFile` borrows its buffer and does not allocate or free it. Native restart callers allocate a 0xa00000-byte temporary write buffer, then copy exactly `bytesUsed` bytes into a separate allocation for reading. `Z_MallocInternal` zeroes the requested allocation; it promises no extra string terminator. The native compiler confirms a 32-byte `MemoryFile`, with flags at offsets 16 and 17 and the full-width archive callback at offset 24. Named field accesses preserve the Mac fields at offsets 12, 13 and 16.

The string reader already has Mac's unchecked initial byte read: calling it with an exhausted or empty buffer can read outside the supplied extent. No new unsafe storage behavior was introduced, and the comparison does not establish defined behavior for invalid buffers or counts.

## Unresolved identities

`MemFile_IsReading`, `MemFile_IsWriting` and `MemFile_Shutdown` have only address-zero EH symbols in Mac, with no callable body. `MemFile_SkipData` has no Mac symbol, and its unique overflow diagnostic is absent from the binary. A scan of all 92 named Mac memory-file references found calls and the two initializer callback stores, with no mode-query comparison. None of these four helpers has a native definition or caller; the Linux reconstruction also has no callers. They remain unchecked because an absent Mac identity cannot establish equality. In particular, the Linux shutdown helper's pointer-sized clear was not introduced into native storage.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. The existing string-length narrowing warning in `MemFile_WriteCString` remains. No per-fix tests, full CI or replays were run, and no memory-file operations were exercised at runtime. The legacy source is unchanged. No engine zlib wrapper occurs in this assigned module; bundled zlib was not examined.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-memfile.noindex/`.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
