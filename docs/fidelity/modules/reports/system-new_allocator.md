# system/new_allocator.h attribution reconciliation

Updated 2026-11-05.

3 intentional, 2 unchecked (5 entries).

The old surface/volume reserve helpers are replaced by typed calloc mip arrays in the active texture providers; CResInfo assignment belongs to excluded MacDisplay storage. Both Device constructors remain unchecked. The selected CreateDevice initializes 4096-byte static native storage through CDirect3DDevice_Init and bypasses the original constructor. The existing exported constructor sets only vtable/refcount, and complete Mac default/state/Reset equivalence is not established. The parent retains that device-initialization audit.

Changed or unresolved examples: `CDirect3DDevice_CDirect3DDevice` at 0x0001a7ba, `CDirect3DDevice_CDirect3DDevice` at 0x0001ab62.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
