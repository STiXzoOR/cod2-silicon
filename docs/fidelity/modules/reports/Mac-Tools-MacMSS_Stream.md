# Mac/Tools/MacMSS_Stream module reconciliation

Updated 2026-11-05.

**14/16 reconciled: 14 intentional, 2 unchecked.** Mac symbols identify the complete
QuickTime-backed stream class and its static-vector lifecycle. Legacy source
stubs are unchanged because the selected provider owns stream playback.

## Replacement responsibilities

`open_stream` at 0x13dad8 resolves Mac folder references, opens/prerolls a Movie,
reads its sound track and inserts the stream into `sQTStreams`. Native
`ReadStreamFile` uses engine filesystem callbacks or stdio, closes that handle
and predecodes via SDK ExtAudioFile into bounded owned PCM. `AIL_open_stream`
installs it in a typed nonspatial pool slot; the driver supplies software stream
pan instead of the original .wav 3D sample/.mp3 QuickTime distinction.

`UpdateAllStreams` at 0x13d8ba calls `MoviesTask`, detects completion and restarts
loops. Native `MixLocked` performs cursor advancement, interpolation, looping
and completion on the AudioUnit callback. Pause/resume/end set typed state;
playback speed uses the typed rate. Track volume/balance becomes direct
per-channel mixer gain. Millisecond query/seek parity remains unresolved below.

`close_stream` at 0x13d98c stops/disposes Movie and erases its vector entry.
Native close frees PCM and clears the pool allocation flag. Constructors,
destructors and the three static initialization/destruction rows map to C static
pool initialization and explicit AIL shutdown; the generated old vector bytes
are not native playback state. No QuickTime class object or polling dependency
remains in selected callers.

## Unresolved media-time behavior

Mac .mp3 dispatch selects this class, and its millisecond get/set methods at
0x13d714/0x13d7a2 use immutable MovieTimeScale, independently of SetMovieRate.
The .wav stream route instead uses sample timing with current playback rate.
Native query/seek delegates both routes to mutable sample-rate conversion, so
pitched MP3 length, current time and seek positions differ in active driver
start/save paths. These two method rows remain unchecked, together with the two
public AIL query/seek entries. The Windows executable delegates those operations
to Miles without time-base compensation; its DLL contract was not established
within the excluded-library scope. WS3 docs/fixtures choose no explicit timing
adaptation. See [MacMSS](Mac-Tools-MacMSS.md) for exact caller addresses and the
outstanding evidence. The implementation is unchanged pending that contract.

## Selection and verification

The configured client selects `src/platform/macos_audio.c` and excludes all six
MacMSS sources and `audio_stubs.c`; driver/voice AIL consumers were traced into
its typed pool, mixer, decoder and filesystem callback storage. The dedicated
selection excludes client audio. Some unused generated old callback/vector data
remains; it is not the selected provider state.

The six-source triage/constants/view runs, native provider comparison, exact-symbol
entry checks and caller inspection are detailed in [MacMSS](Mac-Tools-MacMSS.md).
Retired backend algorithms are not claimed equal. Arm64 Release dedicated build
and actual provider/existing platform-fixture compilation pass. The unchanged
ASan/UBSan offline output suite passes all four cases. The allocation-type
transition and failure reset are compiled/reviewed; neither branch is directly
exercised by those successful-setup cases.
Six inactive guard configurations report zero mismatches; no `__LINE__` occurs
in the changed source. No new tests, full CI, replays, game or physical audio run.
Original acoustic, surround, elevation, Doppler and latency parity remains a
WS3 replacement limit. Four stream query/seek contracts remain unchecked in
MacMSS/MacMSS_Stream, as detailed in the linked report.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.
