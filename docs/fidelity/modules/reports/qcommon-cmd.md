# qcommon/cmd module reconciliation

Updated 2026-11-04.

**33/36 reconciled: 26 equal, 7 intentional, 3 unchecked.** Mac 1.3 i386 is the authority. No engine code changed in this module.

The review covered buffer append and insertion, immediate and queued execution, quote and line splitting, the 4095-byte line limit, wait handling, tokenization, command lookup and dispatch, registration and removal, completion metadata, argument access and listing. `Cbuf_InsertText` was compared in its Mac `Cbuf_ExecuteText` inline block at 0x4f1a4. The Linux helper names `Cmd_FindCommand` and `Cmd_ExecFromDisk` were compared with the corresponding search loops and disk execution block in the Mac callers, rather than treated as separate exports.

The existing native boundaries remain intentional: unsigned argument bounds reject negative indices, dedicated builds omit client dispatch and print unknown commands, PunkBuster callbacks are unsupported, and CoD2x uses its bounded tokenizer and competitive wait restriction. The valid argument operations and the stock tokenizer were read against Mac; the alternate policy paths are not claimed equal.

Three identities remain unchecked: `Cmd_ArgsBuffer`, `Cmd_Echo_f`, and `Cmd_EmptyStub`. None has an established complete Mac counterpart. The native argument-joining convenience wrapper exists; the other two names have no native body. The Mac initialization routine at 0x4f508 registers only `cmdlist`, `exec`, `vstr` and `wait`, so it does not establish an identity for the Linux echo function or unnamed empty stub.

## Verification and limits

The assigned call/string triage and constant checks ran, followed by manual comparison of the identified functions and inline ranges. The existing tokenizer fixture passed with ASan/UBSan, reporting zero failures. The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. No new tests, full CI, game execution or replays ran. This does not establish an identity for the three remaining unchecked rows or reconcile external filesystem and game-command implementations.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-cmd.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.

## Complete server-tokenizer operation — 2026-11-05

`SV_Cmd_TokenizeStringWithLimit` is now equal through the complete existing
Mac/native `Cmd_TokenizeString2` operation. The former rejection considered
only the zero-limit server specialization and did not establish this generic
provider mapping. Original Linux 1.0 address provenance places the client/server
sibling wrappers at 0x080603cc/0x080603fc. The original 1.3 ELF independently
locates them at 0x080605d0/0x08060600: both forward the same two arguments,
shared argv/storage and count destination to the same tokenizer. The server
caller at 0x0806064c targets the latter with limit zero. These original anchors
establish duplicate-wrapper identity, not Linux behavioral authority.

Authoritative Mac `Cmd_TokenizeString2` at 0x4eac6–0x4eaea reads the supplied
text and arbitrary limit, forwards them with `cmd_argv` and `cmd_tokenized` to
`Cmd_TokenizeStringInternal`, then commits its result to `cmd_argc`. Native
`src/PC/qcommon/cmd.c:455–458` implements that complete operation. There is no
separate server token context: Mac `SV_Cmd_TokenizeString` at
0x4efee–0x4f012 and its native counterpart use those exact shared owners with
limit zero. The arbitrary-limit proof comes from the complete generic provider,
not extrapolation from the zero-limit caller or the non-executable
`SV_Cmd_TokenizeString2` exception marker. No new public alias is needed.

The previously documented CoD2x tokenizer-core policy remains separately
intentional; no new policy decision or whole-core equality is claimed. The
other three missing identities remain unchecked. Parent and independent
source/binary reviews support this closure; no source changed, compilation,
tests, CI or replay ran, and no runtime coverage was added. Private original
identity anchors and exact Mac bodies remain in
`CoD2-native-decomp/recon-provenance-parent4.noindex`.

## Remaining original command anchors

Original Linux 1.0 anonymous bodies 0x08060118 and 0x0805ff92 correspond to the 1.3 argument-copy wrapper at 0x0806031c and echo loop at 0x08060196. The copy operation joins from argument one and then copies to the supplied destination/length. Original Mac argument joining is established separately, but a reliable-command writer, menu-name consumer or console-say concatenation does not establish the whole destination-copy wrapper. The native caller alone supplies no original Mac identity. The candidate empty original 1.3 body at 0x0805fc5c lies between a four-word copy helper and `Cmd_Wait_f`; its empty body and reconstruction placement do not establish its unknown original name or a Mac counterpart. All three remain unchecked. Exact original bodies are retained privately in `recon-provenance-parser5.noindex`; no source or resolved verdict changed and no compilation, tests, CI or replay ran.

## Complete argument-operation follow-up (parser12)

The three helpers remain unchecked. Original Cmd_ArgsBuffer0x806031c composes argument joining from index1 with caller-supplied destination/length copying; a generic copy provider or joined-text consumer alone leaves part of the operation unproved. Current menu, reliable-command and console-concatenation owners have different output/state contracts. Original Cmd_Echo_f0x8060196 prints each argument followed by a space, then a separate newline, not the same print sequence as a joined string. The unknown empty helper still has no positive identity; no new absence scan or empty-body closure was used.

Private original evidence remains in recon-provenance-parser5.noindex and recon-provenance-config7.noindex; new complete provider packets are in recon-provenance-parser12.noindex. No production source changes, compilation, tests, CI, replay or runtime exercise occurred.
