# qcommon/cm_polylib module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 6 intentional omissions.** These are Linux-only winding utilities. None of the six names has a Mac text symbol, native definition or native caller. The Linux reference has no callers outside `cm_polylib.cpp` either. Mac also lacks the distinctive winding error strings. There is no native `cm_polylib` translation unit to run through the source triage or constant checker.

`BaseWindingForPlane` and `CopyWinding` reference `AllocWinding`; `ChopWindingInPlace` references allocation and freeing. That internal call graph does not supply a reachable native collision path. No Linux-only implementation was imported, and no behavioural equality is claimed for absent bodies.

## Verification and limits

The symbol table, binary strings and native/reference caller searches were inspected. All six rows record deliberate omission rather than a compared Mac implementation. No code changed in this module.

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.
