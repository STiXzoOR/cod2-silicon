# ui/ui_utils module reconciliation

Updated 2026-11-05.

**30/30 reconciled: 27 equal, 2 fixed, 1 intentional.** Compared every assigned function with its Mac body, including parser failures and partial rectangle writes, first-client rectangle/cursor/flag updates, edit-field type membership, colored-text cursor movement, screen-coordinate border/alignment handling, and menu item iteration. The call/string and constant checks were reviewed.

`Item_EnableShowViaDvar` at 0x192036 skipped only one semicolon before comparing a value. Mac 0x19213f loops back to the semicolon check after each successful parse. The native path now skips every consecutive standalone semicolon and preserves the existing flag-dependent exhaustion result.

`Item_IsEditFieldDef` at 0x192150 used a signed upper-bound check. Mac 0x19215d uses an unsigned comparison against 18, rejecting negative types before shifting. The native path now performs the same unsigned range check. Both corrections preserve the legacy text under `#else`.

`String_Alloc` at 0x1924fa intentionally retains the documented native low-permanent-hunk pool. Mac uses `Hunk_AllocAlignInternal`, the high hunk; native uses `Hunk_AllocLowAlignInternal` so UI strings and pointer-sized nodes survive high-hunk mark rollback. The allocator implementations and mark reset were inspected. Null/empty handling, lowercase hash weighted by index + 119 and masked with 2047, exact string lookup, and allocation/linking behavior were compared. The Mac's predecessor-link insertion quirk is preserved; it was not corrected. Pointer-sized node storage/alignment is retained for LP64.

The arm64 Release dedicated target built successfully. The actual production client `ui_utils.c` translation unit also compiled under the configured flags, with two existing warnings: an ignored `packed` attribute and a string-allocation size conversion. The corrections are compiled and reviewed, but were not exercised in the UI. No tests or replays were added or run. The existing guard checker's five OFF configurations preserve both changed files' baseline bodies, and neither uses `__LINE__`. No visible platform-specific behavior required Windows comparison.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.
