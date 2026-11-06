# system/stl_vector.h attribution reconciliation

Updated 2026-11-05.

5 intentional (5 entries).

CStaticCacheInfo destructors own the original vector of VAR cache blocks. Active native vertex-buffer storage and CPU-array draw submission do not construct that cache model. UserPaneDrawProc is the Carbon monitor-layout preview using sRectList, not a generic library function; the entire MacDisplay source is explicitly excluded for the native SDL display implementation. The other initializer owns the dormant cursor list, which SDL input/system-cursor ownership replaces.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
