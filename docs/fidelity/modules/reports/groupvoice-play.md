# groupvoice/play module reconciliation

Updated 2026-11-05.

**7/7 reconciled: 7 equal at the wrapper boundary.** Each exact Mac 1.3 i386
symbol was read by hand. No source correction was needed.

`Sound_UpdateSample` at `0x1eaf7c` forwards sample, data and byte length to
`DSound_UpdateSample` and returns its result. `Sound_NewSample` at `0x1eaf86`
returns `DSound_NewSample`. `Sound_DestroySample` at `0x1eaf90` really delegates
to `Record_DestroySample`; this unusual target is preserved. `Sound_Frame` at
`0x1eaf9a`, `Sound_SampleFrame` at `0x1eafa6` and `Sound_Shutdown` at `0x1eafcc`
forward to their matching DSound providers. `Sound_Init` at `0x1eafb0` passes
the constant one and caller's handle to `DSound_Init`, returning its result.

The provider boundary was inspected rather than exempting these wrappers.
`src/platform/macos_voice.c` supplies DSound for the native client, replacing
the excluded historical Mac/Miles implementation. It owns 65 persistent sample
slots, allocates a native mixer sample for each used slot, and copies received
PCM into a bounded 32768-byte per-sample queue. Sample service moves queued PCM
into mixer-owned buffers. Destruction reaches `DestroyVoice` through the native
`Record_DestroySample`, releases the mixer handle and clears the slot. Shutdown
releases all allocated slots. Those native allocator, queue and mixer semantics
are the documented replacement, not a claim of backend equality. In particular,
wrapper equality does not assert identical overflow returns or resampling.

The arm64 Release dedicated target built with two jobs at low priority. Since
it excludes groupvoice, the actual client translation unit was also compiled
with generated production flags. The existing playback initialization fixture
passed under ASan/UBSan through all 64 talker queues and two init/shutdown cycles,
with controlled output creation and the real native rendering callback. Triage,
constant checks and the exact forwarding targets were reviewed.

No tests or source were added. Physical audio, live networking, original mixer
behavior and arbitrary queue-overflow inputs were not exercised. Private
evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-groupvoice.noindex/`.

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
