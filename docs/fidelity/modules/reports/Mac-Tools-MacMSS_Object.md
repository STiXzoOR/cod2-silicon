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
0x13f108 supplies data/length/rate/block fields; native 3D metadata setup validates
and decodes first, then replaces PCM while retaining the allocated spatial type.
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
ASan/UBSan offline output suite passes all four cases. The new allocation-type
transition is compiled/reviewed but is not directly exercised by those cases.
Six inactive guard configurations report zero mismatches; no `__LINE__` occurs
in the changed source. No new tests, full CI, replays, game or physical audio run.
Original acoustic, surround, elevation, Doppler and latency parity remains a
WS3 replacement limit. No assigned function identity remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-audio.noindex/`.
