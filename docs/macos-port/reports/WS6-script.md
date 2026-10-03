# WS6 — script VM LP64 correctness

Branch: `port/lp64-script`. Base: `a2f4477` on `port/main`.
Worktree: `/Users/stix/Projects/cod2-native-wt/lp64-script`.
Date: 2026-10-03.

## Result

The native script objects compile, and 28 isolated checks pass. The full client
and dedicated builds reach the existing link failures; no executable was
launched. Several warning-free VM defects were reproduced and corrected as well
as the pointer truncation and allocation defects. This is static/isolated-runtime
verification, not a claim that complete script execution matches i386.

The requested default build has 85 script warnings, down from 94 at this branch's
base (the earlier inventory recorded 95). GSC binding warnings decrease from 13
to 11. No warning-suppression casts were added.

## Verification commands and limits

```sh
cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY=$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386
cmake --build build-macos -j12 -- -k
python3 tests/lp64/script/run.py
python3 tests/lp64/script/verify_legacy.py
git -c core.whitespace=cr-at-eol diff --check a2f4477
```

- Configure exits 0. Full build exits 2 at link, with no compilation errors.
  The 60 parsed unresolved-symbol names are identical before and after these
  fixes. C++/old libstdc++ ABI, SDL/AGL/input/crash handling and other existing
  platform/library seams remain for integration. No new script link symbol is
  missing.
- `run.py` passes six source/storage checks, seven compiler/parser cases, and
  fifteen VM cases. It uses the actual CMake client flags, ASan, and ld64 dead
  stripping. The VM semantics cases also use UBSan. Engine dependencies are
  stubbed; Object/JumpBack opcode bodies are extracted unchanged from the real
  interpreter. This does not execute the complete interpreter or launch a map.
- The storage check links the actual generated BSS object. A real arena
  allocation above `UINT32_MAX` round-trips through stack/vector offsets; native
  arena, string, variable and VM global addresses meet their required alignment.
- `verify_legacy.py` compares nine changed production files against `a2f4477`
  in eight inactive configurations, including i386, Apple i386, MinGW, wasm,
  Clang i386, Linux x86_64 port-off, Apple arm64 port-off, and i386 debugger-on:
  **zero mismatches**. Header macro expansions are included. Includes are
  omitted by the existing guard checker, so this proves source guard discipline,
  not a byte-for-byte 32-bit executable comparison. No i386 runtime was available.
- `scr_memorytree.c` retains its existing CRLF format. The diff check recognizes
  CRLF with the command-local `cr-at-eol` option; no Git configuration is changed.
- No required tool was missing or installed.

Final logs are ignored build artifacts: `build-macos/ws6-build-final.log`,
`ws6-checks-final.log`, `ws6-legacy-final.log`, and `ws6-diagnostics/summary.json`.
The diagnostics replay emits 20 real translation units (19 script plus the
touched GSC binding), using their CMake client commands with redirected output.
Original sources came from this branch's own `a2f4477`, not another worktree.

## Warnings before and after

Counts below are individual diagnostics from one client object compilation per
source, avoiding interleaved/duplicated client and dedicated build output.

| Source | Before | After |
| --- | ---: | ---: |
| `scr_compiler.c` | 10 | 5 |
| `scr_yacc.c` / `yyparse_impl.h` | 74 | 74 |
| `scr_variable.c` | 8 | 6 |
| `scr_animtree.c` | 1 | 0 |
| `scr_memorytree.c` | 1 | 0 |
| Other default script units, including VM/string list | 0 | 0 |
| **Script total** | **94** | **85** |
| `game_mp/g_scr_main_mp.c` | 13 | 11 |

The 74 parser diagnostics represent 60 scalar source positions and 14 interned
string IDs. `sval_t` and AST allocation strides preserve native pointers; a real
lexer/parser/parsetree check parses `+main() { return; }` with nodes above 4 GB.
Remaining compiler diagnostics concern one packed integer callsite count,
three scalar operator kinds and program length. The six variable diagnostics
are string lengths. The remaining GSC diagnostics include scalar string/entity
indices and the separate `XModelBoneNames` problem described below. No added
cast hides a raw pointer at these sites.

## Fixes and supporting evidence

### Pointer-bearing structure offsets and native storage

- `scrVmGlob_t` members replace old byte offsets for the dialog pointer,
  loading flag and local-variable stack base/limit. A real `Scr_Init` check
  verifies pointer clearing, flag clearing and the stack sentinel. The sentinel
  is `localVarsStack - 1`, matching increment-before-store behavior; the existing
  portable limit of 2047 usable locals is preserved.
