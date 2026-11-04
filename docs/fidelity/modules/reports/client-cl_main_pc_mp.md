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
