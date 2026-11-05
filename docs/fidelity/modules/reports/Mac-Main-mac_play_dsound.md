# Mac/Main/mac_play_dsound reconciliation

Updated 2026-11-05.

**6/6 reconciled: 6 intentional.** CMake excludes the reconstructed playback unit and selects `platform/macos_voice.c`, retaining the public DSound calls used by `groupvoice/play.c` and `Voice_Init`. WS3 and WS110 establish independent playback when microphone initialization fails.

Mac NewSample at0x1f888e depends on the recorder pool and allocates a51200-byte ring. Native instead owns65 typed slots with32768-byte queues and separate AIL handles, configured as mono16-bit8192Hz. It validates handles/even PCM length, copies queued bytes into ready AIL buffers and returns actual accepted length. Mac UpdateSample returns requested length after its bounded ring write; its SampleFrame waits for DONE, borrows one contiguous segment and resumes it. Those queue/scheduling/return differences are explicit native adapter behavior. Frame now services allocated queues, and Shutdown releases their handles instead of merely clearing an initialized flag. No old ring object/borrowed pointer reaches native output.

The existing fixture exercises the actual playback adapter with both high-level initialization modes; the native output device itself is replaced by its offline test provider.
## Verification

Read every assigned exact-symbol Mac body, the legacy source, selected native voice/audio providers, actual build source lists and all active wrapper consumers. Native selected/generated sources have no recorder/class-format caller. Triage cannot compile the three excluded source files (retained StopIteration logs); the selected description helper has no call/string flags, which does not prove its missing body equal. Constant output was used only as a supplement.

Arm64 Release dedicated build passed with two background jobs. The unchanged existing voice initialization fixture passed under ASan/UBSan: unavailable capture with64 playback queues, client packet decoding into Render, mute/talk gates, two lifecycles, output-device failure and late-packet rejection. Its successful-capture control is synthetic and does not establish native recording. No production source changed; inactive-source comparison is therefore vacuous. No new tests, microphone/device access, physical audio, live network or original PCM parity claim.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-capture.noindex/`.
