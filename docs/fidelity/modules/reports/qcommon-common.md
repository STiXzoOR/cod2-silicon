# qcommon/common module reconciliation

Updated 2026-11-04.

**65/76 reconciled: 40 equal, 18 fixed, 7 intentional; 11 unchecked.** All 50 assigned Mac code entries were read by hand. The two event jump tables at 0x328600 and 0x328618 were decoded directly. Linux-only helper rows were compared in Mac callers where an identity could be established. Mac 1.3 is the authority, with the Windows checks below. Legacy preprocessor output is unchanged.

## Corrections

**Autoconfiguration used the wrong checksum.** Mac 0x2f74f-0x2f760 and 0x2f443-0x2f454 multiply the previous checksum by 31337 through shifts and LEAs, then add a signed byte. Native used 1000000007. Both paths now preserve the Mac wrapped 32-bit result, mask and checksum-plus-one convention.

**Autoconfiguration could apply empty CPU settings and fail GPU matching.** Mac 0x2f3fa adds an 8 MB tolerance above the 128 MB minimum. Mac 0x2f2cd fails when no CPU row qualifies; native silently continued with an empty configuration. Mac 0x2f5d8 folds whitespace in GPU templates; native replaced uppercase letters with spaces. The native path now follows the Mac section traversal, token pushback, best CPU-row selection, first GPU match, validation of skipped rows and localized errors. The value parser at 0x2ea88 also had incomplete error strings and missing localization bytes.

**Redirected output could truncate instead of flushing.** Mac 0x2dfcf compares the sum of both string lengths with buffer-size-minus-one. Native subtracted another byte. The console-null gate at 0x2e024, logfile name and localtime/open order now match too. Windows 0x431e57 confirms `console_mp.log`.

**Error handling changed state and sent the wrong shutdown message.** Mac 0x2e387 preserves fixed console position for ERR_DROP; native cleared it. Mac 0x2fc96 sends `EXE_DISCONNECTEDFROMOWNLISTENSERVER`; native sent an unrelated server-crash format string. Developer test errors at 0x2e81a/0x2e830 and both event-loop bad-type errors needed localization byte 0x15. The hunk initialization failure at 0x306b6 now has the exact reference text.

**Event dispatch had extra and missing branches.** Mac 0x2ff35 discards server-loopback packets when no server runs. Native delivered them to the client. Mac uses unsigned invalid-type and packet-size comparisons, and forwards key events directly; these behaviors are restored. Native packet-prefix size still follows its native network-address representation.

**Localized output omitted language 14.** Mac 0x2f073 and 0x2f908 include it among comma locales. Windows 0x435636 and 0x435689 confirm both paths.

**Other small differences were observable.** String quoting at 0x2f026/0x2f043 uses signed byte comparisons. Freeze at 0x30bb1 waits through exact equality and has a shorter usage string. The initialization sequence at 0x30f8e-0x30f9f does not initialize language a second time. The legal cinematic and complete intro chain at 0x31484/0x31550 run with the Mac conditions; Windows 0x433fb0 agrees. Finally, optimizing Clang deleted the native crash command's nonvolatile null store. A volatile store preserves the faulting write at Mac 0x2e849.

## Cross-module dependency and intentional behavior

`Com_ExecStartupConfigs` at 0x2fd82 reads the filesystem's selected default configuration. Mac filesystem code 0x37346 selects `default_localize_mp.cfg`, then 0x373aa falls back to `default_mp.cfg`. Windows 0x425868 and 0x4258b5 agree. Native checked and executed only the fallback. A guarded `fs_defaultConfigName` in `universal/com_files.c` now carries that choice into startup execution and restart validation. The rest of the filesystem module has not been audited; its ledger remains unchanged.

Mac 0x2ec86 forces autoconfigured `r_aaSamples` to 1. Windows 0x433870-0x4338fc applies the CSV value and archives the dvar. Native follows Windows for that visible graphics setting. This entry is intentional.

The existing native timer pacing, tracing/state-hash hooks, dedicated client exclusions and optional COD2x FPS/config-write/quit behavior remain deliberate differences. PunkBuster console capture remains absent at the documented null-backend boundary (`punkbuster/pb_null_adapter.c` and the WS44 fidelity report); it is outside this reconciliation's scope.

## Unchecked entries

Eleven Linux-only rows have no established Mac callable or inline identity: `Com_CheckSyncFrame`, `Com_ErrorAbort`, `Com_ForceSafeMode`, `Com_ResetFrametime`, `Com_SetTimeScale`, `Com_StartupConfigs`, `Com_SyncThreads`, `Com_XAnimCreateSmallTree`, `Com_XAnimFreeSmallTree`, `Field_CompleteCommand` and `MT_AllocAnimTree`. The safemode/timescale names exist only as zero-address EH metadata; named text references show reads and the timescale initialization, without a corresponding arbitrary-value setter. The native debugger's `Com_CheckSyncFrame` exists in another source file, but its Mac counterpart was not identified. These rows are not called equal merely because a body is missing.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles and links with two low-priority jobs. Stock-client syntax checks pass for both changed translation units. Triage and constant checks ran before and after edits; they were used as hints, not semantic proofs. Both changed files produce byte-identical preprocessor output against the base with `COD2_X64` undefined. `git diff --check` passes.

No new tests, full CI or replays were run. All corrections are compiled and reviewed, not exercised. In particular, runtime coverage is still needed for autoconfiguration, localized-default selection, redirect boundaries, error recovery, event edge cases and the client intro sequence. The optimized crash store was inspected, not invoked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-common.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
