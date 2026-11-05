# win32/win_voice module reconciliation

Updated 2026-11-05.

**7/7 reconciled: 1 equal, 6 intentional.** Every engine wrapper was compared by hand with exact-symbol Mac 1.3 disassembly. Bundled Speex and Miles implementations are outside this reconciliation; the engine callers are not exempted.

## Native provider boundary

The native client links `win_voice.c` and the `groupvoice/play.c` wrappers. `Sound_*` forwards to `src/platform/macos_voice.c`, which queues mono PCM in 65 native slots and sends it through the native AudioUnit mixer at 8192 Hz. Its `Record_Init` deliberately fails before entering the old fixed-address recorder layout. The reconstructed Mac recorder and `groupvoice/record.c` are excluded by `cmake/macos-arm64.cmake`. The dedicated target excludes win_voice and groupvoice entirely.

The existing WS110 playback adaptation is retained. Mac `Voice_Init` 0x1ea1b0 returns immediately when `Record_Init` fails at 0x1ea273. Native continues to output initialization, codecs and 64 playback samples if output succeeds. All four dvar defaults, ranges and 0x1001 flags, bandwidth setting 0, successful-capture statement order, ushort record level, talk-time clearing and historical zero return were compared. No new native capture implementation was added.

`Voice_GetLocalVoiceData` (0x1ea65c) and `Voice_GetVoiceLevel` (0x1ea34e) return zero when that recorder is unavailable. Their capture-success paths retain lazy recording, scaler clamp, frame polling and the six-entry level average in Mac operation order. `Voice_Playback` (0x1ea3cc) skips unavailable microphone controls but still services all playback samples. `Voice_Shutdown` (0x1ea5a8) skips recorder/mixer restoration when capture never initialized, while releasing playback and codecs in the original order.

`Voice_IncomingVoiceData` (0x1ea43c) retains the native initialization gate against late decode after output failure/shutdown and the real `g_frame_size` plus nonpositive-size guard. Its talker-byte timestamp, decode/queue loop and last-decoded-block save path were compared. `CL_VoicePacket` bounds the actual caller to talkers 0..63 and packet lengths 1..256; the native integer minimum equals Mac's float minimum for these values. The saved-file mode now spells `ab`, matching cstring 0x2e4b64. It was already equivalent to `a` for macOS streams.

`Voice_IsClientTalking` is equal to Mac 0x1ea584: 32-bit subtraction followed by a signed elapsed-time comparison through 299 ms. Its Release arm64 object was also inspected and uses a 32-bit subtraction followed by signed `< 300`.

## Verification

The arm64 Release dedicated target and the actual client compile commands for all three assigned source files succeeded at background priority, at most two jobs. No production source uses `__LINE__`. Guard checking found only native-on differences and no inactive differences; `git diff --check` passes. The existing `tests/voice/playback_init.py` fixture was inspected; no expectation changes were needed, and it was not run by this audit.

No tests were added, no full CI/replays or full client link were run, and these wrappers were not exercised in this audit. Prior WS110/WS112/WS119 runtime evidence is background context, not a new receipt. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-platform-media.noindex/`.
