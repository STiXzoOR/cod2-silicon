# gfx_d3d/r_texturemem module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.**

Mac 0x10a074 was compared with the complete native body: slot-4 available-memory query, bytes-to-MB shift, zero-video-memory diagnostic and unsigned video-memory cap minus sixteen all match.

The native Apple path intentionally uses typed DxGlobals.device and src/platform/macos_display.c's actual-hardware MacDisplay_GetVideoMemoryInfo provider instead of ILP32 dx+8 and the original display implementation. The existing unsigned subtraction behavior is preserved. Current GL is selected; future Metal integration does not replace this engine query.

The arm64 Release object compiles. The hardware memory query was not exercised. No code or tests changed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
