# Mac/Tools/MacMSS_Object module reconciliation

Updated 2026-11-05.

**54/54 reconciled: 54 intentional.** All rows have Mac identities. Legacy
source and its stub-backed missing methods are unchanged.

## Replacement responsibilities

`Release` at 0x13de88 calls virtual stop/close, frees four conversion buffers
and marks a bus available. The native release path frees two owned PCM buffers
and clears the typed pool allocation flag under `audio_mutex`. Constructors,
three destructors, availability/type checks and virtual changed hooks therefore
have concrete native lifecycle/state replacements rather than fabricated
fixed-offset objects. Base idle/changed no-ops have no selected virtual caller.

The address/type/rate/block metadata setters now target typed fields consumed by
`SampleInfo`, `DecodeInfo`, `PrepareSample` and `MixLocked`. `set_sample_info` at
0x13f108 supplies data/length/rate/block fields. Its Mac AIL wrapper first resets
the sample at 0x110ef6. Native setup now resets the validated slot before decode,
converts outside the global mixer lock and installs only if its captured payload
generation is still current. Failure leaves that lifetime DONE/empty, retaining
spatial; superseded decoded PCM is freed. The driver ignores its return; the
earlier claim that it checks success was incorrect.
The native metadata struct has the same native-width field order as
`AILSOUNDINFO`/the initial `AILMIXINFO.Info` field.

`get_sample_length` at 0x13e478 returns source bytes, not milliseconds; the native
3D length/offset API keeps those units for saved fractions. Native millisecond
queries instead use decoded frames and playback rate. Four borrowed Mac source
slots become two owned decoded buffers; raw/voice callers submit provider-returned
indices and the voice producer's temporary bytes are copied before return.

Mac volume-level mono scaling and independent volume/pan fields are replaced by
direct channel gains and derived query values. Position setters/getters preserve
public XYZ while removing old internal negative-Z storage; the driver already
supplies listener coordinates. Room/effects fields feed native feedback reverb.
All these active AIL consumers were read; original DSP/gain equivalence is not
claimed. Four class-static filesystem callback wrappers become typed provider
callbacks invoked by `ReadStreamFile`; generated old callback storage is unused.

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
WS3 replacement limit. Four stream query/seek contracts remain unchecked in
MacMSS/MacMSS_Stream, as detailed in the linked report.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
