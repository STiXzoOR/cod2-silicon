# EffectsCore/FxChannel module reconciliation

Updated 2026-11-05.

**5/6 reconciled: 4 equal, 1 fixed; 1 unchecked.** Mac 1.3 is the authority. The legacy source is unchanged.

`FxChannel_CreateViaMigration` centered the target scale range on the reciprocal scale. Mac 0xaca12–0xaca37 keeps the original scale and its reciprocal separately; 0xac50d–0xac541 uses the original scale plus/minus the start half-range. The native branch now retains that original range scale while preserving the reciprocal for key values. This correction is compiled and reviewed but not exercised.

The default constructor and migration's linear, delayed and sampled paths were compared by hand. Inlined Mac sampling preserves the 16-key cadence, flag modes, clamp quirks, float grouping and random-call order. The native pointer-sized channel and iterator fields are retained. The checked migration caller supplies one or three dimensions and boolean force-unit values.

`FxChannel_Copy` remains unchecked: neither an executable Mac identity nor a native definition/caller was found. The Linux reconstruction describes it as unused; that is insufficient to claim Mac equality.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.
