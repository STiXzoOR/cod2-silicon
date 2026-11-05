# ui/ui_atoms module reconciliation

Updated 2026-11-05.

**4/4 reconciled: 3 equal, 1 intentional.** Compared all four Mac bodies at 0x1cbcf0 through 0x1cbf0c by hand, including negative-width/height UV reversal, load-bar division and upper clamp, optional white-material drawing, and renderer argument order. The constant and call/string checks were also reviewed.

`UI_DrawHandlePic` retains the existing native trace probe behind `COD2_NATIVE_TRACE_ONLY`. Mac 0x1cbcf0 has no probe. It is disabled by default and can print the first four calls when native tracing is explicitly enabled; the rendering behavior matches. No source change was needed.

The arm64 Release dedicated target built successfully. Since that target excludes these client functions, the actual production `ui_atoms.c` translation unit was separately compiled with the configured client flags, with no warnings. No UI execution or replay was run and no tests were added. Windows comparison was unnecessary: the Mac rendering behavior is not a visible platform-specific difference.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
