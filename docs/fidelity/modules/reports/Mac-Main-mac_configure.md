# Mac/Main/mac_configure reconciliation

Updated 2026-11-05.

**4/4 reconciled: 2 equal, 1 fixed, 1 intentional.** All four functions remain live through `WinMain` system-info collection.

Mac 0xc4890 copies the actual display renderer. Native returned the hardcoded `Linux OpenGL` string. The native Apple branch now reads the SDK `IOAccelerator` model, releasing its IOKit service and copied CF property and using `Unknown GPU` on unavailable/unsupported data. `WinMain` gathers system info before creating a render context, so calling the current `MacDisplay_GetGLRenderer` would not supply a valid OpenGL string at that point. The registry's model was inspected on the host; the new function itself was not run.

`SysInfo.gpuDescription` is 512 bytes and feeds archived string dvars and `strcmp`, as well as the autoconfigure consumer. The native terminator and bounded local storage are preserved; Mac's potentially unterminated `strncpy` behavior is not introduced. Nonpositive native capacity returns without writing. The unknown fallback and this defined-string contract are intentional platform/storage adaptations within the repaired row.

The float-to-double CPU conversion at 0xc4858 and signed memory cap at 0xc4876 match. The cap is exactly `0x8000000`, not an inferred one-gigabyte limit. Native SIMD capability remains true instead of querying Mac's obsolete x86 Gestalt attribute 25 at 0xc48b4. The CPU/memory underlying providers remain `MacFeatures` -> `MacSystem`.

The SDK branch is isolated from the legacy type/import copies. Its memory-provider declaration uses the project provider's unsigned-long return and its SSE wrapper uses unsigned char, matching the existing Bool caller rather than introducing cross-unit signature differences. The linked dedicated executable resolves all newly used CF functions from CoreFoundation and all IO functions from IOKit. The legacy branch is unchanged, and neither changed file contains `__LINE__` use.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
