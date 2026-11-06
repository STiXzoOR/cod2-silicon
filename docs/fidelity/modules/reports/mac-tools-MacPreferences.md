# Mac/Tools/MacPreferences reconciliation

Updated 2026-11-05. **9/9 reconciled: 6 equal, 1 fixed, 2 intentional.**

Mac 0x966c calls `CFStringGetCString` with the caller's capacity; failure releases the preference and follows the default/empty path, returning false. Native file preferences previously truncated oversized stored values and returned true. Native Apple reads now close the file and take that same fallback when the value and terminator do not fit. Live `Com_ReadCDKey` uses a 21-byte buffer, so this is a reachable contract. The correction is compiled and reviewed, not exercised.

The six numeric/rectangle wrappers match Mac's ordered suffixes, decimal formatting, byte/default behavior, empty-value branches and `atoi`/`atof` conversions. Project `CGRect` remains four floats; no SDK double rectangle is passed here. Mac rectangle wrappers are void; the extra native success byte has no caller.

`PutString` and `Synchronize` intentionally retain the WS21 shared file store, `~/.cod2/preferences`, used by the launcher and engine CD-key functions. Writes close the mode-0600 temporary file before rename; synchronization has no CF store to flush. This is not a claim of fsync crash durability. The CFPreferences backend is intentionally replaced.

Only Apple `COD2_X64` code changed. The six inactive preprocessor configurations compare identically with base 3d9df23; neither changed file contains `__LINE__`.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
