# universal/dvar module reconciliation

Updated 2026-11-04.

**127/130 reconciled: 99 equal, 7 fixed, 21 intentional; 3 unchecked.** All complete Mac dvar bodies were read, including the real static symbols omitted from the initial ledger pairing. The type dispatch tables were decoded directly from the binary. Linux 1.3 was used to identify helper names and inline operations; Mac 1.3 remains the authority.

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

Integrated source `32434b2` with ownership/evidence commit `ae8a191` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.


## Defined native boundary follow-up

See [native-invalid-boundaries](native-invalid-boundaries.md) for the exact native contracts, original addresses, source corrections and limits. Earlier open statements for the six named Mac rows describe the prior checkpoint. Current module counts are 96 equal, 7 fixed, 21 intentional, 6 unchecked. All other original identity gaps remain open.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.


## Original identity follow-up — provenance-config7

Original Linux 1.0 IDB addresses were mapped to retained original 1.3 ELF bodies using source order, independent decompiler anchors and masked whole-body comparisons. Every unique whole-body match agrees with the mapped module order; ambiguous tiny bodies and changed implementations were located from neighboring unique anchors and inspected separately. An ambiguous non-target `FS_Path_f` comparison was not used as an anchor. The private packet retains the mapping alternatives and exact bodies. These addresses establish original identity and intent only; Mac i386 remains behavior authority.

| Helper | Original 1.0 | Original 1.3 | Verdict |
|---|---|---|---|
| `Dvar_GetVec2` | 0x80b0cac | 0x80b30a0 | unchecked |
| `Dvar_GetVec3` | 0x80b0cfa | 0x80b30ee | unchecked |
| `Dvar_GetVec4` | 0x80b0d48 | 0x80b313c | unchecked |
| `Dvar_LoadDvarsFromBuffer` | 0x80b2d56 | 0x80b52ee | unchecked |
| `Dvar_SaveDvars` | 0x80b2c82 | 0x80b5202 | unchecked |
| `Dvar_SetVec2FromSource` | 0x80b20f0 | 0x80b4598 | unchecked |

The positive original caller Dvar_SetVec2 0x80b49ee passes source 0 at call 0x80b4a10. The complete 1.3 helper forwards its arbitrary source argument to SetVariant and brackets that call with critical section 4; the exact lock providers 0x80d6842/0x80d6864 invoke pthread mutex lock/unlock. Mac SetVec2 0x54bba and ByName 0x550f6 establish only source-0 operations without that wrapper lock contract. The previously approved native z/w-zero policy remains settled and preserved; it does not resolve arbitrary-source forwarding or ownership/locking. This row remains unchecked. The three vector getters still lack complete Mac name-lookup/default/return-storage identities. Original SaveDvars/LoadDvarsFromBuffer use name-NUL/value-NUL blobs; the Mac named-text Com_Save/Load operations remain distinct. All six rows remain unchecked.

No production source changes, defects, compilation, tests, CI or replays in this follow-up. No runtime behavior is certified. Private evidence: `recon-provenance-config7.noindex`; focused ledger preserves all previously resolved rows unchanged.

## Complete-provider boundary review (config13)

All six remaining rows stay unchecked. The vector getter bodies require name/type dispatch, missing-origin fallback and the lifetime/ownership of the returned vector storage; scalar/color conversion or a direct current-value read is incomplete. The original matching-type path returns the current pointer word. The earlier inference that native vector fields were inline was incorrect: the active DvarValue.vector field is a pointer; the dvar42 follow-up below establishes its actual borrowed-pointer consumer. This checkpoint had not yet established that complete named acquisition role. Existing ownership adaptations remain settled. Binary name-NUL/value-NUL serialization is still a different complete representation from named text save/load. The arbitrary-source setter's section4 lock and source forwarding remain unresolved; the approved z/w-zero policy is preserved. No dvar source changes, new compilation or runtime verification were performed.


## Complete named vector acquisition — dvar42

The three GetVec2/3/4 identities are now equal for the complete found-name, matching-type acquisition consumed by the actual formatter. Original1.3 complete bodies at0x80b30a0/0x80b30ee/0x80b313c return the borrowed current-vector pointer on matching type; all77 bytes through each RET were independently read. Their missing-origin and wrong-type conversion variants are not mapped to the selected Mac role. No arbitrary exported getter is claimed.

Fresh full Mac GetVariantString0x51bb2 performs the folded hash/name-chain lookup, loads current's full pointer at0x51c2c and tail-passes it to ValueToString. Its raw type table selects0x50ccf/0x50c9d/0x50c60 for2/3/4 components. Each block consumes every component through that same pointer. The later formatted string is a consumer result, not a substitute for the pointer acquisition. Actual functions[18] binds getcvar to GScr_GetDvar0x197968; this owner gets the name from string argument0 and passes the formatted result to Scr_AddString. Cmd_ExecuteString's admitted Dvar_Command query supplies an independent actual named query and full result consumer.

Successful registration provides real typed backing: CG_Init's cg_hudGrenadePointerPivot, R_RegisterSunDvars' r_sun_fx_position and Con_OneTimeInit's con_inputBoxColor call the corresponding typed register providers. Their full initializer arrays reach the new-vector arms, which allocate dimension*3 floats, assign current/latched/reset pointers, copy all elements and insert the named record into the hash. These are concrete producer spans, not a new audit of whole initializers or allocator/error policy.

The active include chain goes through common_types.h to headers/cod2_types.h and cod2_defs.h. DvarValue.vector is vec_t*, so the native by-value union argument copies the borrowed pointer, not the vector contents. Native preserves the same current backing and complete component consumer. No copy-elision assumption or new ownership adaptation is needed. The prior inline-vector statement is corrected above; the earlier absence/search checkpoints remain historical.

Scope is a valid non-null matching name, corresponding live vector type and backing through immediate consumption. No missing-name fallback, wrong-type conversion, independent lifetime, mutable-copy, racing mutation, trapping FP, exceptional formatted-text or arbitrary returned-error guarantee is added. Scalar/string getter repairs are outside this selected pointer acquisition, so these three entries are equal with no repair inheritance or new defect. The three remaining dvar identities stay unchecked, including arbitrary-source Vec2 locking/forwarding and binary name/value serialization; all approved z/w/storage policies are preserved.

Independent review: units/review-provenance-dvar42.md. Private exact bodies, original bytes, dispatch/format/name/actual function-binding receipts and source hashes: recon-provenance-dvar42.noindex. No source changes, compilation, tests, CI, replays or runtime verification.
