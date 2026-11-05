# system/stl_vector.h attribution reconciliation

Updated 2026-11-05.

5 intentional (5 entries).

CStaticCacheInfo destructors own the original vector of VAR cache blocks. Active native vertex-buffer storage and CPU-array draw submission do not construct that cache model. UserPaneDrawProc is the Carbon monitor-layout preview using sRectList, not a generic library function; the entire MacDisplay source is explicitly excluded for the native SDL display implementation. The other initializer owns the dormant cursor list, which SDL input/system-cursor ownership replaces.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
