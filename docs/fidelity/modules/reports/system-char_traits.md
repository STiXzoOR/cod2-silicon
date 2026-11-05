# system/char_traits.h attribution reconciliation

Updated 2026-11-05.

1 fixed (1 entries).

The Mac helper dereferences two char references and returns byte equality. Its CDirect3DDevice.c export was a void no-op; the native guarded provider now has the correct typed arguments and zero/one result. No direct native caller was found.

Changed or unresolved examples: `ZNSt11char_traitsIcE2eqERKcS2_` at 0x002ed922.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
