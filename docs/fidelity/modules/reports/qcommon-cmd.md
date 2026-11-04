# qcommon/cmd module reconciliation

Updated 2026-11-04.

**32/36 reconciled: 25 equal, 7 intentional, 4 unchecked.** Mac 1.3 i386 is the authority. No engine code changed in this module.

The review covered buffer append and insertion, immediate and queued execution, quote and line splitting, the 4095-byte line limit, wait handling, tokenization, command lookup and dispatch, registration and removal, completion metadata, argument access and listing. `Cbuf_InsertText` was compared in its Mac `Cbuf_ExecuteText` inline block at 0x4f1a4. The Linux helper names `Cmd_FindCommand` and `Cmd_ExecFromDisk` were compared with the corresponding search loops and disk execution block in the Mac callers, rather than treated as separate exports.

The existing native boundaries remain intentional: unsigned argument bounds reject negative indices, dedicated builds omit client dispatch and print unknown commands, PunkBuster callbacks are unsupported, and CoD2x uses its bounded tokenizer and competitive wait restriction. The valid argument operations and the stock tokenizer were read against Mac; the alternate policy paths are not claimed equal.

Four identities remain unchecked: `Cmd_ArgsBuffer`, `Cmd_Echo_f`, `Cmd_EmptyStub`, and `SV_Cmd_TokenizeStringWithLimit`. None has a unique Mac symbol or established inline counterpart. The native argument-joining convenience wrapper exists; the other three names have no native body. The Mac initialization routine at 0x4f508 registers only `cmdlist`, `exec`, `vstr` and `wait`, so it does not establish an identity for the Linux echo function or unnamed empty stub.

## Verification and limits

The assigned call/string triage and constant checks ran, followed by manual comparison of the identified functions and inline ranges. The existing tokenizer fixture passed with ASan/UBSan, reporting zero failures. The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. No new tests, full CI, game execution or replays ran. This does not establish an identity for the four unchecked rows or reconcile external filesystem and game-command implementations.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-cmd.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
