# EffectsCore/FxMemMgr module reconciliation

Updated 2026-11-05.

**1/5 reconciled: 1 equal; 4 unchecked.** No source change.

The exact-symbol Mac `FxMem_Init` entry at 0xacbd8 is a no-op through its return at 0xacbdc, matching native. The apparent later instructions are padding and the next routine, not arena initialization.

`FxMem_AllocBlock`, `FxMem_UnlinkBlock`, `FxMem_FreeBlock` and `FxMem_FreeItem` remain unchecked. No executable Mac identity or identified inline body was found for those Linux names. Native has no definitions, callers, `FxMemBlock` layout or `fx_memBlocks` arena. Existing effects allocations use `__Znam`/`__ZdaPv` paths with native object sizes. The Linux 32KB arena, 32-bit link words and masked pointer-to-block lookup were not introduced without a matching Mac allocator/layout contract. No shared headers were edited.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.
