# bgame/bg_weapons_load_obj module reconciliation

Updated 2026-11-04.

**15/15 reconciled: 13 equal, 1 fixed, 1 intentional.** Mac 1.3 is the authority. The legacy source is unchanged.

All nine Mac functions were read against native source. The transition-time, damage-range, projectile-validation, string-initialization and string-array helpers were compared where Mac inlines them. The parser switch table at 0x32a680 was decoded. A private script compared all 366 weapon-field names, target members and field types, and all seven enum-name arrays, with the Mac data tables; they match.

## Correction

**Weapon-field parse errors lacked the message prefix.** Mac `BG_ParseWeaponDefSpecificFieldType` at 0xbce12 uses strings at 0x24d198 through 0x24d2d8 whose first byte is 0x15. Native omitted it from all eight unknown-value errors and the bad-field-type error. The native branch now preserves the prefix. Every original error statement remains in its legacy branch.

The Linux-only `BG_GetWeaponTypeName` accessor has no Mac code symbol or native caller. Mac references the type-name table only from the parser lookup at 0xbce5d, which native implements directly. Its ledger row records that deliberate absence; there is no missing Mac runtime implementation.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled and linked with `taskpolicy -b nice -n 19` and two jobs. The changed translation unit retains its existing attribute, pointer-cast and size-conversion warnings. Triage and constant checks were run, and the nine new conditional branches were stripped by script to confirm the prior legacy text is recovered exactly.

No new tests were written, and no CI or replays were run. The corrected error paths are compiled and reviewed but were not exercised. No entries remain unchecked. Integration and replay validation belong to the orchestrator.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-bg-weapons-load.noindex/`.

## Integrated batch verification

Integrated source through `98acbdd` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The legacy guard check differs only for the three intended `linux-x86_64-port-on` source paths. An independent reviewer checked the changed source, nearby callers and Mac binary evidence and found no actionable defects; this was not a second whole-module audit.

Both 2,000-frame stock and CoD2x replays are byte-identical to the fresh `819d282` baseline for player, entity and state records. Against the Linux original, all 3,806 player records agree on the six logic fields; maximum origin drift remains 0.11792 units and velocity drift 2.0. The recording does not demonstrate the corrected branches, so those corrections remain compiled and reviewed rather than individually runtime-verified. No new per-fix tests were added.
