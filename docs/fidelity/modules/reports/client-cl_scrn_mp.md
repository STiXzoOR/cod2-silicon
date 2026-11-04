# client/cl_scrn_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 7 equal, 1 fixed, 1 intentional.** Complete Mac 1.3 bodies were compared with native source. The state table at 0x33b3c0 and cubemap suffix pointers at 0x34b7c4 were decoded. Exact-symbol disassembly corrected a misleading small-string prologue from the full-binary dump.

## Corrections

`SCR_UpdateFrame` omitted command sending when the active, non-fullscreen draw returned zero (Mac 0x17302c/0x173152). It also opened the main menu during disconnected WWW downloads instead of keeping the connection screen (0x172fac), refreshed UI during cinematics (0x172e5c), and ended/issued the view before UI instead of afterward (0x172e78–0x172e90). Native also closed the main menu automatically between UI time update and fullscreen selection, a branch absent from Mac 0x172e07–0x172e13.

The recording overlay rounded file size upward instead of using signed division by 1024 (0x173057–0x173063), used a shortened one-third float instead of 0x3eaaaaab (0x1730b1), and omitted the diagnostic string's control prefix. These now follow Mac. The existing CoD2x overlay and its 0.2 font scale remain.

## Verification and limits

The native renderer-availability guard, asset-loading/reentrancy guards and opt-in diagnostic hooks remain. `SCR_UpdateScreen` therefore retains its additional `g_cginit_loading` gate. Renderer export font handles retain the native pointer-sized ABI.

No new tests, full CI or replays were run. Existing input/platform probes were inspected for coverage; they do not pin these branches. The arm64 Release dedicated target built, and this client translation unit compiled separately with the configured client flags. Five `COD2_X64=OFF` preprocessing configurations match the base; this file has no `__LINE__` expansions. Corrections are compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.
