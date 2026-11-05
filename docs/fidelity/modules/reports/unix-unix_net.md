# unix/unix_net module reconciliation

Updated 2026-11-05.

**16/17 entries reconciled: 3 equal, 2 fixed, 11 intentional; 1 unchecked.** Mac 1.3 is the authority. The selected native graph uses `src/PC/win32/win_net.c` for engine networking and `src/unix/linux_net.c` for four POSIX Winsock shims. Both native targets compile those providers. The engine address, packet and socket logic was compared with the Mac bodies and inline helper blocks; this is not a blanket platform-replacement verdict.

`Sys_IsLANAddress` at 0x1e82d4 retains the private IPv4 ranges and local three-octet comparison. `Sys_ShowIP` at 0x1e837e retains the unsigned-octet loop. The Linux-only `NET_ErrorString` helper is folded into `errno`/`strerror` call sites, including 0x1e88cc and 0x1e89d1. Address conversion helpers were identified inside `Sys_StringToAdr`, `Sys_GetPacket`, `Sys_SendPacket` and `NET_OpenIP`; their field values, branches, byte order and write widths were compared.

## Corrections

**Hard send failures were hidden.** Mac `Sys_SendPacket` returns false after the diagnostic at 0x1e89eb–0x1e89f0. It returns true for successful sends, unavailable sockets, would-block, and the ignored broadcast address-not-available error. Native returned void, and its engine caller always reported success. The native provider now returns `Bool` and the authorized `NET_SendPacket` caller propagates it, matching 0x1585c4–0x1585c9. The bad-address diagnostic also regains its Mac 0x15 prefix at 0x1e8914. This supersedes the earlier dispatch-success adaptation recorded in the qcommon/net_chan_mp report.

**The SOCKS greeting checked the wrong send.** Mac 0x1e925c checks the actual three- or four-byte greeting send and takes the send-error path at 0x1e934d. Native ignored that result and checked a separate zero-length send. It now checks the greeting result and emits the Mac diagnostic before leaving SOCKS setup. The wrong-address-family warning also matches Mac 0x1e9190.

## Retained adaptations and limits

The native SDK uses complete `fd_set`, `timeval` and `sockaddr_in` types, fixed four-byte IPv4 copies, and descriptor -1 as the invalid sentinel so descriptor zero remains usable. The native address contains additional IPX storage, placing `port` at offset 0x12 rather than Mac 0x8; typed writes preserve adjacent storage. `NET_Init` follows Mac 0x1e9708 through the native Winsock shim and, on Apple, matching typed storage. Failed descriptor cleanup, SOCKS relay response length checks and the network restart relay reset remain existing native adaptations.

The safe Winsock initialization storage is specific to Apple: its `static WSADATA winsockdata` matches `WSAStartup`'s `sizeof(WSADATA)` clear. The pre-existing non-Apple COD2_X64 path instead allocates `char winsockdata[400]`, while `unix/linux_net.c` clears the LP64 `WSADATA` size of 408 bytes. That exposes an eight-byte overwrite risk outside the declared array. This assignment did not build or exercise Linux, and leaves that production path unchanged.

`NET_GetLocalAddress` is inlined in Mac `NET_OpenIP` at 0x1e8d72. Mac pairs AF_LINK and AF_INET interface records and excludes the loopback interface type. The existing native `getifaddrs` provider enumerates up IPv4 interfaces, includes loopback, caps the list at 16 and releases it. Those addresses feed the compared LAN check and IP diagnostic. The Linux-only `NET_IPSocket` row corresponds to the inlined nonblocking/broadcast socket, resolver, bind and retry blocks in the same Mac caller; the SDK storage and cleanup differences are retained explicitly.

The authenticated SOCKS greeting leaves its fourth scratch byte unassigned in both the Mac instructions at 0x1e9231–0x1e925c and the existing source. Its value cannot be equated across stack layouts. The existing 64-byte credential scratch buffer can also be exceeded by long credentials, and the existing 4096-byte SOCKS packet buffer does not establish behavior for larger OOB payloads. These unresolved storage contracts were not changed or certified. No new indeterminate storage was introduced. Native already rejects relay responses below ten bytes; Mac can reuse stale scratch bytes for short responses.

`Sys_PBSendUdpPacket` at 0x1e8aae resolves the PB server address and sends through the engine provider. It has no native definition or caller after the documented bundled PunkBuster null-adapter replacement (`pb_null_adapter.c` and WS44). Ordinary engine packet I/O was compared separately.

`NET_BaseAdrToString` and `NET_Shutdown` remain unchecked: neither has a Mac executable symbol, a native body, or a securely identified inline counterpart. Mac `NET_AdrToString` includes a port, and `NET_Config(0)` supplies cleanup; neither observation proves the missing identities.

## Verification

