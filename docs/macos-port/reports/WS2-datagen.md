# WS2 — typed data generator

Current status: the follow-up section below records the corrected LP64 scalar
output and successful integrated arm64 client link. The original report is
retained as the initial coverage and migration record.

Branch: `port/datagen`. Base: `1cb3516` (upstream `410342a`). Verified on
2026-10-03 with Python 3.14.7 and Apple clang 21. No pushes, remote changes,
package installations, or sibling-worktree access. Generated sources, objects,
debug metadata, and reference probes stay in ignored `build/x64_gen/`.

## Result and limits

The generator reproduces all allocated section bytes and all 3,979 relocations
of the three original assembly blobs on i386. It also reproduces the current
native generated artifacts: 11,091 allocated bytes and 946 relocations. Both
source sets compile for arm64. Coverage distinguishes genuine types from
explicit fallbacks; reproducing a questionable source relocation does not
establish its LP64 correctness.

262 of the 295 remaining raw BSS arrays are retyped, and the other 33 are
listed below. The existing 899 C declarations are retained. All 295 raw-slot
probes compile on i386 and arm64. Complete generated BSS compilation passes
on arm64 with the already-merged WS1 headers plus this branch's small Darwin
`jmp_buf` guard. A whole-BSS i386 section comparison remains blocked by the
missing i386 Linux libc sysroot. No engine link, launch, or runtime parity is
claimed.

## Upstream study and direction

Read `src/blobs/{data.S,data.c,bss.c,literals.S,import_pointers.S,import_pointers.c}`,
the native/web/win generated artifacts, gfx DLL generation inputs, and
`src/stubs/{arch64_compat.c,link_stubs.c,missing_c_syms.c}`. Inspected the
15 September history, especially:

- `acccfb7`: removes `imp_` casts/indirection from engine users, replaces raw
  storage with typed engine definitions, and modernizes function signatures.
- `f52f550`: reconstructs the FX hierarchy's real vtables and removes duplicate
  stub definitions; compiler-laid-out pointer slots replace fixed byte headers.
- `272f5e9`: makes display/context handling aware of the x64 ABI.
- `d137600`: compares normalized typed simulation fields across architectures.
- `410342a`: fixes initialization of `prim.boltFrame`, illustrating that real
  initialized engine state must survive the migration.

The apparent Stage 2 direction is to move definitions into their owning engine
translation units, use shared header declarations and direct symbol references,
and let the compiler determine ABI layout. The native artifacts deliberately
omit many objects now defined elsewhere and retain numerous one-byte literal,
RTTI, and vtable placeholders. The generator therefore has two outputs: full
assembly-derived sources for the reference proof, and production sources based
on the current `build/native_gen/` object set. Linking the full reference set
would reintroduce migrated definitions.

Searched current files and `git log --all --stat` for generator history.
Comments reference `scripts/gen_data32.py` and `scripts/fix_data_pointers.py`,
but those scripts are absent from the tracked upstream files and history.
Existing web/win/gfx outputs follow the same generated-directory convention;
they are not changed. Native artifacts are compiled to ELF for inventory and
initializer extraction rather than duplicating their omission/alias logic in
a new assembly parser. Both original `data.S` and current native artifacts
already contain symbolic startup pointers; these are preserved.

## Implementation

`tools/datagen/stabs.py` reads the thin i386 Mach-O `LC_SYMTAB` directly,
including nlist type, value, and description fields. It reads `N_GSYM`,
`N_STSYM`, and `N_LCSYM`, compilation units, continuation strings, and
`N_BINCL`/`N_EXCL` cross-file identities. Type references are scoped by
compilation unit and include identity. The grammar covers typedef aliases,
ranges/builtins, enums, arrays, pointers/references, qualifiers, functions,
function pointers, structs/unions, tag cross-references, and C++ method
metadata that does not contribute storage. This reference yields 82,983 type
definitions and 2,109 variable entries. There are 2,002 unsupported/malformed
nested parse attempts, recorded by reason in `coverage.json`; this is not a
claim that the complete C++ STABS grammar is supported.

