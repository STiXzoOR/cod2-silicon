# Mac/Tools/MacMSS module reconciliation

Updated 2026-11-05.

**77/77 reconciled: 2 fixed, 75 intentional, 0 unchecked.** Mac 1.3 is the authority.
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

`AIL_set_3D_sample_info` at 0x110ece calls `init_sample` at 0x110ef6 before
installing replacement metadata. The latter sets DONE and clears the old source,
queued buffers, cursor and length through 0x13f007. Native decoded first and
returned early on failure, retaining the old playable PCM. The actual
`SND_StartAlias3DSample` ignores that return, resets offset and resumes; my earlier
report/ledger claim that the caller checks success was incorrect. Native now
validates and clears the slot under `audio_mutex`, captures its payload generation,
then decodes outside the mutex. Failure leaves that reset lifetime DONE/empty,
while preserving its allocated spatial type.

Decoded PCM remains local until a short installation lock revalidates the handle,
allocation flag and captured generation. Reset/init/release/reallocation and
source-address or successful buffer replacement invalidate the generation. The
mutex-protected counter survives shutdown, so pool clearing and reuse cannot
revive an old decode. If the lifetime changed, temporary PCM is freed without
writing the newer slot. Encoded input remains caller-owned sound-hunk data valid
for the synchronous call; unlocked decode does not access mutable sample fields.

`DecodeInfo`/SDK conversion now runs without the global mixer lock. The previous
correction held that lock for conversion, causing `Render`'s trylock to emit whole-
output silence and mute unrelated audio during each 3D replacement. That new
stall is removed. Trylock/silence behavior remains for the short reset/install
phases and other existing lock owners; no concurrent timing measurement is
claimed. SDK decoding uses only memory `FileRead`/`FileSize` callbacks. Existing
locked preparation/buffer-decode paths were not broadened or used to justify a
new long lock in this path.

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

## Stream media clock resolved

The user requested RAD Game Tools research and delegated this decision on
2026-11-05. The [Miles product page](https://www.radgametools.com/miles.htm)
and [vendor history](https://www.radgametools.com/msshist.htm) describe the
library but do not specify the legacy millisecond clock. Bink's video codec
is a separate API and does not determine the AIL stream contract. No SDK was
acquired or middleware reconstructed.

A narrow read of the supplied Windows `mss32_original.dll` establishes the
missing boundary. Its version string is Miles 6.5e; SHA-256 is
`7855b8fbae917cb8449f2d4361ab61b5ecec4df0a11130d797cb0aa99b4260ea`.
The CoD2x shim loads that DLL and forwards these named exports. Query export
0x2111c310 reaches 0x21111850; seek export 0x2111c380 reaches 0x21111960.
Both use stream file metadata at +0x84/+0x88 (with format/block fields for
PCM/ADPCM and the encoded byte-rate route for ASI), not the playback sample
rate. The data constants at 0x2114be00/0x2114be48 are 1000.0/0.001.
Rate export 0x2111b000 changes only the sample owned at stream+0x0c;
0x21114f30 writes the sample's +0x60 rate for a positive argument. It never
changes the stream metadata used by query/seek. The Windows engine changes
pitch at 0x4b723e before querying at 0x4b7325 and seeking at 0x4b73ae.
This is static original-binary evidence, not a Windows runtime measurement.

Native `AIL_open_stream` now retains the decoder's media rate separately
from mutable playback rate. Stream query/seek use that fixed rate; mixer pitch
still uses the mutable rate. Ordinary sample query/seek remain unchanged.
The decoded-PCM pool, cursor, mutex and reset ownership remain the existing
SDK replacement: reset clears the retained rate and successful stream open
installs it. A non-stream sample passed to a stream API retains the prior
sample-rate fallback. No new allocation or decode occurs in the new setters.

Mac MP3 methods 0x13d714/0x13d7a2 use immutable MovieTimeScale, so the clock
separation also corrects their proven pitch-dependent mismatch. Mac WAV uses
mutable sample timing; native streamed WAV deliberately follows the Windows
file-time contract instead. All four rows are intentional native API mappings,
including this correction; they do not assert identical QuickTime/Miles codec
seek granularity, variable-bitrate duration estimates, encoder padding, invalid
handle behavior or acoustic output. The existing decoded-frame/truncation and
nonnegative-seek choices remain explicit native boundaries.

The actual SDK arm64 Release provider compiles. The unchanged audio and voice
ASan/UBSan fixtures pass; these exercise ordinary decode/mix/ownership, not
pitched stream millisecond calls. The changed timing path is compiled and
reviewed, not runtime verified. Six inactive guard configurations are identical;
no changed source uses `__LINE__`. No tests, cases or assertions were added.

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

The allocation type-transition and failed-replacement reset corrections are
compiled and reviewed. The existing fixture exercises successful in-place 3D
setup but not released-3D to fresh-2D slot reuse, failed metadata setup, stale-
generation rejection or concurrent output during conversion. No new tests, full
CI, replays, game or physical audio run. The six inactive guard configurations have
zero mismatches, and the changed provider contains no `__LINE__` diagnostics.
No original-backend PCM/acoustic, surround, elevation, Doppler, codec-quality or
latency equivalence is claimed. Those quality limits are distinct from the four
unresolved stream-time contracts described above.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
