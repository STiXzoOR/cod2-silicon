# stringed/stringed_interface reconciliation

Updated 2026-11-05.

**4 entries: 2 equal, 2 intentional.** All four exact Mac 1.3 i386 bodies were compared by hand. Source changes were unnecessary.

LoadFileData at 0xbc8bc returns the file buffer only for a positive FS_ReadFile length, and FreeFileDataAfterLoad at 0xbc8e2 delegates directly to FS_FreeFile. Recursive listing at 0xbc8ec preserves directory filtering, directory-before-file order, path formatting, per-file delimiter/count and file-list release order. BuildFileList at 0xbca88 clears the destination/counter and uses the str extension.

The existing native Apple implementation deliberately uses the documented 8-byte Cod2MacString and 24-byte COW representation, with append(";", 1) replacing original i386 -12/-8/-4 header writes. The shared provider's size_t and detach contracts were inspected read-only.

The Release arm64 dedicated build links, and the actual client translation unit compiles with this worktree's production flags. The existing ASan/UBSan cpp_abi fixture passes actual SE_BuildFileList recursion, delimiters, count and COW detachment, together with its existing provider tests. It does not validate the original fixed 64-byte path buffer on long paths or game filesystem assets. No new cases, full game run or replay were added. Private logs and Mac evidence remain under recon-stringed.noindex.
