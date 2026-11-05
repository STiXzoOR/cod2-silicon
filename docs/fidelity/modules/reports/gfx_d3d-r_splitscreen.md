# gfx_d3d/r_splitscreen module reconciliation

Updated 2026-11-05.

**6/6 reconciled: 6 equal.** Every Mac body at 0xee1e4 through 0xee372 was read against native source, supplemented by triage and constants. No source correction was needed.

The review checked five groups, four view markers each, -1 sentinels, glue ownership, first-section return values and close/reopen behavior. Issuing preserves begin, ascending views, end order and resets each issued marker. Native tracing is opt-in and does not change these command/glue fields.

Delayed command generation and the actual pending df8d9cc GOTO/CALL/RETURN consumers were also read. They use native pointer fields and byteCount. The separate R_EndDelayedDrawing correction in r_rendercmds.c keeps the command following a closed section eight-byte aligned; the original four-byte RETURN had broken that native layout contract. This module's equal verdict concerns group state and issue order, not i386 byte offsets or unresolved frame timing.

Verification: r_splitscreen.c compiled with the actual local arm64 Release command. The dedicated target built at low priority with two jobs. No new tests, full CI or replays were run; split-view/group rendering was not runtime exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-commands.noindex/`.
