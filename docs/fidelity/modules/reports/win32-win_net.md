# win32/win_net module reconciliation

Updated 2026-11-05.

**3/3 entries reconciled: 2 equal, 1 intentional.** Mac 1.3 is the authority. This ledger module owns `NET_GetDvars`, `NET_Config` and `NET_Restart`; most other functions in the same native source belong to unix/unix_net ledger rows and are covered in that report.

`NET_GetDvars` at 0x1e83de was compared through all seven existing-pointer modified checks and registrations. Names, order, defaults, flags 0x1021 and SOCKS port bounds 0–65535 match. `NET_Restart` at 0x1e9762 passes the current networking-enabled state directly to `NET_Config`.

`NET_Config` at 0x1e9640 retains the modified-state early return, combined noudp/noipx override, enable/disable transitions, close/restart order and conditional IP opening. The selected native provider retains -1 invalid descriptors and supports descriptor zero, closes valid IP/SOCKS descriptors, and clears the relay-use flag on stop/restart. Those explicit native descriptor and stale-relay adaptations remain intentional. There is no active IPX socket provider.

Both native build graphs select this source and the Unix POSIX Winsock shims. The final arm64 Release `cod2_macos_ded` build passed at low priority with at most two jobs. The existing three system/network fixture variants passed, covering initialization, IP socket operation and disabling networking. The existing ASan/UBSan transport fixture passed after its ABI-only successful-send stub update. Modified-dvar reopen and SOCKS-enabled restart were compiled and reviewed, not exercised. No new tests, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-platform-network.noindex/`.

## Integrated batch verification

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
