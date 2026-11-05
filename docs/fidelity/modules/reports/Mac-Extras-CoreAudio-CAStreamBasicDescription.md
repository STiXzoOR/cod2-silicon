# Mac/Extras/CoreAudio/CAStreamBasicDescription reconciliation

Updated 2026-11-05.

**2/2 reconciled: 2 intentional.** The old helper is still compiled but has no selected native source or generated-data caller. Mac constructor0x113bb4 zeroes40bytes; SetCanonical0x113b64 fills lpcm float32 format, flags9/41,channels,one frame per packet and interleaved/noninterleaved byte strides. The surviving native declaration is an int placeholder and SetCanonical is empty; neither is approved as a usable SDK object.

The discarded MacMSS/recorder graph is replaced by platform/macos_audio.c, which includes actual SDK headers and builds typed AudioStreamBasicDescription values directly for AudioUnit output and file decoding. That specific replacement justifies retirement of these helpers. A no-call/string-difference triage result does not establish semantic equality or valid40-byte storage.
## Verification

Read every assigned exact-symbol Mac body, the legacy source, selected native voice/audio providers, actual build source lists and all active wrapper consumers. Native selected/generated sources have no recorder/class-format caller. Triage cannot compile the three excluded source files (retained StopIteration logs); the selected description helper has no call/string flags, which does not prove its missing body equal. Constant output was used only as a supplement.

Arm64 Release dedicated build passed with two background jobs. The unchanged existing voice initialization fixture passed under ASan/UBSan: unavailable capture with64 playback queues, client packet decoding into Render, mute/talk gates, two lifecycles, output-device failure and late-packet rejection. Its successful-capture control is synthetic and does not establish native recording. No production source changed; inactive-source comparison is therefore vacuous. No new tests, microphone/device access, physical audio, live network or original PCM parity claim.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-capture.noindex/`.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
