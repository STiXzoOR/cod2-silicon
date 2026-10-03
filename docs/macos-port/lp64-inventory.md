# Apple arm64 LP64 inventory (WS1)

Measured on 2026-10-03, branch `port/macos-build`, Apple clang 21, macOS 27.0.1 / SDK 27.0. The CMake client source set is 395 translation units; dedicated is 394 (the SDL input unit is client-only). All 789 compile to arm64 Mach-O objects, and all 789 diagnostic replays exit zero. Neither executable links yet. The source selection mirrors native Linux's PC/Mac/stubs sources, uses SDK zlib instead of eight duplicate bundled inflate units, and excludes all 32-bit blobs/generated data. Windows-specific glue and WebAssembly glue are separate platform builds, outside this source set.

This inventory assigns runtime cleanup; WS1 fixes compiler blockers only. A zero-error compile does not establish native layout, ABI, bounds, networking, rendering or script correctness. Warnings and lexical candidates below have separate counts. The examples marked as verified layout hazards were checked against active source and native type sizes; many remaining candidates are intentional scalars, serialized file offsets, or inactive historical assembly.

## Reproduce

```sh
cmake -S . -B build-macos -DCOD2_X64=ON
cmake --build build-macos -j12 -- -k
python3 tools/macos-port/collect_diagnostics.py
python3 tools/macos-port/lp64_inventory.py build-macos/lp64-probes
```

CMake 4.4.3 rejects a direct `cmake --build ... -k`; `-- -k` passes keep-going to Make. The build exits 2 at the expected link stage; diagnostic collection exits 0. Each replay keeps the actual CMake flags and writes an independent stderr log plus an exit-code manifest under ignored `build-macos/lp64-probes/`. The full build checks object emission and assembly as well as syntax.

Explicit warning flags: `-Wshorten-64-to-32 -Wpointer-to-int-cast -Wint-to-pointer-cast -Wint-conversion -Wincompatible-pointer-types -Wvoid-pointer-to-int-cast`. Clang's default format, bounds, library-redeclaration and fortify diagnostics supply additional categories. Implicit functions and incompatible types remain warnings through the existing `COD2_WNO` policy. Duplicate typedef/duplicate const noise is suppressed only on the new Apple branch. `-ffp-contract=off` is present for both targets.

Raw final [client warnings](lp64-logs/client-warnings.log) and [dedicated warnings](lp64-logs/dedicated-warnings.log) are committed with checkout prefixes removed. [diagnostics.tsv](lp64-logs/diagnostics.tsv) preserves every unique diagnostic and its occurrence count. [counts-by-category.tsv](lp64-logs/counts-by-category.tsv) is the directory/category matrix. [scan-candidates.tsv](lp64-logs/scan-candidates.tsv) preserves the lexical review queue, including source text and whether the file appears in CMake's compile database. The archive is below 2,000,000 bytes; it contains no game data or reference dumps.

<!-- BEGIN GENERATED LP64 COUNTS -->
## Measured diagnostic and scan counts

Input: 789 log files, 805,792 bytes. 2,276 diagnostic occurrences reduce to 1,141 unique warnings and 0 unique compile errors. 805 warnings fall in the LP64/ABI review categories below.

Identity is `(file, line, column, severity, flag, message)`: repeated headers/targets are deduplicated; distinct messages at one location remain distinct. Counts measure diagnostics, not confirmed defects. Narrowing includes bounded lengths and indexes; pointer casts include intentional integer IDs. Compile errors must reach zero before treating warning coverage as complete.

### By directory

