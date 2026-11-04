# universal/assertive module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** Hand-compared Mac `RefreshQuitOnErrorCondition` at 0x7768c and `QuitOnError` at 0x776d0 with the native inline refresh helper and public wrappers.

An inactive dvar system preserves the cached result. When active, `QuitOnError` short-circuits the `r_vc_compile == 2` query, and the combined condition updates the cached boolean. `QuitOnError` then returns that cache. The dvar names, constant, condition order and stores match; triage differences come from the native helper being inline.

No source change was needed. The arm64 Release dedicated target builds. Triage and constant checks ran; these error-policy branches were reviewed, not exercised. No new tests, full CI or replay were run.
