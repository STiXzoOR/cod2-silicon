# qcommon/buildnumber module reconciliation

Updated 2026-11-04.

**1/1 reconciled: 1 equal.** Mac `getBuildNumber` at 0x181f00 formats the build identifier and date into the shared buffer and returns that buffer. Its resolved format, `pc_1.3_1_1` identifier and `Mon May 01 2006 05:05:43PM` date match the native patch-1.3 branch. Windows 0x4011b0 uses the same constants and format, so no platform override is needed.

The module compiled in the arm64 Release dedicated target. The function and constants were reviewed by hand after triage/constant checks. No runtime invocation, new tests, full CI or replays were run, and the inactive pre-1.3 branch was not reconciled.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-common.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