| Directory | All unique warnings | LP64/ABI subset | Lexical candidates |
| --- | --- | --- | --- |
| src/Mac/DirectX 9 | 0 | 0 | 2 |
| src/Mac/DirectX_9 | 250 | 138 | 170 |
| src/Mac/Extras | 0 | 0 | 1 |
| src/Mac/Main | 9 | 5 | 27 |
| src/Mac/Tools | 146 | 33 | 429 |
| src/PC | 0 | 0 | 66 |
| src/PC/EffectsCore | 13 | 13 | 354 |
| src/PC/bgame | 15 | 10 | 114 |
| src/PC/botlib | 9 | 9 | 58 |
| src/PC/cgame | 4 | 4 | 3 |
| src/PC/cgame_mp | 32 | 30 | 541 |
| src/PC/client_mp | 26 | 25 | 273 |
| src/PC/game | 5 | 3 | 13 |
| src/PC/game_mp | 69 | 53 | 343 |
| src/PC/gfx_d3d | 197 | 169 | 1653 |
| src/PC/groupvoice | 0 | 0 | 5 |
| src/PC/jpeg-6 | 30 | 0 | 383 |
| src/PC/qcommon | 58 | 55 | 289 |
| src/PC/script | 95 | 95 | 1125 |
| src/PC/server_mp | 37 | 36 | 262 |
| src/PC/speex | 3 | 0 | 154 |
| src/PC/stringed | 12 | 11 | 66 |
| src/PC/ui | 3 | 2 | 48 |
| src/PC/ui_mp | 18 | 18 | 377 |
| src/PC/universal | 51 | 49 | 250 |
| src/PC/win32 | 32 | 32 | 201 |
| src/PC/xanim | 12 | 11 | 250 |
| src/PC/zlib | 3 | 3 | 37 |
| src/headers | 0 | 0 | 182 |
| src/headers/Mac | 0 | 0 | 2 |
| src/headers/PC/EffectsCore | 0 | 0 | 1 |
| src/headers/PC/bgame | 0 | 0 | 1 |
| src/headers/PC/cgame_mp | 0 | 0 | 2 |
| src/headers/PC/client_mp | 0 | 0 | 1 |
| src/headers/PC/game_mp | 0 | 0 | 2 |
| src/headers/PC/qcommon | 0 | 0 | 1 |
| src/headers/PC/script | 0 | 0 | 1 |
| src/headers/PC/server_mp | 0 | 0 | 9 |
| src/headers/PC/ui_mp | 0 | 0 | 7 |
| src/headers/PC/universal | 0 | 0 | 2 |
| src/headers/PC/win32 | 0 | 0 | 4 |
| src/headers/PC/xanim | 0 | 0 | 3 |
| src/imports | 1 | 0 | 0 |
| src/stubs | 9 | 0 | 16 |
| src/unix | 2 | 1 | 24 |
| src/web | 0 | 0 | 34 |
| src/win32 | 0 | 0 | 1 |
| src/win32/sdl2/include/SDL2 | 0 | 0 | 31 |
| src/win32/shims | 0 | 0 | 1 |
| src/win32/shims-msvc | 0 | 0 | 4 |

### LP64/ABI categories

| Category | Unique warnings |
| --- | --- |
| narrowing | 519 |
| pointer-to-integer | 134 |
| integer-to-pointer | 78 |
| pointer-type-mismatch | 61 |
| pointer/integer-comparison | 5 |
| constant-size-overflow | 4 |
| format/varargs | 2 |
| library-ABI | 1 |
| implicit-pointer/integer | 1 |

### All warning flags

| Flag | Unique warnings |
| --- | --- |
| -Wshorten-64-to-32 | 519 |
| -Wdeprecated-non-prototype | 196 |
| -Wpointer-to-int-cast | 120 |
| -Wint-to-pointer-cast | 73 |
| -Wincompatible-pointer-types | 61 |
| -Wignored-attributes | 38 |
| -Wshift-negative-value | 30 |
| -Wreturn-type | 23 |
| -Wreturn-mismatch | 17 |
| -Wvoid-pointer-to-int-cast | 14 |
| -Wimplicit-function-declaration | 10 |
| -Warray-bounds | 8 |
| -Wdeprecated-declarations | 8 |
| -Wpointer-integer-compare | 5 |
| -Wint-to-void-pointer-cast | 5 |
| -Wbuiltin-memcpy-chk-size | 4 |
| -Wformat | 2 |
| -Wimplicit-const-int-float-conversion | 2 |
| -Wpointer-sign | 1 |
| -Wnull-dereference | 1 |
| -Wincompatible-library-redeclaration | 1 |
| -Wabsolute-value | 1 |
| -Wint-conversion | 1 |
| -Wpointer-bool-conversion | 1 |

