# universal/dvar module reconciliation

Updated 2026-11-04.

**129 entries: 95 equal, 7 fixed, 18 intentional, 9 unchecked.** All complete Mac dvar bodies were read, including the real static symbols omitted from the initial ledger pairing. The type dispatch tables were decoded directly from the binary. Linux 1.3 was used to identify helper names and inline operations; Mac 1.3 remains the authority.

## Corrections

`Dvar_SetColorFromSource` formatted the components with `%g` and parsed them before converting to bytes. Mac 0x539fe, starting its color branch at 0x53a7c, converts the original floats. Native now clamps, multiplies by 255, adds 0.5 and floors those original components. `Dvar_SetColor` uses this corrected path.

The string paths of `Dvar_GetString`, `Dvar_GetFloat`, `Dvar_GetInt` and `Dvar_GetBool` consumed a `va` ring slot unnecessarily. Mac 0x51c56, 0x51d0e, 0x51dca and 0x51e80 read the stored string directly. Native now preserves that pointer and conversion order for string dvars. Its existing defined conversion fallback for incompatible non-string types remains; Mac reinterprets the raw scalar slot as a pointer on those invalid getter calls.

`Com_LoadDvarsFromBuffer` only skipped the rest of unknown lines. Mac 0x549ef–0x54a0c also skips the rest of recognized lines after reading one value. Native now follows that rule and includes the missing newline in the missing-dvar error header at 0x54948.

## Boundaries and unresolved entries

Existing native adaptations remain: bounded domain-description output instead of cursor escape after truncation; null guards; temporary string storage and delayed old-current release during string rebuilding; zero-initialized pointer-sized records; and the optional CoD2x 4096-dvar pool instead of the stock 1280. Each affected row describes the specific adaptation. The existing promotion cleanup at Mac 0x52c48/0x52c67 is preserved.

Nine entries remain unchecked for precise reasons:

- `Dvar_SetVec2`, `Dvar_SetVec2ByName` and the Linux `Dvar_SetVec2FromSource` identity compare against type 4, then give the vec4 setter only two floats. Mac 0x54c16 reads the following saved-register words; the ByName branch at 0x551d0 reads additional uninitialized locals. The native source has the same two-float shape, but the extra values have no established native stack contract. No indeterminate storage was added to imitate them.
- `Dvar_SetColorByName` passes four integers to four `%g` conversions on its missing-name path at Mac 0x557fb. The double reads exceed the supplied argument area. Native has the same format/type mismatch on a different variadic ABI. The existing-name color path was compared, but equality of the missing-name output is unresolved.
- The Linux `Dvar_GetVec2/3/4` APIs have no native definitions or callers. The related Mac scalar getter, color getter and temporary-vector conversion paths were searched and compared, but they do not establish those name-to-vector-return identities or their fallback contract.
- Linux `Dvar_SaveDvars` and `Dvar_LoadDvarsFromBuffer` use binary name-NUL/value-NUL pairs. Mac `Com_SaveDvarsToBuffer` and `Com_LoadDvarsFromBuffer`, used by the actual shellshock/sun consumers, use required-name lists and quoted text. No matching binary-blob path or native callers were found; these rows are not treated as aliases of `Com_*`.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with `taskpolicy -b nice -n 19` and two jobs. The existing F1 domain/promotion/lifetime fixture passed with ASan/UBSan in stock and CoD2x modes. The existing fidelity dvar command fixture passed with zero failures. No tests were added or changed, and no full CI or replays were run.

The corrected color, getter and load paths were compiled and reviewed but not exercised. The legacy OFF preprocessed bodies match the assigned base in all five OFF configurations; neither changed source uses `__LINE__`. No platform-specific behavior requiring a Windows decision was found.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-dvar.noindex/`.
