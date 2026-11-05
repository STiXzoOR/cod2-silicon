# Mac/Tools/MacGlobals reconciliation

Updated 2026-11-05.

**5/5 reconciled: 3 equal, 2 intentional.** The static lock helpers match Mac: increment at 0x9590, decrement only when positive at 0x959c and signed-positive predicate at 0x95b2. Selected SDL/terminal providers do not use that Carbon lock counter.

The two alert identities are physically in `MacTools.c`. Question at 0xa7f4 and message at 0xa95e check the main thread/windowed state, lock around Carbon standard alerts and unlock on both success and exception unwinding. Native wrappers are empty/false but have no external native source or compiled-native-data callers. Live autoconfigure/fatal diagnostics use `MacWin32.c:MessageBoxA` and terminal stderr, not these wrappers. They are recorded as a retired Carbon alert boundary, not as equality for missing alert UI.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
