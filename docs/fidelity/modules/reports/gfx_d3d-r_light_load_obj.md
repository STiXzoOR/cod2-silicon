# gfx_d3d/r_light_load_obj module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.** The selected GL renderer still uses this loader.

Read the exact Mac `R_LoadLightDef` entry at 0xfb7e8. File failure, empty-file freeing, type and sampler bytes, both image strings, `Image_Register(name, 1, 5)`, allocated lowercase name and final file freeing match for complete records. Native uses the widened `GfxLightDef` layout. The native path correctly has no missing-`default` fallback; Mac returns null directly.

The existing native bounds checks remain intentional. Mac scans both strings without validating record length. `FS_ReadFile` allocates one extra trailing zero, which can terminate an incomplete final string but cannot make every truncated field read valid. Native requires both terminators inside the file and enough bytes for the second sampler/string. It rejects these incomplete records instead of reproducing reads beyond their declared data. No loader source changed.

Triage, constant checks and hand comparison used exact-symbol disassembly because the full-binary listing misdecoded the prologue. The arm64 Release dedicated target and the actual client object compile. No new tests, full CI or replays ran; loading game assets and the GL upload boundary were not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-assets.noindex/`.
