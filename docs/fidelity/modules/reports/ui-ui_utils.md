# ui/ui_utils module reconciliation

Updated 2026-11-05.

**30/30 reconciled: 27 equal, 2 fixed, 1 intentional.** Compared every assigned function with its Mac body, including parser failures and partial rectangle writes, first-client rectangle/cursor/flag updates, edit-field type membership, colored-text cursor movement, screen-coordinate border/alignment handling, and menu item iteration. The call/string and constant checks were reviewed.

`Item_EnableShowViaDvar` at 0x192036 skipped only one semicolon before comparing a value. Mac 0x19213f loops back to the semicolon check after each successful parse. The native path now skips every consecutive standalone semicolon and preserves the existing flag-dependent exhaustion result.

`Item_IsEditFieldDef` at 0x192150 used a signed upper-bound check. Mac 0x19215d uses an unsigned comparison against 18, rejecting negative types before shifting. The native path now performs the same unsigned range check. Both corrections preserve the legacy text under `#else`.

`String_Alloc` at 0x1924fa intentionally retains the documented native low-permanent-hunk pool. Mac uses `Hunk_AllocAlignInternal`, the high hunk; native uses `Hunk_AllocLowAlignInternal` so UI strings and pointer-sized nodes survive high-hunk mark rollback. The allocator implementations and mark reset were inspected. Null/empty handling, lowercase hash weighted by index + 119 and masked with 2047, exact string lookup, and allocation/linking behavior were compared. The Mac's predecessor-link insertion quirk is preserved; it was not corrected. Pointer-sized node storage/alignment is retained for LP64.

The arm64 Release dedicated target built successfully. The actual production client `ui_utils.c` translation unit also compiled under the configured flags, with two existing warnings: an ignored `packed` attribute and a string-allocation size conversion. The corrections are compiled and reviewed, but were not exercised in the UI. No tests or replays were added or run. The existing guard checker's five OFF configurations preserve both changed files' baseline bodies, and neither uses `__LINE__`. No visible platform-specific behavior required Windows comparison.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