`layout.py` accepts only layouts whose natural i386 member offsets, widths,
aggregate size, and alignment agree with the debug information. It rejects
bitfields, inheritance/non-C fields, unavailable types, and unusual layouts.
`c_headers.py` prefers matching existing engine declarations, checking field
names, offsets, widths, and pointer/scalar/array structure. It reuses 67 engine
aggregate types, including `NetField`, `WeaponDef`, `PlayerKeyState`,
`scr_const_t`, and `scrVarPub_t`. Standalone blobs receive only the required
declaration slices; BSS uses the engine headers themselves. Equal-width integer
types may use the modern engine declaration (`int` rather than legacy `long`)
when the i386 layout and signedness match.

Initializer bytes come only from repository sources. Pointer fields use
symbol addresses or function names. Interior references with an unambiguous
debug target become addresses of C fields/array elements, so their addends
follow the target ABI. Fourteen references in each data source set are
translated this way, including `playerKeys`, `scr_const` controller names,
and barrel tags. Forty-four original-data and ten native-data nonzero addends
remain unresolved in fallback owners and retain their source byte addends.
Some source relocations appear in debug-declared integer fields; they may be
artifacts of the old pointer-fixing process and are not silently reinterpreted.

Finite float/double literals use exact hexadecimal C constants, including
signed zero. Source-documented import slots, string literal storage, SSE masks,
and two-word vtable headers are typed without claiming missing debug entries.
Intersymbol padding is emitted only on i386. Fallbacks use explicitly marked
`DATAGEN_FALLBACK` byte/pointer storage, preserving i386 bytes and relocations.
They are not counted as successful types. Unknown pointer pointees remain
opaque; unreused function-pointer signatures use a storage-only generic
prototype. These limits need further migration work before runtime correctness
can be claimed.

## Coverage

Objects count storage definitions, excluding aliases. `data.S` includes one
private local object (`pm_str_endparty`); import pointers have 144 aliases.
Typed relocations count relocations inside successfully typed definitions.
The round-trip also compares every fallback relocation.

| Input authority | Typed objects / total | Typed pointer relocations / total | Relocations reproduced |
| --- | ---: | ---: | ---: |
| `src/blobs/data.S` | 222 / 245 | 2,868 / 3,068 | 3,068 / 3,068 |
| `src/blobs/literals.S` | 486 / 490 | 31 / 31 | 31 / 31 |
| `src/blobs/import_pointers.S` | 880 / 880 | 880 / 880 | 880 / 880 |
| **Assembly total** | **1,588 / 1,615** | **3,779 / 3,979** | **3,979 / 3,979** |
| `build/native_gen/data32.c` | 78 / 84 | 26 / 36 | 36 / 36 |
| `build/native_gen/literals32.c` | 9 / 480 | 30 / 30 | 30 / 30 |
| `build/native_gen/import_pointers_native.c` | 880 / 880 | 880 / 880 | 880 / 880 |
| **Production total** | **967 / 1,444** | **936 / 946** | **946 / 946** |

BSS inventory: 1,194 declarations = 899 upstream declarations + 262 recovered
types + 33 raw fallbacks. The 899 include upstream `BSSINT`, pointer, aggregate,
and `jmp_buf` choices; they were retained, not independently proven by STABS.

## Verification commands and evidence

```sh
./tools/datagen/roundtrip_test.sh
```

Passes eight focused grammar/header-reuse tests, assembles all three original
blobs with `clang -target i386-unknown-linux-gnu -c`, compiles the generated
counterparts with `-ffreestanding -std=c11`, and saves `xcrun llvm-objdump -s -r`
records. The standard-library ELF reader compares section bytes and relocation
offset/type/target/addend, plus all original symbol addresses/bindings. Results:

| Reference sections | Bytes identical | Relocations identical | Original symbols identical |
| --- | ---: | ---: | ---: |
| `data.S`: `.data` | 56,724 | 3,068 | 246 |
| `literals.S`: `.rodata` + `.data` | 2,281 + 3,840 | 0 + 31 | 490 |
| `import_pointers.S`: `.data` | 3,520 | 880 | 1,024 |
| Native data: `.bss` + `.data` | 624 + 5,324 | 0 + 36 | 84 |
| Native literals: `.bss` + `.data` | 471 + 1,152 | 0 + 30 | 480 |
| Native imports: `.data.rel.ro` + `.data` | 3,500 + 20 | 875 + 5 | 1,024 |

