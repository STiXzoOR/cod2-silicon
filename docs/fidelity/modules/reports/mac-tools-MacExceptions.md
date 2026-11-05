# Mac/Tools/MacExceptions attribution reconciliation

Updated 2026-11-05. **6/7 reconciled: 6 intentional; 1 unchecked.** This STABS attribution bucket has no C translation unit.

The descriptor operations at Mac 0x2848/0x2a06/0x2a5c reset/dispose descriptors, call the appropriate AppleEvent API, and throw a sign-extended short error in a 4-byte allocation. Their recognizable flow is retained in `MacAppleEvents.c`, but Carbon providers remain stubs and that allocation is not a verified LP64 long exception payload. No external source/generated-data caller reaches this graph. SDK launch URLs and queued shutdown replace it; no functioning Carbon exception ABI is claimed.

`CMacGameEngine::InstallEventHandlers` (0x26166) installs Carbon application/window/menu/AppleEvent callbacks and throws on failures. `OneShotTimer` (0x2641a) changes to the data directory, sets a 640x480 display, installs handlers, hides the cursor and invokes virtual startup. Native startup instead uses launcher data-path arguments, the native mac_main loop and SDL input/window providers. No native object, body or callback root retains those methods. Unused mac_splash constants install no handlers.

`CSoundEngine::GetPreferredChannelCount` (0x115fa0) queries AudioUnit device/surround channel layouts, counts labels/bitmap/tag channels and clamps/defaults them. The excluded MacMSS_Engine backend is replaced by the WS3 typed default-output AudioUnit and two-channel float mixer in `platform/macos_audio.c`; old device-dependent surround/Miles parity is not claimed.

## Unresolved retained mutex error contract

Mac `CMutex` construction at 0x1173fe checks attribute init/type and mutex init, throwing status at 0x117451. Native `macos_threads.c` sets the same recursive type on success but ignores all three failures. The mac-win32 owner independently confirmed this and found no live CMutex/CThread users after the native audio/voice replacement. This row stays unchecked for the retained error contract. No speculative C++ throw or shared-provider edit was made.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
