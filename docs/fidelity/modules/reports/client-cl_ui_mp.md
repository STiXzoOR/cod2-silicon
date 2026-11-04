# client/cl_ui_mp module reconciliation

Updated 2026-11-04.

**28/28 reconciled: 23 equal, 2 fixed, 3 intentional.** Complete Mac 1.3 bodies, UI lifecycle ordering, server-info keys and signedness, key/sound wrapper arguments, CD-key copies and client-name scanning were compared. The native layout probe confirms that `GetClientname`'s existing 0x26e0/0x26e8 offsets still name the snapshot count and parse-client start.

## Corrections

The decoded sort table at 0x33b980 uses column 9 for PunkBuster and column 10 for ping. Native used column 9 for ping and omitted column 10. `LAN_CompareServers` now follows the complete Mac dispatch and tie-breaks, rejects unknown sources, and resolves the second map display name before the first, as 0x181e41–0x181e59 does.

The missing `LAN_GetServerPunkBuster` metadata accessor was restored from Mac 0x1815ce–0x18165a, including invalid-index/source results. Reading advertised server metadata remains distinct from running the bundled PunkBuster backend.

## Native boundaries

The restored `CLUI_SetPbClStatus` wrapper preserves Mac's enable/disable dispatch, but its targets remain the documented `pb_public.h`/`pb_null_adapter.c` null adapter. `CL_InitUI` keeps opt-in native tracing. `LAN_CompareHostname` retains the existing 40-byte native scratch arrays rather than reducing them to Mac's 32 bytes; filtering and fallback ordering match for the declared, bounded server hostnames. Input/key and sound dependencies were read only and required no cross-file edits.

## Verification and limits

No new tests, full CI or replays were run. The arm64 Release dedicated target built, and the client translation unit compiled separately with the configured client flags. All five OFF preprocessing configurations match the base; there are no `__LINE__` expansions in this file. Corrections are compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.