For native reference compilation only, `common_types.h` is replaced with
compiler-builtin `uintptr_t` and the missing `scrMemTreeGlob` extern. The packed
definitions and initializers are otherwise unchanged. The script compiles all
six initialized generated files for `-target arm64-apple-macos`, compiles the
295-slot BSS probe for both targets, and checks compiler-produced Mach-O data:

- `keys -> playerKeys.keys`: old packed native source keeps addend 292 on
  arm64; typed C produces 296.
- `__ZTV12CVertexArray`: first function relocation moves from offset 8 to 16,
  with an eight-byte arm64 relocation.
- A second generation leaves all 14 generated text files byte-identical.

The three initialized production files also compile with `-std=c99`, matching
the engine's CMake setting.

```sh
cmake -S . -B build/x64_gen/cmake -DCOD2_X64=ON \
  -DCOD2_WWWDL_LIBS= -DCOD2_LIBSTDCPP=c++
cmake --build build/x64_gen/cmake --target cod2_datagen -j2
```

Configuration and generation target pass. Checked `compile_commands.json`:
exactly the four generated production files are selected, with original
`src/blobs/bss.c` and `build/native_gen/` files excluded. This verifies source
selection and generation, not an engine build: this branch still has the old
Linux-native target flags/platform path that WS1 replaces on macOS.

Full BSS probe on this branch:

```sh
clang -target arm64-apple-macos -DCOD2_X64=1 -ffreestanding -std=c99 \
  -fsyntax-only -ferror-limit=0 -Isrc -Isrc/headers \
  build/x64_gen/bss_native.c
clang -target i386-unknown-linux-gnu -ffreestanding -std=c99 \
  -fsyntax-only -Isrc -Isrc/headers src/blobs/bss.c
```

The arm64 probe before the jump-buffer fix reported exactly the same eight
errors as original `bss.c`: seven Darwin typedef/structure collisions and the
legacy `jmp_buf[39]`. The i386 command fails because `string.h` is unavailable;
there is no i386 Linux libc sysroot. `llvm-readelf` is also unavailable, so the
requested Xcode `llvm-objdump` fallback is used. No tools were installed.

To verify the BSS integration without reading another worktree, exported only
the tracked `src/headers/` tree from merged `port/main` commit
`d804144b828751448886994e8cfe05f122b77fe5` into the ignored local probe directory,
then overlaid this branch's `com_math.h` guard. The complete generated BSS
compiled successfully:

```sh
clang -target arm64-apple-macos -DCOD2_X64=1 -ffreestanding -std=c99 \
  -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
  -Ibuild/x64_gen/merged-header-probe/src \
  -Ibuild/x64_gen/merged-header-probe/src/headers -Isrc \
  -c build/x64_gen/bss_native.c -o build/x64_gen/arm64-merged-bss_native.o
```

`com_math.h` now selects SDK `<setjmp.h>` only for Apple + `COD2_X64`.
Compared its old/new ILP32 preprocessor output with the identical
`cod2_fwd.h` include omitted to avoid that missing sysroot: all 18,426 bytes
were identical. All other `src/`, original blob, native/web/win/gfx generated
files remain unchanged. The CMake hook is entirely under `if(COD2_X64)`.
This establishes unchanged 32-bit input/source selection; a full 32-bit engine
build comparison still needs the external verification environment.

## Failure list

The exact per-object list is regenerated in `build/x64_gen/fallbacks.txt` and
`coverage.json`. No initialized payload or STABS dump is committed here.

Original data (23 objects):

