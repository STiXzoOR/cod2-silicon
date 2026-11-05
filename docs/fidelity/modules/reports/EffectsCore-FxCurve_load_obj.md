# EffectsCore/FxCurve_load_obj module reconciliation

Updated 2026-11-05.

**1/2 reconciled: 1 equal; 1 unchecked.** No source change.

The complete allocator at Mac 0xac320 was compared by hand, using a disassembly restarted at the exact symbol to avoid a misdecoded prologue. Allocation, endpoint padding and key copying match. Two unusual details are retained: a padded final key copies values from the first input key, and stored `keyCount` excludes added endpoint keys. The allocator uses the matching pointer-free 8-byte curve header, so native pointer widening requires no adjustment here.

`FxCurve_Reference` remains unchecked. There is no executable Mac identity or native definition/caller; Mac's absolute-zero `FxCurve_AddRef.eh` marker does not identify the Linux pointer-return helper.

No new tests were added. All four assigned sources compile cleanly with the configured arm64 Release dedicated-server commands. The low-priority `cod2_macos_ded` arm64 Release build passed with two jobs. No runtime effect path, CI suite or replay was run by this worker. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-fx-foundation.noindex/`.

## Integrated batch verification

Integrated source cef44cb passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
