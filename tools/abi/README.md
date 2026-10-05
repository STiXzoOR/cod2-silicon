# Native ABI checks

Run from the repository root with Apple Clang, CMake, Python 3 and the local
licensed Mac reference described in `docs/macos-port/PLAN.md`. No additional
Python packages are required. These tools never copy reference binary bytes
or decompiler output into their reports.

```sh
cmake -S . -B build-macos/abi -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos/abi --target cod2_macos -j6
sh tools/abi/check.sh build-macos/abi/compile_commands.json build-macos/abi/check
```

The script exits nonzero for compilation errors, cross-file calling-convention
mismatches, direct/indirect unprototyped floating calls, incompatible renderer
bindings or named/member function casts, and proven wrong import loads. Both
function and callback baseline files are empty. Its four real-Clang tests and six import-pattern tests
check the tooling, including detection of variadic, pointer-width, floating,
arity, C++ overload and generic-vtable-storage cases.

The raw-offset gate preprocesses every `cod2_macos` compile entry with its real
flags, then scans the selected source spelling for literal byte offsets, strides
and allocation/clear sizes. `raw-offsets.json` records exact reviewed lines,
occurrence bounds, classifications and reasons. New/duplicated candidates,
unknown classifications and missing evidence fail; inventory output never
approves source automatically. Run both stock and CoD2x databases. For inspection:

```sh
python3 tools/abi/test_raw_offsets.py
python3 tools/abi/raw_offsets.py COMPILE_COMMANDS --inventory-only --output INVENTORY
```

This conservative scanner also captures harmless text, wire/GPU records and
scalar arithmetic. It scans main translation-unit source, not expanded SDK or
project headers, and cannot prove arbitrary computed pointer arithmetic or
continued reachability assumptions. Review changes that activate dormant code
or alter an approved record's layout. Historical `--base COMMIT` inventories
use current headers/flags (and unchanged generated TUs); they are source-only
comparisons. See [WS38](../../docs/macos-port/reports/WS38-lp64-audit.md) for measured
layouts, native fixes and acceptance results.

The optional third argument overrides the reference binary path:

```sh
sh tools/abi/check.sh COMPILE_COMMANDS OUTPUT_DIRECTORY /path/to/cod2mp_mac_1.3_i386
```

`functions.json` contains every extracted declaration/definition with source
location, signature, linker name and ABI classes, plus direct unprototyped calls,
indirect calls and function casts. Apple SDK declarations provide the contracts
for imported APIs whose definitions live in dylibs. `callbacks.json` inventories
all renderer fields and bindings, named/member casts and indirect floating
dispatch. Generic `fnptr_t` conversions in vtable array initializers retain
addresses and are inventoried as storage; actual signature casts remain checked.
`imports.json` records all import symbols, STABS shapes, next-symbol extents,
lexical and preprocessed use sites, and zero-storage declarations/candidates.
STABS decides pointer versus object shape; the extent alone includes padding.

Additional coverage and legacy guard checks:

```sh
python3 tools/abi/excluded.py build-macos/abi/compile_commands.json \
  --output build-macos/abi/excluded.json
python3 tools/abi/legacy.py --base BASE_COMMIT \
  --compile-commands build-macos/abi/compile_commands.json
```

Excluded sources are syntax-probed separately, including the three CoD2x units
with their feature enabled, and are never mixed with active replacement
definitions. Foreign SDK and replaced-source errors are retained in JSON.
The legacy check compares changed C/header bodies in five option-off
configurations using the existing include-free guard checker, then optionally
compares SDK preprocessing tokens of changed compiled C sources with
`COD2_X64` removed, retaining real __LINE__ values and string literals while
ignoring only layout whitespace. Unavailable foreign SDK headers are reported
as errors. These are compiler-input checks; they do not substitute
for an actual i386 object/executable comparison.

The audit uses the supplied build's feature flags. Audit feature-on databases
before enabling optional features. It distinguishes pointer, integer-width,
floating, variadic, count, return and tagged aggregate contracts. It does not
prove complete aggregate field layout, narrow-integer signedness/extension,
arbitrary import aliasing, dynamic vtable object identity, initialization order,
or runtime reachability. Unprototyped integer/pointer calls remain inventoried.
Use the WS12 report and its companion inventories for reviewed limitations.

Outputs and caches belong under ignored `build-macos/`; never commit generated
ASTs or local licensed-reference output. To rerun a single stage, each Python
tool supports `--help`. `audit.py --target cod2_macos.dir` limits the compile
database commands; the default checks every listed native project command.
