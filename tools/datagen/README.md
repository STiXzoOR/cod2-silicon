# Typed data generation

Python 3 (standard library only) reads types directly from the user-supplied
i386 Mac binary's `LC_SYMTAB` STABS records. The repository's blobs remain the
i386 byte/relocation authority. A separate user-owned Steam Mac binary supplies
verified LP64 scalar values for the thirteen known false-relocation objects.
Clang must
support `i386-unknown-linux-gnu` and `arm64-apple-macos`; verification uses
Xcode's `llvm-objdump`. No packages are installed by these tools.

From the repository root:

```sh
./tools/datagen/roundtrip_test.sh
```

The reference defaults to
`$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`. Override it with
the first argument or `COD2_STABS_BINARY`. The optional second argument is the
output directory; the default, `build/x64_gen/`, is ignored by git. `CLANG`,
`PYTHON`, and `OBJDUMP` can select existing tools.

The value source defaults to
`$HOME/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer`.
Override it with `COD2_VALUES_BINARY` or the generator's `--values-binary`
argument. This does not change the STABS type source: the Steam file has no
usable type definitions. Generation matches data symbols by name (including
C++ static mangling), bounds reads by STABS size and the next symbol, and fails
if any nonzero, non-relocated source byte disagrees with the value reference.
Only scalar bytes are recovered; genuine pointers retain repository symbol
references rather than Steam addresses. Proprietary inputs and all generated
payloads remain local.

For generation alone:

```sh
python3 tools/datagen/generate.py \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
```

Generated output is local and must not be committed:

| Output | Authority and use |
| --- | --- |
| `data.c`, `literals.c`, `import_pointers.c` | Complete original `.S` objects, including private storage and aliases; reference round-trip and migration inventory. |
| `data_native.c`, `literals_native.c`, `import_pointers_native.c` | Current `build/native_gen/` object set, preserving Stage 2 omissions/placeholders and original i386 initializers; LP64 scalar corrections use the verified value source. |
| `bss_native.c` | Original `src/blobs/bss.c` with recoverable raw arrays retyped only under `COD2_X64`; existing engine declarations retained. |
| `bss_probe.c` | Standalone layout compilation of the remaining raw BSS slots, with i386 size assertions. |
| `typed_types.h` | Required declarations; matching engine header types are preferred over generated anonymous aggregates. |
| `coverage.json`, `fallbacks.txt` | Per-object type provenance, failures, engine type reuse, omitted objects, and translated/unresolved address addends. |
| `original-*`, `reference-native-*`, `generated-*`, `arm64-*` | Local verification sources, objects, and `llvm-objdump -s -r` records. |

The round-trip script compares allocated ELF section bytes, every `R_386_32`
relocation's offset/target/addend, and original symbol addresses and bindings.
It checks both the full assembly blobs and the production native artifacts,
compiles both source sets for arm64, checks two concrete LP64 address points,
compiles the BSS probe on both architectures, and regenerates all text outputs
to check determinism. Original intersymbol padding is emitted only on i386.
The generator itself compiles all initialized output and the BSS probe for
arm64, then fails if any eight-byte pointer relocation is unaligned. Remaining
fallback pointer fields use natural alignment on LP64; packing is retained only
on i386. Debug addresses are excluded from this runtime fixup check.

Scan actual build objects with:

```sh
python3 tools/datagen/check_alignment.py \
  build-macos/CMakeFiles/cod2_macos.dir/build/x64_gen/*_native.c.o
```

Full `bss_native.c` compilation requires the engine headers. After the macOS
header fixes are merged, check it separately with:

```sh
clang -target arm64-apple-macos -DCOD2_X64=1 -ffreestanding -std=c99 \
  -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
  -Isrc -Isrc/headers -c build/x64_gen/bss_native.c \
  -o build/x64_gen/arm64-bss_native.o
```

The BSS probe is not a whole-BSS round-trip: old byte arrays often include
padding, and the full source requires an i386 libc sysroot unavailable on this
Mac. Original 32-bit source files and CMake selection stay unchanged.

`cmake/datagen.cmake` exposes `cod2_generate_typed_blobs(output_var)` and the
`cod2_datagen` target. Its cache settings are `COD2_STABS_BINARY`, `COD2_VALUES_BINARY`,
`COD2_TYPED_DATA_DIR`, and `COD2_DATAGEN_CLANG`. The function returns the four
production C files. The existing native CMake branch calls it under `COD2_X64`
and removes the original `src/blobs/bss.c` from that source set. The separate
macOS module also calls the function and appends its returned sources.

Conservative failures are explicit `DATAGEN_FALLBACK` definitions. They retain
all source bytes and symbolic relocations on i386, but do not establish correct
LP64 semantics. The thirteen known objects with relocations in debug-declared
integer/byte fields have a corrected LP64 branch; their original i386 fallbacks
remain intact for the round-trip. Upstream native literals still include
hundreds of one-byte placeholders. Pointer
pointees with unavailable C++ types remain opaque, and function pointers whose
engine declaration cannot be reused use a generic storage-only prototype.
`coverage.json` separates typed relocation coverage from total reproduced
relocations. Consult the WS2 report for the verified counts and next steps.
