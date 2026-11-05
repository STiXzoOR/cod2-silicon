# gfx_d3d/r_scene module reconciliation

Updated 2026-11-05.

**18/18 reconciled: 12 equal, 5 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

All eighteen entries were hand compared. `R_AddStaticModelToScene` indexes 96-byte DX7 lighting records at 0xc6f07–0xc6f09 instead of advancing by bytes, and its entity-limit format no longer contains an escaped percent. `R_AddPolyToScene` truncates the lightmap index before packing the sort key at 0xc5937 and uses the Mac signed polygon-slot remainder.

`R_SetViewParmsForScene` selects the 0.01 default for an unordered near-clip dvar at 0xc5b7b–0xc5b94. `R_RenderScene` now checks both window dimensions (0xc69bc), gates early effects on draw-loop completion and full-window rendering (0xc6592), and applies late effects to full-window views (0xc6699). Fog interpolation compares previous and target technique offsets (0xc6451), uses the Mac duration denominator (0xc66b8), and truncates interpolated colors through an integer. The full debug entity-count dump at 0xc61b4–0xc6430 was restored.

`qsortDrawSurfs` now follows the complete custom unstable partition/selection algorithm at 0xc52c6–0xc5558, including the extra division by eight in its pivot/cutoff calculation and tie behavior. Native pointer-sized records remain. The entity-dump comparator intentionally orders `uintptr_t` keys instead of truncating pointer differences; identity grouping and type precedence match. The optional Windows v60 forwarding bridge is inactive in the native GL build.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
