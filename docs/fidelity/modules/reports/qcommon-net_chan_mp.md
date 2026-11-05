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

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.

## Platform-network provider follow-up, 2026-11-05

The platform reconciliation supersedes the earlier void/dispatch-success adaptation above. The selected native provider is `PC/win32/win_net.c`, backed by the Unix POSIX Winsock shims. Exact-symbol Mac `Sys_SendPacket` 0x1e8900 returns false on a hard send error at 0x1e89f0 and true on success or the explicitly ignored error paths; `NET_SendPacket` propagates its low-byte result at 0x1585c4–0x1585c9. Native now declares the provider as `Bool` and returns its result. All earlier OOB-printing and loopback corrections remain unchanged. The orchestrator explicitly authorized this prior fixed row's evidence update.

The existing `f1_transport` fixture's send stub now returns `Bool` true, preserving its successful-send model and all existing cases/assertions. It passed under ASan/UBSan, and the final low-priority arm64 Release dedicated build passed. The native system/network fixture's three variants also passed. Hard send failure propagation is compiled and reviewed, not exercised. No new tests, full CI or replays were run. COD2_X64=OFF production preprocessing remains identical; the fixture signature is an intentional ABI-only correction.

Independent review found that the initial unconditional `Bool` fixture stub conflicted with the old void declaration when `f1_transport.py --baseline cef44cbe5ab119e14176b8495c826bff3cc12de9` selected the previous source. The runner now detects the selected source's native declaration and defines `F1_TRANSPORT_SEND_RETURNS_BOOL` explicitly. The stub uses the matching signature and returns true for the current `Bool` API. Current and baseline fixtures both pass under ASan/UBSan; all existing cases and assertions are unchanged.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
