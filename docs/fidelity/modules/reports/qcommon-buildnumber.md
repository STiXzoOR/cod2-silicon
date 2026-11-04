# qcommon/buildnumber module reconciliation

Updated 2026-11-04.

**1/1 reconciled: 1 equal.** Mac `getBuildNumber` at 0x181f00 formats the build identifier and date into the shared buffer and returns that buffer. Its resolved format, `pc_1.3_1_1` identifier and `Mon May 01 2006 05:05:43PM` date match the native patch-1.3 branch. Windows 0x4011b0 uses the same constants and format, so no platform override is needed.

The module compiled in the arm64 Release dedicated target. The function and constants were reviewed by hand after triage/constant checks. No runtime invocation, new tests, full CI or replays were run, and the inactive pre-1.3 branch was not reconciled.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-common.noindex/`.
