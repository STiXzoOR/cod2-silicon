# gfx_d3d/r_staticmodel_load_obj module reconciliation

Updated 2026-11-05.

**13/13 reconciled: 11 equal, 1 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

All thirteen entries were hand compared, including the full AABB partitioning/flattening recursion, model quaternion and bounds construction, cell filtering, LOD distances and lighting paths. `R_ValidateStaticModel` now emits the complete rigid-model diagnostic at Mac 0x106be2, including asset-manager guidance.

The zero-initialized allocation contract was verified in the selected `Hunk_AllocAlignInternal`. Lighting arithmetic, six-direction coefficient packing, atlas indices, saturation/floor order and lookup coordinates match. `R_CacheStaticModelLighting` retains native BGRA bytes instead of Mac ARGB because the current GL upload consumes BGRA; this conversion remains necessary for the selected renderer.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
