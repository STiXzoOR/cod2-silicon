# Mac/DirectX 9/CMemoryBuffer reconciliation

Updated 2026-11-05.

**10/10 reconciled: 9 equal, 1 fixed.** The Mac 1.3 i386 binary is the authority. Every assigned body was read; the repeated static-initializer symbol was selected by its actual memory-module address, 0x1fe42. The OFF source path is unchanged.

Reset at 0x1fdd6 calls `glFinish` before freeing delayed allocation payloads. Native had omitted that synchronization. The call is restored before the freeing loop, including when the list is empty.

Constructor 0x1fb72, Recreate 0x1fbe0 and Resize 0x1fc96 match the 32-byte allocation rounding/alignment, stored length and freed flag. Destructors at 0x1fc1a/0x1fc54 match the freed/allocation guards and pointer clearing. FreeLater at 0x1fcf6 matches tail insertion, delayed-byte accounting, frame count and owner-state clearing. The native list sentinel and request nodes have real pointer-sized storage rather than reusing the four-byte reconstructed scalar global. That existing LP64 adaptation is retained.

The initialization/destruction wrappers at 0x1fe80/0x1fe90 pass the expected priority and selector. Their helper initializes self-links or deletes request nodes without freeing payloads, matching the original static destructor. The adjacent Update body at 0x1fd6c was also checked: a nonzero frame count is decremented and retained; a request that starts an update at zero is freed.

The backing-state type is now shared with the buffer owners without changing its Apple layout: 32 bytes, with allocation/data pointers at 8/16 and length/freed fields at 24/28. The arm64 Release dedicated target and this actual Apple client object compile. All applicable OFF guard comparisons pass, and no changed file uses `__LINE__`. No new tests or replays were run. Reset is compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-buffers.noindex/`.
