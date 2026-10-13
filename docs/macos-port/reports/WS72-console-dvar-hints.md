# WS72 — Console dvar hints and bounded domain descriptions

2026-10-07, base `0f07adc`, branch `codex/console-hints-fidelity`.

Native detailed dvar completion now displays the original name/current-value
row, optional latched-value row, labeled default, and a separate type/domain box.
Enum argument completion runs only for enum variables. The previous implementation
used three/four separate unlabeled value lines, omitted the domain entirely, and
passed every variable's domain union to enum completion. Existing CoD2x wider
name/value columns remain intentional extensions.

## Independent reference

The owned Mac 1.3 `ConDrawInput_DetailedDvarMatch` at `0x15f646` was inspected
in private disassembly. Its first box has two lines, or three with a latched value;
the name/current value share a row with a 200-unit value offset. `0x15fa66` draws
`  latched value`, then the default branch at `0x15f823` draws `  default`, using
the inactive-value color for labels and values. After one blank line,
`Dvar_DomainToString_GetLines` at `0x15f960` fills a 1024-byte domain buffer.
The domain box uses returned extra-line count plus one. The enum-type check at
`0x15fa51` precedes the two-argument completion call at `0x15fb80`.

The existing native domain formatter advances its enum cursor by `snprintf`'s
would-have-written length. A long option can move it beyond the supplied buffer,
then the next option writes out of bounds. The new hint path uses that formatter,
so this unit also bounds its native cursor and stops on truncation; zero/negative
buffer capacity returns without writing. Actual emitted partial enum lines count
for box height. Ordinary descriptions remain unchanged. The original disassembly
also has unchecked advances; truncation safety is an explicit native robustness
correction, not a claim of reproducing undefined reference behavior. Legacy
inactive source is preserved.

## Evidence

`tests/fixes13/console_hints.c` runs the production hint routine and production
domain formatter/helpers. Synthetic draw capture and dvar lookup/value services
isolate text, row positions, colors, box sizes and completion dispatch. It covers
bool, bounded int, bounded float, enum and string with/without a latched value,
prefix/exact-match rejection and enum-only argument completion. Normal formatting
expectations are independent strings and coordinates.

The pre-change layout fixture produced 109 failed assertions. A separate long
enum case with a 64-byte heap buffer reproduced an AddressSanitizer heap-buffer
overflow. Corrected stock and CoD2x pass both layout and bounded enum cases under
assertions and ASan. Bounds cases include a 3000-byte option, capacities 1–32,
untouched trailing canaries, returned cursor range and zero-capacity no-write.

The actual Release compiler commands pass all three affected client/dedicated
translation units per mode (console client plus both dvar targets). Scoped ABI
checks report three TUs per mode, zero errors/mismatches/new findings. Ten legacy
source configurations, five generic native-off configurations, and all three
actual CoD2x SDK commands with the native definition removed compare identically.
`git diff --check` passes. No checker or baseline changed.

Run `python3 tests/fixes13/run.py --build <build> --test console_hints`. For prior
behavior add `--baseline 0f07adc`; its normal failure stops the runner before the
overflow case, which was invoked separately from the generated baseline binary.
Private logs are under `~/Library/Application Support/CoD2-native-protocol/validation.noindex/console-*`
and `ws72-console/`. No proprietary input is committed.

This proves the production draw-command contract with synthetic dependencies,
not live console rasterization, long enum menu clipping or every dvar domain.
The finite fixture does not certify the complete formatter/parser. Final live
console acceptance and composed engine gates remain parent integration work.
