# gfx_d3d/r_water module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 fixed.** Current GL remains selected; future Metal work does not replace this uploader.

Read the full exact Mac `RB_UploadWaterTexture` entry at 0xfa340. The frame cache precedes the enable check; animation uses scene float time, clears zero-frequency terms, rounds/masks a 1024-entry phase and uses phase+255 for the real component. Row FFTs precede column FFTs. Renderer value 2 selects normal generation with the original approximate square root, horizontal predecessor carry, wrapped vertical predecessor and 127.5 channel scaling. Other modes generate magnitude heights and successive L8 mips. Both paths bracket uploads with the same native `g_WarmOff` storage. These match the existing WS75 corrections.

Mac 0xfaa0c/0xfaa0f saturates unordered heights to 255. Native previously continued through a NaN-to-int conversion; the native condition now preserves the unordered branch. Mac `cvttss2si` at 0xfa478 returns INT_MIN on invalid phase input, whose mask is zero. ARM64 compilation used `fcvtms`, which can saturate positive overflow to INT_MAX and select phase 1023. A local native conversion helper now preserves the Mac invalid result for phases and rounded output bytes, including 0xfa8d9, 0xfa915 and 0xfa94a. This keeps conversion defined without changing finite arithmetic order.

The actual current image, mip and low-precision square-root providers were inspected, including pending GL provider cd687d6; its square-root body is unchanged. Texture provider eecc169 retains native eager upload ownership. No provider or shared header changed. This comparison establishes this function's calls and data generation, not the complete texture/device or frame lifecycle.

The arm64 Release dedicated target and actual client object compile at low priority, at most two jobs. Five OFF preprocessing configurations and actual SDK OFF preprocessing, retaining `__LINE__`, match baseline c1bb3b0. No new tests, full CI or replays ran. Corrections are compiled and reviewed, not exercised; prior WS75 fixture results are historical evidence only. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-assets.noindex/`.
