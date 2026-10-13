# WS80: native GSC loop local analysis and parser fidelity

Date: 2026-10-07. Baseline: `088e9bc`, branch
`codex/script-compile-fidelity`. Changes are uncommitted for the parent to
review and integrate. This is a bounded current-readiness follow-up to WS44
FA13; it does not establish general script or gameplay parity.

The private authority is Mac 1.3 i386
`cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Disassembly and binary-derived trace data remain outside Git, under
`/tmp/ws80-private`. The tests contain synthetic GSC and project source only.

## Confirmed loop difference

The reference local-analysis function at `0x9290a` distinguishes numeric
constant-true conditions before collecting break exits. Its while condition
dispatch at `0x92f2c` accepts primitive expressions or binary operators;
evaluation calls are `0x93fe8` and `0x94171`. For evaluates its initializer
at `0x93127`, checks the optional-condition wrapper at `0x9312c`, and treats
an omitted condition as constant true at `0x93135`. Its corresponding
evaluation calls are `0x94802` and `0x94875`.

For numeric types 5/6, the reference calls `Scr_CastBool` at `0x948c4` or
`0x948ed` and tests the resulting integer. All successful evaluations release
their value at `0x94015` or `0x9482f`, including nonnumeric constants. The
while break-array branch at `0x92fa6` allocates only for a constant-true
condition (`0x93f3a`); otherwise it installs a null break array at `0x92fac`.
For has the same branch at `0x9319b` (`0x93f59` allocation, `0x931a1` null).
The while break merge is additionally gated at `0x93796`. Continue-path
merging remains independent of that condition.

The previous native pass always allocated and merged break arrays. A real
parse of `main(flag) { while(flag) { inside=7; break; } tail=3; return tail; }`
therefore promoted `inside` into the enclosing function. The same occurred
with `for(;flag;)`. Twenty-two sequential variable-condition loops with three
distinct locals each falsely exhausted the enclosing 64-local limit, although
the locals belong to separate loop scopes. The reference branch behavior
predicts only `flag,tail` at function scope in that program. A constant-true
version really does export the break-path locals and should exceed the limit.

The fix evaluates primitive/binary loop conditions with the retained compiler
helpers, casts numeric constants, releases evaluated values, and collects
break exits only for constant-true conditions or omitted for conditions.
Continue handling, break-path intersection, and post-block behavior are
retained. Every production change is inside `COD2_X64` gates.

## Boolean literal correction

The original lexer and parser were traced through the complete input
`+main(){for(;true;){break;}}` and its false counterpart, using the original
DFA/LALR tables and the original lexer action jump table at `0x33abac`.
The native parser/scanner tables match the meaningful original bytes;
`yy_ec` matches its first 1,024 bytes used by the 256 character classes.

| Input | DFA action | Original action handler / token assignment | Token | Parser reduction / node | Primitive result |
| --- | --- | --- | --- | --- | --- |
| `true` | 83 | `0x10cddc` / `0x10cdf6` | `0x150` | 59, `0x10e4ad`, node 73 | 1, `0x9268a` |
| `false` | 82 | `0x10ce00` / `0x10ce1a` | `0x14f` | 58, `0x10e4e1`, node 72 | 0, `0x9266d` |

The LALR trace checks the consumed literal's position at the top of the
semantic stack when the one-token primitive rule reduces. `Scr_CastBool`
(`0x9127c`) preserves these integer truth values through the compare/set at
`0x912dd`–`0x912e3`. This is a binary/table trace, not execution of the Mac
i386 application.

The native lexer had reversed the first two tokens in its actions 82–89
token map. Numeric tests alone did not expose this. Separate literal while/for
tests failed before the token correction, with true treated as false and
false treated as true; they pass after restoring the original mapping under
`COD2_X64`. An initial inference that this was an original quirk was rejected
when the complete original lexical-action-to-token path was checked.

## Parser undefined behavior

The existing real-parser fixture, compiled with remapped actual Release
flags plus ASan/UBSan, aborted at baseline `yyparse_impl.h:602`:
`index -1 out of bounds for type 'short[200]'`. Forming `yyssa - 1` is invalid
C even though the parser increments it before its first store. The reference
first state store reaches the array's first entry (`0x10bf92`–`0x10bfa5`).

The native parser now starts its pointer at the first array element and skips
the first increment. Subsequent shift/reduction/error pushes retain the same
increment. The legacy pointer expression is retained in the inactive branch.
Both the original small parse and a 240-parenthesis parse that grows beyond
the initial 200-entry stack pass ASan/UBSan. Bad-syntax and unexpected-EOF
diagnostic controls also pass without disabling UBSan.

## Fixtures and receipts

`compiler_loops.c` uses the production scanner, parser, syntax-node
constructors, thread-block setup, and local-variable analysis. The runner
extracts the unchanged numeric-pair, equality, and Bool conversion bodies
from `scr_variable.c`; its equality dispatch seam rejects other operators.
Input, interning, allocation, and reference-count seams use synthetic data.
Expected local scopes come from the original branch semantics above, and
limit/parser diagnostics are asserted independently.

There are 27 new cases: constant/variable while and for, omitted for condition,
integer/float/binary true and false, string cleanup, continue, nested loops,
break intersection, true/false literals, the false and real local-limit
controls, parser growth, bad syntax, and unexpected EOF. The seven existing
compiler/scanner/parser cases remain in the runner. `-UNDEBUG` enables fixture
assertions while the actual Release optimization/FP flags remain in place.
The runner remaps all repository source/include paths to this worktree and
can select client or dedicated commands from an external engine database.

Baseline receipts:

- `/tmp/ws80-red-loops`: variable while and for scopes fail; the scoped-loop
  limit program fails with `LOCAL_VAR_STACK_SIZE exceeded`; constant while
  and omitted-for controls pass.
- `/tmp/ws80-private/parser-red.log`: the unchanged real-parser fixture aborts
  under UBSan before parsing its small valid script.
- `/tmp/ws80-red-literals`: all four literal loop cases fail after the loop
  analysis fix but before the lexer token correction.

All 34 cases pass in each of the four actual Release configurations:

| Configuration | Receipt directory | Result |
| --- | --- | --- |
| Stock client | `/tmp/ws80-final-stock-cod2_macos` | 34/34 |
| Stock dedicated | `/tmp/ws80-final-stock-cod2_macos_ded` | 34/34 |
| CoD2x client | `/tmp/ws80-final-codx-cod2_macos` | 34/34 |
| CoD2x dedicated | `/tmp/ws80-green-codx-ded` | 34/34 |

Each directory retains build and per-case run logs. Parser and loop fixtures
use ASan/UBSan with recovery disabled; the existing nonparser compiler and
scanner fixtures retain ASan. A representative rerun from the worktree is:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/compiler_checks.py \
  --compile-db /Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json \
  --target cod2_macos --output /tmp/ws80-review-stock
```

