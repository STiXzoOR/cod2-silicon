# Mac/Tools/MacMSS_Sample2 module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.** Legacy source is unchanged. Its empty
reconstructed helper is not used by native audio.

## Spatial calculation boundary

`CSampleSound::Calculate3DAttributes` at Mac 0x1e9778 has a real body. It reads
the object's position, builds listener-basis vectors, computes azimuth/elevation
and scales/clamps distance for old mixer parameters. Its callers live in the
retired Mac sample backend. Recreating the empty source helper would restore an
obsolete second coordinate/mixer implementation.

The active driver supplies listener-space right/up/forward coordinates through
`SND_Set3DPosition` (documented in WS90). Native `MixLocked` computes relative
distance, uses X/distance for equal-power stereo pan and optionally applies its
simple min/max/factor/rolloff attenuation. Driver initialization disables
provider rolloff because engine curves supply game attenuation. Listener/position
storage and the AIL setter/query APIs were traced to this real consumer. Original
surround, elevation, Doppler and old mixer distance response are not reproduced.
The verdict records this specific native spatialization boundary rather than
claiming numerical equality or excusing an active engine caller.

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

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
