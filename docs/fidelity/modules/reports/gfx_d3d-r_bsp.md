# gfx_d3d/r_bsp module reconciliation

Updated 2026-11-05.

**11/11 reconciled: 9 equal, 1 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

`R_LoadWorld` now calls `Dvar_ChangeResetValue(dvar, current)` for seven sun dvars, as Mac 0xe1e36–0xe1ed9 does. Native had called `Dvar_SetModified`. The import identity was checked through `CL_InitRef` at 0x14811b and the slot at 0x1acd591, which contains `Dvar_ChangeResetValue` at 0x5312e.

The eleven bodies were hand compared, including world registration, dvar registration, sun-color normalization, defaults, lookups and vertex-buffer finish. `R_UpdateLights` retains the existing native no-world guard. Mac 0xe1cb0 can write through a null-derived world address before a map is loaded; loaded-world arithmetic matches.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
