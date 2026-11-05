# universal/dvar module reconciliation

Updated 2026-11-04.

**124/130 reconciled: 96 equal, 7 fixed, 21 intentional; 6 unchecked.** All complete Mac dvar bodies were read, including the real static symbols omitted from the initial ledger pairing. The type dispatch tables were decoded directly from the binary. Linux 1.3 was used to identify helper names and inline operations; Mac 1.3 remains the authority.

## Corrections

`Dvar_SetColorFromSource` formatted the components with `%g` and parsed them before converting to bytes. Mac 0x539fe, starting its color branch at 0x53a7c, converts the original floats. Native now clamps, multiplies by 255, adds 0.5 and floors those original components. `Dvar_SetColor` uses this corrected path.

The string paths of `Dvar_GetString`, `Dvar_GetFloat`, `Dvar_GetInt` and `Dvar_GetBool` consumed a `va` ring slot unnecessarily. Mac 0x51c56, 0x51d0e, 0x51dca and 0x51e80 read the stored string directly. Native now preserves that pointer and conversion order for string dvars. Its existing defined conversion fallback for incompatible non-string types remains; Mac reinterprets the raw scalar slot as a pointer on those invalid getter calls.

`Com_LoadDvarsFromBuffer` only skipped the rest of unknown lines. Mac 0x549ef–0x54a0c also skips the rest of recognized lines after reading one value. Native now follows that rule and includes the missing newline in the missing-dvar error header at 0x54948.

## Boundaries and unresolved entries

Existing native adaptations remain: bounded domain-description output instead of cursor escape after truncation; null guards; temporary string storage and delayed old-current release during string rebuilding; zero-initialized pointer-sized records; and the optional CoD2x 4096-dvar pool instead of the stock 1280. Each affected row describes the specific adaptation. The existing promotion cleanup at Mac 0x52c48/0x52c67 is preserved.

The original nine gaps were recorded below; three Mac dvar storage/varargs rows are now resolved by the native-boundary follow-up. The six Linux-only identities remain unchecked:

- `Dvar_SetVec2`, `Dvar_SetVec2ByName` and the Linux `Dvar_SetVec2FromSource` identity compare against type 4, then give the vec4 setter only two floats. Mac 0x54c16 reads the following saved-register words; the ByName branch at 0x551d0 reads additional uninitialized locals. The native source has the same two-float shape, but the extra values have no established native stack contract. No indeterminate storage was added to imitate them.
- `Dvar_SetColorByName` passes four integers to four `%g` conversions on its missing-name path at Mac 0x557fb. The double reads exceed the supplied argument area. Native has the same format/type mismatch on a different variadic ABI. The existing-name color path was compared, but equality of the missing-name output is unresolved.
- The Linux `Dvar_GetVec2/3/4` APIs have no native definitions or callers. The related Mac scalar getter, color getter and temporary-vector conversion paths were searched and compared, but they do not establish those name-to-vector-return identities or their fallback contract.
- Linux `Dvar_SaveDvars` and `Dvar_LoadDvarsFromBuffer` use binary name-NUL/value-NUL pairs. Mac `Com_SaveDvarsToBuffer` and `Com_LoadDvarsFromBuffer`, used by the actual shellshock/sun consumers, use required-name lists and quoted text. No matching binary-blob path or native callers were found; these rows are not treated as aliases of `Com_*`.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with `taskpolicy -b nice -n 19` and two jobs. The existing F1 domain/promotion/lifetime fixture passed with ASan/UBSan in stock and CoD2x modes. The existing fidelity dvar command fixture passed with zero failures. No tests were added or changed, and no full CI or replays were run.

The corrected color, getter and load paths were compiled and reviewed but not exercised. The legacy OFF preprocessed bodies match the assigned base in all five OFF configurations; neither changed source uses `__LINE__`. No platform-specific behavior requiring a Windows decision was found.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-dvar.noindex/`.

## Routed identities

Separately indexed Mac rows moved from universal headers were resolved only where the exact symbol/address and existing complete-body audit established identity with an already reconciled Linux-indexed row. The source matches that audited implementation. Both IDs and original provenance remain; these are duplicate work items, not new implementations. Current module counts: 96 equal, 7 fixed, 18 intentional, 9 unchecked.

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.


## Defined native boundary follow-up

See [native-invalid-boundaries](native-invalid-boundaries.md) for the exact native contracts, original addresses, source corrections and limits. Earlier open statements for the six named Mac rows describe the prior checkpoint. Current module counts are 96 equal, 7 fixed, 21 intentional, 6 unchecked. All other original identity gaps remain open.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
