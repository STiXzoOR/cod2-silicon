# qcommon/cm_staticmodel_load_obj module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 1 equal, 2 fixed.** Read all of `CM_LoadStaticModels` at 0x77724-0x77d9c. Mac inlines the ledger's `CM_CreateStaticModel` and `CM_InitStaticModel`; their native equivalents are blocks in that loader.

## Corrections

The invalid-name error at 0x77ce1 references `\x15Invalid static model name\n`; native used a different message without the prefix or newline. The zero-scale errors at 0x77d13, 0x77d51 and 0x77d83 also carry the leading 0x15 byte, which native omitted. All four diagnostics now use Mac's bytes under `COD2_X64`. The legacy branches retain their previous text.

The remaining two-pass parser, valid-name filter, failed-precache count adjustment, axis scaling, matrix inversion and conditional bounds translation match the compared Mac paths. The existing native allocation uses `sizeof(cStaticModel_t)` for the wider pointer layout.

## Verification and limits

The binary's four error strings were read directly and the string triage rerun after the change; no string difference remains. Extra Mac parse/copy calls in triage are loop expansion, checked in the full body. A script removed only the newly added `COD2_X64` branches and reproduced the original source bytes exactly. The corrections were compiled and reviewed, not exercised with invalid map entities.

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.

## Integrated batch verification

Integrated source through `40d92c3` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
