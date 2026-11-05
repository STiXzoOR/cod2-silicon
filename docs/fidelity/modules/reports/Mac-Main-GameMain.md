# Mac/Main/GameMain reconciliation

Updated 2026-11-05.

**7/7 reconciled: 7 intentional.** The Mac bootstrap object is replaced by `linux_common.c:main` -> `mac_main.c:WinMain`, rather than being reconstructed as a Carbon object.

Read the singleton guard/construction/atexit registration at 0x111d2c, its cleanup at 0x111e48, the forwarding MacMain at 0x111e3e and the parent-application wait at 0x111da6. The derived destructors at 0x2f0b9e/0x2f0bb4 are in `__TEXT,__textcoal_nt`; their vptr restoration, base cleanup and deleting-destructor call were read there. Full-binary text output misaligns the singleton prologue, so exact-symbol disassembly was used.

Initialization at 0x111e66 resets the button mask, invokes the base initialization, stops the old launcher and presents/validates the serial-key dialog. The selected native launcher has its own `KeyStore` validation/preferences setup and starts the engine helper without exiting itself. Engine file initialization calls `Com_ReadCDKey`. Native startup, `Com_Init`, SDL input and `Sys_Quit` own those responsibilities. There is no corresponding native object construction, installed vtable, or selected generated-data reference. This is a specific bootstrap replacement, not equality for missing C++ methods.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
