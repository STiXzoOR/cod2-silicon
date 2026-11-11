# Mac/Main/GameMain reconciliation

Updated 2026-11-05.

**7/7 reconciled: 7 intentional.** The Mac bootstrap object is replaced by `linux_common.c:main` -> `mac_main.c:WinMain`, rather than being reconstructed as a Carbon object.

Read the singleton guard/construction/atexit registration at 0x111d2c, its cleanup at 0x111e48, the forwarding MacMain at 0x111e3e and the parent-application wait at 0x111da6. The derived destructors at 0x2f0b9e/0x2f0bb4 are in `__TEXT,__textcoal_nt`; their vptr restoration, base cleanup and deleting-destructor call were read there. Full-binary text output misaligns the singleton prologue, so exact-symbol disassembly was used.

Initialization at 0x111e66 resets the button mask, invokes the base initialization, stops the old launcher and presents/validates the serial-key dialog. The selected native launcher has its own `KeyStore` validation/preferences setup and starts the engine helper without exiting itself. Engine file initialization calls `Com_ReadCDKey`. Native startup, `Com_Init`, SDL input and `Sys_Quit` own those responsibilities. There is no corresponding native object construction, installed vtable, or selected generated-data reference. This is a specific bootstrap replacement, not equality for missing C++ methods.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