### Worst files by LP64/ABI warning count

| File | Unique warnings |
| --- | --- |
| src/PC/script/yyparse_impl.h | 74 |
| src/PC/gfx_d3d/rb_backend.c | 72 |
| src/Mac/DirectX_9/CDirect3DDevice.c | 55 |
| src/PC/qcommon/huffman.c | 42 |
| src/PC/game_mp/g_cmds_mp.c | 34 |
| src/PC/cgame_mp/cg_event_mp.c | 26 |
| src/PC/gfx_d3d/r_material_load_obj.c | 25 |
| src/PC/universal/com_files.c | 21 |
| src/PC/server_mp/sv_init_mp.c | 20 |
| src/PC/ui_mp/ui_main_mp.c | 15 |
| src/PC/win32/win_net.c | 14 |
| src/Mac/Tools/MacDisplay.c | 12 |
| src/PC/game_mp/g_scr_main_mp.c | 12 |
| src/PC/gfx_d3d/rb_tess.c | 12 |
| src/PC/gfx_d3d/r_material.c | 11 |
| src/Mac/DirectX_9/COpenGL.c | 10 |
| src/PC/client_mp/cl_main_pc_mp.c | 10 |
| src/PC/script/scr_compiler.c | 10 |
| src/Mac/DirectX_9/CDirect3DPixelShader.c | 9 |
| src/PC/gfx_d3d/rb_light.c | 9 |
| src/PC/script/scr_variable.c | 9 |
| src/Mac/DirectX_9/CColorConverter.c | 8 |
| src/Mac/DirectX_9/CDirect3DCubeTexture.c | 8 |
| src/PC/win32/cinematics.c | 8 |
| src/Mac/DirectX_9/CDirect3DSurface.c | 7 |

### Lexical scan categories

Scanned 662 `.c`/`.h` files under `src`, excluding `src/blobs`; found 7,823 line/category candidates. The scan is intentionally not preprocessed and includes inactive `#else`, optional features, historical headers and unused sources. A line can appear in multiple categories. `listed-TU` means only that CMake lists the file, not that the matching branch executes. These are review candidates, not confirmed bugs.

| Heuristic category | Line/category matches |
| --- | --- |
| constant-pointer-arithmetic | 3462 |
| small-integer-cast | 2463 |
| fixed-byte-access | 1124 |
| pointer-named-integer-field | 381 |
| fixed-size-memory-call | 193 |
| x86-only-width-guard | 122 |
| integer-cast-hash-or-key | 78 |

Full locations/messages: [diagnostics.tsv](lp64-logs/diagnostics.tsv). Full lexical queue: [scan-candidates.tsv](lp64-logs/scan-candidates.tsv).
<!-- END GENERATED LP64 COUNTS -->

## Verified hazards and representative sites