| Reason | Objects |
| --- | --- |
| Ambiguous static types | `sResult`, `sSystemCursorVisible` |
| Nonzero pointer without symbolic relocation | `sCurrentWinCursor` |
| Missing debug variable | `__ZZN7WinIconC4EvE7sNextID`, `__ZZN9WinCursorC4EvE11sNextHandle`, `pm_str_endparty`, `gc_orders` |
| Symbolic relocation in a non-pointer field | `sGerman_ISO_VK_Map`, `sFrench_ISO_VK_Map`, `sANSI_VK_Map`, `FastTranslateTbl`, `sD3DTextureOpToOpenGL`, `infoParms`, `s_XModelSurfaceSize`, `virtualKeyConvert`, `dlText`, `g_encoder_samplerate`, `g_sound_recordFrequency`, `inflate_mask`, `fixed_td` |
| Debug type larger than storage | `serverStatusDvars`, `ucmds` |
| Relocation beyond debug type | `cg_shock_dvar_names` |

Original literals (4 objects): `refEntIsInWorldSpace` and `sign` have debug types
larger than their storage; `__ZTI14COpenGLTexture` and `__ZTI17IDirect3DTexture9`
have no usable debug variable.

Native data (6 objects): `cod2_d32_environ` and the two mangled WinIcon/WinCursor
statics above lack a matching debug variable; `dlText`, `g_encoder_samplerate`,
and `fixed_td` contain relocations in debug-declared non-pointer fields.

Native literals (471 objects): 331 `lit4_*` and 119 `lit8_*` definitions are
upstream one-byte placeholders. This rule names all 450 floating-literal
failures; the remaining 21 failures are:

- Types larger than storage: `str_002157b8`, `bg_numItems`, `faceAxis`,
  `iSlotPreferenceOrder`, `sign`, `color`.
- No usable debug variable for one-byte storage: `sse_float_abs_mask`,
  `sse_float_sign_mask`, `__ZTI14COpenGLTexture`, `__ZTI17IDirect3DTexture9`,
  `__ZTV10COpenGLVAO`, `__ZTV12IncludeClass`, `__ZTV14COpenGLTexture`,
  `__ZTV15CColorConverter`, `__ZTV17IDirect3DTexture9`, `__ZTV18IDirect3DResource9`,
  `__ZTV21IDirect3DBaseTexture9`, `__ZTV21IDirect3DCubeTexture9`,
  `__ZTV22IDirect3DVertexShader9`, `__ZTV23IDirect3DVolumeTexture9`, `__ZTV8IUnknown`.

BSS (33 objects):

| Reason | Objects |
| --- | --- |
| Ambiguous static types (10) | `buf_00482a80`, `buf_017dd900`, `initialized`, `initialized_00f00780`, `s`, `sResult_00334b10`, `sResult_00334b2c`, `sSystemCursorVisible_00334e84`, `string_00f0ec60`, `string_00f3b9c0` |
| Debug type exceeds original array (3) | `g_largeLocalBuf`, `svs`, `sys_packetReceived` |
| Missing debug variable (1) | `__ZGVZ16GetMacGameEnginevE13theGameEngine` |
| Non-C member names/inheritance (5) | `cg_entitiesArray`, `g_clients`, `lockPvsViewParms`, `s_backEndData`, `sv` |
| Non-native i386 aggregate size (3) | `scrMemTreeGlob`, `scrStringGlob`, `scrVarGlob` |
| Unresolved C++ containers (2) | `__ZN13CMemoryBuffer20sDelayedFreeRequestsE`, `__ZN12CStreamSound10sQTStreamsE` |
| Unresolved numbered types (9) | `theGameEngine`, `__ZN10CVAOPacket11sAllPacketsE`, `__ZN10CVAOPacket14sGenericPacketE`, `sShaderPrograms`, `sCursorList`, `sStdConverterARGB`, `__ZN6CFence15sUnusedFenceIDsE`, `__ZN7COpenGL7sOpenGLE`, `sRectList` |

## Merge notes and next steps

The CMake edit is a five-line native `COD2_X64` hook plus a separate
`cmake/datagen.cmake` module. The only shared header change is the three-line
Apple/LP64 jump-buffer guard in `PC/universal/com_math.h`. No original blob or
engine definition was edited. Commits are focused and can be merged in order:
`c011165`, `602495b`, `3a86c3f`, `5301a9e`, `7345c80`, `bb542e5`, then this report.

WS1's merged macOS target bypasses the old native branch. Before its
`foreach(target cod2_macos cod2_macos_ded)` in `cmake/macos-arm64.cmake`, the
orchestrator must connect this module:

