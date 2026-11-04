# server/sv_voice_mp module reconciliation

Updated 2026-11-04.

**7/7 reconciled: 7 equal.** Mac 1.3 exact-symbol disassembly was compared with native source after triage and constant checks. No source change was needed.

`SV_SendClientVoiceData` at 0x1bb770 matches the active/nonempty gates, 0x20000 buffer, `v` command, per-packet writer, overflow return, OOB send and success-only queue reset. `SV_WriteClientVoiceData` is inlined at 0x1bb7e6 and 0x1bb882. The existing native `v` command correction was retained.

`SV_UserVoice` at 0x1bba24 and `SV_PreGameUserVoice` at 0x1bbada match their different byte/short packet lengths and unsigned 1..256 size checks. The pregame loop checks all 64 recipients, excluding self, inactive, muted and voice-disabled listeners, with the same 40-packet queue cap. The voice preference/mute accessors and `SV_QueueVoicePacket` copy/count updates also match.

An arm64 Release `cod2_macos_ded` build succeeded with two low-priority jobs. No new tests, full CI or replays were run, and voice was not exercised in a game. The legacy preprocessed source is byte-identical to the starting commit. No row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-snapshot.noindex/`.
