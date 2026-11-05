# gfx_d3d/r_texturemem module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.**

Mac 0x10a074 was compared with the complete native body: slot-4 available-memory query, bytes-to-MB shift, zero-video-memory diagnostic and unsigned video-memory cap minus sixteen all match.

The native Apple path intentionally uses typed DxGlobals.device and src/platform/macos_display.c's actual-hardware MacDisplay_GetVideoMemoryInfo provider instead of ILP32 dx+8 and the original display implementation. The existing unsigned subtraction behavior is preserved. Current GL is selected; future Metal integration does not replace this engine query.

The arm64 Release object compiles. The hardware memory query was not exercised. No code or tests changed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
