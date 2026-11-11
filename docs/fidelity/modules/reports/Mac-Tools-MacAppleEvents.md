# Mac/Tools/MacAppleEvents reconciliation

Updated 2026-11-05.

**15/15 reconciled: 15 intentional.** The old ProcessSerialNumber/Carbon object-property transport has no external source caller or reference from the compiled native generated-data files. This conclusion uses the selected call graph, not the module name or the presence of stubs.

Read both `GetData` overloads at 0x2640/0x2736, including type coercion, Pascal length limit 255 and signed status handling. Read event/target destruction, null initialization, all C1/C2 target and object-specifier constructors, and `AESend` at 0x2cba. The property helpers at 0x2d72/0x2ef4/0x30c8 build core `setd`/`cnte`/`getd` requests with object/type/data parameters and reply extraction. Their dormant native stubs or missing overloads are explicitly not equality claims, and the legacy exception/runtime wrappers are not approved for live use.

Native engine startup and launcher lifecycle no longer create the old Carbon engine or target these objects. The separately reviewed pending Unix-runtime commit `5ee587bb47bd3e6a6d1a098bef94de96fa2c1311` opens the required application/URL through SDK CoreFoundation/LaunchServices rather than reviving this object scripting layer. That dependency was read, not cherry-picked or modified in this unit. General AppleEvent property automation is not an active native feature.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
