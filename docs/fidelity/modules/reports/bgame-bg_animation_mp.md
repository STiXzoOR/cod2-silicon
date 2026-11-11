# bgame/bg_animation_mp module reconciliation

Updated 2026-11-04.

**44/48 reconciled: 39 equal, 5 fixed; 4 unchecked.** Mac 1.3 is the authority. All 22 Mac function bodies were read by hand, including the helpers inlined into the parser, player animation, controllers and loader. Linux source was used only to identify helper roles. No Windows-specific behavior was identified.

## Corrections

**Prone controller radians used float multiplication.** Mac 0x1830cd converts the yaw to double, multiplies by double pi/180, then converts to float before `sinf` and `cosf`. Native now follows that order. The arm64 object contains the corresponding float-to-double conversion, double multiply and double-to-float conversion.

**Movement conditions compared the wrong client record.** Mac 0x1843d3 compares the animation movetype against the supplied `ci` record before updating the indexed client conditions. Native compared against the indexed global record instead. `BG_UpdateEntityPlayerAnimConditions` now uses the supplied record.

**Parser scopes retained a stale script pointer.** Mac 0x185e95 clears the current script when closing a movement scope, and 0x185ee1 clears it when closing a state-change or event scope. Native now clears it at the same depths.

**Empty parser tokens selected different errors.** Mac 0x185cdd, 0x186400, 0x186495 and 0x1864c4 test for a null token before the equals/state lookup, allowing empty strings to reach the subsequent token error. Native also tested for an empty string and reported the earlier end-of-line error. These branches now match Mac.

All changes are under `COD2_X64`; the legacy text is preserved. With `COD2_X64` undefined, the original and edited translation units preprocess byte-identically with line directives omitted.

## Verification and limits

The isolated Release arm64 `cod2_macos_ded` target compiled and linked with two low-priority jobs. No new per-fix tests, CI run or replay was performed. All corrections are compiled and reviewed but not exercised.

The constant checker and call/string triage were run. A binary-data check matched all ten active string tables through their first null sentinel, the nine active condition type/table pairs, and the parser dispatch table at 0x33ba00. This is not a comparison of every byte or of alignment padding.

