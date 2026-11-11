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

## Integrated batch verification

Integrated source `ca4ed64` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
