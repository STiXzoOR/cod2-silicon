# Mac/Tools/CAudioRecorder reconciliation

Updated 2026-11-05.

**15/15 reconciled: 15 intentional.** This discarded capture object is excluded from the selected native sources. Its only legacy source consumers are in excluded mac_record_dsound; generated native data contains no method references. The compatibility destructor stub is not a live object or callback. WS3/WS110 deliberately fail native capture initialization, before any CAudioRecorder allocation.

Read the complete original constructor/destructors, HAL input initialization, converter/render callbacks, ring transfer/overflow recovery, device-volume baseline scaling, start/stop/mute and shutdown/unwind paths. The Mac object has a mutex at+0x20, ring at+0x60 and hardware callbacks; those byte offsets are not transplanted into native opaque storage. Existing legacy partial methods/stubs remain unusable for native capture.

The replacement here is an explicit unavailable capability, with incoming playback independently provided by macos_voice/macOS AudioUnit output. Implementing microphone permission, device lifecycle and capture remains future native platform work; no recording functionality is claimed.
## Verification

Read every assigned exact-symbol Mac body, the legacy source, selected native voice/audio providers, actual build source lists and all active wrapper consumers. Native selected/generated sources have no recorder/class-format caller. Triage cannot compile the three excluded source files (retained StopIteration logs); the selected description helper has no call/string flags, which does not prove its missing body equal. Constant output was used only as a supplement.

Arm64 Release dedicated build passed with two background jobs. The unchanged existing voice initialization fixture passed under ASan/UBSan: unavailable capture with64 playback queues, client packet decoding into Render, mute/talk gates, two lifecycles, output-device failure and late-packet rejection. Its successful-capture control is synthetic and does not establish native recording. No production source changed; inactive-source comparison is therefore vacuous. No new tests, microphone/device access, physical audio, live network or original PCM parity claim.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-capture.noindex/`.
