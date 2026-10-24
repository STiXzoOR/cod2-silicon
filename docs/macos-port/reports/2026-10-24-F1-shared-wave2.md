# F1 shared-code follow-up

This batch starts from `6c6edd0` and fixes five bounded areas. It does not
complete the shared-module reconciliation or establish full Mac parity.

- [Entity fields](2026-10-24-F1-entity-fields.md): restore native transform,
  health and read-only callbacks; preserve missing string/entity field results.
- [World spawning](2026-10-24-F1-world-spawn.md): initialize the script field
  registry before consuming BSP fields. Deathmatch now removes the 37 Toujane
  entities reserved for other gametypes.
- [Script recovery](2026-10-24-F1-error-recovery.md): preserve interpreter state
  across builtin and entity-field setter errors; release arguments and partial
  results before resuming. This addresses the stack-growth route to the packet
  crash without disabling notify handling or hiding script errors.
- [Dvars](2026-10-24-F1-dvar.md): restore Mac NaN/domain and enum lookup behavior.
- [Transport](2026-10-24-F1-transport.md): restore the Mac 128 KiB message limit.

## Behavioral results

The original frame-4 mismatch is resolved: both servers now have 168 active
entities and quantized hash `de316057`. The original Linux raw hash is
`d37849ea`; native is `69ef7e36`. Quantized agreement is not raw-float equality.

Both stock and CoD2x replays complete 2,000 frames (4–2003), with 3,776 live
player records starting at frame 116 and no script runtime errors. The first
quantized difference from Linux has moved from frame 4 to frame 116. Raw player
fields first differ by one velocity bit at spectator frame 102. At frame 116,
Linux velocity includes `(100,12,0)` where native has `(99,11,0)`. Mac
`Sys_SnapVector` uses truncation, while Linux uses rounding; native already
matches the Mac operation. A separately labeled oracle experiment is needed
to establish its contribution to later divergence. The stock oracle is intact.

After review corrections, the CoD2x replay again completes 2,000 frames and
matches the earlier candidate's complete quantized state and raw player fields.
The unchanged installed test-client menu helper, which selects an invalid
weapon on British Toujane, also completes 2,000 frames on stock: 3,237 live
player records and 14 expected script runtime errors, without a crash. The
normal valid-weapon replay has zero errors. Runtime-error counts come from
`home/main/qconsole_mp.log`, which is more complete than captured stdout.

Valid menu-driven Carentan smokes complete 600 frames on both stock and CoD2x,
with 784 live player records and no script runtime errors. Those smokes precede
the final error-recovery review corrections; the focused tests and Toujane runs
above cover the final corrections.

## Checks

Stock and CoD2x client/dedicated targets build. The 13 compiler/VM recovery cases
pass ASan/UBSan in both configurations, and all 20 existing runtime cases pass
with CoD2x, including the production StopGameType error/drop fixture. The full
LP64 game fixture suites and fidelity fixtures pass in both configurations.
Focused transport and dvar tests pass, including the existing dvar-lifetime
fixture. Each engine file's `unifdef -UCOD2_X64` output matches the base; the
legacy configuration comparator reports zero mismatches across 10 configurations.
The changed translation units pass raw-offset and function ABI checks in both
configurations; callback checks report zero new mismatches. The initial broad
raw-offset scan flagged a new stack expression, which now uses the existing
`VM_PushValue` helper. A whole-tree function audit was stopped in favor of the
changed-source audit; these are bounded checks, not a new full-tree ABI claim.

A completed code review found partial builtin-result cleanup and newly reachable
read-only field errors; both findings are fixed and independently checked against
the expanded fixtures. Review run: `20261024-wave2-xatgbtmk`. The attempted Claude
review exhausted its turn budget; the completed review used the skill's fresh
local adversarial fallback. No unresolved finding remains in this batch.

Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F1-shared-wave2.noindex/`, with
focused VM evidence under `F1-error-recovery.noindex/`. No reference source,
assets, private traces or disassembly are committed. The Linux OFF binary
comparison remains unavailable; source preprocessing is a narrower check.
The known legacy ABI, inactive CoD2x and Metal stub baseline limitations remain
separate from these focused passes. General opcode recovery, full map-restart
lifecycle reconciliation and remaining replay divergence are still open.
