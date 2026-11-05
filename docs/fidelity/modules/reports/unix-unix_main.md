# unix/unix_main reconciliation

Updated 2026-11-05.

**40/52 reconciled: 7 equal, 4 fixed, 29 intentional; 12 unchecked.** Mac 1.3 i386 is the authority. Linux supplied names only. Native changes are guarded; inactive source comparisons against `cef44cbe` pass.

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

## Verification and gaps

Both arm64 Release client and dedicated targets compile and link with two low-priority jobs. Existing Debug `system_network`, `system_closed_stdin`, `system_no_home` and `threads_ring` fixtures pass. They do not exercise the fatal-error, client backlog, URL, application launch or activity-assertion corrections. No external app, URL or user-activity assertion was executed. No new tests, full CI, replay or push was run.

The 12 unchecked Linux-only rows are `Sys_AppActivate`, `Sys_BeginProfiling`, `Sys_CheckCD`, `Sys_Chmod`, `Sys_FileTime`, `Sys_FunctionCheckSum`, `Sys_FunctionCmp`, `Sys_LowPhysicalMemory`, `Sys_NullQuery1`, `Sys_NullQuery2`, `Sys_NullStub` and `Sys_Warn`. No exact executable Mac identity or selected native contract was established. Their absence is not equality.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-unix-runtime.noindex/`.
