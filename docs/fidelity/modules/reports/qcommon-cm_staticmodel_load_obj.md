# qcommon/cm_staticmodel_load_obj module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 1 equal, 2 fixed.** Read all of `CM_LoadStaticModels` at 0x77724-0x77d9c. Mac inlines the ledger's `CM_CreateStaticModel` and `CM_InitStaticModel`; their native equivalents are blocks in that loader.

## Corrections

The invalid-name error at 0x77ce1 references `\x15Invalid static model name\n`; native used a different message without the prefix or newline. The zero-scale errors at 0x77d13, 0x77d51 and 0x77d83 also carry the leading 0x15 byte, which native omitted. All four diagnostics now use Mac's bytes under `COD2_X64`. The legacy branches retain their previous text.

The remaining two-pass parser, valid-name filter, failed-precache count adjustment, axis scaling, matrix inversion and conditional bounds translation match the compared Mac paths. The existing native allocation uses `sizeof(cStaticModel_t)` for the wider pointer layout.

## Verification and limits

The binary's four error strings were read directly and the string triage rerun after the change; no string difference remains. Extra Mac parse/copy calls in triage are loop expansion, checked in the full body. A script removed only the newly added `COD2_X64` branches and reproduced the original source bytes exactly. The corrections were compiled and reviewed, not exercised with invalid map entities.

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.
