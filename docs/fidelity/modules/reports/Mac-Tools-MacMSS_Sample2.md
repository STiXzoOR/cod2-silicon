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
