# groupvoice/encode module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 4 equal, 1 intentional.** All five exact Mac 1.3 i386
symbols were read by hand. No source correction was needed.

`Encode_Init` at `0x1eac1c` retains the 0/1/2 mode dispatch, invalid-mode print,
encoder creation, bit initialization, initialized flag, option setup, repeated
frame-size query, bandwidth store and pointer-based result. Mac GOT targets
were resolved to the named narrowband/wideband/ultra-wideband objects. Native
typed data retains `g_encoder_samplerate = 8192` and `g_encoder_quality = 1`.
`Encode_Shutdown` at `0x1eacdc` preserves conditional encoder destruction,
pointer clearing, independent bit destruction/flag clearing and return one.
`Encode_GetFrameSize` at `0x1eadaa` returns the same global.

`Encode_Sample` at `0x1ead26` loads the `sv_voiceQuality` dvar, conditionally
updates quality through control request 4, resets bits, calls integer encoding,
then returns the bounded bit write. The Mac quality GOT slot was resolved to
`sv_voiceQuality`; the native typed dvar access preserves that object and its
integer value. Encode status is intentionally ignored in both wrappers.

`Encode_SetOptions` at `0x1eab70` retains request order 24, 4, 3, 30 and 34,
then stores quality and frequency. Native uses a four-byte integer one for the
last two requests, while Mac initializes only a one-byte local and passes its
address. The actual bundled narrowband provider reads both controls as integers
for VAD and DTX. Native therefore preserves the existing WS110 defined-storage
adaptation instead of reproducing an overread of the Mac local's neighboring
stack bytes. No claim is made that those original bytes were all zero.

The arm64 Release dedicated target built with two jobs at low priority, and
this actual client translation unit compiled with generated production flags.
The existing playback initialization fixture passed under ASan/UBSan, exercising
encoder initialization/options, frame-size access and shutdown as part of two
voice lifecycles. The dormant recording queue fixture models encoding; it does
not exercise this wrapper's `Encode_Sample`. Quality changes and actual outgoing
encoding remain compiled and reviewed, not exercised by this unit's fixtures.
Triage and constant checks were completed, followed by exact-entry hand review.

No tests or source were added. Bundled Speex implementation and its remaining
PCM differences are outside this module. The controlled fixture does not prove
microphone capture, live packets or bit-for-bit original encoding. Private
evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-groupvoice.noindex/`.

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
