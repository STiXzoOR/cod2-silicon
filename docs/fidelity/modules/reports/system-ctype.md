# system/ctype.h attribution reconciliation

Updated 2026-11-05.

1 fixed, 3 intentional (4 entries).

Console_Key contains substantial inlined engine behavior. Native Tab completion now preserves arguments, completes enum values and filenames, strips extensions and follows the binary’s multi-match cursor behavior. Ctrl-wheel scrolls three pages; only lowercase Ctrl-L clears. History-up still restores the current slot when it cannot decrement. The discarded PunkBuster completion backend remains absent while its prefix formatting is retained. Native null/bounds guards avoid introducing invalid storage accesses. The three libwww NEWS/rule/FTP parser entries are explicitly outside scope; engine downloads use SDK libcurl.

Changed or unresolved examples: `Console_Key` at 0x001405ce.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source 8cf8aa8 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both 819d282 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
