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

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