| Area / category | Site | Evidence and required correction |
| --- | --- | --- |
| Architecture guards / script representation | `src/headers/cod2_defs.h:6008`, `:6016`, `:6027`; `src/PC/script/scr_variable.c:1394` | `VariableUnion` and `SCR_ARENA_*` choose the LP64 representation only for x86_64. arm64 gets eight-byte pointer union members and lacks arena macros. `_SCR_ARENA_ENC` and `_SCR_ARENA_PTR` even appear as undefined functions at link. Make the representation choice apply to the intended LP64 architectures, then validate arena/code-position encoding rather than widening protocol IDs. |
| VM fixed offsets | `src/PC/script/scr_vm.c:486`, `:983`, `:7839` | Active code uses `scrVmGlob + 24`. Native probes give `sizeof(VariableValue)=16`, `offsetof(scrVmGlob_t,starttime)=44`, `offsetof(...,localVarsStack)=48`. Replace accesses with members and audit associated stack limits. WS1 already changed timeout reset to the typed starttime field and monotonic milliseconds; timeout comparisons remain absent from the portable VM. |
| Parser narrowing | `src/PC/script/yyparse_impl.h:125` and the remaining 74 diagnostic locations in that header | `node*` constructors use `uintptr_t`, while semantic values/locals narrow their results. Separate syntax-node pointers from token IDs/source positions throughout parser/compiler boundaries. Some scanner length differences are bounded integers and need proof, not indiscriminate widening. |
| Huffman pointers / keys | `src/PC/qcommon/huffman.c:11`, `:83`; `src/headers/cod2_defs.h:8749` | `nodetype` link members are `intptr_t`, but `NODE_PTR_CAST` chooses `(int)` on arm64. Native pointers are eight bytes and integers four. Extend the existing width guard; verify swaps, freelists and message compression round trips. |
| Material global raw layout | `src/PC/gfx_d3d/r_material_load_obj.c:15` | arm64 takes the ILP32 `mtlLoadGlob + 4` pointer slot and `(int)` store, while the x86_64 path uses `+8`. Coordinate the typed global with WS2 and replace byte offsets with members. Serialized shader constant-table offsets elsewhere in this file are a separate file format. |
| Temporary buffers / effects | `src/PC/EffectsCore/FxArchive.c:313`, `:322`; `src/PC/EffectsCore/FxPrimitives.c:522`, `:523` | Compiler and native size probe agree: a 104-byte buffer receives `sizeof(FxGfxEntity)=112`; a 116-byte buffer is cleared with `sizeof(GfxEntity)=128`. Use typed native temporaries, with explicit serialization for the historical 104-byte archive format. |
| Pointer ordering / hash identity | `src/PC/EffectsCore/FxUtil.c:172` | Sorting subtracts two `MaterialHandle` pointers after conversion to `int`. Preserve full pointer identity and define a non-overflowing ordering; audit the 78 lexical hash/key candidates. String hashes and 32-bit resource IDs should keep their intended integer semantics. |
| Virtual table and list storage | `src/Mac/DirectX_9/CMemoryBuffer.c:9`, `:13`, `:35`, `:64` | Native pointers are stored in an `int vptr`; a single four-byte global is cast to a multi-pointer linked-list sentinel. Allocate a correctly typed sentinel and pointer-sized vtable field, then reconcile the public object layout and delayed-free constructors. |
| GL/D3D API widths | `src/headers/cod2_defs.h:3029`; `src/Mac/DirectX_9/CDirect3DSurface.c:142` | Reconstructed `GLuint` is `unsigned long` (eight bytes here), while the SDK OpenGL handle type is 32-bit. This can silently break output-array strides and function signatures. The surface code also compares a truncated allocation address to an i386 address-range constant. Audit GL scalar types and every GL prototype/array boundary, rather than treating all `long` values as pointers. |
| Ring-buffer storage | `src/Mac/Tools/CCircularBuffer.c:119`, `:120` | `malloc` is stored in the first four-byte `int` slot, followed immediately by size. Readers load an eight-byte `void *` from that storage. Reconstruct a typed object and update every accessor/allocation. |
| Native threads / raw offsets | `src/Mac/Tools/MacThreads.c:24`, `:27`, `:71`–`:75` | Vtable, `pthread_t`, argument pointer and mutex live at `+0`, `+4`, `+8`, `+0xc`. Eight-byte pointers overlap; SDK `sizeof(pthread_mutex_t)=64`, incompatible with the old running flag at `+0x38`. Reconstruct CThread/CMutex and lock wrappers together. |
| Display constructor / consumer | `src/stubs/macos_compat.c:206`–`:213`; `src/Mac/Tools/MacDisplay.c:284` | Eight-byte pointers are written into four-byte-spaced slots `0x14/0x18`, `0x30/0x34/0x38`; `0x3c` overwrites part of the preceding pointer. The consumer reads the mode pointer as `int`. A 100-byte fake layout cannot be retained as native storage. Replace producer and consumers together (WS3). |
| Client packet bounds | `src/PC/client_mp/cl_main_pc_mp.c:662`, `:668` | Server-browser byte pointers are truncated before comparisons. Use pointer differences/bounds valid for the original buffer; keep wire address/port widths explicit. |
| Client gameplay pointer fields | `src/PC/cgame_mp/cg_event_mp.c:823`–`:833` | `reloadSoundPlayer` is read through `int`, then cast to `snd_alias_list_t *`; the weapon table also uses four-byte stride arithmetic. Native `weaponInfo_t` is 632 bytes and `reloadSoundPlayer` is at offset 336. Use typed weapon records and full pointers. |
| UI conversion arguments | `src/PC/ui_mp/ui_main_mp.c:1715`–`:1720`; `src/headers/PC/ui_mp/ui_types.h:21` | A ten-int stack array is cast to `ConversionArguments`, whose `args` are pointers. `(int)replaceString` truncates, and native alignment/size also differ. Construct the actual struct and audit the formatting/varargs contract. |
| Renderer callback ABI | `src/PC/win32/cinematics.c:792`–`:800` | `MaterialHandle` is stored in `int` and passed through a function-pointer cast whose last parameter is `int`. Fix the callback signature and all callers together; integer widening at one site cannot repair the call ABI. |
| Server parse pointer | `src/PC/server_mp/sv_game_mp.c:322`–`:328` | x86_64 uses `const char *`, arm64 takes an `int parse_point` and converts it back. This is another architecture-selection error, separate from network wire integers. |
| Animation object fields | `src/PC/xanim/dobj.c:339`, `:340`; `src/PC/xanim/xmodel.c:130`–`:133` | Model pointers are read via `*(int *)model`; `XModelBoneNames` returns a pointer as `int`. Replace raw accesses and correct the return type across declarations/callers, preserving bone IDs as integers. |
| Dvar flag constants represented as addresses | `src/PC/server_mp/sv_init_mp.c:416`, `:427`–`:430`; `src/PC/stringed/stringed_hooks.c:156`, `:157` | Addresses of `__mh_execute_header` stand in for original flag-bit numbers. These need original numeric values recovered from verified reference data, not pointer-sized flag fields. The similar `infoParms` initializer in `surfaceflags.c` is deferred to WS2 and remains undefined on Apple. |

