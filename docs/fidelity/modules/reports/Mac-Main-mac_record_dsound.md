# Mac/Main/mac_record_dsound reconciliation

Updated 2026-11-05.

**12/12 reconciled: 12 intentional.** Native recording is explicitly unavailable under the existing WS3/WS110 contract. `macos_voice.c:DSOUNDRecord_Init` clears capture flags, prints its one-time notice and returns0 before the old fixed-address recorder/pool path. Native Voice_Init still starts incoming playback; capture polling and mixer setup/update/restoration are gated by recording availability.

Read original construction/cleanup,36-byte sample-pool advance, Start/Stop flags and errors, callback sample metadata,16-bit level normalization, source label and inverse mute flag. The selected provider allocates no microphone object and installs no input callback. Start/Stop fail, NewSample returnsNULL, Frame is inert and mixer calls return unavailable/no-op values. Their old bodies are not claimed equal or ready for native reuse.

DestroySample additionally owns release of a native playback handle because Sound_DestroySample forwards to this historically shared API. Native DestroyVoice validates the handle, releases its AIL sample and zeros the slot. Capture shutdown only clears its flags; playback shutdown is separate. This is required native ownership, not the Mac initialized-predicate destructor.
## Verification

Read every assigned exact-symbol Mac body, the legacy source, selected native voice/audio providers, actual build source lists and all active wrapper consumers. Native selected/generated sources have no recorder/class-format caller. Triage cannot compile the three excluded source files (retained StopIteration logs); the selected description helper has no call/string flags, which does not prove its missing body equal. Constant output was used only as a supplement.

Arm64 Release dedicated build passed with two background jobs. The unchanged existing voice initialization fixture passed under ASan/UBSan: unavailable capture with64 playback queues, client packet decoding into Render, mute/talk gates, two lifecycles, output-device failure and late-packet rejection. Its successful-capture control is synthetic and does not establish native recording. No production source changed; inactive-source comparison is therefore vacuous. No new tests, microphone/device access, physical audio, live network or original PCM parity claim.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-capture.noindex/`.

## Integrated batch 14 verification

Integrated source 8cf8aa8 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both 819d282 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
