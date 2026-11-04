# win32/win_net module reconciliation

Updated 2026-11-05.

**3/3 entries reconciled: 2 equal, 1 intentional.** Mac 1.3 is the authority. This ledger module owns `NET_GetDvars`, `NET_Config` and `NET_Restart`; most other functions in the same native source belong to unix/unix_net ledger rows and are covered in that report.

`NET_GetDvars` at 0x1e83de was compared through all seven existing-pointer modified checks and registrations. Names, order, defaults, flags 0x1021 and SOCKS port bounds 0–65535 match. `NET_Restart` at 0x1e9762 passes the current networking-enabled state directly to `NET_Config`.

`NET_Config` at 0x1e9640 retains the modified-state early return, combined noudp/noipx override, enable/disable transitions, close/restart order and conditional IP opening. The selected native provider retains -1 invalid descriptors and supports descriptor zero, closes valid IP/SOCKS descriptors, and clears the relay-use flag on stop/restart. Those explicit native descriptor and stale-relay adaptations remain intentional. There is no active IPX socket provider.

Both native build graphs select this source and the Unix POSIX Winsock shims. The final arm64 Release `cod2_macos_ded` build passed at low priority with at most two jobs. The existing three system/network fixture variants passed, covering initialization, IP socket operation and disabling networking. The existing ASan/UBSan transport fixture passed after its ABI-only successful-send stub update. Modified-dvar reopen and SOCKS-enabled restart were compiled and reviewed, not exercised. No new tests, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-platform-network.noindex/`.
