# system/stl_algo.h attribution reconciliation

Updated 2026-11-05.

6 equal, 3 fixed, 6 intentional (15 entries).

The active engine owners are r_material.c, r_material_load_obj.c, r_image.c and r_init.c. Material_Sort now synchronizes the render thread and uses element counts for its depth and 16-element cutoff; the former byte cutoff overran native9–16-item ranges. The material/shader/image/mode heap and insertion bodies were compared with coalesced Mac code. Both GfxImage and D3DDISPLAYMODE introsort partitions had reversed, negated predicates; both are corrected. GfxImage depends on the independently compiled signed gfx-images commit d857f51, whose final guarded hunk was reviewed here; it is not included in this worktree. CResInfo/monitor sorting belongs to the excluded Carbon MacDisplay model, replaced by platform SDL mode storage.

Changed or unresolved examples: `Material_Sort` at 0x000d2fe0, `ZSt16__introsort_loopIP15_D3DDISPLAYMODEiPFhRKS0_S3_EEvT_S6_T0_T1_` at 0x002efcea, `ZSt16__introsort_loopIPP8GfxImageiPFiS1_S1_EEvT_S5_T0_T1_` at 0x002f0510.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
