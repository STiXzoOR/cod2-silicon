# system/ctype.h attribution reconciliation

Updated 2026-11-05.

1 fixed, 3 intentional (4 entries).

Console_Key contains substantial inlined engine behavior. Native Tab completion now preserves arguments, completes enum values and filenames, strips extensions and follows the binary’s multi-match cursor behavior. Ctrl-wheel scrolls three pages; only lowercase Ctrl-L clears. History-up still restores the current slot when it cannot decrement. The discarded PunkBuster completion backend remains absent while its prefix formatting is retained. Native null/bounds guards avoid introducing invalid storage accesses. The three libwww NEWS/rule/FTP parser entries are explicitly outside scope; engine downloads use SDK libcurl.

Changed or unresolved examples: `Console_Key` at 0x001405ce.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
