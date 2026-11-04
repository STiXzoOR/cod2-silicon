# bgame/bg_animation_mp module reconciliation

Updated 2026-11-04.

**42/48 reconciled: 37 equal, 5 fixed; 6 unchecked.** Mac 1.3 is the authority. All 22 Mac function bodies were read by hand, including the helpers inlined into the parser, player animation, controllers and loader. Linux source was used only to identify helper roles. No Windows-specific behavior was identified.

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
