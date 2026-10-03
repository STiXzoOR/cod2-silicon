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

Full locations/messages: [diagnostics.tsv](diagnostics.tsv). Full lexical queue: [scan-candidates.tsv](scan-candidates.tsv).
<!-- END GENERATED LP64 COUNTS -->
