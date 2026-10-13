# WS87: packed script literals and local names

2026-10-07. Baseline `3508e1a` contains WS80/WS84. This follow-up changes
`scr_compiler.c` and extends the existing compiler operand fixture. It does
not change bytecode widths, allocation, VM readers, shared layouts or legacy
stores. Other packed branch/backpatch operations remain outside this unit.

## Reproduced failures

WS81's real script loader/compiler/VM, with the earlier parser fixes and
fail-fast UBSan, stopped at the canonical-local-name unsigned-short store at
baseline line 1722. Receipt: `/tmp/ws81-composed-fullvm-ubsan-red/run.log`.

Independent synthetic cases execute the complete production compiler and
`scr_tempmemory.c` allocator. Its `TempMallocAlign` and `TempMallocAlignStrict`
both advance by the requested byte count without padding. An opcode therefore
legitimately leaves the following operand unaligned. Before edits:

| Case | Baseline misaligned store |
| --- | --- |
| Float literal | `scr_compiler.c:1291`, float |
| Integer literal | `scr_compiler.c:1525`, int |
| String literal | `scr_compiler.c:1243`, short |
| Localized string | `scr_compiler.c:1262`, short |
| Vector literal | `scr_compiler.c:1282`, float |
| Canonical local name | `scr_compiler.c:1722`, unsigned short |

All six abort under UBSan. Receipts are `/tmp/ws87-red-literals` and
`/tmp/ws87-red-vector-local`. These are executed failures, not an inference
from the allocator's name or an alignment warning.

## Change and byte contract

Three native-only helpers copy a short, int or float into a byte destination
with `memcpy`. Nine literal/local-name write sites use them, including both
positive and negative integer-width branches. The original typed stores remain
verbatim when `COD2_X64` is absent. Neither allocator gains padding.

Fixture expectations are literal little-endian bytes for strings, localized
strings, `1.5f`, `(1.5f, -2.0f, 0.25f)`, an integer `0x12345678`, and a
canonical string identifier. Each runs at all four byte offsets modulo four.
Ten integer boundary values additionally check zero, byte/short/int width
transitions on both signs and `INT32_MIN`. Cursor, prefix, trailing sentinels,
string transfer count, vector release count and saved constant-ref state are
checked. Allocation/diagnostic/reference services are synthetic; this fixture
does not prove real string/vector lifetime. The preexisting four builtin
operand cases remain intact.

## Verification

- Stock client: all 44 cases before the additional integer-boundary case pass
  at `/tmp/ws87-green-stock`; the added boundary case separately passes at
  `/tmp/ws87-green-boundaries`.
- CoD2x dedicated: all 45 cases pass at `/tmp/ws87-green-codx-ded`.
- Four complete compiler translation-unit builds use the actual stock/CoD2x
  client/dedicated Release commands, retaining optimization, `-DNDEBUG` and
  `-ffp-contract=off`. Logs and remapped compiler databases are in
  `/tmp/ws87-private`.
- Focused function ABI audits of the compiler's client/dedicated commands
  report two translation units per mode, zero errors and zero mismatches;
  receipts are `/tmp/ws87-private/abi-stock.json` and `abi-codx.json`.
- Independent review found no production defects and independently reran all
  seven new groups in stock client and CoD2x dedicated configurations.
- Production compiler preprocessing matches `3508e1a` in all five inactive
  configurations. Both modes' exact client/dedicated SDK commands with
  `COD2_X64` removed also report zero mismatches/errors.
- The generic legacy command still exits 1 because the changed *test fixture*
  has five token differences. Its logs identify only
  `tests/lp64/script/compiler_operands.c`; no production difference is waived,
  and the checker is unchanged. This is not a complete generic-gate pass.
- WS81's real synthetic script/VM recovery fixture passes fail-fast ASan/UBSan
  using this compiler through its explicit source override:
  `/tmp/ws81-runtime-error-realvm-green3`. That composition is separate from
  the earlier live binary and is not whole-game sanitizer acceptance.

Run from this worktree:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/compiler_checks.py \
  --compile-db /Users/stix/.codex/worktrees/config-catalog/cod2-native/build-macos/compile_commands.engine.json \
  --output /tmp/ws87-review
```

The opcode/operand format is unchanged; no new execution of the original
Mac i386 binary was performed. Full composed builds, live acceptance, full
ABI/import/raw-offset gates and unavailable Linux multilib artifact comparison
remain distinct from these bounded compiler checks.