The native layout measurements above came from a throwaway C program including `common_types.h`, compiled with the actual CMake client flags and run on this Mac. It printed: pointer 8, int 4, long 8, pthread mutex 64, reconstructed GLuint 8, FxGfxEntity 112, GfxEntity 128, VariableValue 16, VariableUnion 8, nodetype 56 (left/right offsets 0/8), weaponInfo_t 632. These are measurements of this checkout, not the layout the finished port must retain.

## Independent implementation packets

Each row can be assigned to a separate WS6 agent after the architecture/data contracts are settled. Ownership is bounded by the listed directories/files. Shared type changes must be small, tied to that packet's types and coordinated with WS2; agents should not perform a global header rewrite. Every packet must preserve the 32-bit path and attach compile plus focused runtime/serialization evidence where the data dependency allows it.

| Packet / owner | Work and boundaries | Acceptance evidence |
| --- | --- | --- |
| LP64 architecture/type contract (orchestrator + WS2 first) | Inspect all 122 width-guard candidates, beginning with `VariableUnion`/arena encoding in `cod2_defs.h`; decide which are pointer-width choices versus CPU instructions. Agree fixed-size wire/disk integers and typed global definitions. Supply bounded patches for subsystem owners. | Native layout dump; 32-bit layout pins in reference CI; no unresolved `SCR_ARENA_*`; document data-generator agreement. |
| Script (`src/PC/script`, script type sections) | Parser semantic values and node constructors; compiler code positions; VM stacks/localVars/notify lists; memtree arena pointers; replace active `+24` assumptions. Restore timeout comparisons using the same monotonic clock as reset. Keep arena IDs/code offsets explicit and avoid replacing them with arbitrary addresses. | Compile representative scripts and execute variables, arrays, threads/notify and timeout cases; check stored code/arena offsets and VM state parity with WS5. |
| Qcommon compression (`huffman.c`, related `msg.c` boundaries) | Extend native pointer cast selection; audit node links, freelist casts and compressed message state. Keep bitstream representation unchanged. | Round-trip all byte values and varied messages, plus cross-architecture encoded-byte parity. |
| Qcommon collision (`cm_*`, `cm_world.c`) | Audit byte offsets into pointer-bearing brush/leaf/model structs; distinguish disk lumps from native structs. Check trace work and thread-local buffers, including remaining prototype warnings. | Collision map-load and representative trace/sight-trace fixtures against WS5 reference. |
| Universal files/memory (`src/PC/universal/com_files.c`, `com_memory.c`, `mem_track.c`, `assertive.c`; `src/PC/zlib/unzip.c`) | File lengths/positions, allocation sizes/alignment and pointer differences; retain file-format width with range checks. Review callbacks and SDK zlib ABI instead of assuming reconstructed `uLong` means one width everywhere. | IWD/unzip fixtures, file seek/read/list tests and allocator alignment tests on native pointers. |
| Universal sound (`com_sndalias*.c`, sound type sections) | Alias linked-list pointers, `SoundFileInfo` arrays, packed temporary records and callback arguments. Coordinate typed initializer layout with WS2. | Load/find/pick aliases, allocate/unload sound metadata and validate linked lists under native address sizes. |
| Universal data/flags (`surfaceflags.c` and owners of address-valued dvar flags; WS2) | Recover numeric flag constants and supply native `infoParms`; inspect remaining reconstructed address-as-constant uses. Do not widen flag bits into pointer identities. | Typed initializer comparison to original i386 values; source consumers use flags with their documented numeric meaning. |
| Renderer core (`src/PC/gfx_d3d/rb_*`, `r_dpvs*`, scene/culling files) | Tessellation, command buffers, callback signatures, scene entity pointer storage and fixed allocations. Prioritize `rb_backend.c` (72 warnings) and separate bounded command offsets from native pointers. | Command-buffer traversal, scene/mesh/culling fixtures and renderer state parity; bounds instrumentation where runnable. |
| Renderer material/assets (`r_material*`, `r_image*`, `r_bsp_load_obj.c`, load types) | `mtlLoadGlob` pointer layout; material maps/hash entries, routing/technique arrays and pointer-bearing resource allocation sizes. Preserve serialized shader/BSP offsets. | Load material/technique/image/world fixtures; native allocation sizes match native structs; table lookup identities stay intact. |
| D3D/GL ABI (`src/Mac/DirectX_9/CDirect3D*`, `COpenGL*`, converters, math) | Correct GL fixed-width types/prototypes and output arrays; method return/argument types, vtables, object sizes, refcounts and scratch/address heuristics. Legacy 2.1 remains the context. C++ runtime references require an ABI decision with the orchestrator. | SDK boundary compile checks, GL scalar/output-buffer checks, native renderer creation tests and handle/refcount integrity. |
| D3D helpers (`CMemoryBuffer.c`, `CVAOPacket.c`, cache/vertex-array helper types) | Typed delayed-free sentinel, vtable storage, rb-tree/list nodes and packet key semantics. Audit 32-bit keys: some are resource IDs, others may encode pointer identity. | Delayed-free lifecycle and rb-tree lookup/erase tests with native pointers; allocation bounds and key collision checks. |
| Effects (`src/PC/EffectsCore`) | Fix verified stack overflows; typed entity temporaries; archive conversion; effect sorting pointer identity; emitter/list/vtable fields and capacities. | Effect archive round trip with explicit historical format, entity submission and sort/list lifecycle checks. |
| Client (`src/PC/client_mp`, `src/PC/cgame` asset loader) | Packet bounds, asset handle returns, font/material callback signatures, client state pointer arrays and download buffer lengths. | Parse server-browser packets, config strings and effect loads; asset identity survives calls. |
| Client gameplay (`src/PC/cgame_mp`) | WeaponInfo table stride and sound/material pointers; cg event arrays, entity interpolation layouts and renderer callback contracts. | Representative weapon/event/sound dispatch cases and native client-state parity snapshots. |
| Server (`src/PC/server_mp`) | Entity/client array strides and parse cursor; snapshot/network buffers; dvar flag values in coordination with universal/WS2. Keep wire field widths intact. | Parse entity strings, build snapshots and exchange unchanged protocol messages against WS5 reference. |
| Game (`src/PC/game_mp`, `src/PC/game`) | Entity/gclient/tagInfo pointer fields, builtin callback signatures and `g_cmds_mp.c` pointer-difference/argument handling. Coordinate script-builtin type changes with script owner. | Spawn/link entities, dispatch representative commands/builtins, and compare gameplay state with WS5. |
| Animation/bgame (`src/PC/xanim`, `src/PC/bgame`) | DObj/model raw pointer reads, bone-name return type, skeleton/animtree allocation lengths and weapon metadata pointers. Keep bone/animation IDs distinct from addresses. | Model/bone/animtree lookup and allocation tests; expected asset format width and pose parity. |
| UI (`src/PC/ui_mp`, `src/PC/ui`) | Construct actual `ConversionArguments`, replace pointer-valued ints and varargs misuse, menu/item/window strides, asset handle callbacks. | Text conversion with several strings, menu asset dispatch and native structure traversal. |
| Stringed (`src/PC/stringed`) | Dvar flag constants, local string package/container ABI and translated-text callback types. | Language initialization and lookup fixtures; verified flag constants and native string lifetime. |
| Botlib (`src/PC/botlib`) | Script buffer/line sizes and allocation-length narrowing; review parser callbacks, preserving token value semantics. | Preprocessor/token fixtures and controlled input-size bounds. |
| Platform (`src/Mac/Tools`, `src/Mac/Main`, `src/stubs/macos_compat.c`, `src/unix`; WS3) | Typed display-list producer/consumers, CThread/CMutex/CCircularBuffer, paths, clocks, sockets/ifreq/SOCKS layouts, SDL input bridge and app lifecycle. Existing `win_net.c` raw address/ioctl constants remain suspect. Eliminate accidental Windows diagnostics imports. | Launch/headless lifecycle, native thread/lock and ring-buffer tests, socket address exchange, legacy GL context and raw mouse evidence. |
| Cinematics (`src/PC/win32/cinematics.c`, shared video callback types) | Pointer-valued material callback and RoQ buffer/table widths; review `long` coefficients and native client renderer interface. | Decode a legal test clip and submit frames using full material pointers; pixel/reference parity. |
| Codec audit (`src/PC/jpeg-6`, `src/PC/speex`, `src/PC/groupvoice`) | Review scan candidates even where no LP64 warning appears; distinguish codec fixed-size arrays and bitstream integers from native buffer sizes. Bundled inflate sources are not linked by the Apple build. | Decode/encode small fixtures with unchanged bitstreams; documented bounds for retained narrowed sizes. |