- String globals now use `inited` and `nextFreeEntry`. Mac STABS puts the
  pointer after a 65536-byte hash table and a byte flag: i386 offset 65540;
  native offset 65544. The old access failed the pointer-layout check.
- `Scr_GetAnims` uses `scrAnimPub.xanim_lookup[1][index].anims`. Mac disassembly
  selects server row 1; the former absolute address and four-byte stride crashed
  in the isolated check.
- Compiler shutdown follows typed `PrecacheEntry.next`. Verified STABS field
  order is filename/include/sourcePos/next at 0/2/4/8, i386 size 12. Native next
  remains at 8, size 16. This removes an offset assumption; shutdown already
  passed before the change.
- Fixed script records remain fixed: `VariableUnion` is 4 bytes,
  `VariableValue` 8, `VariableValueInternal` 16, and archive records 5. Their
  integer offsets/strides were deliberately retained.

### Pointer encoding and marshalling

- VM suspension returns a native `VariableStackBuffer *` until the existing
  `SCR_STACK_ENC` stores it. The old unsigned return truncated an actual arena
  address. Archive/slot round-trip checks reproduce the failure and pass now.
- `CopyArray` uses the existing value-reference helper, which decodes vectors
  through `SCR_VEC_PTR`. The former raw vector slot dereference crashed under
  ASan. The copied array retains its vector after the source is cleared, and
  clearing both arrays releases the allocation. Thread dumps use `SCR_STACK_PTR`.
- Builtin caches store one-based indices in the existing native
  `scrCompilePub.func_table`; zero still means lookup failure. Bytecode retains
  its existing zero-based 16-bit table index. Full pointer identity is used for
  deduplication. Two addresses with identical low words and all 1024 cache
  entries round-trip in the checks. No parallel pointer table was introduced.
- Animation references already had an upstream tagged external-pointer table.
  Native XAnim cache producer and consumer now use that same `AnimRef_Enc/Dec`
  pair instead of pretending arbitrary XAnim allocations are opcode offsets.
  Undefined-animation diagnostics use the matching decoder as well. Checks
  cover program-to-external reference chains and cached heap XAnim pointers.
- Existing code-position encoding reserves `0xffffffff` for external
  `g_EndPos`; endon can replace an archived frame position with this sentinel.
  Null remains zero, and ordinary positions remain program offsets. Program
  lengths use signed ints, so this reserved value is outside their valid range.
  A real archive check failed before this extension and passes afterward.
- Arena and program offsets use native unsigned address subtraction; vector
  classification uses a numeric range test rather than relational comparisons
  between unrelated pointers. Encoded slots and the existing vector tag remain
  unchanged. `MT_Free` derives its index through `SCR_ARENA_ENC`.
- Three native-only GSC declarations match the actual pointer-returning
  callees: `SV_AddTestClient`, `Com_FindSoundAlias`, and `Scr_GetTypeName`.
  Separate translation-unit fixtures reproduce entity truncation, a false
  sound-exists result when the low word is zero, and a truncated diagnostic
  string. Only declaration/marshalling hunks in the game file were changed.
- Compiler `CompileError2` calls retain decoded code pointers and the real
  variadic signature. All three error branches are checked.

### Allocation sizes and portable VM semantics

- Ten compiler child-pointer arrays allocate 1024 native slots. Their former
  4096-byte allocations overflowed at element 512 under ASan. Scanner buffer
  allocation uses its native size 56 instead of historical size 40; the old
  initialization overflowed. Native layout assertions accompany the checks.
- Root/thread returns retain the returned value above their frame sentinel;
  child returns are also characterized. Previously the suspension helper
  replaced root/thread returns with undefined.
- `VMOP_Object` reads class number then entity number, matching compiler output.
- Apple backward jumps enforce the Mac binary's **2500 ms** loop timeout. The
  source fork's 5000 ms value was not treated as authoritative. Checks cover
  the threshold, wrap, loading warning/reset, terminal errors, parent cleanup,
  and suspended-caller restoration. Reset and check share monotonic milliseconds.
- Opcode diagnostics retain full native positions. U16/I32/F32 operand helpers
  use `memcpy` for packed, potentially unaligned bytecode.

## Arena/layout audit

Read-only references were the Mac binary's STABS/disassembly and
`~/Projects/cod2-native-refs/CoD2rev_Server/src/script/`. No reference dump is
added to the repository. Relevant commands included `nm -a` and
`xcrun llvm-objdump --disassemble-symbols=__Z8Scr_Initv`,
`__Z9VM_Resumej`, and `__Z10VM_Execute16function_stack_t` on the reference binary.

