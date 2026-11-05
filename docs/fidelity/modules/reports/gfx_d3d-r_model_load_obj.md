# gfx_d3d/r_model_load_obj module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 equal.** Mac 1.3 i386 is the authority.

`R_LoadXSkins` at Mac 0x000d4248 was hand compared with the selected native body: LOD iteration, material count, lowercased 64-byte material names, registration arguments and pointer-sized material-table allocation match. Scripted triage and literal checks supported the body comparison.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