The final arm64 Release `cod2_macos_ded` build passed with `taskpolicy -b nice -n 19` and at most two jobs. Existing `system_network`, `system_closed_stdin` and `system_no_home` fixtures passed, exercising loopback UDP payloads, address storage, native wait sets and descriptor cleanup, including descriptor zero. Existing `f1_transport` passed under ASan/UBSan. The runner selects the stub signature from the selected source's native declaration: current `Bool` returns true, while the prior baseline retains void. Both current and `--baseline cef44cbe5ab119e14176b8495c826bff3cc12de9` fixtures passed under ASan/UBSan after the compatibility correction; all cases and assertions are unchanged.

The two production files have identical normalized preprocessing in all five COD2_X64=OFF guard configurations. Neither changed file uses `__LINE__`. The general guard script also reports the intentional native-on changes and the architecture-independent fixture signature change; those are not legacy production changes. No new tests, full CI, replay, external packet/service check or live SOCKS handshake was run. The error-result branches, SOCKS correction and diagnostics are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-platform-network.noindex/`.

## Remaining-identity audit (batch 22)

Both remaining rows stay unchecked. The exact Mac `NET_Config` body at 0x1e9640 owns conditional networking reconfiguration: unchanged disabled state can return immediately; state changes/restarts can close both UDP and SOCKS sockets. The selected `win_net.c` preserves that state machine with its already documented native descriptor adaptations. Mac `Sys_Quit` at 0xc3be0 and selected `mac_main.c` instead terminate the process. These are positive cleanup owners, but neither is a returning unconditional UDP-close-and-reset helper. No original caller mapping establishes the Linux-only `NET_Shutdown` identity.

The native `NET_AdrToString` owner includes a port. Sharing octet formatting does not prove the bare-address `NET_BaseAdrToString` identity, output contract or static-buffer ownership. It also remains unchecked.

This follow-up changes reports only and finds no proven source defect. The full 101-row unit ledger preserves all 78 previously resolved status/evidence pairs exactly and refines the reasons for the 23 unchecked rows. No new row is certified. The existing `macos_system_network_test` and `macos_threads_ring_test` targets compiled in a private Debug build with two low-priority jobs; they were not run and do not prove any missing identity. No production build, new test/case/assertion, full CI, replay, push or external side effect was performed. Source and inactive builds are unchanged. Private inspection material is under `~/Library/Application Support/CoD2-native-decomp/recon-unix-remaining.noindex/`.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.


## Original executable provenance follow-up (platform6)

The original bare-address formatter is now anchored at original 1.0 0x80d2c0e and original 1.3 0x80d520a by a unique complete-body masked match. It formats four octets into its own 64-byte static buffer without a port. This is not the complete NET_AdrToString output/ownership contract.

NET_Shutdown is anchored at original 1.0 0x80d360e and original 1.3 0x80d5c7e. Its short cleanup shape also matches four unrelated original bodies; the actual ip_socket field 0x8608e34, close import and placement immediately before NET_ErrorString disambiguate it. It conditionally closes a nonzero descriptor and clears that field, without the NET_Config state guard. No positive original caller or function-pointer use was recovered to relate this returning helper to Mac's terminal quit operation. Both rows remain unchecked; no conclusion is drawn from missing callers.

This is a report-only provenance audit. No verdict or production source changes, compilation, tests, CI, replays or runtime exercise were performed. Private original body/address/caller packets are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-platform6.noindex/`. Prior verification records above describe earlier work, not this follow-up.

## Complete selected UDP shutdown operation (netshutdown14)

`NET_Shutdown` is now intentional under an independently reviewed complete consumed-operation mapping. Original 1.0 0x80d360e and 1.3 0x80d5c7e establish the helper: empty owned UDP state returns unchanged; otherwise close the current descriptor and then clear ownership regardless of the close result. Actual original acquisition writes the same UDP global. Primary Mac `NET_Config` contains the whole selected operation at 0x1e968b and 0x1e96f4–0x1e9706: 0/-1 skips both call and store; a live descriptor is passed to `closesocket` and then cleared to zero, joining before separate SOCKS processing. Exact Mac `closesocket` at 0x282b0 directly forwards to `close`. `NET_OpenIP` supplies the actual acquired descriptor or empty sentinel.

The selected native close phase preserves both paths, ordering and ignored error result, with the previously approved `-1` invalid sentinel and descriptor-zero support. This is why the verdict is intentional. Both actual close-phase entries were checked: enabled-to-disabled transition and modified enabled-state restart. The containing state machine, SOCKS handling, relay reset and restart are retained, not attributed to this helper. No successful OS release is promised after a close error.

This supersedes the remaining-identity rejection only for that complete consumed phase. `NET_Config(0)` is still not an unconditional shutdown API: an unchanged disabled state returns before the phase. No standalone helper export, literal original inline ancestry, arbitrary corrupted descriptor state, original Linux -1 behavior, or final errno after subsequent SOCKS/start work is claimed. Missing callers and process exit supply no evidence for this verdict. `NET_BaseAdrToString` remains unchecked with its separate output/ownership gap.

Report-only, with zero new defects or production changes. Exact original/Mac bodies and native source were reviewed independently. No compilation, tests, CI, replays or runtime exercise occurred; prior verification receipts are not new coverage. Private packet: `recon-provenance-netshutdown14.noindex`; independent review: `units/review-provenance-netshutdown14.md`.
