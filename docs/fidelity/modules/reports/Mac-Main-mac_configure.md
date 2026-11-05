# Mac/Main/mac_configure reconciliation

Updated 2026-11-05.

**4/4 reconciled: 2 equal, 1 fixed, 1 intentional.** All four functions remain live through `WinMain` system-info collection.

Mac 0xc4890 copies the actual display renderer. Native returned the hardcoded `Linux OpenGL` string. The native Apple branch now reads the SDK `IOAccelerator` model, releasing its IOKit service and copied CF property and using `Unknown GPU` on unavailable/unsupported data. `WinMain` gathers system info before creating a render context, so calling the current `MacDisplay_GetGLRenderer` would not supply a valid OpenGL string at that point. The registry's model was inspected on the host; the new function itself was not run.

`SysInfo.gpuDescription` is 512 bytes and feeds archived string dvars and `strcmp`, as well as the autoconfigure consumer. The native terminator and bounded local storage are preserved; Mac's potentially unterminated `strncpy` behavior is not introduced. Nonpositive native capacity returns without writing. The unknown fallback and this defined-string contract are intentional platform/storage adaptations within the repaired row.

The float-to-double CPU conversion at 0xc4858 and signed memory cap at 0xc4876 match. The cap is exactly `0x8000000`, not an inferred one-gigabyte limit. Native SIMD capability remains true instead of querying Mac's obsolete x86 Gestalt attribute 25 at 0xc48b4. The CPU/memory underlying providers remain `MacFeatures` -> `MacSystem`.

The SDK branch is isolated from the legacy type/import copies. The linked dedicated executable resolves all newly used CF functions from CoreFoundation and all IO functions from IOKit. The legacy branch is unchanged, and neither changed file contains `__LINE__` use.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
