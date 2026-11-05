# unix/unix_main reconciliation

Updated 2026-11-05.

**40/52 reconciled: 6 equal, 4 fixed, 30 intentional; 12 unchecked.** Mac 1.3 i386 is the authority. Linux supplied names only. Native changes are guarded; inactive source comparisons against `cef44cbe` pass.

## Corrections

`Sys_Error` omitted the `com_errorEntered = 1` store at Mac 0xc384a. The GOT slot 0x1accf0d points to 0x10cceb4, that global. The omission allowed `Com_Quit_f` to take its normal shutdown path after a fatal error.

`Sys_GetEvent` pumped input before returning an existing event. Mac 0xc3c7a returns the queued event first and only pumps at 0xc3cda. Native now preserves that priority.

`Sys_StartProcess` was an unsupported-Linux stub. Mac 0xc36f8 checks the `cod2sp` substring before `cod2mp`, selects the exact stock sibling bundle name, opens it and queues `quit\n` only after success when requested. Native Apple now follows those decisions. It obtains the installed application's parent with CoreFoundation rather than the discarded Carbon process-folder implementation. Unknown names call `game_dprintf`; the original diagnostic at 0x3fb4 is itself a no-op.

Normal packaging runs `<installed app>.app/Contents/Helpers/CoD2 Game.app`, as fixed by `make_macos_app.py` and `launcher/Model.swift`. The corrected provider recognizes only that exact helper/Helpers/Contents sequence beneath an `.app` root and searches beside the outer installed app. Renaming that outer app is supported. Direct `--engine-only` apps and bundles outside that exact layout retain the immediate-parent rule. The fixed stock target names, LaunchServices status check and queued-quit timing are unchanged. Every copied URL/component/extension is released; no app was launched to verify this correction.

`Sys_OpenURL` selected `xdg-open` on Apple and ignored its second argument. Mac 0xc37a4 uses `CFURLCreateWithBytes` with encoding 0, LaunchServices and a balanced release, queuing quit only after successful open. The Apple branch now uses that API sequence. The argument is a quit request, despite its inherited `activate` name. The actual download caller also queues its own quit afterward, as before.

## Native boundaries

The native entry is `linux_common.c:main` -> `mac_main.c:WinMain`, not `CMacGameEngine::Run`. The bounded command-line join, typed data fixups, native crash handler, dedicated signal checks and CoD2x lifecycle replace that Carbon bootstrap. The original command strings and engine queue behavior still require individual comparisons.

The selected console is `mac_syscon.c`: a 512-byte `select`/`read` stdin reader and flushed stdout/stderr. It has no Carbon window, terminal editing or history. Mac's console producer at 0xc433e–0xc4340 appends newline plus NUL; `Sys_GetEvent` then drops the newline by passing `strlen` as copy capacity. Native input has already removed newline, so its `len+1` capacity preserves the same command. Native packet events retain the established 20-byte `netadr_t` adaptation. SDL quit queues an engine command instead of the Carbon pump returning a quit Boolean. Dedicated has no SDL input provider.

`Sys_Error` deliberately calls `Com_Quit_f` immediately after terminal diagnostics. Mac 0xc38d2 instead waits for the Carbon event pump or a cleared `gConsoleRunning` flag. The selected native `CMacGameEngine_ProcessAllEvents` returns false, while terminal `Sys_ShowConsole(1)` sets that flag and has no GUI close handler to clear it. Copying the Carbon wait would therefore hang the established native console replacement.

`Sys_LoadingKeepAlive` at 0xc3a66 called the obsolete `UpdateSystemActivity(0)`. Its native no-op is replaced by `IOPMAssertionDeclareUserActivity(kIOPMUserActiveLocal)`. A single static assertion ID is passed back on every call, following the SDK's update/replacement contract; the assertion expires according to the user's display timer. The CoD2x heartbeat remains. This is an intentional modern platform replacement.

`macos_launch.c` includes SDK declarations directly. Native CMake now compiles it in both targets and links CoreServices. Exactly eight local stub APIs are excluded on Apple with `COD2_X64`: `CFBundleGetMainBundle`, `CFBundleCopyBundleURL`, `FSCloseIterator`, `FSDeleteObject`, `FSGetCatalogInfoBulk`, `FSOpenIterator`, `FSPathMakeRef` and `FSSetCatalogInfo`. The client symbol table confirms SDK ownership, including the URL constructors, `CFRelease`, LaunchServices and IOKit activity API. Existing native exclusions already cover `CFStringCreateWithCString` and `CFRelease`.

