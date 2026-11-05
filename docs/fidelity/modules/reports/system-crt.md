# system/crt.c attribution reconciliation

Updated 2026-11-05.

2 intentional (2 entries).

Both bodies are Apple Csu startup, not engine code. They initialize dyld/keymgr/runtime state, call main and exit. Current SDK/libSystem/libc++ startup owns that work for native arm64; src/unix/linux_common.c supplies the engine entry and WinMain handoff.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
