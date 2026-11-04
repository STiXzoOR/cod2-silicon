# EffectsCore/FxCurve module reconciliation

Updated 2026-11-05.

**4/5 reconciled: 4 equal; 1 unchecked.** No source change.

The iterator constructor at Mac 0xb9fc8 and integration at 0xb9fde were compared by hand. Integration retains the strict next-key comparison, scalar interpolation, trapezoid grouping and original unchecked end traversal. The segment and interpolation helpers are represented by matching inline expressions in Mac and native integration. The pointer-free curve header remains 8 bytes; the iterator uses the native pointer width.

`FxCurveIterator_Release` remains unchecked. Mac has an absolute-zero `.eh` marker but no executable symbol. No native definition/caller or identified inline release site was found, so the Linux null-master body was not assumed equivalent.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.
