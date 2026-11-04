# universal/com_shared module reconciliation

Updated 2026-11-04.

**7/7 reconciled: 5 equal, 2 fixed.** The two fixed rows describe `Com_Filter` and its inlined `Com_StringContains` helper. Mac 1.3 is the authority; legacy source is byte-for-byte unchanged after removing `COD2_X64` branches with `unifdef`.

## Corrections

After `*`, Mac `Com_Filter` at 0x47f84 compares each remaining name suffix with the complete literal chunk. Its function-pointer slots at 0x1accd75 and 0x1accd79 resolve to `I_strcmp` at 0x2937c and `I_stricmp` at 0x295ee. Native incorrectly used length-limited prefix comparisons. This preserves the Mac quirk: `*a*` does not match `ab`, although a conventional glob implementation would accept it.

Bracket handling also differed. Mac 0x48144 rejects a closing `]` unless it is doubled; native reversed that condition. After a successful match, native advanced filter/name twice; Mac 0x48107 through 0x48117 advances once. Mac 0x481e4 permits `]` as a range endpoint when another `]` follows. Examples from those branches: `[a]z` must reject `ax`, `[]]` accepts `]`, and `[A-]]` accepts `Z`.

The entire Mac wildcard/case/bracket flow was compared. The Linux helper row is the inline suffix-search loop at 0x47ffa and the match continuation at 0x48254, not a separate Mac body.

## Verification and limits

`Com_FilterPath` matches the Mac's 63-byte truncation and slash conversion; hash signed-byte weighting and shifts, nine-field local-time copy, memcpy and memset argument order also match. The full-binary disassembler misaligned the `Com_HashKey` prologue, so its exact-symbol disassembly was used.

The arm64 Release dedicated target builds. Triage, constant checks, legacy-source comparison and `git diff --check` ran. The filter corrections are compiled and reviewed against Mac, not exercised. No new tests, full CI or replay were run.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-memory.noindex/`.
