# qcommon/cm_staticmodel module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** Read the Mac 1.3 bodies at 0xbd718 through 0xbd94c alongside the native source, with call/string triage and literal checks. No code changes were needed.

Both allocation callbacks forward to `Hunk_AllocInternal`; precache passes the callbacks in the same order. `CM_TraceStaticModel` subtracts the model origin, transforms both endpoints, obtains the base pose, and traces. A nonnegative hit writes entity 0x3fe and transforms then normalizes the surface normal. The complete variant initializes only the fraction to 1.0 and returns the sign bit of the trace result, as Mac does.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.
