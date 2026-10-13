# WS58 — CoD2x MG mouse sensitivity

2026-10-07; base `88a0b28`.

The native CoD2x client applied only stock MG mouse factors of 2.5 horizontally
and 2 vertically. It did not register or apply `sensitivity_MG`, so configured
MG sensitivity and mouse acceleration had no effect while using a heavy weapon.

The current-release source authority is the external CoD2x checkout pinned at
`d8c54695a5239ac99d1dfe96212b809b1a54bfee` (v1.4.6.8). Its
`src/mss32/mouse.cpp:97–106` independently establishes the MG contract:
mouse rate times `cl_mouseAccel`, plus `sensitivity_MG`, followed by the stock
2.5/2 axis factors. Normal sensitivity and the cgame zoom multiplier apply only
to ordinary mouse input. Lines 151 and 158 establish the float registration
(default 1, range 0.01–100, archive plus changeable-reset flags) and the
unconditional client hook. `src/mss32/hook.cpp:189,304` invokes initialization
and patching without a server-revision condition. The native implementation
was authored independently; no reference implementation was copied.

`CL_InitInput` now registers `sensitivity_MG`, and `CL_MouseMove` uses that
contract in the MG branch. Both changes require `COD2_X64` and `COD2_CODX`.
The new value uses the existing native finite-value fallback policy, with 1
as its fallback, and nonfinite combined sensitivity returns without movement.
The existing input buffering, restricted-state check and angular limits remain
in the production path. No dvar pointer is exported and no shared layout changes.

The focused fixture runner inherits the actual engine Release compile command,
remaps its repository paths to the executing checkout, enables assertions and
adds AddressSanitizer/UndefinedBehaviorSanitizer. It extracts complete active
production functions for `CL_InitInput`, `CL_MouseEvent`, `CL_MouseMove`,
`CL_CreateCmd`, its input helpers, `ClampChar` and `Cod2x_GameVersion`; it links
the real `src/PC/universal/dvar.c`. Synthetic services cover command registration,
allocation/string utilities, UI delivery and the unrelated binoculars predicate.
The real caller chain is mouse event → command creation → mouse movement; the
fixture does not substitute a mouse result or copy an expected formula from the
implementation under test.

Before the production fix, the fixture reported 17 independently expected
failures: missing dvar/type/default/domain/flags, configured MG angles,
acceleration, filtering, strafe movement, actual command angles, applicability
in server revision states 0–6, and registration of a preloaded config value.
The same fixture against `--baseline 88a0b28` reproduces those 17 failures.
Stock and CoD2x both pass after the fix. Their checks include default MG input,
both heavy-weapon flag bits, ordinary acceleration and zoom, zero-frame buffer
consumption, restricted movement, cgame axis limits, positive/negative movement
clamping, native finite defaults, command metadata and persisted config values.

Run the behavioral checks from the repository root:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/run_mouse_sensitivity.py \
  --compile-db output/ws47-stock/compile_commands.engine.json \
  --output output/ws58-mouse-stock
taskpolicy -b nice -n 19 python3 tests/cod2x/run_mouse_sensitivity.py \
  --compile-db output/ws47-codx/compile_commands.engine.json \
  --output output/ws58-mouse-codx
taskpolicy -b nice -n 19 python3 tests/cod2x/run_mouse_sensitivity.py \
  --compile-db output/ws47-codx/compile_commands.engine.json \
  --output output/ws58-mouse-baseline-codx --baseline 88a0b28
```

Focused verification evidence:

| Check | Result |
| --- | --- |
| Stock and CoD2x source fixtures, ASan + UBSan | 0 failures in both modes |
| Actual Release `cl_input.c` commands, separate object destinations | Both compile successfully |
| Scoped ABI: `cl_input.c`, `dvar.c`, `com_math.c` | 3 TUs per mode; 0 errors, 0 mismatches, 0 new |
| `tests/cod2x/check_inactive_gates.py --base 88a0b28` | 1 changed production file; 17 configurations; 0 mismatches |
| `tools/abi/legacy.py --base 88a0b28 --compile-commands output/ws47-codx/compile_commands.engine.json` | 5 inactive native configurations and 1 actual SDK command with `COD2_X64` removed; 0 mismatches/errors |
| `unifdef` with native undefined/zero or CoD2x undefined/zero | Baseline and current source tokens identical in all 4 cases |
| `git diff --check` | Pass |

Local evidence is under `output/ws58-mouse-*` and `output/ws58-engine/`.
Compiles ran serially at background policy and nice 19. This work did not run
the game, exercise a physical mouse, benchmark, change TCC/global configuration,
or perform the Linux i386 byte-for-byte artifact comparison. Source inactive-gate
checks supplement that comparison; they do not establish binary identity.
The scoped ABI audit is not the full integration gate. Composed builds, full
review and canonical commits remain the parent task's responsibility.
