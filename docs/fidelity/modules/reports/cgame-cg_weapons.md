# cgame/cg_weapons module reconciliation

Updated 2026-11-05.

**28/28 reconciled: 16 equal, 6 fixed, 6 intentional.** Mac 1.3 i386 is the authority. Every assigned body was read, including `CG_AddViewWeapon`, which was routed here from `universal/com_vector`. Native changes use `COD2_X64` guards; the legacy bodies are unchanged.

## Corrections

`CG_AddViewWeapon` at 0x1da2f8 had a reduced implementation. The complete native path now includes the test-gun gate, shellshock sway scaling, predicted ADS bookkeeping, lean, stance and movement offsets, minimum smoothing steps, ADS position fade and landing interpolation. Position uses the previous viewmodel origin/angles/axes, as Mac does. Weapon-angle time and nonzero damage time use `ps->deltaTime`; updated movement angles, idle factor, gun offsets and recoil speed are copied back. Overlay-aware ADS pitch/yaw use the final axis and angle normalization. Native had used the saved-shellshock-screen flag, refdef frame and command time, and omitted the state writeback. The existing opt-in viewmodel diagnostics remain.

`CG_WeaponRunXModelAnims` now forces ADS-out during the early reload interval, as 0x1dc57c does. Its animation bounds are unsigned, matching 0x1dc45f; negative values take the unknown-animation path. All 20 targets of the switch table at 0x33c5e8 were decoded and checked against the native animation map. Every goal-weight branch was compared.

`CG_UpdateViewWeaponAnim` now retains the preceding notetrack sound alias when a later notification has an unrecognized name, matching 0x1dd70a–0x1dd7e5. This copies a deterministic Mac quirk. Native had cleared the alias each iteration.

`CG_OutOfAmmoChange` used a zero-filled tentative declaration for the preferred-slot table. Mac data at 0x33c5e0 contains `{1, 2}`. The native initializer is restored after the existing field assertions, preserving their line expansions.

`CG_CycleWeap` now validates ownership after its final fallback selection. Mac's slot fallback at 0x1dbf8c and non-slot fallback at 0x1dbf6f reach the ownership check at 0x1dbd64; native returned early.

`CG_Weapons_SetToDefault` restores the full fallback warning at 0x1d7d44, including the switching-to-default phrase and the single trailing newline. Independent review corrected an extra newline introduced in the first reconciliation commit; parent read the NUL-terminated Mac bytes at 0x2e3724 to confirm it.

## Preserved native behavior

The native pointer-width repairs and typed entity layouts remain. In particular, impact-effect pointer indices from dac9522 correspond to Mac's six 23-entry blocks. The pre-existing null effect-table handling and `CG_AddPlayerWeapon` DObj guard remain defined native behavior; missing models do not trigger a null brass-tag lookup.

Gated CoD2x radar and bullet diagnostics from a046b8a remain. The oversized item-configstring rejection from 380bf2b, no-argument weapon-slot command callback from 87393ca and disabled-by-default diagnostics from 83fd4e7 are retained. These six affected rows are recorded as intentional with the compared Mac core described individually.

## Verification and limits

Both assigned client translation units compile without warnings using the generated `cod2_macos` arm64 Release flags. `cg_weapons.c` also compiles with CoD2x and viewmodel diagnostics enabled. The required arm64 Release `cod2_macos_ded` target built with `taskpolicy -b nice -n 19` and two jobs. Dedicated excludes cgame, so that build alone is not evidence for these client paths.

Call/string triage and the constant checker ran for every row. Exact-symbol-entry disassembly confirms all 37 assigned entries begin at their ledger addresses. STABS field offsets, imported gun dvars, the animation switch, preferred-slot table and offhand string table were checked directly. All five legacy configurations match the base preprocessing, including a comparison with actual line expansions; the existing field-assertion lines are preserved. `git diff --check` passes.

All corrections are compiled and reviewed but not exercised in a running client. No new tests, full CI, replays, pushes or shared-header edits were made. Runtime weapon sway, reload transitions, landing/lean/stance motion, notetrack repetition, ammo switching and fallback warnings remain for parent integration validation. No assigned rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-weapons.noindex/`.
