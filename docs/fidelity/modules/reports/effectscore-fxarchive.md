# EffectsCore/FxArchive module reconciliation

Updated 2026-11-05.

**25/26 reconciled: 17 equal, 5 fixed, 3 intentional; 1 unchecked.** Mac 1.3 i386 is the authority. Each executable entry was read from exact-symbol disassembly; Linux supplied helper names only. The legacy OFF bodies are unchanged.

## Corrections

**The writer only copied bytes.** Mac `WriteData` at 0xa9f0e creates and extends literal controls, combines one, two or four literals with zero runs, splits at 64 bytes, and rewinds a literal run after three trailing zeroes. Native now follows those transitions, including writes split across calls.

**A curve pointer was only half cleared.** Mac 0xaaa08 resets the iterator index and master after restoring four scale bytes. Native now clears the full pointer. Measured native sizes are 32 bytes for `FxArchive`, 16 for its curve iterator, and 24 for a channel instance; the original 24/8/12-byte layouts cannot be used for native allocation.

**Entity archive copies used the wrong layout.** Mac 0xaacac copies 104 bytes. The native entity is 112 bytes: an 8-byte pointer followed by 100 scalar bytes and trailing padding. The native archive maps a four-byte material token plus those 100 scalar bytes, preventing a 112-byte copy into a 104-byte temporary. The token retains only the low 32 bits and is process-local, as in the original archive. `Effect_Archive`, the sole production caller, replaces it through `ArchiveMaterial` before using the material. Cross-process pointer identity is not claimed.

## Storage and error contracts

Valid positive-count reads match Mac 0xa9e00's literal and zero-run state machine. The normal `MemFile_InitForReading` path sets `errorOnOverflow=1`; `MemFile_ReadData` calls `Com_Error(1)`, which longjmps. The archive read API has no failure result. With a nonfatal reader, Mac can expose a local byte that was never written. Native instead zero-fills the unread remainder and leaves `memoryOverflow` set; nonpositive reads are no-ops. Native writes stop after nonfatal overflow instead of continuing direct control-buffer mutations. These are intentional defined-storage adaptations, with no claim of malformed-stream equivalence.

`FX_Restore` is the only production `BeginReading` caller, although no call to `FX_Restore` itself was found in the native source. It stops on effect type zero, so a later zero-filled read terminates its loop. `FxScheduler_Archive` can continue through its already decoded positive pending count after overflow; zero-filled remaining names produce null effects and the allocated nodes are freed. This remains a caller contract limitation, and the archive API is not a general malformed-input validator. Primitive archive consumers also continue because they have no read-error result.

`BeginWriting` remains unchecked. Mac has only a zero-valued absolute exception symbol for it, no executable body or identified inline initializer; native has no definition or caller. Writer verification assumes a valid initialized `MemoryFile` and `startPos`. No implementation was inferred from Linux.

## Verification and limits

Both owned files compile with the native arm64 client engine flags. The arm64 Release `cod2_macos_ded` target builds, but excludes `FxArchive`; that build does not exercise archive code. Existing dedicated FX and `fx_channels` fixtures pass and do not cover archive reads or writes. No archive runtime fixture or new per-fix tests were added. No full CI or replay was run by this worker. Five OFF preprocessing configurations per changed source match the base, and `git diff --check` passes.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-template.noindex/`.
