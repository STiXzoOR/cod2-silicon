# Mac/Tools/MacMemory attribution reconciliation

Updated 2026-11-05. **5/5 reconciled: 5 intentional.** This STABS attribution bucket has no C translation unit; the actual owners were located and read.

`FindFirstFileA` (Mac 0xb408) implements HFS catalog batches and `FFItem` vector state. Its retained `MacWin32.c` POSIX implementation has no native engine caller or generated callback root. Live native enumeration is `Sys_ListFiles`/`Sys_ListFilteredFiles` in `mac_common.c`; only the Windows-only MSVC dirent shim calls the Win32 name. Old HFS fallback and metadata parity are not claimed.

`MacDisplay_SetGammaRamp` (0x47b2) changes fullscreen desktop transfer tables on Mac. Its live typed replacement in `platform/macos_display.c` keeps a 3x256 RGB16 presentation LUT with identity/revision tracking. It does not change desktop gamma.

`R_GetFrontBufferData` (0xe87f4) is live in `r_screenshot.c`. The documented F2 screenshot contract replaces packed BGRA readback with RGBA bytes, RGB JPEG input, TGA serialization conversion, engine-FBO routing and back/front failure fallback. Row reversal and ownership were read. Native card type is 0, retiring old vendor disable branches. This does not certify the entire renderer or a runtime readback.

`SearchForApplication` (0xaafe) and `StShowCursor` construction (0xae28) are dormant Carbon launch/alert helpers. The former's apparent generated callback is pointer-size-4 key-table fallback only. SDK URL launching is owned by unix-runtime; SDL input owns live visibility/capture. The retained empty constructor and differently signed search stub are not asserted equivalent. These external owner files were not edited.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
