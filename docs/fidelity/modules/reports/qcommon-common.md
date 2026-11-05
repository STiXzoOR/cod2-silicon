# qcommon/common module reconciliation

Updated 2026-11-04.

**69/76 reconciled: 43 equal, 18 fixed, 8 intentional; 7 unchecked.** All 50 assigned Mac code entries were read by hand. The two event jump tables at 0x328600 and 0x328618 were decoded directly. Linux-only helper rows were compared in Mac callers where an identity could be established. Mac 1.3 is the authority, with the Windows checks below. Legacy preprocessor output is unchanged.

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

Seven Linux-only rows remain without an established complete Mac/native identity: `Com_CheckSyncFrame`, `Com_ErrorAbort`, `Com_ForceSafeMode`, `Com_ResetFrametime`, `Com_SetTimeScale`, `Com_SyncThreads` and `Field_CompleteCommand`. The safemode/timescale names exist only as zero-address EH metadata; named text references show reads and timescale initialization, without a corresponding arbitrary-value setter. The native debugger's `Com_CheckSyncFrame` exists in another source file, but its Mac counterpart was not identified. These rows are not called equal merely because a body is missing. The cross-module follow-ups below resolve the three animation wrappers and the startup-config replacement.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles and links with two low-priority jobs. Stock-client syntax checks pass for both changed translation units. Triage and constant checks ran before and after edits; they were used as hints, not semantic proofs. Both changed files produce byte-identical preprocessor output against the base with `COD2_X64` undefined. `git diff --check` passes.

No new tests, full CI or replays were run. All corrections are compiled and reviewed, not exercised. In particular, runtime coverage is still needed for autoconfiguration, localized-default selection, redirect boundaries, error recovery, event edge cases and the client intro sequence. The optimized crash store was inspected, not invoked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-common.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.

## Cross-module animation allocator consumers — 2026-11-05

Three previously unchecked common-helper operations are present in concrete client animation consumers. Exact Mac `CG_AllocAnimTree` at 0x163622–0x16363c forwards the caller size with allocator tag 5 and returns `MT_Alloc` unchanged. Native `cg_ents_mp.c` has that complete operation. This establishes the Linux name `MT_AllocAnimTree` as an allocator alias without inventing a disconnected helper.

Mac `CG_PreProcess_GetDObj` at 0x164016–0x164028 passes the original animation pointer and that callback to `XAnimCreateTree`, retaining the result unchanged. The native call has the same arguments and result. This establishes the complete `Com_XAnimCreateSmallTree` wrapper operation, including its allocation callback rather than merely a similar tree allocation.

The complete `Com_XAnimFreeSmallTree` operation occurs in `CG_SafeDObjFree` at 0x1e420b–0x1e4217 and the client/entity cleanup loops at 0x1e4273–0x1e4280 and 0x1e42f1–0x1e42fe. Original pointer-table entry 0x1acd9cd contains 0x44af2, the `MT_Free(void*, int)` function address. Native `cg_animtree_mp.c` forwards the same tree/callback pair. Null guards and later tree-pointer clearing remain consumer behavior; these closures do not re-certify the complete containing functions.

The original common-only search had missed these cross-module consumers. Parent source/binary review only; no production changes, new tests, builds or CI runs. Existing caller verdicts remain unchanged. Eight common rows remain unchecked.

## Startup-config replacement — 2026-11-05

The Linux dedicated `Com_StartupConfigs` wrapper selects the fixed `config_mp_server.cfg`. The Mac/native startup uses player profiles instead. This entry is now **intentional**, with a concrete replacement rather than an assumed missing helper.

Exact Mac initialization calls `Com_InitPlayerProfiles` at 0x30f52 before executing the command buffer at 0x30f63. The complete profile initializer at 0x111aa6–0x111c06 registers its dvars, reads `players/active.txt`, parses the selected name, frees the file buffer and checks that name against the player directories. A missing file, empty name or missing directory reaches `Com_ExecStartupConfigs(NULL)` at 0x111b4b–0x111b52. A valid selection sets the profile, builds its `config_mp.cfg` path in a 64-byte buffer and executes it at 0x111ba9–0x111bd8. The actual native initializer and its two inline helpers preserve this selection and both executor outcomes.

The exact executor at 0x2fd78–0x2fe05 was reread as well: selected default config, language config, optional profile config, command-buffer execution, autoexec flag on/execute/off, conditional safemode config and final execution. Original strings and the default-config import pointer were read directly. Native `common.c` already implements this sequence, including the previously repaired default-config selection. No new startup policy or source change was introduced. This closure does not claim equality to the Linux fixed-file policy or re-certify every startup path. Parent source/binary review only; no builds, tests or CI. Seven common entries remain unchecked.
