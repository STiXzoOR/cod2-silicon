# system/stl_algo.h attribution reconciliation

Updated 2026-11-05.

6 equal, 3 fixed, 6 intentional (15 entries).

The active engine owners are r_material.c, r_material_load_obj.c, r_image.c and r_init.c. Material_Sort now synchronizes the render thread and uses element counts for its depth and 16-element cutoff; the former byte cutoff overran native9–16-item ranges. The material/shader/image/mode heap and insertion bodies were compared with coalesced Mac code. Both GfxImage and D3DDISPLAYMODE introsort partitions had reversed, negated predicates; both are corrected. GfxImage depends on the independently compiled signed gfx-images commit 7c11105, whose final guarded hunk was reviewed here; it is not included in this worktree. CResInfo/monitor sorting belongs to the excluded Carbon MacDisplay model, replaced by platform SDL mode storage.

Changed or unresolved examples: `Material_Sort` at 0x000d2fe0, `ZSt16__introsort_loopIP15_D3DDISPLAYMODEiPFhRKS0_S3_EEvT_S6_T0_T1_` at 0x002efcea, `ZSt16__introsort_loopIPP8GfxImageiPFiS1_S1_EEvT_S5_T0_T1_` at 0x002f0510.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source 8cf8aa8 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both 819d282 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