Memory nodes contain no pointers: 65536 eight-byte nodes occupy `0x80000`,
three 256-byte tables follow, heads start at `0x80300`, and counters at
`0x80324/0x80328`. Native struct size is `0x8032c`, within the encoding's
`0x80330` bound. Generated arena storage is `0x80380` bytes. The entire arena
does **not** need to be below 4 GB: its relative offsets do. Static assertions
and the actual generated-storage check verify this contract.

The generated `scrVmGlob` uses an oversized STABS-derived aggregate that still
has the original pointer-sized value union and extra `starttime2`. Its only
initializer is zero; active script consumers consistently use the reconstructed
`scrVmGlob_t`, whose native offsets are dialog 16, loading 24, starttime 28,
locals 32. The backing object is sufficiently large/aligned for these accesses.
WS2 should eventually migrate this global to the engine type. No generated
blob or generator was changed here.

## Unresolved behavior and first-launch checks

1. **Script error recovery:** the portable interpreter's nonterminal `longjmp`
   recovery lacks the reference's opcode-specific operand advancement and
   reference/stack cleanup. Modified nonvolatile locals can also become
   indeterminate after the jump. A script/builtin error can therefore resume
   at an invalid opcode or corrupt the stack. This needs a dedicated recovery
   reconstruction and whole-interpreter error-injection tests; the focused
   fixes do not establish semantic identity for this path.
2. **Other packed accesses:** direct bytecode loads and five-byte archive-record
   loads/stores remain outside the three operand-helper alignment fixes.
   Full-interpreter UBSan coverage is still needed, including wait/notify,
   switch and resumed stacks.
3. **`XModelBoneNames`:** `GScr_GetPartName` receives an `int` from a callee
   declared/defined that way in xanim, then dereferences it as `unsigned short *`.
   The orchestrator must coordinate the native callee return and GSC declaration
   with the xanim workstream. A caller-only cast cannot recover the lost bits.
4. **String lifetime:** upstream's enabled LP64 workaround retains zero-ref
   strings to avoid corrupt hash-chain recycling. The string check explicitly
   characterizes this policy; it is not a leak fix. Repeated maps/new strings
   may exhaust the fixed script arena.
5. **Animation external references:** the existing 16384-slot AnimRef table is
   not reset between levels and returns zero on exhaustion. Its lifecycle still
   needs integration testing/repair. This work reuses, rather than replaces,
   that upstream scheme.
6. **Debugger:** `COD2_FEATURE_SCRIPT_DEBUGGER=0` excludes all debugger sources
   and initialization in the requested build. Enabling it is unsafe: pointer
   backlinks and call-stack positions pass through ints, breakpoint/watch
   allocations retain i386 sizes, raw member offsets/absolute addresses remain,
   and some reconstructed fields disagree about line-number versus pointer
   meaning. Enabled watch initialization reaches `(int)&p` at
   `Scr_GetBreakpointType`. No matching debugger class STABS was found; keep
   this feature off pending a separate reconstruction.
7. Full map/script-pack load, nested/developer builtin resolution, jump/program
   size limits, timeout clock cost, and complete i386/native bytecode/execution
   parity remain unverified until the platform link is repaired.

The six compiler assembly references were already `#if 0`, rather than live
implementations replaced by WS1. Static review found matching core behavior in
their portable paths (including reversed constant-vector ordering). If/developer
paths assume the preceding local-variable pass has built their AST block.
The VM discrepancies above mean portable-C semantic identity is **not** claimed.

## Merge notes

Only seven `src/PC/script/*.c` files, one shared header and one GSC binding
file change. The shared header is `src/headers/cod2_defs.h`, solely its existing
`SCR_ARENA_*`, `SCR_CODEPOS_*`, and `SCR_VEC_*` macro block. `common_types.h`'s
inactive duplicate remains untouched. All new production behavior is guarded
by `COD2_X64` or its Apple SDK guard; original i386 expressions remain available.
No CMake, blob, generator, remote, sibling worktree or system-package change was
made. No push, PR or issue was created.

Focused commits, in order:

- `9b40ed6`: native archive pointers and VM global members.
- `0b15ece`: encoded endon sentinel and offset arithmetic.
- `35d67ef`: vector references, string globals and arena/storage checks.
- `f9e93d3`: animation lookups, external cache and chain decoding.
- `71db961`: three GSC pointer-return declarations.
- `e185b7b`: compiler caches/allocations and scanner sizing.
- `5f2e3c9`: portable VM returns, operands, timeout and diagnostics.
- `d8c67cf`: aggregate verification and complete inactive-source preservation.

Merge the branch in order onto a base containing `8537fa4` and `a2f4477`.
The game agent only needs to reconcile the three guarded declarations near the
top of `g_scr_main_mp.c`. The xanim ABI issue above remains an integration action.
An independent review of the production diff found no new correctness defect;
the known recovery/debugger limitations remain explicit.
