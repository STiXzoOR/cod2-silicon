# gfx_d3d/r_debug module reconciliation

Updated 2026-11-05.

**13/13 reconciled: 13 equal.** All original Mac entries at 0xed2dc through 0xeda68 were read against the active native bodies and inline helpers. Triage and constant checks supplemented that comparison. No source correction was needed.

The review covered initialization limits, all seven free/null branches, external line/string allocation and copies, string truncation, polygon and line capacity gates, plume insertion/expiry/swap removal, and scaled-string arithmetic. The 24 edge-table integers at 0x32b380 exactly match the twelve native box edges. Scaled strings retain the original ordered dot product, subtraction of 0.995, distance multiplication and lower clamp at 1, including retaining a NaN result.

Native typed allocation sizes were checked: DebugGlobals grows from 84 to 112 bytes because its pointers widen; line, string, plume and polygon records remain 44, 128, 40 and 24 bytes. R_AddPlume at 0xed694 writes RGB but leaves alpha untouched in Mac and native. The active native Z_MallocInternal zeroes new allocations, and the field remains unchanged afterward; no indeterminate storage was introduced to imitate that omitted write. Shutdown deliberately leaves counts unchanged, and expiry keeps a plume whose age equals its duration.

Verification: r_debug.c compiled with the actual local arm64 Release command. The dedicated target also built at low priority with two jobs. No new tests, full CI or replays were run; debug rendering and allocation/error boundaries were not runtime exercised. The other units' renderer/provider work is not evidence of whole-frame parity.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-commands.noindex/`.
