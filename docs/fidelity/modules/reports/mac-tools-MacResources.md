# Mac/Tools/MacResources reconciliation

Updated 2026-11-05. **8/8 reconciled: 8 intentional.**

Mac 0x9c0c reads `GameStrings`, converts to a 256-byte MacRoman Pascal string and releases the result. Mac 0x9cbc caches a numeric localized language with sentinel 0x7fff. Mac 0x9c7a and 0x9e14 use distinct cached strings. The other four assigned exports build cached startup-error messages with product/data-folder substitutions and release the temporary format.

No active native caller reaches this resource graph. Pending display reconciliation restores a compiled GetLanguageCode call in VK_Map, but its only consumers are the dormant Carbon key callbacks; the active SDL event path bypasses them. The retained language provider still returns ANSI (zero), so this does not restore French/German selection through that dormant graph. The generated `GetMissingDataFolderError` reference belongs only to the pointer-size-4 key-table fallback. Native data paths come from the launcher, localization from the `loc_language` dvar in `stringed_hooks.c`, display startup from the SDL provider, supported OS versions from deployment/launcher checks, and native media/audio replaces QuickTime. The old optical-disc check is retired.

The retained i386 literal key addresses, int-typed CF references and merged `sResult` cache are not asserted equivalent to those Mac caches. No live path uses that storage, so no speculative CF/resource reconstruction was introduced. The real SDK `CFBundleGetMainBundle` needed by unix-runtime belongs to its separately audited URL launch provider, not these dormant exports.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

## Batch 19 follow-up

The formerly pending VK_Map dependency is resolved as a retired key/resource boundary by mac-key-resource-imports.md. The selected SDL key/text path and file localization bypass the retained Carbon graph. Zero/int-typed dormant CF helpers remain unapproved for future live use; no keyboard-layout parity is claimed.
