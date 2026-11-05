# Mac/Tools/MacMSS module reconciliation

Updated 2026-11-05.

**77/77 reconciled: 1 fixed, 76 intentional.** Mac 1.3 is the authority.
The legacy source is unchanged. The active native provider preserves the engine's
AIL entry points, with the documented WS3 implementation boundaries below.

## Correction

`AIL_allocate_sample_handle` at Mac 0x110438 calls `NewSampleSound` at 0x115964,
which supplies sound type zero to `GetAvailableSampleBus`. Both bus-reuse passes
call `InitSample` with that type, and 0x114b0c–0x114b0f writes it to the object.
Native `ResetSample` preserves `spatial` for in-place 3D initialization, and
release reused that helper. A released 3D slot could therefore be allocated as a
2D/raw/voice/stream handle while still marked spatial. The native allocator now
clears `spatial` before reset. The 3D allocator still explicitly sets it, and
in-place `AIL_init_sample`/`AIL_set_3D_sample_info` retain the 3D type.

## Active API contracts

All 77 AIL definitions have selected native counterparts. Driver initialization
checks startup and digital-driver failure, enumerates synthetic provider token 2,
requests its 256-sample capacity and allocates native handles. Token 1 identifies
the shared digital driver; the typed sample pool and `audio_mutex` replace raw
class-pointer lookup. Stop/resume/end and the mixer maintain stopped/playing/done
states 8/4/2; rate fields drive frame advancement and duration queries. The Mac
source-byte length API remains a byte count for 3D save fractions.

`AIL_open_stream` at 0x110916 originally selects QuickTime for .mp3 and a 3D sample
object for .wav. Native reads registered filesystem callbacks or stdio and
predecodes through ExtAudioFile with a 256 MiB bound. Streams are deliberately
nonspatial typed samples: the driver selects its software stream-pan branch.
`AIL_stream_info` uses int-sized outputs required by the selected driver, with
format in the second output. PCM buffer handoff uses two owned buffers; raw and
voice producers submit only the indices returned by the provider. Voice stack
PCM is decoded/copied before the load call returns.

`AIL_WAV_info` at 0x111254 has real Mac parsing despite the reconstructed zero
stub. Native parsing/metadata/conversion is active in `SND_LoadSoundFile` and uses
native-width fields matching `AILSOUNDINFO`; native hunk data begins at
`offsetof(MssSound,data)`. The native one-source size/conversion functions remain
implemented even though the corresponding Mac engine helper bodies return zero.

Gain staging is intentional: native stores direct per-channel gains, derives
query pan/volume from them, uses X/distance equal-power positional pan and simple
feedback reverb. Mac mono gain doubling, independent master/pan fields, AUGraph
spatial parameters and QuickTime track controls are not recreated. CPU reporting,
error strings, provider names, redistributable no-op and preference returns follow
the concrete selected provider and their actual callers, as detailed per row.

## Selected native boundary

WS3 explicitly replaces these reconstructed fixed-offset MacMSS objects with
`src/platform/macos_audio.c`: a typed software mixer and the SDK default-output
AudioUnit. `cmake/macos-arm64.cmake` excludes all six MacMSS units and
`src/stubs/audio_stubs.c`, and adds the native provider to the client. The actual
configured client commands select `snd.c`, `snd_driver.c`, `macos_audio.c` and
`macos_voice.c`. Dedicated excludes those audio consumers/provider. Some generated
legacy callback/vector data still exists; it is not the provider's live state.
The linked dedicated binary contains no AIL, MacAudio or MacMSS class symbols.

I read every assigned ledger row, the six reconstructed source files, the selected
provider and its driver/voice consumers. The retired methods were identified from
Mac symbols and call/storage responsibilities; their algorithms were not declared
equal to the native mixer. Every intentional row names the concrete replacement
or removed internal responsibility. Engine API consumers remain in scope. No
bundled Miles implementation was imported or reconstructed.

## Verification and limits

The prescribed triage and constant checkers were run for all six reconstructed
sources. Triage cannot preprocess those excluded sources because they have no
configured compile entry; the raw-source constant results include obsolete stubs
and are not equality evidence. Triage/constants were also run against the selected
provider and the Mac AIL wrappers. `fidelity_view.py` was run for every assigned
name; Mac class method naming can require direct symbol/range inspection.
Exact-symbol `otool` corrected misleading full-disassembly starts in startup,
the stream constructor and all three base-object destructors.

Configured arm64 Release and built `cod2_macos_ded` with background priority and
two jobs. The selected provider and existing `tests/platform/audio_test.c` also
compile with the actual client command. That fixture was not run because its
main opens physical audio. The existing offline `tests/audio/output.py --case all`
passes with ASan/UBSan: 18 listener directions, moving entity offset, rate/delay/
pause/completion timeline and 138 footstep event cases through MixLocked/Render.
The current driver and its existing timeline expectation were used unchanged;
the peer's reviewed driver timing fix must be combined and verified by the parent.

The allocation type-transition correction is compiled and reviewed; the existing
fixture does not exercise released-3D to fresh-2D slot reuse. No new tests, full CI,
replays, game or physical audio run. The six inactive guard configurations have
zero mismatches, and the changed provider contains no `__LINE__` diagnostics.
No original-backend PCM/acoustic, surround, elevation, Doppler, codec-quality or
latency equivalence is claimed. These remain documented replacement limits,
not unchecked function identities.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.
