# Mac/Tools/MacMSS_Sample module reconciliation

Updated 2026-11-05.

**24/24 reconciled: 24 intentional.** Mac symbols identify the retired
AudioConverter/AUGraph sample implementation. Legacy bodies are unchanged;
native output does not call the reconstructed rendering stubs.

## Replacement responsibilities

`InitSample` at 0x114ad0 resets converter/base state, writes availability and
requested sound type, then configures mixer channel/spatial properties. Native
fresh allocation/reset owns typed fields; 2D allocation now clears stale spatial
state, 3D allocation sets it and in-place metadata reset retains it. Constructor
and three destructor rows map to typed slot initialization/PCM release, with
one shared AudioUnit callback instead of per-sample converter/callback ownership.

`DoConvert`/`AudioConverterProc`/`UpdateStreamFormat` are replaced by
`DecodeInfo`, `DecodeFile` and frame interpolation. Source data becomes owned
float PCM; format setters invalidate preparation, rate changes directly control
frame advancement, and raw/voice buffer handoff consumes two decoded slots.
Sample-backed stream file resources are read/closed during predecode.

`DoRender` at 0x1146dc, resume and render notifications use old mixer-bus
callbacks, deferred property flags and callback detachment. Native `Render`
validates the actual SDK stereo output buffer, emits silence on mutex contention
and invokes `MixLocked`; typed status controls sample readiness/completion and
the callback remains registered until shutdown. Old pre/post-render helpers
have no selected bus-notification caller.

`ChangedVolume` converts gain to dB for the old mixer, while `Update3DValues`
writes azimuth/elevation/distance. Native direct channel gains, X/distance pan,
optional simple attenuation and feedback reverb are the documented WS3 audio
replacement. Engine listener transforms/falloff remain active in the driver;
old mixer algorithm/Doppler/acoustic parity is not asserted.

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
