# ui/ui_component module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 equal.** Compared `UI_Component_Init` with Mac 0x1b54f6 by hand: the `cls.uiStarted` gate, full component-state clearing, integer video-size conversion, 8/16/16 metrics, developer gate, and both material registrations with flags 0x30 and track 1 match. Native clears its full pointer-sized component structure, corresponding to the Mac 0xac-byte structure. No source change was needed.

The full-binary dump misdecodes the prologue. The exact-symbol `otool -arch i386 -tV -p __Z17UI_Component_Initv` dump starts with the correct frame setup and was used for the verdict. The material strings were also read directly from the binary.

The arm64 Release dedicated target built successfully. The actual production client `ui_component.c` translation unit was separately compiled under the configured flags, with no warnings. The UI was not exercised and no tests were added. No visible platform-specific behavior required Windows comparison.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.

## Integrated batch verification

Integrated source 5bf663a passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
