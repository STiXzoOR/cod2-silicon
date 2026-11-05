# Mac/Tools/MacDebug reconciliation

Updated 2026-11-05. **1/1 equal.** Exact-symbol Mac dprintf at 0x3fb4–0x3fbb has only frame setup and return; it does not format or print. The selected native game_dprintf takes the same format/variadic arguments and is also a no-op. Native Sys_StartProcess uses it for an unknown application name. The legacy reconstruction's return type remains unchanged. Parent read the full body; the dedicated target compiled and linked. No runtime output claim or new tests.