Old `MacResources` functions still have incompatible integer CF declarations and remain a queued dependency. They have no native source caller or callback reference; the only external declaration is in excluded `MacDisplay.c`. None of the four compiled native generated data TUs references `MacResources` or `MacFolders`. Diagnostic `x64_gen/data.c` contains obsolete false relocations but is not compiled. This change does not approve those old resource APIs for live use.

Linux DLL lifecycle helpers are replaced specifically by directly linked game functions and `GetRefAPI`/renderer shutdown. Linux tty helpers are replaced specifically by the stdin/stdout provider. Other unlocated Linux helpers were not classified by absence alone.

`Sys_GetClipboardData` at 0xc38f8 returns an empty static literal in Mac and the original reconstruction. Independent lifecycle review traced all three callers: the field editor and two script-debugger paste handlers release nonnull results through `Z_FreeInternal`, which ultimately calls `free`. Native now allocates one byte through `Z_MallocInternal` and explicitly terminates it, preserving empty content and the debugger paste call while satisfying that real ownership contract. This defined-storage adaptation avoids the inherited invalid free; it does not provide OS clipboard contents. The paste paths were not exercised.

## Verification and gaps

Both arm64 Release client and dedicated targets compile and link with two low-priority jobs. Existing Debug `system_network`, `system_closed_stdin`, `system_no_home` and `threads_ring` fixtures pass. They do not exercise the fatal-error, client backlog, URL, application launch or activity-assertion corrections. No external app, URL or user-activity assertion was executed. No new tests, full CI, replay or push was run.

The 12 unchecked Linux-only rows are `Sys_AppActivate`, `Sys_BeginProfiling`, `Sys_CheckCD`, `Sys_Chmod`, `Sys_FileTime`, `Sys_FunctionCheckSum`, `Sys_FunctionCmp`, `Sys_LowPhysicalMemory`, `Sys_NullQuery1`, `Sys_NullQuery2`, `Sys_NullStub` and `Sys_Warn`. No exact executable Mac identity or selected native contract was established. Their absence is not equality.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-unix-runtime.noindex/`.

## Remaining-identity audit (batch 22)

All 12 remaining rows stay unchecked. Concrete native owners were inspected, but they do not establish the missing identities:

- `Sys_AppActivate`: SDL focus events in `linux_input.c` call `MacDisplay_FocusChanged`, clear held input and deactivate raw mouse. Window raising belongs to `macos_display.c`. No original call-site mapping establishes the parameterless Linux alias as either owner.
- `Sys_BeginProfiling`: optional `MacSystem_ObserveFrame`/`ObserveWait` callbacks are native instrumentation, not an identified replacement for this unnamed original operation. `Sys_CheckCD` has no established original media-query identity or selected owner.
- `Sys_Chmod`: native preference writers select mode 0600, which is not a generic existing-mode OR operation. `Sys_FileTime`: existence/listing operations do not return modification time with the missing-file sentinel.
- `Sys_LowPhysicalMemory`: Mac `Sys_SystemMemoryMB` at 0xc4876 and the selected native provider return a memory quantity; neither proves a Boolean predicate or threshold. `Sys_FunctionCmp` and `Sys_FunctionCheckSum` lack original function-pointer query identities; content/configuration checksums are different contracts.
- `Sys_Warn`: `Sys_Print`/`Conbuf_AppendText` own terminal output, but that does not identify the separate warning-prefix/tty-management wrapper.
- `Sys_NullStub`, `Sys_NullQuery1` and `Sys_NullQuery2`: the reconstruction explicitly assigns unknown original names to unreferenced stubs. Neither an original Linux executable address/caller nor a Mac identity was established. Matching an empty body or constant return cannot resolve their provenance.

Linux reconstruction names and candidate contracts guided the search only. No Linux implementation was copied or accepted as Mac authority.

This follow-up changes reports only and finds no proven source defect. The full 101-row unit ledger preserves all 78 previously resolved status/evidence pairs exactly and refines the reasons for the 23 unchecked rows. No new row is certified. The existing `macos_system_network_test` and `macos_threads_ring_test` targets compiled in a private Debug build with two low-priority jobs; they were not run and do not prove any missing identity. No production build, new test/case/assertion, full CI, replay, push or external side effect was performed. Source and inactive builds are unchanged. Private inspection material is under `~/Library/Application Support/CoD2-native-decomp/recon-unix-remaining.noindex/`.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
