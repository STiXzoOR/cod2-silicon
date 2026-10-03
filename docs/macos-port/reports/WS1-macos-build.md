# WS1 — macOS arm64 build

Status: compile bring-up complete; linking and runtime remain incomplete by design. Worktree `/Users/stix/Projects/cod2-native-wt/macos-build`, branch `port/macos-build`. Read the complete `docs/macos-port/PLAN.md` before implementation. No sibling worktrees were accessed, no packages installed, no global configuration or remotes changed, and nothing pushed or published. No game data, Activision binaries or reference/decompiler dumps were added.

## What works and how it was verified

Host commands `sw_vers`, `clang --version`, `cmake --version`, `sdl2-config --version`, `xcrun --show-sdk-version` report macOS 27.0.1, Apple clang 21.0.0 (`arm64-apple-darwin27.0.0`), CMake 4.4.3, SDL2 compatibility version 2.32.72, SDK 27.0. No required tool was missing.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON
cmake --build build-macos -j12 -- -k
python3 tools/macos-port/collect_diagnostics.py
python3 tools/macos-port/lp64_inventory.py build-macos/lp64-probes
python3 tools/macos-port/check_legacy_guards.py --base 1cb3516
git diff --check
```

Observed results:

- Configure exits 0 and finds installed SDL2, SDK ZLIB and OpenGL.framework.
- CMake builds all 395 client and 394 dedicated C translation units into arm64 Mach-O objects. Both link attempts then fail on undefined symbols; the full keep-going build exits 2. There are zero compilation errors and zero duplicate-symbol errors. CMake 4.4.3 rejects `-k` as its own option, so the requested keep-going invocation needs `-- -k`.
- Diagnostic replay exits 0: `789 translation units; 0 failures`. It uses each actual CMake compile command with `-fsyntax-only`, preserving requested warning flags and storing independent logs, with an exit-code/command manifest under ignored `build-macos/lp64-probes`.
- Inventory reports 1,141 unique warnings, including 805 LP64/ABI diagnostics, and zero errors. The lexical scan produces 7,823 line/category candidates across 662 C/header files. Diagnostic identity includes location, flag and message, deduplicating repeated targets/headers. Heuristic candidates are explicitly not a count of confirmed bugs.
- Guard check exits 0: `30 files; 6 inactive configurations; 0 mismatches`. It compares preprocessed source/header bodies to pre-port commit `1cb3516` for Linux i386, Apple i386 with the port off, Linux x86_64 with the port on, MinGW i386, wasm and clang i386. Foreign includes are removed and built-in line/time values fixed for this host-independent comparison. This establishes guard discipline, not binary parity.
- `git diff --check` passes.

Object verification was run directly against the compile database: every entry has `-arch arm64` and `-ffp-contract=off`; every expected `-o` exists; `file` identifies all 789 as `Mach-O 64-bit object arm64`. Neither blob paths nor `native_gen` sources occur in the database.

`nm` on the built client objects confirms `_declEnd`, `_string`, `__Z10VM_ExecutejPKcj`, `_s_techniqueTypeNames` at the same address as `_Material_TechniqueNames`, `_UI_Component_g` as an indirect alias for `___ZN12UI_Component1gE`, and references to `___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE`. A replay of the VM command with `-DVM_EXECUTE_USE_ASM_REFERENCE=1` still compiles: Apple LP64 always chooses the existing C implementation.

`nm -u` over the entire dedicated object set shows no SDL library API references. The only SDL-named undefined symbol is the engine wrapper `_SDL_GL_SwapWindowDirect`. Its link command has no SDL library. It retains engine/renderer translation units for whole-source compilation, so platform/rendering wrappers remain unresolved rather than being replaced with no-op implementations.

Native layout probes compiled with the CMake flags and executed on this Mac confirm eight-byte pointers, four-byte ints, eight-byte longs, 64-byte SDK pthread mutexes, FxGfxEntity 112, GfxEntity 128, VariableValue 16, VariableUnion 8, and scrVmGlob starttime/localVarsStack offsets 44/48. The [inventory](../lp64-inventory.md) records these measurements, exact warning counts, worst files, verified defects and concrete subsystem work packets.

## Implementation and decisions

The top-level CMake change is a five-line `elseif(APPLE AND COD2_X64)` branch including `cmake/macos-arm64.cmake`. The module creates `cod2_macos` and `cod2_macos_ded`, enforces arm64, applies FP contraction off and diagnostic flags, and links the client to installed SDL2 and OpenGL.framework. Existing Linux/Windows/wasm branches remain intact. Source selection follows native Linux's PC/Mac/stubs/root set, adding common/socket/statehash support and client-only SDL input. SDK zlib replaces the same eight bundled inflate units excluded by native Linux. Other platforms' glue is not part of this native source set.

One central macro, `COD2_APPLE_SDK`, is true only when both `__APPLE__` and `COD2_X64` are defined. It suppresses the old STABS Darwin typedefs/structs and imports `<sys/resource.h>` alongside existing SDK includes. SDK pthread types, Darwin scalar typedefs and rlimit win. Other platforms and the 32-bit build keep their original declarations. The generic legacy `regparm` macro is empty on this Apple path; existing ABI macros already disable sseregparm/cdecl/stdcall/fastcall on non-GCC-i386 targets.

Redundant libc externs were guarded rather than disabling Darwin's fortify macros. Two stale internal prototypes now match their implementations on Apple. Mach-O section syntax replaces an ELF section string only on Apple. `win_net.c` uses SDK `struct ifconf`, avoiding expansion of the SDK `ifc_buf` macro inside a hand-written struct; raw ioctl/sockaddr/interface assumptions still need WS3.

`r_material_code_tables.c` now takes its existing pointer-wide `intptr_t` initializer path on Apple LP64. `surfaceflags.c` is different: its `infoParms` initializer embeds addresses of i386 code/globals as integer flag values. Its definition is excluded on Apple and `_infoParms` remains unresolved. WS2 must recover the actual numeric constants and provide a native initializer; no zero values or guessed addresses were substituted.

The SDL compatibility layer now selects framework GL headers, requests a legacy 2.1 context (profile mask 0, major/minor 2/1), and swaps using SDL on Apple. `glVertexArrayParameteriAPPLE` is the one authorized unsupported-extension no-op. Dedicated omits SDL-dependent implementations. Linux X11 polling is excluded on Apple; `_Linux_PollInputEvent` remains unresolved for WS3's input bridge. Window creation, context operation, raw mouse, paths/audio/network and application startup were not runtime-tested.

## Assembly and symbol audit

| Site | Why it exists / Apple LP64 treatment |
| --- | --- |
| `scr_compiler.c`, six blocks at 419, 1451, 2246, 3332, 4120, 5792 | Historical byte-matching assembly references, already under `#if 0`. Existing active C implementations handle binary expression evaluation, local lookup, primitive/variable expression emission, if statements and developer statements. No edit needed; i386 reference text retained. |
| `com_sndalias_load_obj.c`, block at 1597 | Historical `Com_WriteLocalizedSoundAliasFiles` reference, already `#if 0` with active C. Direct regparm macro is disabled on Apple LP64. |
| `scr_vm.c`, VM reference block | Optional naked i386 interpreter; Apple LP64 is forced to the existing C candidate even if the reference flag is requested. |
| `scr_vm.c`, timeout `rdtsc` | Existing instruction is GCC/i386-only. Apple now records monotonic milliseconds with `clock_gettime` into the typed starttime field instead of the i386 byte offset. The portable VM currently has no corresponding timeout comparisons; restoring timeout enforcement is a script packet, not a verified runtime result. |
| `sv_client_mp.c`, `sv_main_mp.c`, `g_main_mp.c`, `.L*_fmt` emitters | Empty diagnostic labels used by historical x86 references. Their emission is excluded on Apple LP64; active C diagnostic paths remain. |
| `r_material.c`, `declEnd` rename | Explicit assembler names bypass Darwin's normal C prefix. Apple emits `_declEnd`. |
| `scr_vm.c`, VM external rename | Mach-O spelling is `__Z10VM_ExecutejPKcj`: the original C++ mangling receives the Darwin underscore. |
| `CDirect3DDevice.c`, three static-member renames | Reconstructed C identifiers beginning `__ZN` receive the normal C prefix, so aliases reference `___ZN...`. Apple uses the existing COFF prefix macro's extra underscore. |
| `material_tech_names.c` | Darwin clang does not support the existing alias attribute; Apple uses Mach-O `.set` aliases and `_string` spelling. |
| `link_stubs.c`, UI alias | Mach-O `_UI_Component_g` aliases reconstructed C symbol `___ZN12UI_Component1gE`. |
| `cg_draw_mp.c:1224`, `cg_newDraw_mp.c:1053` | MSVC/i386-only x87 rounding references already have portable floorf paths; inactive on Apple. No edit. |
| `src/blobs/bss.c:1374` | Blob `g_banIPs` assembler alias is excluded from this build. WS2 must account for Darwin spelling if retaining that alias. No blob edit. |

