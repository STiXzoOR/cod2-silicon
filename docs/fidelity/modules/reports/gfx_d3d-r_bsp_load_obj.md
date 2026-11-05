# gfx_d3d/r_bsp_load_obj module reconciliation

Updated 2026-11-05.

**15/15 reconciled: 9 equal, 4 fixed, 2 intentional.** Mac 1.3 i386 is the authority.

`R_LoadWorldInternal` is provided by `r_loadworld_new.c`; its complete Mac 0xe4fa0–0xe6a84 body was compared there. It now uses the optimization dvar and texture limit for merge dimensions (0xe552e/0xe6272), chooses the globally strongest unused lightmap pair with Mac's pair emission order (0xe55dc–0xe5709), preserves 32-bit coupling wrap, rejects invalid empty lump offsets and loads occluders unconditionally. The selected GL shader-cache fallback and native BGRA upload conversion remain; current A8R8G8B8 uploads use GL_BGRA.

`R_LoadPortals` uses signed coefficient bits for side bytes at 0xe3b5a–0xe3b96. `R_LoadSurfaces` tests `MaterialInfo.gameFlags & 8` for sky geometry at 0xe4aa2. `R_LoadOccluders` restarts its disk-edge input cursor at the lump start for each occluder at 0xe41a1; `firstEdge` still positions output. This deterministic cursor quirk is separate from invalid disk index concerns. Native range/storage guards and unsigned index allocation remain.

All fifteen bodies were hand compared, including the complete entity loader, sun interpretation, node/leaf conversion, AABB/cell/group fixups, portal vertices and all lump checks. `mnode_t.parent` remains an integer that receives truncated parent addresses. A complete selected-source search found no reader in the loader, DPVS, marks or static-model loader. This is dormant metadata, not preserved pointer layout; parent/cell recursion and active links were checked.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