Both production translation units compiled separately using unmodified actual
Release flags (`-O3`, `-DNDEBUG`, `-ffp-contract=off`) for stock and CoD2x,
client and dedicated targets: eight successful compilations. Receipts are
`/tmp/ws80-private/{stock,codx}-*-{scr_compiler,scr_yacc}-{client,ded}.log`.
The inactive checks against `088e9bc` report 2 changed files, 5
`COD2_X64`-off and 10 legacy configurations, with zero token mismatches.
The stock actual-SDK inactive-token check also passes for both changed
translation units. No shared layouts or data tables changed.

Function ABI checks are bounded to the two changed translation units for both
client and dedicated commands. Stock and CoD2x each report four translation
units, zero errors, zero mismatches and zero new unprototyped floating calls
against `tools/abi/baseline.json`. The receipts are
`/tmp/ws80-private/abi-{stock,codx}-compiler-functions.json`. The corresponding
retail import checks are
`/tmp/ws80-private/abi-{stock,codx}-compiler-imports.json`; both have zero
proven extra or missing dereferences. These checks are source-level ABI
evidence, not executable compatibility proof.

An earlier stock raw-offset inventory of 391 sources and 1,198 sites reports
the preexisting `src/PC/universal/dvar.c:1327,1341` expression
`cursor = outBufferEnd - 1;` as an unreviewed residual. That source is unchanged
from `088e9bc`; WS79 also confirmed the same residual on `4b63491`. It is not
fixed or waived by WS80. A redundant full-engine function audit was stopped
in favor of the completed source-bounded audits above; no full-engine audit
success is claimed here.

## Retained packed-short emitter issue

`EmitBuiltinMethodOpcode` still writes its two-byte operand with
`*(short *)SCRCG->codePos = (short)index` at the baseline's line 2159
(current line 2199). WS76's client and dedicated boundary fixtures report
UBSan misaligned short stores when an opcode starts at an aligned buffer
base and its operand begins at offset one. The retained receipts are
`/tmp/ws76-green-first/cod2_macos-result.log` and
`/tmp/ws76-green-first/cod2_macos_ded-result.log`. This write is unchanged
by WS80. WS76's later fixture prefixes `GetSelf`, which aligns that fixture's
operand; its passing result does not establish arbitrary packed-output
alignment. The WS80 tests do not exercise this emitter and do not resolve
this separate compiler gap.

## Limits

These fixtures execute the compiler's local-analysis pass and retained
constant evaluators, not whole-program bytecode or VM execution. They do not
prove live game-script loading or runtime outcomes, arbitrary packed emitter
alignment, all GSC operators, or the remaining reconstructed lexer actions.
The relevant emitter already rejects always-false numeric loop conditions;
that complete emission path is not exercised by these new local-analysis
fixtures. The full Linux multilib byte-for-byte comparison remains an
integration gate unavailable on this host; inactive token checks are a
supplement, not a replacement. No game, GUI, or benchmark was launched.
