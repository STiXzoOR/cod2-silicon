# EffectsCore/Fxexport module reconciliation

Updated 2026-11-05.

**16/16 reconciled: 9 equal, 7 intentional.** Mac 1.3 i386 is the authority. All 13 Mac wrapper entries were read from exact-symbol `otool -arch i386 -tV -p` output. The two Linux overload rows were paired by signature with the corresponding Mac wrappers and scheduler overloads. No source change is needed in this module.

## Export behavior and native adaptations

The bone lookup, low-byte renderer initialization argument, free flags, camera-valid stores, active-counter copies, warp-time dispatch and effect-length dispatch match the Mac. `Server_SwitchToValidFxScheduler` is empty at Mac 0x60e30. The Linux-only empty scheduler-selection helper is intentionally omitted with its dedicated-server selection loop.

Native arm64 exports inline orientation work from Mac scheduler overloads. The three-argument wrapper at 0x60cb4 reaches the 0x61802 overload, which copies the forward vector and calls `MakeNormalVectors`. The four-argument wrapper at 0x60ce0 reaches 0x6178e, which copies forward/up and forms right with `forward cross up`. Native `FX_PlayEffect` and `FX_PlayOrientedEffect` perform those operations before calling the shared scheduler entry with a null bolt. Entity effects forward the supplied axis/bolt unchanged.

All four effect-playing native exports retain their optional `COD2_NATIVE_TRACE` combat receipt. That additional diagnostic is intentional, so the six ledger rows for simple/entity effects and the two overloaded exports are marked intentional, including their duplicate Linux rows.

## Scheduler dependency

`FX_PlaySimpleEffect` at Mac 0x60c8e reaches the 0x61862 scheduler overload. That overload builds rows `{0,0,1}`, `{1,0,0}`, `{0,1,0}`. Native preserves its existing NULL-axis sentinel contract. The required matching sentinel basis is supplied by the separately reviewed FxScheduler commit `99f7a49`, with unit tip `407728342fa3eae851be04f205b2c521126ec378`. That pending source was inspected read only; it must merge before this unit. The FxSystem worktree base still contains the older identity-basis fallback. No neighboring scheduler file was changed here.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built successfully with two low-priority jobs and compiled the actual `Fxexport.c` production source. Its existing axis-pointer type warning remains; no new warning was introduced in this unchanged file. Triage/constant scripts and hand comparison of wrappers plus orientation overloads were completed. The inactive-guard comparison found no change in five legacy configurations.

No tests, replays, full CI or gameplay sessions were run. The scheduler dependency was inspected but not executed in this worktree; integration owns combined runtime validation. The comparison covers the native arm64 Apple SDK path, not other native platform paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-system.noindex/`.

## Integrated batch verification

Integrated source 4584867 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production sources differ only under linux-x86_64-port-on; five inactive configurations remain unchanged. Actual line-dependent assertion expansions in FxUtil match the previous source in all five configurations. Four existing fixture files were corrected for default axes, file-open API, bolt-motion expectations and warning/print-provider linkage; no cases were added. The original step 13 failed linking the existing channel fixture against the production FX_Print helper; its aborting stub was moved to Com_Printf, both mode-specific channel fixtures passed, independent review passed, and the entire source-fixture stage passed on retry. Original failure logs are retained. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units. Review corrected primitive RGB clamping. An isolated combined checkout also passed existing stock/CoD2x primitive ASan/UBSan fixtures and the dedicated effects parser fixture. The final integrated CI includes the same existing fixture families. Twenty-five entries remain unchecked across the effects units, with their exact identity or input-contract gaps recorded individually. Archive streams and unreached client rendering paths remain compiled and reviewed, not runtime exercised.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client effects rendering or UI interaction.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
