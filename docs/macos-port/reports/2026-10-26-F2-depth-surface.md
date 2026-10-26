# F2: native GL depth-surface transitions

The native GL wrapper now disables effective depth testing/writes and stencil
testing/writes when its depth surface is detached, restores configured state
when reattached, and retains absence through both indexed draw paths. Clearing
while detached preserves the physical depth/stencil pixels. Stock and CoD2x
pass 18 synthetic CGL cases each with ASan/UBSan.

## Evidence and scope

Mac 1.3 `CDirect3DDevice::SetDepthStencilSurface`, entry `0xdce0`, disables
depth writes at `de06–de0d`, depth testing at `de4e–de55`, stencil writes at
`de3d–de44`, and stencil testing at `de17–de33`. Attachment restores configured
bits at `dd15–dd49` / `ddac–ddfc`. An already-null surface returns without GL
state changes (`dd52–dd57`). The native implementation previously only assigned
the pointer; both its ARB and fixed-function draws unconditionally reapplied
configured depth state.

`RB_SetRenderTarget` reaches this setter when the target's depth surface differs.
`R_InitRenderTargets` creates the framebuffer depth surface explicitly. Before
that, native `CDirect3D_CreateDevice` zeroes a 4096-byte static allocation, calls
`CDirect3DDevice_Init`, then creates only the backbuffer/render target. It creates
neither `depthBuffer` nor `depthStencil`. `Reset` reconfigures the window and
backbuffer size, leaving these fields alone. Therefore pointer-null alone cannot
mean disabled depth at native startup. An appended native byte records explicit
detachment; Init resets it. Existing field offsets stay unchanged, and the
fixture checks the resulting struct fits the allocation. First attachment
preserves the already-present implicit GL default; explicit detach/reattach
applies the transition. The default-surface getter is unchanged.

Mac `SetRenderState`, entry `0x15e18`, gates depth-enable (`15f18–15f47`),
depth-write (`1607d–160ae`), and stencil-write-mask (`161cd–161fa`) GL updates
on surface presence. The mask is limited to eight bits. Stencil-enable
(`16187–161c8`) is deliberately different: a changed value is applied even
without a surface. That asymmetry, including an ensuing repeated NULL setter,
is preserved and tested. Only stencil enable/write-mask setters and matching
getters were added; other stencil operations remain outside this repair.

Mac constructor C1 writes the initial stencil write mask `0xffffffff` at
`0x1adaa` (C2 does so at `0x1aa02`). Native Init previously left it zero, and
`RB_SetInitialState` supplies no replacement mask. Init now sets this default,
so detach/reattach does not accidentally suppress default stencil writes.
Mac `Clear`, entry `0x141b0`, gates depth/stencil setup at `141d1–141d6` and
restoration at `142ed–142f2` on surface presence. Native Clear now excludes
those flags when detached. Its pre-existing transparent-color-only shortcut
continues to inspect the original requested flags.

Changed production files: `CDirect3DDevice.c`, `lp64_shader_draw.h`.
New fixture files: `tests/lp64/renderer/depth_surface.c` and
`tests/lp64/renderer/run_depth_surface.py`. No changes to sampler/anisotropy,
Metal dispatch, surface allocation, COM ownership, or the depth-surface getter.

## Validation

From the integration checkout, with evidence outside Git:

```sh
EVIDENCE="$HOME/Library/Application Support/CoD2-native-decomp/F2-depth-surface.noindex"
DB=/Users/stix/Projects/cod2-native-wt/decomp-f1-script
# Expected failure: 16 failing transition/default cases, 2 reattach controls pass.
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_depth_surface.py \
  --compile-db "$DB/build-macos/compile_commands.engine.json" \
  --output "$EVIDENCE/red" --baseline d497f60
# Each command passes 18 cases (9 each for ARB and fixed-function draws).
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_depth_surface.py \
  --compile-db "$DB/build-macos/compile_commands.engine.json" \
  --output "$EVIDENCE/green-stock"
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_depth_surface.py \
  --compile-db "$DB/build-macos-codx/compile_commands.engine.json" \
  --output "$EVIDENCE/green-codx"
git diff --check
```

The runner uses the actual engine compiler flags, enables assertions, and adds
ASan/UBSan. Both draws use `CDirect3DDevice_DrawIndexedPrimitive` directly,
synthetic vertex/index buffers and original test-authored ARB programs. It reads
actual color, depth and stencil pixels from a private packed-depth/stencil FBO.
Cases cover initial defaults from production Init, detach state, drawing while
detached, reattachment, configured disabled states, state updates while detached,
the original stencil-enable asymmetry, and attached/detached clears.

For direct Init testing, the runner replaces exactly one exported device vtable
initializer with a zero table in its private source copy; every function body
is unchanged. The fixture never dispatches through that table. Unrelated texture
and matrix boundaries assert if reached.

The existing fallback `glBindVertexArray(0)` uses the core entrypoint in a legacy
CGL context and produces `GL_INVALID_OPERATION` (`0x502`) on this machine, before
and after the fix. The fixture forwards this actual call, explicitly logs and
isolates only that known error, and asserts no other GL error after drawing.
It does not claim the fallback path is GL-error-free. All fallback pixel and
state assertions remain active.

For both changed production files, `unifdef -UCOD2_X64` followed by exact `cmp`
against `git show d497f60:<path>` passed; outputs are under `off/`. This proves
inactive-source equality, not the unavailable Linux i386 object/binary gate.
No full engine build, live game, reference capture, or screenshot parity was
run by this unit. This closes this bounded GL state mismatch, not all stencil
or renderer fidelity work. No commits were made by this worker.

## Integration

Both full client and dedicated builds pass. Scoped ADS smoke runs and later
combat/death/respawn runs pass in stock and CoD2x. These exercise the integrated
renderer but do not establish reference depth/stencil parity. The focused review
completed with zero actionable findings at
`/tmp/compound-engineering-501/ce-code-review/20261026-080848-67709771/`.
A correctness pass and fresh local adversarial pass completed; the attempted
Claude peer exhausted its turn limit without a usable result. All five simplify
lenses completed, retaining the original stencil asymmetry and existing GL
state writes. Detailed integration results are in the wave 13 report.
