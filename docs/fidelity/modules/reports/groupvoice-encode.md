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
