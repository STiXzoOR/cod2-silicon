# unix/unix_dir reconciliation

Updated 2026-11-05.

**0/1 reconciled; 1 unchecked.** `Sys_DirectoryHasEntries` has only a Linux ledger identity. No executable Mac symbol, selected native definition or caller was found.

The active native filesystem uses `mac_common.c:Sys_DirectoryHasContents`, whose Mac identity is 0xc4e62. That function accepts only visible regular dirents and has been corrected and recorded in `unix-unix_shared.md`. Similar names do not prove these are the same function or contract. The Linux-only row remains unchecked pending an exact Mac inline identity or a demonstrated provider mapping.

No source changes were made for this module. Both native executables compile; no directory probe was added or run.
