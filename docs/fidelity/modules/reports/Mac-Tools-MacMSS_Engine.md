# Mac/Tools/MacMSS_Engine module reconciliation

Updated 2026-11-05.

**37/37 reconciled: 37 intentional.** Mac 1.3 symbols identify all rows; none
is closed merely because its source filename is excluded. Legacy source is
unchanged, including missing old constructors and destructors.

## Replacement responsibilities

The Mac constructors at 0x116194/0x116a70 build an AUGraph mixer, allocate buses
and run a worker thread. Native `AIL_open_digital_driver` configures one real
SDK output unit, registers `Render`, starts it and reports failure to
`SND_InitDriver`. Static typed pool state, owned PCM and `audio_mutex` replace
the old engine object/vector/bus storage. `AIL_shutdown` stops the callback
before freeing PCM and disposing output, replacing all three destructor rows.

`GetAvailableSampleBus` at 0x115868 has two reuse passes, and both call
`InitSample` with the requested sound type. Native allocation now explicitly
clears stale spatial type for fresh 2D slots; 3D allocation then sets it. In-place
reset preserves type. This active correction is recorded against the AIL entry
in MacMSS; these internal allocation rows document the typed replacement.

`Execute` at 0x115980 iterates virtual idle hooks and sleeps; `UpdateAllSounds`
at 0x1157ac pumps QuickTime. Native `Render`/`MixLocked` own progress, loops,
buffer transitions and completion. CPU statistics never drive playback. Handle
lookup methods become `FindSample`, while available flags and object-vector
insert/erase become native pool allocation/release.

The field getters/setters have explicit consumers in the replacement: provider
capacity is 256, listener coordinates/distance/rolloff are typed mixer state,
room/dry/wet control feedback reverb, and the driver performs listener transforms
and game falloff. Old mixer default-distance/face/up/property queries have no
selected backend consumer. The zero-return digital-conversion helpers are
replaced by active bounded PCM sizing/conversion for `SND_LoadSoundFile`.

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
