# Mac/Tools/MacMemory attribution reconciliation

Updated 2026-11-05. **5/5 reconciled: 5 intentional.** This STABS attribution bucket has no C translation unit; the actual owners were located and read.

`FindFirstFileA` (Mac 0xb408) implements HFS catalog batches and `FFItem` vector state. Its retained `MacWin32.c` POSIX implementation has no native engine caller or generated callback root. Live native enumeration is `Sys_ListFiles`/`Sys_ListFilteredFiles` in `mac_common.c`; only the Windows-only MSVC dirent shim calls the Win32 name. Old HFS fallback and metadata parity are not claimed.

`MacDisplay_SetGammaRamp` (0x47b2) changes fullscreen desktop transfer tables on Mac. Its live typed replacement in `platform/macos_display.c` keeps a 3x256 RGB16 presentation LUT with identity/revision tracking. It does not change desktop gamma.

`R_GetFrontBufferData` (0xe87f4) is live in `r_screenshot.c`. The documented F2 screenshot contract replaces packed BGRA readback with RGBA bytes, RGB JPEG input, TGA serialization conversion, engine-FBO routing and back/front failure fallback. Row reversal and ownership were read. Native card type is 0, retiring old vendor disable branches. This does not certify the entire renderer or a runtime readback.

`SearchForApplication` (0xaafe) and `StShowCursor` construction (0xae28) are dormant Carbon launch/alert helpers. The former's apparent generated callback is pointer-size-4 key-table fallback only. SDK URL launching is owned by unix-runtime; SDL input owns live visibility/capture. The retained empty constructor and differently signed search stub are not asserted equivalent. These external owner files were not edited.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
