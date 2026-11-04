# universal/com_vector module reconciliation

Updated 2026-11-04.

**0/15 reconciled; 15 unchecked.** The native `com_vector.h` files contain types and declarations, with no executable vector implementation. Every assigned Mac row is a complete caller defined in another source file. Inline vector arithmetic cannot establish equality for those whole functions.

Examples include `CG_AddViewWeapon` at 0x1da2f8 in `cgame_mp/cg_weapons.c`, `CM_PointTraceToEntities_r` at 0xb017a in `qcommon/cm_world.c`, `FxScheduler_PlayEffect` at 0x61322 in `EffectsCore/FxScheduler.c`, and `RB_DrawStretchPic` at 0xd8148 in `gfx_d3d/rb_backend.c`. All 15 rows have exact native source/module/function routing in the private `universal-math.routing.json`. No ledger rows were moved and no caller code was changed.

Triage cannot preprocess this header independently because it has no compile-database entry. The complete callers remain unchecked pending their actual owning-module audits. The math worktree's arm64 Release dedicated-server target builds; that does not verify these client and renderer functions.

## Ownership correction

15 unchecked whole-caller rows were moved to their actual engine modules after integration. Original Mac attribution, IDs and unchecked verdicts are preserved. The preceding audit totals describe the original assignment. This module now holds 0 rows: 0 equal, 0 fixed, 0 intentional, 0 unchecked. The destination owners must audit each complete caller; matching a shared inline helper does not certify it.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
