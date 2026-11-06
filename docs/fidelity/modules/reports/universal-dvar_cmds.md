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

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
