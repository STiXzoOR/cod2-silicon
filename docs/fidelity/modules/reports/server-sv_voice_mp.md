# server/sv_voice_mp module reconciliation

Updated 2026-11-04.

**7/7 reconciled: 7 equal.** Mac 1.3 exact-symbol disassembly was compared with native source after triage and constant checks. No source change was needed.

`SV_SendClientVoiceData` at 0x1bb770 matches the active/nonempty gates, 0x20000 buffer, `v` command, per-packet writer, overflow return, OOB send and success-only queue reset. `SV_WriteClientVoiceData` is inlined at 0x1bb7e6 and 0x1bb882. The existing native `v` command correction was retained.

`SV_UserVoice` at 0x1bba24 and `SV_PreGameUserVoice` at 0x1bbada match their different byte/short packet lengths and unsigned 1..256 size checks. The pregame loop checks all 64 recipients, excluding self, inactive, muted and voice-disabled listeners, with the same 40-packet queue cap. The voice preference/mute accessors and `SV_QueueVoicePacket` copy/count updates also match.

An arm64 Release `cod2_macos_ded` build succeeded with two low-priority jobs. No new tests, full CI or replays were run, and voice was not exercised in a game. The legacy preprocessed source is byte-identical to the starting commit. No row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-snapshot.noindex/`.

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
