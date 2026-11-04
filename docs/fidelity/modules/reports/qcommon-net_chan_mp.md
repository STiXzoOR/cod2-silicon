# qcommon/net_chan_mp module reconciliation

Updated 2026-11-04.

**30/30 entries checked: 13 equal, 13 fixed, 4 intentional.** Mac 1.3 is the authority. Every Mac body and the caller blocks corresponding to the Linux-only helpers were read. Exact-symbol disassembly confirms the function entries; the full-binary disassembly misdecodes only the first three prologue instructions of `NetProf_PrepProfiling`. Triage, string and constant checks supplemented the manual comparison.

The native corrections are:

- `NetProf_PrepProfiling` now reads the server-running boolean, tests the requested profile value when selecting the client, and prints the selected source name. Mac 0x157d89, 0x157dec and 0x157d9e establish these operations. Native previously tested the still-zero profiling state and passed `g_qport` to `%s`.
- `NET_SendPacket` prints OOB packets when the first word is -1, as at 0x158561. Loopback replies use the destination port as the client queue and store source port zero, as at 0x15866d; client sends retain their source index, as at 0x158676. The native queue-zero shortcut differed.
- `NET_OutOfBandVoiceData` uses a LargeLocal allocation of 128 KiB plus four header bytes and rejects larger payloads, matching 0x1586b7 and 0x1586d4. Its old 32 KiB stack buffer had no bound. The missing `NET_OutOfBandPbPacket` body was independently implemented from Mac 0x1587bc, including its separate diagnostic and both profiling branches.
- `NET_OutOfBandPrint` accepts the Mac limit of 128 KiB minus four header bytes, including the string terminator, at 0x158aba. Native used a 16 KiB limit.
- `NET_StringToAdr` traps only `localhost`, as at 0x15843f, and swaps the low 16 bits of the parsed port, as at 0x1584e3. Native also trapped `loopback` and let higher port bits contaminate the swap.
- `NET_CompareAdr` returns true after the bad-type diagnostic, matching 0x158bdf–0x158bf8. Both address comparison wrappers now retain the Mac signed-helper diagnostic names.
- `Netchan_Process` uses an unsigned accumulated-fragment bound, matching the `ja` at 0x1596be. `Netchan_Transmit` preserves the error string's 0x15 prefix at 0x159189. The existing long fragment offsets and 128 KiB transmit limit were checked and retained.

The retained intentional differences are unsigned port display in CoD2x, dedicated builds' unavailable client-profile diagnostics, and direct Mac diagnostic client-number literals in place of the Linux-only `Net_LocalClientNum` helper. The native platform send API remains void and reports dispatch success; its replacement does not establish socket-delivery equivalence. The extended native address layout is handled with typed fields and a bounded whole-object clear.

No shared headers were changed. `unifdef -UCOD2_X64` output is byte-identical to base e92ca5f02da88f521838bb2de89d1563524f5ba3. `cod2_macos_ded` compiled in the worker's arm64 Release build with `taskpolicy -b nice -n 19` and two jobs. The changes are compiled and reviewed, not exercised. No new tests, full CI or replays were run; the client-only profiling branches were not compiled by this dedicated target. Nothing in this module remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-net.noindex/`.
