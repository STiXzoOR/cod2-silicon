# universal/dvar_cmds module reconciliation

Updated 2026-11-04.

**28/28 reconciled: 23 equal, 1 fixed, 4 intentional.** All complete Mac bodies were read. The Mac symbols for the combined-string helper, toggle helper, registration handlers, reset, list and dump commands were recovered even where the ledger only named Linux entries. The simple-toggle type table at 0x328aa0 was decoded directly from the binary.

## Correction

`Com_DvarDump` now evaluates the latched display value before the current display value, as Mac 0x50595 and 0x50662 do. Its summary now passes the traversal count before `dvarCount`, following Mac 0x505f2. Native had reversed those arguments. The counts normally agree, but the argument order is preserved exactly.

## Boundaries

`PBdvar_set`, `PbDvarWalk` and `PbDvarValidate` were read at Mac 0x5077c, 0x507ae and 0x50816. They remain absent as bundled PunkBuster callbacks at the documented `pb_null_adapter.c`/WS44 boundary; no native definitions or callers remain. Ordinary engine dvar lookup, mutation, traversal and validation were audited separately. Native dedicated `Dvar_InfoString` retains its omission of the client username override; the client path matches Mac 0x508d8.

The empty `Dvar_GetCombinedString` ABI shell is not the active implementation: native `Dvar_Command` and `Dvar_Set_f` call `Dvar_GetCombinedString_impl`, compared with the complete Mac body at 0x4f798. Full-binary disassembly misaligned the `Dvar_ForEach` prologue; restarting at its exact mangled symbol established the correct entry at 0x4f76c.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. The existing fidelity dvar command fixture passed with zero failures; the existing F1 domain/promotion/lifetime fixture also passed in stock and CoD2x modes with ASan/UBSan. No tests were added or changed. The dump correction was compiled and reviewed but not exercised. No full CI or replay was run.

The OFF preprocessed source matches the assigned base in all five OFF configurations, and the changed file has no `__LINE__` expansion. No entries remain unchecked in this module.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-dvar.noindex/`.
