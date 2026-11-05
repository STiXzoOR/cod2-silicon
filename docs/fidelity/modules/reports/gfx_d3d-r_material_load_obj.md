# gfx_d3d/r_material_load_obj module reconciliation

Updated 2026-11-05.

**16/16 reconciled: 8 equal, 7 fixed, 1 intentional.** Mac 1.3 i386 is the authority. Current Apple GL is the selected renderer. Legacy OFF source is unchanged.

## Corrections

`Material_ParseCodeConstantSource_r` now gets a whole matrix's row count from the constant-entry register count. Mac 0x101aa5-0x101aac reads that record; native read the type's element count. Private native routing carries both values separately. `Material_SetPassShaderArguments` also preserves an explicitly selected first row, as Mac 0x102b58 does, and reproduces the low-byte count test at 0x1023e9: 256 constants take the empty-block branch with zero arguments.

The unsupported blend-operation rewrite in `Material_LoadPassStateMap` now changes the set-bits word at rule-set base+0x14, as Mac 0x103903/0x103929 and 0x103975/0x10399e do. Native incorrectly inspected and changed condition values at base+0x0c. The rule cursor includes the four-byte rule-count header; its clear-mask word at base+0x1c already matched.

`Material_LoadPassTextureStateDx7` now rejects a missing opening bracket in `stage[index]`, following Mac 0x101c34-0x101c56. `Material_Load` suppresses missing-file diagnostics only for `$` names, as Mac 0x10585f does. Pass-validation, conditional-rule and state-map diagnostics and the state-map error prefix now match.

## Comparisons and native boundaries

Include open/close were checked with exact-symbol disassembly. Shader text ordering/loading, sampler recursion, constant arrays/ranges and matrix selection, vector parsing, grouped state rules, all eleven state-map groups, and explicit/default CTAB mapping and cleanup were read against their Mac bodies. Binary-data comparison found no differences in the eleven constant/sampler source tables or all 21 Dx7 texture-function rows.

`Material_FinishLoadingInstance` is intentionally supplied by the bounded `Material_Marshal32To64` plus `Material_ResolveTechniqueSet` path. Fixed 68/12/20-byte disk records become native pointer-bearing allocations. Native validates ranges, strings and bounded power-of-two water dimensions and initializes pass storage, preserving defined storage instead of importing Mac stack bytes. The Dx7/Dx9 parsing and material argument validation were compared through Mac 0x1057d0. The existing material-disk fixture was inspected; no fixture expectations changed.

The current `lp64_shader_cache.h` provider returns validated extracted CTAB data when available and a zero-count table for generic ARB shaders. The existing generic sampler/channel-mixer mapping remains that concrete no-CTAB adapter. Counts above 256 retain the native rejection instead of reproducing Mac's mismatched byte-count allocation and full-count traversal.

Actual pending System 5f88bd5 source was read against 02a2156. Its minimum shader-version clamp and truncate-before-shift program length belong to `Material_LoadPassShader`; those hunks remain for integration before this unit. The existing native shader-blob cleanup also survives. Image/font 84ba122 and device 9f05cde call boundaries were inspected.

## Verification and limits

The arm64 Release dedicated target and both changed Apple client objects compiled at low priority. Ten inactive-source comparisons and `git diff --check` passed. Neither changed source contains `__LINE__` sites.

No new tests, full CI or replays were run. These corrections were compiled and reviewed, not exercised; shader-cache loading, unsupported-capability rewrites and malformed parser inputs still require integration runtime validation.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-materials.noindex/`.
