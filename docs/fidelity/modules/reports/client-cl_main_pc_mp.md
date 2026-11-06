# client/cl_main_pc_mp module reconciliation

Updated 2026-11-04.

**20/20 reconciled: 14 equal, 3 fixed, 3 intentional.** Every assigned Mac body was read alongside the native source, with call/string triage and constant checks. Exact-symbol disassembly corrected the full-binary decoder's misaligned `CL_CompareAdrSigned` entry. The legacy-off source is unchanged.

## Corrections

**The browser read the wrong PunkBuster response key.** Mac 0x160cb1–0x160cc9 reads `pb`, before reading `voice`. Native read `sv_punkbuster` after `voice`. The native patch-1.3 path now follows Mac. Windows 0x4b34ae–0x4b34c6 confirms the same key. This is engine browser metadata, which remains meaningful with the documented PunkBuster null adapter.

**Rcon omitted its terminating NUL on the wire.** Mac 0x1617b5–0x1617ba passes the string length including NUL to `CL_Netchan_SendOOBPacket`. Native now includes that byte; its downstream packet sender forwards the supplied length unchanged.

**Master replies used different delimiter and EOT decisions.** Mac 0x162043–0x1620a4 requires the following backslash before counting an endpoint, prints it, and checks the next EOT marker afterward. Windows 0x4b3d93–0x4b3dea follows the same order. Native now restores those decisions and the debug print, preserving pointer-width comparisons and bounded reads. The earlier F2 report documented accepting terminal tuples without a following delimiter and checking EOT first; neither difference was supported by Windows. Short EOT tails remain bounded instead of reproducing the original possible out-of-buffer reads.

## Retained native behavior

`CL_ServerInfoPacket` keeps one extra local byte for the original maximum announcement append and the CoD2x protocol extension. Its stock ping handling, LAN selection, stale-slot clears and message formatting match Mac. `CL_GlobalServers_f` retains CoD2x queries for both protocols and its configurable second master; the stock request follows Mac.

`CL_WWWDownload` belongs to this Mac module but its native body is in `cl_parse_mp.c`. It was inspected without editing that other owner. The curl status API intentionally replaces the libwww pump return. The engine still handles MOTD loading, installation under the saved local name, rename fallback, reconnect/completion commands and failures. Safe empty-path trimming, literal error forwarding and cleanup of additional native connection filename copies remain intentional adaptations.

Equal examples include the signed-byte CD-key CRC, all 64 mute bytes, rcon login/logout/host handling, ping expiry and oldest-slot selection, local/global/favorite address updates, and both server-status request paths. The private ledger records each comparison.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled and linked with two jobs at low priority. That target omits client sources, so the complete client translation unit was also compiled separately in stock and CoD2x modes. The unchanged browser packet fixture passed all ten groups in both modes; the unchanged master-response and WWW-download fixtures also passed under AddressSanitizer and UndefinedBehaviorSanitizer.

The five legacy-off guard configurations pass. Actual SDK preprocessing with `COD2_X64` removed is byte-equal to the base, including actual `__LINE__` expansions. `git diff --check` passes. No tests were added or changed, and no full CI, replay, live browser, rcon send or HTTP transfer was run. The positive `pb` value, rcon byte count and every corrected master edge decision were compiled and reviewed without targeted runtime coverage. The parent owns integration and runtime validation.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-platform.noindex/`.

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