Six Linux-only ledger identities remain unchecked: `BG_AnimScriptStateChange`, `BG_AnimationForString`, `BG_GetAnimScriptEvent`, `BG_GetAnimString`, `BG_IsMantleAnim` and `BG_PlayAnimName`. They have no native body or caller, no caller in the private Linux reconstruction, and no Mac text symbol; `BG_GetAnimScriptEvent` has only an absolute exception-metadata symbol. No matching Mac inline path was positively identified, so absence was not treated as equality or an intentional omission.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-bg-animation.noindex/`.

## Integrated batch verification

Integrated source through `98acbdd` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The legacy guard check differs only for the three intended `linux-x86_64-port-on` source paths. An independent reviewer checked the changed source, nearby callers and Mac binary evidence and found no actionable defects; this was not a second whole-module audit.

Both 2,000-frame stock and CoD2x replays are byte-identical to the fresh `819d282` baseline for player, entity and state records. Against the Linux original, all 3,806 player records agree on the six logic fields; maximum origin drift remains 0.11792 units and velocity drift 2.0. The recording does not demonstrate the corrected branches, so those corrections remain compiled and reviewed rather than individually runtime-verified. No new per-fix tests were added.

## Remaining-helper follow-up — 2026-11-05

The six unchecked rows remain unchecked after reviewing the Mac symbol/STABS inventory, current native owners and exact event/index/parser code. Mac `animation_s` metadata establishes the 96-byte record, name/hash fields and 64-bit movement mask; `animScriptData_t` establishes event and state-change tables. These are storage contracts, not complete implementations of the absent helpers.

`BG_AnimScriptEvent` selects and executes a command; its shared selection prefix does not establish the query-only `BG_GetAnimScriptEvent` return contract. The latter has only a zero-valued exception symbol. Parser state-change tables do not establish state-change execution. `BG_AnimationIndexForString` has its own load/runtime lookup and parse-error contract, so it does not prove the pointer-returning lookup or play-name wrapper. Existing index checking and movement masks do not establish the missing string-range error or mantle-mask predicate. No code was invented from Linux definitions, and no prior verdict changed.

Configured stock/CoD2x Release objects and existing game-animation fixtures pass both modes. No production source, tests, cases or assertions changed in this module. No full CI or replay ran. Private follow-up evidence is in `recon-animation-remaining.noindex`.


## Original-address follow-up (inline-effects4)

The original Linux 1.3 range0x80d97ce–0x80d9805 (exclusive end), anchored by its exact range-error string, establishes BG_GetAnimString's original index-check/name-return identity. The original BG_IsMantleAnim body at0x80d9cac–0x80d9cff clears the animation toggle bit, resolves the animation and tests the movetype mask0x7ff00000. These Linux identities do not establish a corresponding complete Mac getter/predicate. A Mac semantic immediate search did not supply a new complete operation; missing constants, strings or direct calls are not closure evidence. All six unresolved rows remain unchecked. No source changes or runtime verification.

## Original selection and name-helper bodies

Four additional original bodies now have exact address anchors. Linux 1.0 0x80d4832 maps to 1.3 0x80d6f56 for the name-to-animation-pointer search. Its own error and null-return path differ from the exact Mac index provider at 0x184cd6, which also has a load-time insertion path. Original `BG_PlayAnimName` at 0x80d92a0 performs name-to-index lookup followed by `BG_PlayAnim` with duration zero. That concrete original two-call sequence still requires a complete Mac/native combined owner; it is not a strict alias to either separately verified provider.

Original state change at 0x80d9600 is mislabeled `BG_AnimScriptCannedAnimation` in the 1.0 database. Its actual table indexing, dead-state checks, random selection and execution flags identify the old-state/new-state operation. Original event lookup at 0x80d9920 instead returns a sign-extended animation index after selection. Exact Mac `BG_AnimScriptEvent` at 0x18729a calls `BG_ExecuteCommand` at 0x1873b1 and returns its execution result, so its selection prefix does not close the non-playing getter. All six remaining rows stay unchecked.

The complete original bodies, relocation receipts, fresh exact Mac candidates and targeted PPC source/owner comparison are retained in `CoD2-native-decomp/recon-provenance-animation10.noindex`. Static callback-pointer and direct-call inventories supply no completion argument. No source changes, compilation, tests, CI, replay or runtime exercise ran.

## Checked animation-name address and remaining mantle predicate (animstring31)

`BG_GetAnimString` is equal through the complete checked writable name-address operation in the actual loader. Full original1.3 body0x80d97ce–0x80d9804 ignores its client argument, checks unsigned index against the global animation count, invokes its parse-error provider on failure and returns the complete name-field pointer. The actual Mac BG_LoadAnim call0x186742 passes bgs's first-field animScriptData to the parser, which stores that input in globalScriptData0x185a00 (also after the first-load file path). The loader reads this same global0x1867d9 and sets its count from the actual XAnim tree. It does not assume that cached globalScriptData equals whichever bgs context becomes current later.

The entered loop starts at index1 and global+0x60, advances index/address together and performs the complete unsigned count check0x18683b–0x186849 before each selected use. The full EDI result is consumed as the actual writable name[64] destination by I_strncpyz at0x1868c0/0x1868c3,0x1869e6/0x1869e9 and0x186a3c/0x186a3f; primitive name hashing also traverses the resulting field. Native computes the typed animation/name address and supplies the same consumers, preserving native stride. A writable pointer consumer establishes the complete result; no prior contents read or hypothetical pointer adapter is required.

The error edge remains part of the evidence: Mac0x1869fa–0x186a14 calls Com_Error with the index/count range diagnostic, then0x186a19 continues after the check if it returns. Native retains that direct error and continuation. Original BG_AnimParseError's optional filename/parse-line formatting differs; this mapping follows the complete actual Mac guard/error/result role, not Linux diagnostic equivalence. Scope is stable serialized loader state, positive entered-loop index/count and valid512-record backing. Native signed comparison agrees on that actual domain; forged negative counts, global retargeting/concurrency/reentrancy, arbitrary returned-terminal-error state changes, standalone ABI/root0 mapping, whole loader or later string-copy/hash behavior are not certified. Normal producer indices do not provide invalid-index runtime coverage. This selected guard/address phase was already correct; unrelated parser/loader fixes are not inherited. The earlier death-loop diagnostic route remains insufficient to prove persistent runtime global/current-bgs identity.

`BG_IsMantleAnim` stays unchecked after a concrete complete-predicate comparison. Original0x80d9cac–0x80d9cfe clears toggle0x200, obtains the checked indexed animation and materializes Boolean membership in low movetype mask0x7ff00000. Full actual Mac RunLerpFrameRate candidates also clear the toggle, validate indices and materialize Boolean results, but select low0xc4/high0x180 at0x1837e1–0x1837fc and0x183819–0x18383a, or low0x308 at0x18384f–0x183862 and0x183874–0x183884. Those results compare old/new movement classes for stance transition; no complete mantle-role bridge is established. Movetype transfers or movement flags are not substitutes.

Independent review accepted the name-address scope in `units/review-provenance-animstring31-name.md`; parent also reviewed the proposal. Complete original bodies, fresh exact selected Mac owners, raw diagnostic strings, layout/source and review receipts remain in `recon-provenance-animstring31.noindex`. One closure and one unchecked refinement; no source change or new defect, compilation, tests, CI, full build, replay or runtime verification. All previous source, storage and ownership decisions remain intact.

## Complete found-name record lookup (animlookup48)

`BG_AnimationForString` is equal through the actual complete found-name-to-borrowed-record operation in the loader's second parse pass. Fresh original1.0 [0x80d4832,0x80d48c5) maps to original1.3 [0x80d6f56,0x80d6fe9), through RET0x80d6fe8; the following NOP is excluded from the earlier padded range. Its full hash, ordered nameHash/case-insensitive-name scan and matching full record-pointer return were read, including its separate direct-error/NULL continuation.

The actual Mac loader first populates the live animation table, then0x1868fd–0x186906 passes that same global table with NULL load-name/count pointers. The parser stores its input as globalScriptData0x185a00 and later passes the same retained argument to BG_ParseCommands0x18610f–0x186129. The actual animation token from Com_ParseOnLine0x185543 reaches the runtime lookup0x185562. The caller stores/sign-extends AX and performs index*96+scriptData0x185572–0x18557c, producing the complete matching record address. Its duration read0x18557f and downstream writable-field addresses establish real full-record consumers. This closes the previous index-only gap without inventing an index-to-pointer API. Native uses typed array indexing and an explicit animation_t pointer, retaining native alignment and borrowed backing.

Scope is the successful serialized loaded second pass, a live1..512-record table, found nonempty ASCII name of at most63 bytes, and valid preceding parser/item capacity and backing. Found indices0..511 survive the actual16-bit command-field narrowing. No installed asset, runtime occurrence or whole-loader invariant is asserted. Full misses differ: original direct Com_Error followed by NULL if it returns, versus actual Mac parser error and -1 followed by invalid indexing if fatal handling returns. This does not certify a standalone NULL-on-miss API, malformed/overlong names, load-time insertion, arbitrary global replacement/reentrancy or returned-fatal-handler continuation. Record mutations remain downstream consumers, not extra effects assigned to lookup.

The selected hash/search/address phase was unchanged and equal; surrounding parser/loader fixes and approved storage/ownership adaptations are preserved without reclassification. Parent and independent review accepted the scope in `units/review-provenance-animlookup48.md`. Five fresh exact Mac containers, complete original147-byte bodies/hash, raw table/error strings and source receipts are retained in `recon-provenance-animlookup48.noindex`. One closure, zero new defects or source changes; no compilation, tests, CI, replay or runtime verification. All other unresolved animation rows remain unchecked.
