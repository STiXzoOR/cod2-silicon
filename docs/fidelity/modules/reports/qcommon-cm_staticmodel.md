# qcommon/cm_staticmodel module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** Read the Mac 1.3 bodies at 0xbd718 through 0xbd94c alongside the native source, with call/string triage and literal checks. No code changes were needed.

Both allocation callbacks forward to `Hunk_AllocInternal`; precache passes the callbacks in the same order. `CM_TraceStaticModel` subtracts the model origin, transforms both endpoints, obtains the base pose, and traces. A nonnegative hit writes entity 0x3fe and transforms then normalizes the surface normal. The complete variant initializes only the fraction to 1.0 and returns the sign bit of the trace result, as Mac does.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
