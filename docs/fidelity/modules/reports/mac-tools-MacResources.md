# Mac/Tools/MacResources reconciliation

Updated 2026-11-05. **8/8 reconciled: 8 intentional.**

Mac 0x9c0c reads `GameStrings`, converts to a 256-byte MacRoman Pascal string and releases the result. Mac 0x9cbc caches a numeric localized language with sentinel 0x7fff. Mac 0x9c7a and 0x9e14 use distinct cached strings. The other four assigned exports build cached startup-error messages with product/data-folder substitutions and release the temporary format.

None has an external native source or active generated-data caller. The generated `GetMissingDataFolderError` reference belongs only to the pointer-size-4 key-table fallback. Native data paths come from the launcher, localization from the `loc_language` dvar in `stringed_hooks.c`, display startup from the SDL provider, supported OS versions from deployment/launcher checks, and native media/audio replaces QuickTime. The old optical-disc check is retired.

The retained i386 literal key addresses, int-typed CF references and merged `sResult` cache are not asserted equivalent to those Mac caches. No live path uses that storage, so no speculative CF/resource reconstruction was introduced. The real SDK `CFBundleGetMainBundle` needed by unix-runtime belongs to its separately audited URL launch provider, not these dormant exports.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
