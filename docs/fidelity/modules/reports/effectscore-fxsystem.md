# EffectsCore/FxSystem module reconciliation

Updated 2026-11-05.

**19/20 reconciled: 14 equal, 5 fixed, 1 unchecked.** Mac 1.3 i386 is the authority. All 19 named Mac entries were read from exact-symbol `otool -arch i386 -tV -p` output. Linux was read only for names and the unmatched empty helper. The legacy source is unchanged.

## Corrections

**Archive mode came from the widened pointer.** Mac 0xab6f1 and its repeated checks read the byte after the four-byte `memFile` pointer. Native read raw byte 4, which belongs to the eight-byte pointer. All four integer transfers and the final `time` reset now use `arch->isReading`.

**Cylinder culling lost unordered branches.** Mac 0xab2dc-0xab305 uses unsigned above/below-or-equal branches after `ucomiss`. Both predicates accept unordered comparisons. Native `<=` did not; native now uses `!(dot > -radius)` for each endpoint.

**Zoom calculation widened too early.** Mac 0xab6a3-0xab6af halves the FOV in single precision, then widens and multiplies by double radians. Native now preserves that float intermediate, which also preserves subnormal rounding.

**Seed wrap is now explicit.** Mac 0xab0c2 keeps the low 32 bits of the seed product. The native signed multiply had undefined overflow; the unsigned product now defines the same wrap and returns the old seed.

**Frustum arithmetic follows the Mac instruction order.** Mac 0xab3bb-0xab44d and 0xab4c5-0xab557 add cosine products before sine products and negate cosine before multiplying opposite-plane axes. Native now expresses those operations directly. This is operation-order alignment, not a measured gameplay difference.

## Comparison examples and remaining gap

`AdjustTime` retains the frozen/initial frame behavior, [0,200] clamp, old/current-time order and 112-byte previous-camera copy. `WarpTime` retains its negative-delta exit and 200ms update chunks. The sphere cullers retain the strict ordered comparison; the cylinder's inclusive/unordered behavior is distinct in the Mac. Renderer forwarding slots, trace argument order and entity sentinels, constructor stores and the empty camera-shake method match.

The Linux-only `FxHelper::Unused` has no identified Mac symbol or inline call site and no native body/caller. Its original name is unknown. That one row remains unchecked; absence was not recorded as equality.

## Verification and limits

The arm64 Release `cod2_macos_ded` target configured and built successfully using `taskpolicy -b nice -n 19` and two jobs. The build compiled the actual `FxSystem.c` and `Fxexport.c` translation units. The inactive-guard comparison found zero mismatches for both assigned sources in five `COD2_X64=OFF` configurations; neither source contains `__LINE__`. `git diff --check` passed. Triage and range-limited constant checks were used as leads; exact-symbol disassembly resolved ambiguous `Init`/`Archive` name matches.

All five changes are compiled and reviewed, not exercised. No tests, replays, full CI or gameplay sessions were run. Runtime validation belongs to integration. Other native platform paths were not compiled in this unit.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-system.noindex/`.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.


## Original helper provenance — fx9

The original FxHelper empty slot is located at 1.0 0x812ae6c / 1.3 0x812d28c between the light and camera-shake slots, with the following GetSeed owner independently mapped at 0x812d298. The reconstructed Unused label still has no recovered original name or complete Mac/native consumed role. No closure follows from its empty body or dedicated context.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.
