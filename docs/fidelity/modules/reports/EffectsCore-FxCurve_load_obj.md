# EffectsCore/FxCurve_load_obj module reconciliation

Updated 2026-11-05.

**1/2 reconciled: 1 equal; 1 unchecked.** No source change.

The complete allocator at Mac 0xac320 was compared by hand, using a disassembly restarted at the exact symbol to avoid a misdecoded prologue. Allocation, endpoint padding and key copying match. Two unusual details are retained: a padded final key copies values from the first input key, and stored `keyCount` excludes added endpoint keys. The allocator uses the matching pointer-free 8-byte curve header, so native pointer widening requires no adjustment here.

`FxCurve_Reference` remains unchecked. There is no executable Mac identity or native definition/caller; Mac's absolute-zero `FxCurve_AddRef.eh` marker does not identify the Linux pointer-return helper.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.
