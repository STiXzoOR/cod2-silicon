# Mac/Main/mac_record_dsound reconciliation

Updated 2026-11-05.

**12/12 reconciled: 12 intentional.** Native recording is explicitly unavailable under the existing WS3/WS110 contract. `macos_voice.c:DSOUNDRecord_Init` clears capture flags, prints its one-time notice and returns0 before the old fixed-address recorder/pool path. Native Voice_Init still starts incoming playback; capture polling and mixer setup/update/restoration are gated by recording availability.

Read original construction/cleanup,36-byte sample-pool advance, Start/Stop flags and errors, callback sample metadata,16-bit level normalization, source label and inverse mute flag. The selected provider allocates no microphone object and installs no input callback. Start/Stop fail, NewSample returnsNULL, Frame is inert and mixer calls return unavailable/no-op values. Their old bodies are not claimed equal or ready for native reuse.

DestroySample additionally owns release of a native playback handle because Sound_DestroySample forwards to this historically shared API. Native DestroyVoice validates the handle, releases its AIL sample and zeros the slot. Capture shutdown only clears its flags; playback shutdown is separate. This is required native ownership, not the Mac initialized-predicate destructor.
## Verification

Read every assigned exact-symbol Mac body, the legacy source, selected native voice/audio providers, actual build source lists and all active wrapper consumers. Native selected/generated sources have no recorder/class-format caller. Triage cannot compile the three excluded source files (retained StopIteration logs); the selected description helper has no call/string flags, which does not prove its missing body equal. Constant output was used only as a supplement.

Arm64 Release dedicated build passed with two background jobs. The unchanged existing voice initialization fixture passed under ASan/UBSan: unavailable capture with64 playback queues, client packet decoding into Render, mute/talk gates, two lifecycles, output-device failure and late-packet rejection. Its successful-capture control is synthetic and does not establish native recording. No production source changed; inactive-source comparison is therefore vacuous. No new tests, microphone/device access, physical audio, live network or original PCM parity claim.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-capture.noindex/`.
