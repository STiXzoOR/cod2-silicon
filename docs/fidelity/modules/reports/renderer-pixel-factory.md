# Renderer pixel shader factory reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.** This covers the entire
`CDirect3DDevice_CreatePixelShaderOpenGL` at Mac 0x1b0da, including its
shader-type switch, parsing, GL status, output and lifetime paths. It does not
claim native ATI text-fragment or NVIDIA register-combiner execution.

The exact-symbol body 0x1b0da–0x1b3f5 was read by hand, including the C++
cleanup paths. The separator bytes at 0x241084 were checked directly as `::`.
The source is split at its first separator before the switch. Types 0, 1 and 2
construct ATI, NVIDIA and ARB objects respectively. ATI/NVIDIA receive the
original source pointer; ARB receives the split name and code. An unknown type
does not write the shader output and still queries GL_PROGRAM_ERROR_POSITION_ARB.
The result is success for -1 and D3DERR_INVALIDCALL otherwise. The original
formats GL_PROGRAM_ERROR_STRING_ARB into an unused local buffer on failure.

Three deterministic differences were corrected under `COD2_X64`. Unknown
types now preserve output and return the GL status (0x1b179/0x1b19f). A nonempty
unprefixed string without a separator uses the whole source as its name and
code starting at byte one: 32-bit `string::npos + 2` wraps to one at 0x1b12a.
An empty code following `name::` now reaches the ARB constructor and GL upload,
rather than being rejected by the factory or skipped by the constructor. The
exact ARB C1 body at 0x1c34c unconditionally generates, binds and uploads at
0x1c4ea–0x1c542, including a zero-length string, then restores the prior binding.
These corrections were compiled and reviewed, not exercised with GL.

The complete native shader-class implementations and the live compiler/cache,
device and material-loader callers were read. The selected Apple display
provider returns card type zero, and `CreatePixelShader` requests ARB type two.
The existing external shader cache and approximate generic compiler produce
bare `!!ARB` text; this representation still bypasses the original name split,
including any `::` in cached comments. The cache and generic rendering
approximation remain the boundaries documented in `renderer-shader-compiler.md`.
ATI/NVIDIA requests retain D3DERR_NOTAVAILABLE and a null output: their existing
methods do not constitute supported native activation. The original ATI and
NVIDIA constructor argument and storage contracts were checked directly at
0x1bb9a and 0x1bc62; they were not enabled by this change.

Mac publishes the new shader before its GL status query at 0x1b19a, including on
failure, and retains C++ string members. Native ARB creation keeps ownership
until success and releases a failed program because `Material_LoadPassShader`
discards its material header after a failed HRESULT without releasing the shader.
The native provider uploads synchronously and retains neither name nor code.
Null pointers and empty unsplit input retain defined native invalid-call
results, replacing the original invalid string construction/substr exception;
allocation failure returns E_OUTOFMEMORY instead of C++ unwinding. The unused
original error-string formatting is omitted. These are explicit native
adaptations. A narrow shader-class allocation helper now uses the actual LP64
implementation size instead of the factory's arbitrary 0x80-byte allocation.
No device layout, render-state or draw code changed.

Both actual arm64 Release Apple client objects, `CDirect3DDevice.c` and
`CDirect3DPixelShader.c`, compiled successfully using the remapped renderer
compilation database, sequentially under `taskpolicy -b nice -n 19`. The five
inactive source configurations match the prepared base
`c6dd2b11fd4264b3261d46650c76651384bdcee4`. Twelve additional comparisons
preserving actual `__LINE__` expansions passed: those five configurations for
both files and each complete Apple client preprocessing path with `COD2_X64`
removed. Neither changed file uses `__LINE__` or `COD2_ASSERT_FIELD`.

No tests, cases or assertions were added or changed. Existing shader-cache
fixtures were inspected but do not exercise this factory and were not run.
No full CI, replay, dedicated build, game or GL execution was performed.
The dependency batches and parent integration gates remain pending. No
assigned entry remains unchecked; unsupported ATI/NVIDIA execution and native
failure ownership remain intentional differences, not equality claims.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-pixel-factory.noindex/`.

## Integrated batch 19 verification

Production f65aaad and the catalog/existing-fixture follow-ups through 3f45bb4 passed both dedicated builds, full local CI02–13, source guards, actual OFF line expansion checks including the C++ bridge and Objective-C owner, unit parent/independent reviews and independent combined-source review. Integrated production/build/fixture files exactly match the reviewed private composition. Both 2,000-frame dedicated replays exactly match both 819d282 baselines in players, state hashes and entities; all six Linux logic fields agree, maximum origin/velocity drift0.11792/2.0.

Existing raster/saved-screen checks and 18 depth cases passed ASan/UBSan. The depth fixture uses the actual typed vertex-shader owner and provider accessors, real borrowed-buffer AddRef/Release methods, existing outer depth defaults and actual fence/index/dispatch providers. Its fallback mode explicitly covers the approximation path, not shader-free native validation. GL cases and result assertions are unchanged. The catalog was generated twice identically: only the restored r_mode/r_displayRefresh registration sites change factual metadata; other changes are source provenance. Initial CI02 failed stale coverage and its complete retry passed after regeneration. The existing Swift loader then passed after updating its revision literal and display consensus expectation; no assertions/cases were added or removed. Fresh source/resource review confirms the preserved copy/restrictions and consumer contract. No tests, cases or assertions were added.

This batch closes 24 capability, defined-boundary, GPU budget, render-state, shader factory, mutex/Optimize exception, VM recovery, startup splash, indexed-draw and retired platform-boundary entries. Two formerly equal splash no-op lifecycle verdicts become intentional Windows-style native ownership. Indexed drawing restores GL context state and separate logical caches around the explicit approximation; extended formats use the original driver and independent color buffers. The inherited multi-color scratch alias found during fresh review is corrected. Licensed/source-shader approximation and bounded storage adaptations remain explicit; no visual-equivalence claim follows from whole-row intentional closure. Original fresh-context state defaults are explicitly restored where native Reset retains its context. Exception cleanup/output ordering, malformed opcodes and unreached registration/state paths remain compiled and reviewed where fixtures did not execute them. Existing builtin/setter VM checkpoints are an explicit native adaptation, not original zero-opcode longjmp equivalence. Actual startup visibility, sampled-texture journal entries, fallback-to-normal transitions, multiple converted colors and their reallocation/reset branches remain compiled/reviewed only. Dedicated replay does not exercise rendering; this is not a complete rendered-frame acceptance.
