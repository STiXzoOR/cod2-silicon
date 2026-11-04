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