```cmake
include(${CMAKE_SOURCE_DIR}/cmake/datagen.cmake)
cod2_generate_typed_blobs(MACOS_GEN_C)
list(APPEND MACOS_C ${MACOS_GEN_C})
```

Keep original `src/blobs/bss.c` out of that target if its source discovery is
expanded later. Do not append the full assembly-derived files. The module
requires the local reference binary; set `COD2_STABS_BINARY` if its path differs.
It does not ship a reference binary or checked-in generated data. On x86_64
Linux, upstream `arch64_compat.c` also defines `TheStringPackage`, CSound callback
slots, and `__ZTV12IncludeClass` that the current native artifacts contain;
resolve that existing duplicate ownership before claiming an x86_64 link. The
shim is inactive on arm64.

Next work:

1. Merge the WS1 headers, the jump-buffer guard, and the macOS hook; regenerate
   and compile all four production files in the integrated target. Then
   inventory remaining link ownership/undefined symbols.
2. Restore native placeholder objects only after checking whether their engine
   owner now supplies the definition. Promote literals/vtables from the full
   output selectively; do not replace the native object set wholesale.
3. Investigate relocations in integer fields against the source and debug type
   before changing them. Resolve ambiguous statics using symbol/source/address
   identity, then extend unsupported C++/bitfield handling where needed.
4. Retype the remaining BSS aggregates with the owning subsystem, especially
   client/server state and script globals. Add an actual whole-BSS round-trip
   when the verification environment has an i386 Linux libc sysroot.
5. Run the existing normalized statehash/parity harness after linking. Passing
   object-layout checks alone does not prove engine runtime equivalence.

## Follow-up — Steam scalar recovery and successful client link

Merged `port/main` first, fast-forwarding to `5d5c331`. This includes WS1–WS5,
the macOS generator hook, the `__aarch64__` guards, the Apple `infoParms`
definition, Mach-O seam aliases, and the post-merge duplicate/link fixes. This
section supersedes the earlier LP64 failure entries for the thirteen objects
whose debug-declared scalar fields contained symbolic relocations. Their
original i386 fallbacks remain deliberately unchanged.

The requested `cod2_macos` build now links. Before this change, the same command
failed at `_dlText+0xC`. After the changes below, it produces an arm64 Mach-O
executable with `NOUNDEFS`. No client launch or runtime parity test was performed
in this follow-up, and the dedicated import-pointer problem was not modified.

### Reference decision and recovery rules

The separate value source is the user's licensed Steam Mac executable:

```text
~/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer
```

Its SHA-256 is
`a6aa70d4e2b0bf5f388b0653752847bddac95398ee0d860e757ec79e466f5d1b`.
Direct `LC_SYMTAB` inspection confirms 10,243 non-STABS symbols and 33,586 STABS
entries. These STABS entries have names/addresses but no usable type
definitions: the existing parser obtains zero types and zero typed variables.
Consequently `COD2_STABS_BINARY` remains the original full-debug reference. It
was not silently switched to Steam. `COD2_VALUES_BINARY` is a new, independent
CMake cache path; its default is the Steam path above under `$HOME`. The
generator accepts `--values-binary`, and the round-trip script accepts the
`COD2_VALUES_BINARY` environment variable. Both binaries are generation
dependencies, and neither is copied into the repository.

`macho32.py` reads `LC_SEGMENT` sections and the symbol table directly. Matching
uses symbol names: the platform underscore, Itanium file-static `__ZL...`
names, and unique function-static `__ZZ...E<length><name>` suffixes. Ambiguous
matches fail. No old `str_XXXXXXXX` address is used to find a Steam object.
Storage reads are bounded by the section and next symbol, with logical sizes
from the full-debug STABS. The `infoParms` size additionally follows its
existing engine definition of 54 entries, including two zero sentinels; the
old debug entry describes only 53.

For every recovered object, generation compares all nonzero, non-relocated
source bytes against the corresponding Steam storage. Source-only tail padding
must contain no nonzero unrelocated bytes. **No mismatches were found**, in
either the full assembly or production artifact instances. Any mismatch,
missing/ambiguous symbol, or incompatible extent fails generation rather than
choosing a value silently.

