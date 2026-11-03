# game/g_debug_mp module reconciliation

Updated 2026-11-03; baseline `41d5e1b`.

**5/5 entries reconciled as intentional Linux-only debug-helper omissions.**
No engine implementation changes are required. This does not classify these
functions as semantically equal Mac bodies.

All five complete Linux bodies were read. Searching the entire reference source
found only their definitions and calls within this module. G_DebugLine is empty;
the other helpers construct local geometry ending at that sink. There are no
native source references, no corresponding Mac export/STABS inventory entries,
and no defined Mach-O symbols containing G_Debug. The ledger retains all five
rows rather than dropping them from the work denominator.

## G_DebugArc

Complete Linux body adjusts wrapped angle range, computes sixteen local XY vertices and emits fifteen segments solely to empty G_DebugLine. No external source caller, native reference or Mac named identity found. Intentionally omit this unreachable Linux drawing helper.

## G_DebugBox

Complete Linux body computes eight rotated/translated local vertices and sends twelve edges solely to the empty G_DebugLine. No caller outside g_debug_mp.cpp, native reference, or Mac named identity found. Intentionally omit this unreachable Linux debug helper; no game-state output is removed.

## G_DebugCircle

Complete Linux body selects an upward normal or computes one from the first client view origin, then calls G_DebugCircleEx. No external source caller, native reference or Mac named identity found. Intentionally omit this unreachable Linux debug helper; do not claim an arbitrary callable implementation is equivalent.

## G_DebugCircleEx

Complete Linux body normalizes a local direction, computes perpendicular/cross vectors and sixteen local vertices, then sends a closed loop solely to empty G_DebugLine. No caller outside the isolated debug module and no native reference/Mac named identity found. Intentionally omit the unreachable Linux drawing helper.

## G_DebugLine

Complete Linux body is empty. All source references are internal to g_debug_mp.cpp; no native reference or Mac named export/defined symbol exists. Intentionally omit this Linux-only drawing sink, retaining the ledger row.

## Verification and limits

The decision rests on complete source bodies, absence of external callers and
absence of a Mac/native named identity, not on names alone. Anonymous or renamed
code elsewhere in the binary is not exhaustively ruled out. No reachable engine
behavior is changed, and no diagnostic renderer is being replaced by a new stub.

This batch changes only reconciliation metadata and documentation. The completed
stock/CoD2x builds, ABI/legacy checks and 2,000-frame replay evidence from combat
baseline `41d5e1b` remain applicable to the unchanged engine. No duplicate build
or runtime rerun is required. One independent local review found no findings and confirmed all five
classifications. Its receipt is retained in the private evidence directory.

Private evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-debug-20261103.noindex/`.
