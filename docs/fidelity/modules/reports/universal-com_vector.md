# universal/com_vector module reconciliation

Updated 2026-11-04.

**0/15 reconciled; 15 unchecked.** The native `com_vector.h` files contain types and declarations, with no executable vector implementation. Every assigned Mac row is a complete caller defined in another source file. Inline vector arithmetic cannot establish equality for those whole functions.

Examples include `CG_AddViewWeapon` at 0x1da2f8 in `cgame_mp/cg_weapons.c`, `CM_PointTraceToEntities_r` at 0xb017a in `qcommon/cm_world.c`, `FxScheduler_PlayEffect` at 0x61322 in `EffectsCore/FxScheduler.c`, and `RB_DrawStretchPic` at 0xd8148 in `gfx_d3d/rb_backend.c`. All 15 rows have exact native source/module/function routing in the private `universal-math.routing.json`. No ledger rows were moved and no caller code was changed.

Triage cannot preprocess this header independently because it has no compile-database entry. The complete callers remain unchecked pending their actual owning-module audits. The math worktree's arm64 Release dedicated-server target builds; that does not verify these client and renderer functions.

## Ownership correction

15 unchecked whole-caller rows were moved to their actual engine modules after integration. Original Mac attribution, IDs and unchecked verdicts are preserved. The preceding audit totals describe the original assignment. This module now holds 0 rows: 0 equal, 0 fixed, 0 intentional, 0 unchecked. The destination owners must audit each complete caller; matching a shared inline helper does not certify it.
