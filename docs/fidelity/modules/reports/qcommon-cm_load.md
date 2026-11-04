# qcommon/cm_load module reconciliation

Updated 2026-11-04.

**17/17 reconciled: 15 equal, 2 fixed.** Mac 1.3 is the authority. The nontrivial bodies were read against the Mac disassembly, including the thread initialization and header-swap helpers inlined into their callers. Mac initializes one trace-thread record here; the extra Linux database-thread initialization is absent from Mac.

## Corrections

**Error messages lost localization bytes.** `CM_LoadMap` now retains the leading 0x15 byte used at Mac 0x73a24. `CM_SaveLump` retains the byte before the filename placeholder at 0x73d65.

**Two save errors continued into file rewriting.** Mac returns after the invalid-version error at 0x73bb6 and write-open error at 0x73da4. The native source now has those returns. The ordinary 39-lump offset, alignment, write and optional checksum paths match.

## Verification and limits

The ARM64 Release `cod2_macos_ded` target built with `COD2_X64=ON`, at low priority with two jobs. A before/after preprocessing comparison with `COD2_X64` disabled was byte-identical for both assigned translation units. Final call/string triage has no string mismatches; the remaining call-count differences are compiler-inlined copies.

No new per-fix tests were written or fixtures changed. No CI or replay was run by this worker. These corrections are compiled and reviewed, but not exercised. Nothing remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-loading.noindex/`.
