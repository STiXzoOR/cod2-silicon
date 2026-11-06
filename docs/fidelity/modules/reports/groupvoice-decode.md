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

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
