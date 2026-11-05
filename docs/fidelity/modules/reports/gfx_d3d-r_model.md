# gfx_d3d/r_model module reconciliation

Updated 2026-11-05.

**33/33 reconciled: 27 equal, 5 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

All thirty-three bodies were hand compared, including complete rigid/weighted skinning commands, preprocessing, bounds and debug geometry.

Mac has independent warning counters at 0xd40b00/04/08/0c/10 for cached vertex allocation, pre-skin surfaces, scene DObjs, static surfaces and XModels. Native incorrectly shared one. The guarded paths now use the separate counters and full cache, temp-buffer, scene-material and scene-surface diagnostics.

Weighted DX7 position transforms at 0xd2324–0xd232b and 0xd23b8–0xd23bf group `(x + y) + (z + translation)`; native used sequential additions. The initial and blend transforms now follow the SSE order. DX9 fourth-component weighting, first-bone frame rotation, rigid paths, surface masks and pointer-sized command strides were checked. The 72 integers in the debug-box corner table match Mac data at 0x32ad80.

`R_LockSkinnedCache` retains the existing portable lock-flag selection and null-buffer guard. Mac 0xd0030 always uses 0x2000; native can choose 0x1000. The selected GL vertex-buffer lock ignores both flags and returns its data plus offset. Native temp-skin storage lives in `DxGlobals`; the model callers and selected init/render-command reset/allocation providers use that same storage. Zeroed command padding remains defined native storage.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
