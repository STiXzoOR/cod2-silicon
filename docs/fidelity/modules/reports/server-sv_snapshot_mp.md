# server/sv_snapshot_mp module reconciliation

Updated 2026-11-04.

**21/21 reconciled: 11 equal, 10 fixed.** Mac 1.3 i386 is the authority. All assigned rows were read, including the Linux-named helpers inlined into `SV_SendClientSnapshot` at 0x195f84. Exact-symbol disassembly was used alongside the initial triage and constant checks. The legacy preprocessed source is byte-identical to the starting commit.

## Corrections

**The archive base search stopped at delta frames.** Mac 0x194e62–0x194e74 skips them and continues looking for a full base. Native stopped, unnecessarily writing a new full archive. Both full and delta archive paths also admitted nonbroadcast entities with `svFlags & 1`; Mac 0x195209 and 0x195655 exclude them. The frame-slot remainder now uses signed `% 1200`, matching 0x1950d4–0x1950f4 rather than a mistranslated negative correction.

**Archived player state shifted two durations.** Mac 0x195c2a–0x195cac rebases six timestamps, leaving `legsTimer` and `adsDelayTime` unchanged. Native shifted those two as well. HUD rebasing, fade clamping and `deltaTime` accumulation match Mac.

**Current visibility diverged on broadcasts and cluster overflow.** Mac 0x19706f excludes the enumerated client entity, rather than comparing `state.number`. Expired positive `broadcastTime` is cleared at 0x19736d before the flag/PVS path; native always included that entity. The overflow scan at 0x197251 starts from the final explicitly tested cluster, rather than the first cluster plus one. The native 16-cluster cap was removed; the Mac endpoint rejection, including its odd all-invisible fallthrough, is preserved.

**Snapshot setup changed values before its early return.** Mac 0x196533–0x19656a leaves the first entity/client indices untouched for an absent game entity or a zombie client. The native path now does the same, and the client-number bound at 0x1966b6 uses the same unsigned comparison. The inline archive lookup at 0x196584–0x196656 now preserves disabled/nonpositive archive requests and clears failed lookups as Mac does.

**Several constants and diagnostics differed.** The client merge sentinel is 9999 at 0x1963f1/0x19643e. Overflow command diagnostics run through `reliableSequence` and print the ten-dash separator at 0x19699d–0x1969bc. Cached-snapshot errors now use Mac's end-of-message and wrapped-counter strings. The BPS diagnostic at 0x197665 again prints the calculated average compression percentage and newline.

## Verification and limits

The complete archive reader/writer, current/cached visibility, packet entity/client merges, snapshot writer, overflow recovery, reliable commands, rate arithmetic, archived client restoration, position interpolation and send loop were compared by hand with Mac. Existing native fixes for `sv_fps`, archive-buffer advancement, positive `deltaMessage`, `rateDelayed` and download placement were retained.

An arm64 Release `cod2_macos_ded` build succeeded with two low-priority jobs. The build retains six existing incompatible-pointer warnings in the archive decoder and linker warnings; no new tests, full CI or replays were run. These corrections are compiled and reviewed, not exercised in a game. Parent integration owns runtime validation. No row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-snapshot.noindex/`.

## Integrated batch verification

Integrated engine source through `868a08f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `b1f110d` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `b275adb` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
