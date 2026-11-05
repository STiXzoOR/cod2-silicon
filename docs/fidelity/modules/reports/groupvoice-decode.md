# groupvoice/decode module reconciliation

Updated 2026-11-05.

**3/3 reconciled: 2 equal, 1 intentional.** All three exact Mac 1.3 i386
symbols were read by hand. No source correction was needed.

`Decode_Init` at `0x1ea9e0` selects narrowband, wideband and ultra-wideband
modes for 0, 1 and 2; other values print and return zero. The Mac GOT targets
were resolved to the three named mode objects and `g_encoder_samplerate`.
The native typed imports preserve these identities, including the initial
8192 Hz rate. Enhancement request 0, sample-rate request 24, the original
encoder-control request 3 on the decoder object, state writes, bit initialization
and unconditional successful return are retained. `Decode_Shutdown` at
`0x1eaab8` destroys bits before the decoder, only when non-null, then clears it.

`Decode_Sample` at `0x1eaaec` reads the supplied packet into bits, decodes once,
and returns zero on any nonzero decoder result. Its successful Mac loop converts
`frame_size * 2` floats into shorts, then returns that same number as PCM bytes.
Native deliberately converts only `frame_size` floats. The bundled decoder
produces one frame of floats; the native caller allocates 4096 shorts and submits
only the returned byte count to playback. For the active 160-sample narrowband
frame, playback consumes the first 160 shorts, or 320 bytes. Converting the
following 160 unproduced floats would read indeterminate stack storage without
changing consumed PCM. This existing defined-storage adaptation is documented
in WS110 and is retained; it is not an equality claim for those unused writes.

The arm64 Release dedicated target built with two jobs at low priority.
The actual client translation unit also compiled with its generated production
flags. The existing `tests/voice/playback_init.py` fixture passed with ASan/UBSan,
including its 160-produced-sample sentinel and 320-byte return check, reception
through 64 native queues, two lifecycles, failed output startup and late packets.
Triage and constant checks were followed by exact-entry disassembly because
full-binary disassembly misaligns the `Decode_Init` prologue.

No tests or source were added. Bundled codec fidelity is outside this module;
the WS91 PCM differences remain. Controlled device rendering does not establish
physical audio, original PCM parity, arbitrary direct-call frame sizes or live
network behavior. Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-groupvoice.noindex/`.