`recover.py` retains relocations only at debug-declared pointer fields. Scalar
and byte-array data come from Steam; real pointers keep the repository's
symbol/field references and addends, not Steam addresses. The generated source
selects the original definition for `__SIZEOF_POINTER__ == 4` and the recovered
definition for LP64. Remaining unknown fallback pointer storage is packed only
on i386 and naturally aligned on LP64. The original blob sources and native
artifacts were not edited.

### Objects recovered

| Object | Steam symbol/address | Value bytes | False relocations removed on LP64 |
| --- | --- | ---: | ---: |
| `sGerman_ISO_VK_Map` | `_sGerman_ISO_VK_Map`, `__DATA,__data` `0x375200` | 128 | 2 |
| `sFrench_ISO_VK_Map` | `_sFrench_ISO_VK_Map`, `__DATA,__data` `0x375180` | 128 | 2 |
| `sANSI_VK_Map` | `__ZL12sANSI_VK_Map`, `__DATA,__data` `0x375280` | 128 | 2 |
| `FastTranslateTbl` | `__ZL16FastTranslateTbl`, `__TEXT,__const` `0x300090` | 8 | 1 |
| `sD3DTextureOpToOpenGL` | `__ZL21sD3DTextureOpToOpenGL`, `__TEXT,__const` `0x300020` | 60 | 4 |
| `infoParms` | `_infoParms`, `__DATA,__data` `0x37c4c0` | 1,080 | 10 |
| `s_XModelSurfaceSize` | `__ZL19s_XModelSurfaceSize`, `__TEXT,__const` `0x301cb8` | 8 | 1 |
| `virtualKeyConvert` | `__ZL17virtualKeyConvert`, `__TEXT,__const` `0x30ea60` | 292 | 8 |
| `dlText` | `__ZZL22UI_DisplayDownloadInfoPKcffP6Font_sfE6dlText`, `__DATA,__data` `0x37e8e4` | 16 | 1 |
| `g_encoder_samplerate` | `_g_encoder_samplerate`, `__DATA,__data` `0x3809e4` | 4 | 1 |
| `g_sound_recordFrequency` | `_g_sound_recordFrequency`, `__DATA,__data` `0x3809ec` | 4 | 1 |
| `inflate_mask` | `_inflate_mask`, `__DATA,__data` `0x381ce0` | 68 | 4 |
| `fixed_td` | `_fixed_td`, `__DATA,__data` `0x382d40` | 256 | 8 |

The addresses above document the lookup result; they are not hardcoded in the
generator. `FastTranslateTbl` has eight logical debug bytes followed by zero
blob padding; that padding is also cross-checked where reference storage exists.

Notable true values:

- Steam's complete `dlText[16]` is `EXE_DOWNLOADING` plus its terminating NUL.
  The task's `EXE_DOWNLOAD` comes from the truncated `src/blobs/data.c`
  reconstruction. The false relocation at byte 12 covers `ING\0`.
- Both sample-rate globals contain integer `8192`, not a pointer to the Mach-O
  header plus 4096.
- The four falsely relocated final `inflate_mask` entries are `8191`, `16383`,
  `32767`, and `65535`.
- The eight falsely relocated `fixed_td` base fields are `4097`, `16385`,
  `8193`, `24577`, `6145`, `24577`, `12289`, and `24577`.

The production native object set contains only `dlText`, `g_encoder_samplerate`,
and `fixed_td` from this category, so its ten false fixups are removed. The
other ten definitions remain owned by their existing engine translation units
or omitted as upstream intended. In particular, `infoParms` is still absent
from `data_native.c`; `surfaceflags.c` owns its Apple definition. The full
reference output has a corrected table for independent validation and is not
linked into the engine.

### Alignment and value checks

`check_alignment.py` scans compiler-produced Mach-O relocations and rejects
every eight-byte `ARM64_RELOC_UNSIGNED` fixup whose actual section address plus
offset is not divisible by eight. It excludes DWARF/debug sections, whose
address attributes are not runtime fixups. Generation automatically compiles
and checks all six initialized output files and the zero-initialized BSS
probe. It also checks the compiled scalar bytes against Steam, including all
four integer fields in each of the 54 LP64 `infoParms` rows. A validation
failure removes the custom command's primary production output so a failed
generation is not cached as successful.

