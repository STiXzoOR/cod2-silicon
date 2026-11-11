# system/char_traits.h attribution reconciliation

Updated 2026-11-05.

1 fixed (1 entries).

The Mac helper dereferences two char references and returns byte equality. Its CDirect3DDevice.c export was a void no-op; the native guarded provider now has the correct typed arguments and zero/one result. No direct native caller was found.

Changed or unresolved examples: `ZNSt11char_traitsIcE2eqERKcS2_` at 0x002ed922.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source 8cf8aa8 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both 819d282 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
