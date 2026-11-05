# qcommon/msg_mp module reconciliation

Updated 2026-11-04.

**55/61 reconciled: 46 equal, 6 fixed, 3 intentional; 6 unchecked.** Mac 1.3 is the authority. All 61 entries have verdicts. The legacy preprocessed source is unchanged.

## Corrections

**A missing remove bit was treated as removal.** At Mac 0x174321, `MSG_ReadDeltaStruct` sets overflow and continues delta handling. Native treated the read-bit result -1 as true. The native path now requires exactly 1 for removal. Its existing unsigned field-count check still rejects the missing count safely.

**The public bit writer used a different loop condition.** Mac 0x173238 checks for zero and 0x17327b terminates when the index equals the requested bit count. Native used a less-than comparison; its inline core already used equality. The public native wrapper now follows Mac, including its behavior for invalid negative counts.

**Client name metadata used a different sentinel.** All eight name words in Mac `clientStateFields` at 0x36d680 use -98. Native used 32. Both select unsigned 32-bit payloads in the current codecs; the table now preserves the Mac sentinel.

**Archived field diagnostic names included extra prefixes.** Mac `archivedEntityFields` at 0x36d7a0 uses names such as `absmin[1]` and `pos.trBase[1]`. Native included the enclosing `r.` or `s.`. Native now omits those prefixes without changing offsets.

The two metadata corrections cover both read and write wrapper ledger entries. All changes are guarded by `COD2_X64`.

## Comparison

The bit/byte primitives, string cleanup, compression wrappers, default command, generic deltas, HUD deltas, both usercmd codecs and both playerstate codecs were read against the Mac disassembly. Inline helper rows were checked in their callers. Examples include the usercmd extra-field key selection at 0x1764c3, unsigned-short angle recovery at 0x1759b7, the fourth ammo-mask test at 0x179df5, and archival-before-current HUD ordering at 0x1753ba and 0x179633.

A script compared the compiled native tables with the binary: names, offsets and widths for 105 player fields, 22 client fields, 68 archived fields, 59 entity fields, 32 HUD fields and six objective fields, plus all 256 Huffman frequencies. Every final record matches. Triage and the constant checker were also run; neither was treated as proof of semantic equality.

The three intentional entries retain existing malformed-input hardening: bounded decompression, clamping a missing HUD element count before clearing, and rejecting playerstate field counts beyond the table. `MSG_ReadDeltaStruct` also retains its existing unsigned field-count guard; that row is marked fixed for the separate remove-bit correction.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. Both changed source files were preprocessed with `COD2_X64` undefined and compared with the base commit; the outputs are identical. The corrections are compiled and reviewed, not runtime exercised. No new tests, CI suite or replays were run.

Six Linux-only interfaces remain unchecked: `MSG_GetUsedBitCount`, `MSG_ReadDeltaKeyByte`, `MSG_WriteDeltaKeyByte`, `MSG_WriteAngle`, `MSG_ReadInt64` and `MSG_WriteInt64`. There is no body or call in native source and no matching Mac text symbol. An exact Mac inline identity was not established; no implementation or equality was inferred from Linux.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-msg.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.

## Remaining-interface audit, 2026-11-05

The native source and test tree has no definition, declaration or caller for the six names. Mac symbol and STABS inspection found only zero-address absolute `.eh` names for `MSG_GetUsedBitCount`, `MSG_WriteAngle` and both byte-key helpers; neither Int64 name has that metadata. None has an executable or `FUN` identity.

The candidate usercmd codecs at 0x1757bc–0x17640a and 0x17640a–0x1778de use short-angle and 4/18/7-bit keyed payloads. The seven-bit loops at 0x175f35/0x175fd2 and counts at 0x176e91/0x176f50 do not identify a byte-helper contract. The four-byte primitives at 0x1735fa and 0x1739b2 establish their own bounds and cursor effects, but do not establish the ordering, partial transfer or failure semantics of an eight-byte operation. No exact consumer for the used-bit-count or byte-angle interface was identified.

All six entries remain unchecked. No production defect was established, and no source or existing resolved verdict changed. Missing executable identities are not a claim that no inlined operation could exist elsewhere. No behavior was reconstructed from Linux; names and signatures served only as the search scope.

Stock and CoD2x Release objects for both translation units compiled from their configured client commands. Existing message and transport fixtures and the four existing message/Huffman regression inputs passed in both configurations under low-priority execution with at most two build jobs. These checks exercise the existing codecs, not the absent standalone interfaces. No tests, cases or assertions were added; no full CI or replay ran. Source is byte-identical to base a52e5e27175e86cf0ee0d6dc7c571ab848261ac5, so legacy/OFF diagnostics and line expansions are unchanged.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-message-remaining.noindex/`.


## Original helper provenance

Retained original1.0 IDB bytes now anchor all six remaining operations in original1.3: used-bit count at 0x8067c3e, byte-angle write at 0x806831c, byte-key write/read at 0x80687fa/0x8068838, and Int64 write/read at 0x8068138/0x80684ac. The original byte-key writer compares low bytes and emits a separate byte payload; its reader truth-tests the change bit, reads/XORs/truncates a byte, and otherwise returns the old full value. These complete contracts remain distinct from seven-bit usercmd paths. The byte-angle body has separately verified 256/360 constants; its Linux x87 arithmetic is identity evidence only.

The original Int64 writer checks eight-byte capacity once and commits cursize after both payload words. Its endian callback chain is now traced through original initialization and Long64NoSwap. Fresh exact Mac Swap_Init at 0x292fe and Long64NoSwap at 0x2850a corroborate the initialized little-endian representation. Exact Mac MSG_WriteData at 0x173554 provides a compatible generic pointer/count operation, but no observed scalar-by-value adapter identifies the complete Int64 API. That gap remains open after independent review. The reader has a separate concrete distinction: original overflow returns zero without cursor advance, while Mac MSG_ReadData at 0x173626 fills the output with FF. Neither hypothetical composition closes the row.

All six remain unchecked. This follow-up adds original body/caller evidence only; no source, settled verdict, native hardening or runtime claim changed. No compilation, tests, CI or replay ran.
