# system/stl_heap.h attribution reconciliation

Updated 2026-11-05.

4 equal, 3 intentional (7 entries).

The active heap-adjustment helpers for materials, images, shader records and display modes were compared by hand against __textcoal_nt. Their larger-child selection, single-child handling, parent sift and final value store match. Original CResInfo push/make/adjust helpers belong to the excluded MacDisplay vector model; their owner replacement is specific to SDL display modes.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