The old packed native data compiled for arm64 has seven alignment candidates:
`dlText+0xC`, `fixed_td+0x1C`, `fixed_td+0x64`, `fixed_td+0xAC`,
`fixed_td+0xF4`, and the real pointer fields at the unaligned object addresses
of `cmd_text` and `legacyHacks`. Correct scalar types remove the false fixups;
the compiler's typed layouts/object alignment correctly place the real ones.
No unaligned runtime pointer relocation remains in any generated output or
in the four actual generated client build objects.

| Generated arm64 output | Pointer relocations checked | Unaligned |
| --- | ---: | ---: |
| Full data | 3,023 | 0 |
| Full literals | 31 | 0 |
| Full imports | 880 | 0 |
| Production data | 26 | 0 |
| Production literals | 30 | 0 |
| Production imports | 880 | 0 |
| BSS probe / actual BSS object | 0 / 0 | 0 |

There are 45 false fixups removed from the full LP64 data, and ten from the
production data. Original i386 relocation counts remain 3,979 and 946.
LP64 typed coverage is now 235/245 full data objects and 81/84 production data
objects. The existing `typed`/`typed_relocations` coverage fields still describe
the unchanged i386 input; new `lp64_typed`, recovery provenance/type, scalar
verification, and alignment fields describe LP64. The fallback list marks
these thirteen failures as **i386 only (LP64 recovered)**. Following the merged
upstream BSS changes, its inventory is 903 retained declarations, 260 recovered
raw arrays, and 31 remaining raw fallbacks (1,194 total).

### Verification and handoff

```sh
./tools/datagen/roundtrip_test.sh
```

Passes all original i386 section-byte, relocation, and symbol comparisons for
the full blobs and production artifacts; twelve tests now cover named scalar
recovery, preservation of genuine pointer references, mismatch rejection,
next-symbol bounds, and rejection of a compiled unaligned pointer while
ignoring debug addresses. The two earlier LP64 address-point tests still pass.
All sixteen recovered instances (thirteen full plus three production) have
compiler-emitted scalar bytes identical to Steam, and all fourteen generated
text files remain deterministic.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY=$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386
cmake --build build-macos -j12 --target cod2_macos
python3 tools/datagen/check_alignment.py \
  build-macos/CMakeFiles/cod2_macos.dir/build/x64_gen/*_native.c.o
file build-macos/cod2_macos
xcrun llvm-objdump --macho --private-headers build-macos/cod2_macos
```

The client build reaches `[100%] Built target cod2_macos`. Actual generated
objects contain 26 + 30 + 880 + 0 aligned pointer fixups. `file` reports
`Mach-O 64-bit executable arm64`; the Mach-O header reports `EXECUTE` and
`NOUNDEFS`. The link retains existing warnings for duplicate `-lc++` and the
common-section alignment reduction; no data-generation link errors remain.
Detailed local logs are `build/x64_gen/{client-before.log,client-after.log,followup-roundtrip.log}`.

No shared engine/header definitions or original 32-bit blobs were changed in
this follow-up. Merge `d4c00fb` (recovery/validation helpers), `d8a2bb2` (generator
and isolated CMake integration), and this report commit onto the integrated `port/main`;
the macOS hook is already wired. Keep the original `COD2_STABS_BINARY` and
ensure the new `COD2_VALUES_BINARY` resolves to the owned Steam executable (or
set the cache path explicitly). Only generator code, its isolated CMake
module, tests, and documentation are committed; generated payloads remain
ignored. A runtime smoke/parity check remains for the orchestrator and WS5.

The dedicated target still receives the client's complete import-pointer
table, including renderer/client functions excluded from its pruned source
set. It was not built or changed here. Recommendation: generate a separate
dedicated import set from its actual providers and required consumers, and
fail on missing required imports; do not add renderer stubs merely to satisfy
unused client slots. Remaining raw BSS/literal placeholders still need the
owning subsystem's runtime migration, independently of this successful client
link.