Packets involving shared renderer callbacks, script builtins or WS2-generated globals require explicit agreement on those interfaces before independent integration. This is an ordering dependency, not a reason to assign the entire codebase to one agent.

## Remaining link surface

The client has 1,366 unique undefined names; dedicated has 1,375. Full linker spellings (including demangled C++ names) are listed in [undefined-client.txt](lp64-logs/undefined-client.txt) and [undefined-dedicated.txt](lp64-logs/undefined-dedicated.txt). There are no duplicate-symbol errors.

Both contain 417 `imp_*` slots and numerous global tables/vtables/engine globals removed with 32-bit blobs. Other remaining classes include old libstdc++ ABI functions, Windows API/CRT diagnostics, platform entry points and the missing `SCR_ARENA_*` macros noted above. Adding a C++ library alone cannot establish compatibility with reconstructed 32-bit libstdc++ data layouts.

Dedicated omits SDL implementations and adds unresolved `_IN_Frame`, `_MacDisplay_CreateScreenContext`, `_SDL_GL_SwapWindowDirect`, `_aglDestroyContext`, `_aglSetCurrentContext`, `_aglSetDrawable`, `_aglSwapBuffers`, `_sdl_gl_height`, `_sdl_gl_width`, `_sdl_gl_window`. The SDL-prefixed swap name is an engine wrapper, not an SDL library API. The full dedicated object set has no unresolved SDL library API; the link command contains no SDL library.
