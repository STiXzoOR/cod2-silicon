# ui/ui_gameinfo_mp module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 1 equal, 1 fixed.** Compared both complete Mac bodies by hand and reviewed the call/string and constant checks. `UI_ParseInfos` at 0x180a0c matches the brace/error handling, 1024-byte key/value buffers, missing-value substitution, allocation sizing, null-allocation retry, and info copying.

`UI_LoadArenas` at 0x180bee read the wrong key. Mac 0x180dd8 passes the string `gametype` at 0x2d5f58; native passed `type`. The native path now reads `gametype`. Mac 0x180ea9 also continues the game-type scan after setting a matching bit; native stopped at its first match. The scan now preserves every matching entry. An unsigned shift represents the Mac 32-bit mask, including bit 31 of the 32-entry game-type table. These changes are confined to `COD2_X64`; the legacy text is preserved.

The remaining file-list iteration, 8191-byte size limit, arena cap, map names, loadscreen registration, absent-game-type mask, and 128-map limit match. No visible platform-specific behavior required Windows comparison.

The arm64 Release dedicated target built successfully. The actual production client `ui_gameinfo_mp.c` translation unit also compiled under the configured flags; it reported three existing size-to-int conversion warnings. The corrections are compiled and reviewed, but were not exercised in the UI. No tests or replays were added or run. All five inactive configurations from the existing guard checker preserve the baseline bodies; neither changed file contains `__LINE__`.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.

## Integrated batch verification

Integrated source 4584867 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
