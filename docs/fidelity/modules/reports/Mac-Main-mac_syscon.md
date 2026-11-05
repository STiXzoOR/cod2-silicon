# Mac/Main/mac_syscon reconciliation

Updated 2026-11-05.

**8/8 reconciled: 8 intentional.** These are precise Carbon-console replacement boundaries. The selected provider is this file's bounded stdin line reader and flushed stdout/stderr; live `WinMain`, `Sys_Print`, `Sys_GetEvent`, `Sys_Error` and `Sys_Quit` callers were traced.

Mac creates a pre-window Handle at 0xc4546 and builds a Carbon/TXN console window at 0xc439c, configuring editor/output controls and a 30-second activity timer. Destruction at 0xc4588 releases its window/Handle/timer. Native allocates none of those objects; it resets the two 512-byte command buffers and uses terminal streams. Append at 0xc471e buffers/appends and scrolls a TXN selection on Mac; native immediately calls null-safe `fputs` and `fflush`. Error text at 0xc47f4 hides the editor and calls a message box on Mac; native prints the error to stderr.

The CR/LF edit hook at 0xc42bc retrieves and echoes a nonempty command, appends newline/NUL and clears its control. Native `select`/`read` supplies a bounded completed line directly to the engine queue. The `clos` builder callback at 0xc4368 is never installed; native quit comes through the SDL engine command or dedicated signal path. The timer callback at 0xc4388 updates old system activity, but the native console installs no timer. The separately reviewed pending Unix-runtime commit `b8facc48110ee340ba1d1311818604de7934fd55` provides the modern loading-activity assertion and documents the direct terminal fatal-error path. It was read, not cherry-picked or modified here.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
