# system/stl_pair.h attribution reconciliation

Updated 2026-11-05.

1 fixed, 1 intentional (2 entries).

CVAOPacket_Cache default-constructed two packets instead of copying them. Native copy construction now preserves the values while installing base vtables, and insertion reaches the restored typed tree provider. The string-pair tree insert belongs to the original sShaderPrograms table; the selected native external ARB/CTAB cache does not construct that runtime map.

Changed or unresolved examples: `CVAOPacket_Cache` at 0x0011391e.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
