# EffectsCore/Fxexport module reconciliation

Updated 2026-11-05.

**16/16 reconciled: 9 equal, 7 intentional.** Mac 1.3 i386 is the authority. All 13 Mac wrapper entries were read from exact-symbol `otool -arch i386 -tV -p` output. The two Linux overload rows were paired by signature with the corresponding Mac wrappers and scheduler overloads. No source change is needed in this module.

## Export behavior and native adaptations

The bone lookup, low-byte renderer initialization argument, free flags, camera-valid stores, active-counter copies, warp-time dispatch and effect-length dispatch match the Mac. `Server_SwitchToValidFxScheduler` is empty at Mac 0x60e30. The Linux-only empty scheduler-selection helper is intentionally omitted with its dedicated-server selection loop.

Native arm64 exports inline orientation work from Mac scheduler overloads. The three-argument wrapper at 0x60cb4 reaches the 0x61802 overload, which copies the forward vector and calls `MakeNormalVectors`. The four-argument wrapper at 0x60ce0 reaches 0x6178e, which copies forward/up and forms right with `forward cross up`. Native `FX_PlayEffect` and `FX_PlayOrientedEffect` perform those operations before calling the shared scheduler entry with a null bolt. Entity effects forward the supplied axis/bolt unchanged.

All four effect-playing native exports retain their optional `COD2_NATIVE_TRACE` combat receipt. That additional diagnostic is intentional, so the six ledger rows for simple/entity effects and the two overloaded exports are marked intentional, including their duplicate Linux rows.

## Scheduler dependency

`FX_PlaySimpleEffect` at Mac 0x60c8e reaches the 0x61862 scheduler overload. That overload builds rows `{0,0,1}`, `{1,0,0}`, `{0,1,0}`. Native preserves its existing NULL-axis sentinel contract. The required matching sentinel basis is supplied by the separately reviewed FxScheduler commit `67360de`, with unit tip `187988dc25ab3310367e68165285caed62795526`. That pending source was inspected read only; it must merge before this unit. The FxSystem worktree base still contains the older identity-basis fallback. No neighboring scheduler file was changed here.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built successfully with two low-priority jobs and compiled the actual `Fxexport.c` production source. Its existing axis-pointer type warning remains; no new warning was introduced in this unchanged file. Triage/constant scripts and hand comparison of wrappers plus orientation overloads were completed. The inactive-guard comparison found no change in five legacy configurations.

No tests, replays, full CI or gameplay sessions were run. The scheduler dependency was inspected but not executed in this worktree; integration owns combined runtime validation. The comparison covers the native arm64 Apple SDK path, not other native platform paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-fx-system.noindex/`.
