# WS57: CoD2x item-mask overflow guard

Date: 2026-10-07. Base: `05d426d` (`codex/current-readiness`).
Worktree branch: `codex/cod2x-client-fixes`.

## Change and reference evidence

The native CoD2x client now drops item configstrings whose length is at least
`0x108` before `CG_RegisterItems` copies them or registers item visuals.
The guard requires both `COD2_X64` and `COD2_CODX` to be defined and nonzero.
The original item-mask decoding and stock path remain intact.

The read-only CoD2x reference checkout was clean at
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`. Its `src/mss32/version.rc`
identifies version 1.4.6.8. The behavior was checked directly in
`src/mss32/cgame.cpp:133–139`: the wrapper checks the length of `CS_ITEMS`,
raises `ERR_DROP` at `0x108` or greater, and then invokes the original function.
Lines 252–253 install the wrapper at two engine call sites.
`src/shared/cod2_shared.h:101` defines `CS_ITEMS` as 8.

The native function had an unchecked copy from configstring 8 into
`char items[0x109]`. This implementation independently expresses the reference
threshold as `sizeof(items) - 1`, preserving its rejection of length `0x108`
even though that length plus its terminator fits in the native buffer.
The diagnostic is `CG_RegisterItems: overflowed` with severity `ERR_DROP`.
Native `Com_Error` in `src/PC/qcommon/common.c:347–414` exits this case through
`longjmp`, so the copy and registrations are not reached after the drop.

Changed files:

- `src/PC/cgame_mp/cg_weapons.c`: the five-line feature-gated guard.
- `tests/fixes13/items.c`: a fixture executing the extracted production function.
- `tests/fixes13/run.py`: register the fixture with the existing extraction runner.
- This report.

No third-party implementation was copied. The reference was used to determine
the behavior and exact boundary.

## Regression evidence

The fixture was added before the production change. With `--baseline 05d426d`,
the `0x108` case failed because registration returned without an `ERR_DROP`.
Running that baseline executable with `overflow` selected length `0x109` and
AddressSanitizer reported a stack-buffer-overflow at the original `strcpy`.

The fixture uses the actual preprocessed `CG_RegisterItems`, the real import
shape for `bg_numItems`, and small substitutes for the configstring lookup,
asset registration and nonreturning error boundary. Its hand-derived mask
`a5c0` must register item IDs `1, 3, 4, 6, 10, 11` in order.

| Case | Stock native | Native CoD2x |
| --- | --- | --- |
| Four-character mask | Registers the six expected items | Same |
| Length `0x107` | Registers the six expected items | Same |
| Length `0x108` | Original registration behavior | `ERR_DROP`, zero registrations |
| Length `0x109` | Not executed, exceeds original buffer capacity | `ERR_DROP`, zero registrations |
| Length `0x400` | Not executed, exceeds original buffer capacity | `ERR_DROP`, zero registrations |

Item count is 16, so all tested valid masks cover every byte the decoder reads.
Short-mask and item-count behavior are outside this change.

## Verification

Actual stock and CoD2x engine compile databases were copied from the parent's
private validation directory, remapped to this worktree and separate private
outputs. Engine flags retained `-O3`, `-DNDEBUG`, arm64, macOS 13 minimum,
`-ffp-contract=off` and `-fno-strict-aliasing`; fixture assertions were enabled
by the existing runner. Builds and fixtures ran under `taskpolicy -b nice -n 19`,
with at most three compiler processes at a time. Pure Python tools tests ran
at normal priority.

Private evidence is under
`~/Library/Application Support/CoD2-native-ws57/validation.noindex/`.
The following commands passed from this worktree:

```sh
validation_root="$HOME/Library/Application Support/CoD2-native-ws57/validation.noindex"
taskpolicy -b nice -n 19 python3 tests/fixes13/run.py --build "$validation_root/stock"
taskpolicy -b nice -n 19 python3 tests/fixes13/run.py --build "$validation_root/codx"
python3 tests/fixes13/legacy.py --base 05d426d
python3 tests/cod2x/check_inactive_gates.py --base 05d426d
python3 tools/abi/legacy.py --base 05d426d
taskpolicy -b nice -n 19 python3 tools/abi/audit.py \
  "$validation_root/stock/compile_commands.selected.json" --jobs 2 \
  --output "$validation_root/stock/abi-selected.json" --baseline tools/abi/baseline.json
taskpolicy -b nice -n 19 python3 tools/abi/audit.py \
  "$validation_root/codx/compile_commands.selected.json" --jobs 2 \
  --output "$validation_root/codx/abi-selected.json" --baseline tools/abi/baseline.json
python3 tools/abi/test_audit.py
python3 tools/abi/test_raw_offsets.py
python3 tools/abi/test_bit_conversions.py
python3 -m unittest discover -s tools/tests -v
git diff --check
```

- Both full fixes13 suites passed all 13 test groups under AddressSanitizer.
- The items fixture also passed in both variants with
  `-fsanitize=address,undefined -fno-sanitize-recover=all`, compiled with flags
  inherited from the actual `cg_weapons.c` engine entry. Executables and logs
  are named `items-asan-ubsan*` in each private build directory.
- All six selected release objects compiled: `cg_weapons.c`,
  `cg_servercmds_mp.c` and `common.c` in each variant. The first two files had
  zero warnings. Untouched `common.c` emitted three existing warnings concerning
  a packed attribute, a length conversion and its null-pointer trap.
  `selected-engine-compilation.json` records object sizes and results.
- Both scoped ABI audits reported 3 TUs, zero errors, zero mismatches and zero
  unprototyped floating-point calls.
- Legacy token checks: ten fixes13 configurations, seventeen inactive CoD2x
  configurations and five ABI legacy configurations; zero mismatches.
- Independent `unifdef` comparisons produced byte-identical whole-source output
  for five inactive gate combinations, covering omitted and explicit-zero
  feature macros. Flags and hashes are recorded in private `unifdef.json`.
- ABI checker self-tests: 4 + 3 + 4 passed. Tools suite: 32 passed.
- Both existing full-suite logs contain the unrelated linker warning that
  `__DATA,__common` alignment was reduced from `0x8000` to `0x4000`.

## Limits and handoff

The fixture executes item registration in isolation; it does not execute the
live server-command or graphics-initialization pipeline. The actual
server-command caller was compiled and included in the scoped ABI audit.
No GUI, online session, game process or benchmark was run.

This is scoped validation, not the complete engine link/import/callback ABI
gate or a Linux multilib i386 byte-identity result. Those remain integration
checks. No item-count, short-configstring, graphics, launcher or future-release
feature changes are included.

The four changed files are left uncommitted and unstaged for the parent to
review and integrate. All generated artifacts and logs remain outside the repo.
