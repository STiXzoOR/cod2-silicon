# qcommon/dvar_cmds module reconciliation

Updated 2026-11-04.

**10/10 assigned entries reconciled: 5 equal, 5 fixed.** Mac 1.3 i386 is the authority. The changes apply only to COD2_X64; the legacy source is unchanged.

## Corrections

**The joined-value cutoff counted the wrong bytes.** Mac `Dvar_GetCombinedString` starts its length scan at the second byte of each argument, at 0x4f7d1, and accumulates that length before the 0xffd cutoff at 0x4f7e3. The live native helper, `Dvar_GetCombinedString_impl`, counted the entire argument. It now preserves the Mac scan, including its unusual treatment of an empty argument. Concatenation still uses the existing 4096-byte capacity. The unused no-argument stub is not the implementation compared here.

**Float and integer toggles chose the wrong values.** Mac 0x4facb and 0x4fad9 use the domain endpoints when the minimum is positive or the maximum is below one, including unordered float comparisons. Native used a different conjunction. For integers, Mac 0x4fb15, 0x4fb20 and 0x4fc27 switch between the minimum and maximum when the domain does not contain both zero and one. Native instead incremented positive domains or used a remainder for nonpositive domains. The decoded nine-entry jump table at 0x328aa0 confirms the bool, float, integer, enum and unsupported-type branches. The sequence match at 0x4f9e0 also passes the next argument directly to `Dvar_SetCommand`; native unnecessarily converted the next enum argument first.

**Numeric declaration commands re-registered existing numeric dvars.** Mac returns immediately for an existing boolean at 0x4fe0d, an integer or enum at 0x4fef2, and a float at 0x5001d. Native continued into registration and could alter flags, domain or latched state. Those early returns are restored. Missing variables and externally created strings still register with 0x4000; incompatible types still print the original diagnostics, including the float handler's integer wording.

The other assigned entries were read against Mac: dvar listing and its flag columns, source-to-destination assignment, reset with external source 1, the dump wrapper, and the toggle wrapper. This report covers the ten assigned ledger entries, not every other function present in the source file.

## Verification and limits

Call/string triage, constant checks, manual branch comparison and jump-table decoding supplied the reference evidence. The existing tokenizer and dvar fixtures passed with ASan/UBSan, each reporting zero failures. They exercise ordinary joining and declaration paths, but do not isolate the corrected cutoff, toggle branches or existing-numeric early returns. These corrections are compiled and reviewed, not independently exercised.

The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. `unifdef -UCOD2_X64` and `unifdef -DCOD2_X64=0` both produced byte-identical legacy source against the base revision, and `git diff --check` passed. No new tests, full CI, game execution or replays ran. No assigned entry remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-cmd.noindex/`.