Every explicit symbol rename in the included source set was checked; final object emission and nm verify the active ones. No x86 assembler source or generated 32-bit data is assembled by the Apple branch.

## What does not work yet

The client has 1,366 unique undefined linker names, dedicated 1,375. Complete sorted lists are [undefined-client.txt](../lp64-logs/undefined-client.txt) and [undefined-dedicated.txt](../lp64-logs/undefined-dedicated.txt). Each includes 417 `imp_*` slots and many omitted data globals, tables and vtables. Other unresolveds include old libstdc++ ABI functions, Windows API/CRT diagnostics (`GetModuleHandleA`, `GetProcAddress`, `RtlCaptureStackBackTrace`, `_ReturnAddress`), missing arm64 script arena macros, `_Linux_PollInputEvent`, and platform wrappers. These were inventoried without linker alias hacks or undefined-symbol suppression.

Dedicated adds unresolved IN_Frame, MacDisplay_CreateScreenContext, SDL_GL_SwapWindowDirect, AGL calls and SDL window dimension/state globals because SDL implementations are absent. The renderer is still compiled, so headless source pruning/platform boundaries need later integration.

No native executable, game-data launch, network session, rendering frame, 250 fps measurement, raw mouse measurement or cross-architecture state parity exists yet. Severe LP64 issues remain, including stack overflows, pointer truncation, pointer-bearing four-byte slots, incorrect GL type widths and architecture guards that recognize x86_64 but not arm64. Compilation is not evidence that these runtime paths are safe.

