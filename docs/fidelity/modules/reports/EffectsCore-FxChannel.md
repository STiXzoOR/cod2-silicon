# EffectsCore/FxChannel module reconciliation

Updated 2026-11-05.

**5/6 reconciled: 4 equal, 1 fixed; 1 unchecked.** Mac 1.3 is the authority. The legacy source is unchanged.

`FxChannel_CreateViaMigration` centered the target scale range on the reciprocal scale. Mac 0xaca12–0xaca37 keeps the original scale and its reciprocal separately; 0xac50d–0xac541 uses the original scale plus/minus the start half-range. The native branch now retains that original range scale while preserving the reciprocal for key values. This correction is compiled and reviewed but not exercised.

The default constructor and migration's linear, delayed and sampled paths were compared by hand. Inlined Mac sampling preserves the 16-key cadence, flag modes, clamp quirks, float grouping and random-call order. The native pointer-sized channel and iterator fields are retained. The checked migration caller supplies one or three dimensions and boolean force-unit values.

`FxChannel_Copy` remains unchecked: neither an executable Mac identity nor a native definition/caller was found. The Linux reconstruction describes it as unused; that is insufficient to claim Mac equality.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.

## Integrated batch verification

Integrated source cef44cb passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.


## Original helper provenance — fx9

Original 1.0 0x812123c uniquely maps to original 1.3 0x81236cc. The complete helper passes source->curve through FxCurve_Reference at call 0x81236de to 0x81240d0, writes the returned pointer to target, then copies both 32-bit scale-range endpoint words. This is a real reference/copy contract, not an inferred unused symbol. Fresh exact Mac FxChannelInstance_Create 0x9ff88 creates an iterator from the curve and samples one scale through FxRange::GetVal; it does not copy both endpoint fields into another FxChannel. Construction/migration owners likewise do not establish this complete copy. No positive Mac/native copy owner was recovered, so the row remains unchecked.

The retained direct-call scan finds the positive channel-copy-to-curve-reference chain; raw absolute address references add only unwind records. Those bounded searches are not evidence of semantic absence and close no row. No source or prior resolved verdict changed, no new defect was established, and no compilation, tests, CI, replay or runtime exercise ran. Original bodies, disambiguating neighbors, decoded table and fresh exact Mac candidates are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-fx9.noindex/`.
