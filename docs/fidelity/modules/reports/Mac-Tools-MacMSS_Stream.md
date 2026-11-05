# Mac/Tools/MacMSS_Stream module reconciliation

Updated 2026-11-05.

**16/16 reconciled: 16 intentional, 0 unchecked.** Mac symbols identify the complete
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
per-channel mixer gain. Millisecond query/seek now use a fixed decoded media clock as detailed below.

`close_stream` at 0x13d98c stops/disposes Movie and erases its vector entry.
Native close frees PCM and clears the pool allocation flag. Constructors,
destructors and the three static initialization/destruction rows map to C static
pool initialization and explicit AIL shutdown; the generated old vector bytes
are not native playback state. No QuickTime class object or polling dependency
remains in selected callers.

## Stream media clock resolved

The typed native stream now retains the decoder's media rate separately from
mutable playback pitch. Query and seek therefore preserve Mac MP3's immutable
MovieTimeScale contract at 0x13d714/0x13d7a2. Streamed WAV intentionally follows
the Windows file-time contract instead of Mac sample-rate timing. Narrow
inspection of the supplied Miles6.5e DLL proves that its stream millisecond
APIs use stream metadata independently of the separately owned sample rate.
See [MacMSS](Mac-Tools-MacMSS.md#stream-media-clock-resolved) for vendor research,
exact original addresses, DLL hash, native lifecycle and verification limits.
These are intentional SDK replacement mappings, not codec-seek, padding or
acoustic equality claims. Existing audio/voice sanitizer fixtures pass; the
new pitched stream-time path is compiled and reviewed, not directly exercised.

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
transition, failure reset and generation-guarded unlocked installation are
compiled/reviewed. Failure/stale-generation branches and concurrent conversion
output are not directly exercised by those successful-setup cases.
Six inactive guard configurations report zero mismatches; no `__LINE__` occurs
in the changed source. No new tests, full CI, replays, game or physical audio run.
Original acoustic, surround, elevation, Doppler and latency parity remains a
WS3 replacement limit. The later stream-clock follow-up above resolves the four query/seek contracts;
this earlier validation paragraph describes the original platform batch.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