The 32-bit Linux/Windows targets cannot be built on this Mac. Byte-for-byte binary comparison is unverified and remains WS5/reference CI work. The inactive preprocessor bodies match, and original i386 assembly text was preserved.

## Every changed file and inactive-path effect

`COD2_APPLE_SDK` below means exactly `__APPLE__ && COD2_X64`. Therefore every source change is inactive when the port is off or the platform is not Apple. Original declarations/instructions remain in the other branches.

| File | Change / why the original build is unaffected |
| --- | --- |
| `.gitignore` | Ignores `/build-macos/`; no compile effect. |
| `CMakeLists.txt` | Adds the Apple-and-X64 include branch; all other branch bodies are unchanged. |
| `cmake/macos-arm64.cmake` | New module, included only by that branch. |
| `src/headers/cod2_platform.h` | Defines the central SDK macro and resource include; disables regparm only on Apple LP64. |
| `src/headers/cod2_defs.h` | Guards old Darwin typedef/struct groups with the central macro; keeps originals otherwise. |
| `src/PC/bgame/bg_animation_mp.c` | SDK wins over redundant vsnprintf extern only under the central macro. |
| `src/PC/botlib/l_precomp.c` | Same treatment for vsnprintf/strncat. |
| `src/PC/cgame_mp/cg_main_mp.c` | Same treatment for sprintf/memcpy/memset. |
| `src/PC/client_mp/cl_cgame_mp.c` | Avoids x86 intrinsic header and redundant strcat declaration only on Apple LP64. |
| `src/PC/client_mp/cl_main_pc_mp.c` | SDK sprintf declaration wins only on Apple LP64. |
| `src/PC/game_mp/g_main_mp.c` | Excludes old diagnostic label and conflicting SL_ConvertToString externs on Apple LP64. |
| `src/PC/gfx_d3d/r_bsp_load_obj.c` | Apple-only prototype matches typed AABB implementation. |
| `src/PC/gfx_d3d/r_material.c` | Apple-only assembler name `_declEnd`. |
| `src/PC/gfx_d3d/r_material_code_tables.c` | Extends existing pointer-wide initializer path to Apple LP64. |
| `src/PC/qcommon/cm_trace.c` | Apple-only prototype matches sight-trace implementation. |
| `src/PC/qcommon/crash_handler.c` | Apple LP64 includes sys/ucontext.h, avoiding deprecated ucontext API gate. |
| `src/PC/script/scr_animtree.c` | SDK sprintf declaration wins only on Apple LP64. |
| `src/PC/script/scr_vm.c` | Apple-only C VM selection, typed monotonic reset, Mach-O external spelling. |
| `src/PC/server_mp/sv_client_mp.c` | Apple-only SDK sprintf declaration and exclusion of unused labels. |
| `src/PC/server_mp/sv_main_mp.c` | Excludes unused label only on Apple LP64. |
| `src/PC/ui_mp/ui_main_mp.c` | Direct regparm macro is empty only on the new Apple path. |
| `src/PC/universal/com_sndalias.c` | Apple-only SDK strcpy declaration and valid external function spelling instead of undefined forceinline. |
| `src/PC/universal/com_sndalias_load_obj.c` | Apple-only regparm suppression and SDK sprintf declaration. |
| `src/PC/universal/surfaceflags.c` | Excludes the i386-address-valued infoParms definition only on Apple LP64; retains unresolved native symbol. |
| `src/PC/win32/cinematics.c` | Apple-only Mach-O section spelling; original ELF section retained. |
| `src/PC/win32/win_net.c` | Apple-only SDK ifconf include/type; original hand-written type retained. |
| `src/PC/win32/win_shared.c` | Apple-only ordinary timeGetTime declaration; original DLL attribute retained. |
| `src/Mac/DirectX_9/CDirect3DDevice.c` | Apple LP64 uses the extra assembler symbol prefix. |
| `src/stubs/agl_stubs.c` | Apple-only unsupported extension no-op and dedicated SDL exclusion. |
| `src/stubs/agl_stubs.h` | Apple-only framework GL headers; original include retained. |
| `src/stubs/link_stubs.c` | Apple-only Mach-O UI alias. |
| `src/stubs/macos_compat.c` | Apple-only GL headers, SDL legacy context/swap, X11 exclusion and dedicated SDL exclusion. |
| `src/stubs/material_tech_names.c` | Apple-only Mach-O aliases and `_string`. |
| `tools/macos-port/collect_diagnostics.py` | New opt-in diagnostic tool; no engine build effect. |
| `tools/macos-port/lp64_inventory.py` | New opt-in inventory tool; no engine build effect. |
| `tools/macos-port/check_legacy_guards.py` | New opt-in source guard checker; no engine build effect. |
| `docs/macos-port/lp64-inventory.md` | New inventory and implementation packets; documentation only. |
| `docs/macos-port/lp64-logs/client-warnings.log` | Raw compiler warnings; documentation only. |
| `docs/macos-port/lp64-logs/dedicated-warnings.log` | Raw compiler warnings; documentation only. |
| `docs/macos-port/lp64-logs/diagnostics.tsv` | Unique diagnostic locations/messages/counts; documentation only. |
| `docs/macos-port/lp64-logs/counts-by-category.tsv` | Directory/category matrix; documentation only. |
| `docs/macos-port/lp64-logs/scan-candidates.tsv` | Lexical candidate queue; documentation only. |
| `docs/macos-port/lp64-logs/summary.md` | Generated count summary; documentation only. |
| `docs/macos-port/lp64-logs/undefined-client.txt` | Full undefined linker-name list; documentation only. |
| `docs/macos-port/lp64-logs/undefined-dedicated.txt` | Full undefined linker-name list; documentation only. |
| `docs/macos-port/reports/WS1-macos-build.md` | This report; documentation only. |

