# game/g_load_utils module reconciliation

Updated 2026-11-03; baseline `ce8b539`.

**5/5 complete — 3 equal, 2 fixed.** All four Mac exports and the Linux inline
helper were compared across their complete bodies. Native changes restore only
fatal-error text; successful parsing and storage behavior are unchanged.

## G_AddSpawnVarToken (fixed)

Complete Linux helper maps to both inline key/value blocks in Mac G_ParseSpawnVars0x1cc44e-0x1cc4bd and0x1cc4ca-0x1cc526. Length+NUL, signed2048 capacity check, append copy, increment and pointer store match; native diagnostic now matches binary0x2e2bcc: leading0x15 and G_AddSpawnVarToken: MAX_SPAWN_VARS. Separate key/value overflow regressions fail baseline and pass bothmodes.

## G_NewString (equal)

All170 Mac bytes0x1cc26a-0x1cc313 compared. strlen+1, unsigned16384 limit and exact0x15 error, byte loop includingNUL, backslash-n conversion and other escape consumption, thenSL_GetString(user0) match. Original trailing unpaired backslash can suppress terminator; no equality claim for resulting undefined read. Normal terminated transformed strings and bounded lengths are covered.

## G_ParseSpawnVars (fixed)

All691 Mac bytes0x1cc374-0x1cc626 compared. Initial valid/count/char reset, empty-token return, opening-firstchar check,1024-byte key/value token reads, brace/EOF/64-pair guards, keythenvalue append/count and validflag success match. Restores exact five diagnostic literals at0x2e2b18/48/78/a8/cc, including0x15, EOF without closing brace and MAX_SPAWN_VARS text. Seven error regressions fail baseline/pass bothmodes; successful parse/lookup controls pass. FatalCom_Error contract and validbounded token provider assumed.

## G_SpawnStringInternal (equal)

All94 Mac bytes0x1cc20c-0x1cc269 compared. Ascending key/value pair scan uses case-sensitive strcmp, firstmatch sets output/returns1; no match writes default/returns0. Native typed pointer stride adapts LP64. Count stable across purestrcmp; valid parser-owned pairs. Mac release omits Linux debug assertion, asnative.

## vtos (equal)

All95 Mac bytes0x1cc314-0x1cc372 compared. Select current oneofeight32byte buffers, advanceindex modulo8, truncate three floatcomponents towardzero, format exact (%i %i %i) with size32,returnbuffer. Native matches finite int-representable vectors; control verifies truncation,8 retainedoutputs andwrap reuse.

## Verification and limits

The complete production translation unit passes eight test groups in stock and
CoD2x under ASan/UBSan with assertions enabled. All seven malformed-input
regressions fail on the baseline; the control group passes. Controls cover real
parser output, case-sensitive first-match/default lookup, escape conversion,
float truncation and eight-buffer reuse.

No oracle rerun is needed for fatal-diagnostic-only corrections: the preceding
item-module 2,000-frame stock/CoD2x traces remain the runtime baseline, explicitly
not a fresh replay of this build. No whole-project parity claim is made. A
trailing unpaired backslash in G_NewString retains an original undefined-read
edge; this batch does not invent a replacement malformed-string policy.

Private evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-load-utils-20261103.noindex/`.
Both client/dedicated builds and scoped ABI audits pass in stock and CoD2x.
Both legacy checks pass all ten inactive configurations. One fresh local review
independently checked all bodies, literals and evidence and found no findings.
No public signatures/layouts changed; no i386 binary identity is claimed.
