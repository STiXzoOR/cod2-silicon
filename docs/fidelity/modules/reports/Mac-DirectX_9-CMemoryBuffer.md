# Mac/DirectX 9/CMemoryBuffer reconciliation

Updated 2026-11-05.

**10/10 reconciled: 9 equal, 1 fixed.** The Mac 1.3 i386 binary is the authority. Every assigned body was read; the repeated static-initializer symbol was selected by its actual memory-module address, 0x1fe42. The OFF source path is unchanged.

Reset at 0x1fdd6 calls `glFinish` before freeing delayed allocation payloads. Native had omitted that synchronization. The call is restored before the freeing loop, including when the list is empty.

Constructor 0x1fb72, Recreate 0x1fbe0 and Resize 0x1fc96 match the 32-byte allocation rounding/alignment, stored length and freed flag. Destructors at 0x1fc1a/0x1fc54 match the freed/allocation guards and pointer clearing. FreeLater at 0x1fcf6 matches tail insertion, delayed-byte accounting, frame count and owner-state clearing. The native list sentinel and request nodes have real pointer-sized storage rather than reusing the four-byte reconstructed scalar global. That existing LP64 adaptation is retained.

The initialization/destruction wrappers at 0x1fe80/0x1fe90 pass the expected priority and selector. Their helper initializes self-links or deletes request nodes without freeing payloads, matching the original static destructor. The adjacent Update body at 0x1fd6c was also checked: a nonzero frame count is decremented and retained; a request that starts an update at zero is freed.

The adjacent Update body needed a selected caller before the restored discard paths could be safe. Mac Present calls SwapContext at 0x157ee, Update at 0x157f3 and increments COpenGL's frame count at 0x157fd. A focused native device extension restores that ordering after its SDL swap continuation. Device destruction also calls Reset before releasing the context, after successful activation through a typed display helper; this follows the delayed-storage portion of the original Shutdown boundary at 0x1b05f. Activation failure leaves pending storage intact rather than calling glFinish against an unproven context. The broader device/display rows are not closed by these dependency repairs.

The backing-state type is now shared with the buffer owners without changing its Apple layout: 32 bytes, with allocation/data pointers at 8/16 and length/freed fields at 24/28. The arm64 Release dedicated target and affected actual Apple client objects compile. The existing GL buffer fixture passes ASan/UBSan using this real memory owner and its existing deferred-byte/count assertions; its glFinish boundary is a no-context stub, so GPU synchronization and the selected Present/teardown callers remain compiled and reviewed only. All applicable OFF guard comparisons pass, and no changed production file uses `__LINE__`. No new cases, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-buffers.noindex/`.