No tests were added to mirror conditional syntax changes. The requested full object builds, independent warning replays, symbol inspection, native type probes and inactive-body comparison are the replacement verification. Runtime behavior remains explicitly unverified.

## Orchestrator merge notes

Four focused implementation commits precede the inventory/report commit:

1. `766fa75` — Apple targets and SDK type seam.
2. `70467aa` — declaration/section compile blockers and out-of-scope infoParms exclusion.
3. `c5cfbc4` — portable VM and Mach-O symbols.
4. `74393a9` — SDL legacy GL compilation stubs.

Merge on top of the shared plan baseline; no branch was rebased or merged from another worktree. `CMakeLists.txt` changes only the new branch. Shared headers change by a small SDK mechanism/guard patch; reconcile WS2's typed data against it. Every other source patch is limited to its Apple LP64 conditional path.

WS2 should supply native blobs, import slots/vtables and the deferred infoParms numeric initializer. Do not re-add data32/literals32 or assembly blobs to this Apple branch. WS3 should replace the temporary display/thread/ring-buffer layouts, SDL input bridge and startup/platform boundaries, keeping dedicated free of SDL. WS6 should start with architecture/type agreement, then use the inventory's bounded subsystem packets. WS5 owns binary parity and runtime/reference verification after integration.

The committed warning/archive files total less than 2 MB. Build artifacts and throwaway probes/logs remain ignored in this worktree. No tool or data prerequisites were hidden by substitute stubs.
