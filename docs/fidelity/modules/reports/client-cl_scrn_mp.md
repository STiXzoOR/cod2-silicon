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

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
