# Mac/Tools/MacGlobals reconciliation

Updated 2026-11-05.

**5/5 reconciled: 3 equal, 2 intentional.** The static lock helpers match Mac: increment at 0x9590, decrement only when positive at 0x959c and signed-positive predicate at 0x95b2. Selected SDL/terminal providers do not use that Carbon lock counter.

The two alert identities are physically in `MacTools.c`. Question at 0xa7f4 and message at 0xa95e check the main thread/windowed state, lock around Carbon standard alerts and unlock on both success and exception unwinding. Native wrappers are empty/false but have no external native source or compiled-native-data callers. Live autoconfigure/fatal diagnostics use `MacWin32.c:MessageBoxA` and terminal stderr, not these wrappers. They are recorded as a retired Carbon alert boundary, not as equality for missing alert UI.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
