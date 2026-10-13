# WS75: animated water and generated texture coordinates

The native material draw path now reaches water texture generation and DX7
generated-coordinate updates. This resolves the absent-call part of WS44 FA12
and three confirmed source discrepancies in its callees. It does not establish
visual parity with the original game or resolve the original GPU buffer lifetime
contract.

Worktree: `codex/water-fidelity`, based on `0f07adc`. Production changes are limited
to `rb_shade.c`, `rb_state.c`, and `r_water.c`, gated by `defined(COD2_X64)` for both
stock and CODX native builds. No platform renderer, Metal, configuration, shared
layout, or vertex-buffer implementation changed.

## Reference contracts

Evidence is the owned Mac multiplayer 1.3 i386 executable, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The executable and private disassembly remain outside the repository. Addresses
below identify observations, without checking in original code or assets.

| Contract | Original address | Native result |
| --- | --- | --- |
| DX7 sampler argument type 3 selects a code texture; other types use a material texture name. | `0xf8730`, code resolver call `0xf8eae` | Restore the two sampler slots in `RB_DrawSingleTechnique`. |
| Material texture semantic 5 resolves the water definition's map and its image. | `0xf878c`, `0xf9298`; DX9 `0xf933f` | Share this resolution between DX7 samplers and DX9 pixel argument type 4. |
| Image `mapType == 3` dispatches by **material texture semantic**. Semantic 5 calls the uploader and then forces sampler state 6 before binding. | `0xf87a3`, `0xf8ebb`–`0xf8eef`; DX9 `0xf92ea`, `0xf8f52`–`0xf8f57` | Upload before binding. Do not require `GfxImage.semantic == 5`; that extra condition was rejected by source review and a failing control. |
| Bind the DX7 sampler, then change generated coordinates when the mode changes or when the mode is 2. | `0xf87c1`, `0xf87c9`–`0xf87de`, call `0xf8e6c` | Restore both calls, including refresh of cached mode 2 each draw. |
| Coordinate mode 0 disables transformation; mode 1 uses the inverse view matrix with translation cleared; mode 2 negates the backend scroll amount in matrix `_32`. | `RB_ChangeGenTexCoords` at `0xccee2`; scroll load `0xcd06f` | Correct mode 2 from `depthHackNearClip` to `texScrollAmountDx7`. Original STABS identifies backend offset `0x4dc` as `texScrollAmountDx7`. |
| Water caches its frame count before checking whether drawing water is enabled. Animation uses global scene `floatTime`. | `RB_UploadWaterTexture` at `0xfa340`; cache `0xfa355`–`0xfa360`, enable test `0xfa369` | Preserve the existing frame cache and timing. |
| A zero frequency clears both complex components rather than retaining the zero-frequency coefficient. | `0xfa449`–`0xfa44c`, zero branch `0xfa3ff`/`0xfa406` | Clear and advance the native coefficient. |
| Real and imaginary amplitudes use the corresponding `H0` components. | `0xfa4a4`, `0xfa4cc` | Already correct; no change. |
| Renderer current integer 2 selects the normal-map path; other modes generate a height map with mips. | `0xfa596`, `0xfa788` | Read the typed dvar current value instead of the obsolete raw offset. |
| The normal-map horizontal predecessor begins at `M - 1`, then carries the preceding linear index across row boundaries. | initialization `0xfa7c8`–`0xfa7cb`, read/update `0xfa835`–`0xfa848` | Move native predecessor initialization outside the row loop. |

The original DX7 material lookup compares interned name pointers. Native code
uses pointer equality plus the `strcmp` fallback already used by its DX9 material
lookup. The fixture exercises both successful forms. Missing-name and invalid
code controls validate deterministic native handling; they are not a claim about
original behavior for malformed material records. Other original dynamic
material semantics are outside this bounded water change.

The backend producer sets `texScrollAmountDx7` to the fractional part of
`sceneDef.floatTime - materialTime` for DX7. Water spectrum generation uses
`sceneDef.floatTime` directly. These are separate time contracts.

## Production-path fixtures

`tests/lp64/renderer/run_water.py` reads actual engine compile commands and remaps
their source root to the fixture's checkout. It retains engine options, enables
assertions after Release flags, and adds ASan/UBSan. Temporary source copies and
extracted helpers come from the current production files, or `--baseline`; they
are not a separately maintained renderer implementation.

The fixture runs full production `RB_DrawSingleTechnique`, `RB_SetVertexData`,
and `RB_UploadWaterTexture`, with real `RB_SetSampler`, generated-coordinate
helpers, FFT, matrix math, mip downsampling, native buffer lock/unlock, and color
conversion. A synthetic device observes calls; unrelated rendering services and
the final image-upload/device boundaries are fixture replacements.

Five fixture groups cover:

- DX7 material and code sampler dispatch, DX9 water arguments, upload-before-bind,
  the actual sampler cache, state 6 filter modes, clamp-to-wrap changes, ordinary
  images, non-water map types, differing image/material semantics, missing texture
  names, and invalid code sampler 24. DX7 misses clear the image binding; existing
  DX9 missing-name handling skips binding and retains the cache. Null image
  handling does not overwrite cached sampler-state bits.
- All three generated-coordinate modes, inverse-view translation removal, the
  scroll value independent of `depthHackNearClip`, and repeated mode-2 updates.
- A synthetic asymmetric 4×4 complex spectrum, including nonzero `H0` at zero
  frequency. An independent direct two-dimensional Fourier sum supplies expected
  heights. Height-map bytes and all three mip payloads are checked exactly.
- Normal-map bytes against independently normalized heights, allowing one byte
  per channel for the production low-precision square root. Before the predecessor
  carry fix, pixel 8 horizontal channel was 124 versus expected 121. The original
  uses predecessor index 7 there; resetting every row incorrectly uses index 3.
  The final fixture passes for scene times 1.5 and 3.25 and observes changed
  payloads.
- Supported native vertex strides 20, 24, 32, 36, 64, and 68, independently expected
  color byte swaps, appended offsets, retention of earlier CPU buffer regions,
  and draw-boundary payloads. Unsupported stride 28 is characterization only.

Repeated uploads at the same timestamp in the same frame are suppressed, as are
uploads after `floatTime` changes within that frame. The cache key is **frame
count**, not timestamp: a new frame uploads again even at the same timestamp.
Disabling water marks the frame before returning; re-enabling it within that
frame still skips upload. All of these controls execute the production uploader.

At base `0f07adc`, draw, spectrum, renderer-mode, and coordinate groups fail at
their respective behavior assertions; vertex characterization passes. The
normal-map predecessor control also failed after the other fixes and before the
carry correction. The unsupported image-semantic guard failed its dedicated
control before its removal. These failures are recorded in the local evidence.

## Vertex-buffer diagnosis and limits

Original `RB_SetVertexData` (`0xf6e1e`) selects DISCARD (`0x2000`) at offset zero
and NOOVERWRITE (`0x1000`) otherwise (`0xf6e4d`–`0xf6e57`). Native code selects
DISCARD at zero or with `gpuSync == 0`, and zero otherwise. Original unsupported
stride dispatch bypasses copying (`0xf6ece`–`0xf6ef3`); native code copies it.

Original buffer `Lock` (`0x1eb66`) has GPU resource behavior: dynamic DISCARD
branches at `0x1eb8b` to deferred free/recreate (`0x1ebf5`/`0x1ebfd`), while the
non-dynamic route can wait on a fence (`0x1ebd0`). Native
`CDirect3DVertexBuffer_Lock` ignores flags and returns the CPU allocation plus the
offset; `Unlock` is a no-op. Its OpenGL draw consumers read that CPU storage.
The ARB shader consumer explicitly binds array and element buffers to zero and
uses client-memory pointers before `glDrawElements`.

Production call tracing finds the active `RB_EndSurface` dynamic routes use
supported strides; cached vertex routes bypass `RB_SetVertexData`. The fixture
confirms CPU allocation stability across flags 0, NOOVERWRITE, and DISCARD, and
correct observed payloads for supported native draw inputs. It does not execute
the original GPU buffer or an OpenGL context, test asynchronous consumption, or
prove GPU lifetime parity. Changing only integer flags would not restore the
original resource contract in this CPU-backed implementation. No lock, lifetime,
or unsupported-stride change is justified here; that part of FA12 remains open.

## Validation and reproducibility

Local evidence directory:
`/Users/stix/Library/Application Support/CoD2-native-ws75/qa.noindex/water`.
Actual engine databases came from the parent's `build-macos` and
`build-macos-codx` directories in the protocol-fidelity checkout. Scoped audit
databases contain the three changed translation units plus unchanged `r_init.c`,
required by the callback checker for its bootstrap lookup.

| Check | Result and scope |
| --- | --- |
| New fixtures, actual stock and CODX engine flags, ASan/UBSan | All five groups pass in both configurations. |
| Existing `tests/lp64/renderer/run.sh` | All 25 fixtures pass. |
| Actual stock/CODX Release compilation | All three modified production translation units compile with their real `-O3`, `-DNDEBUG`, SDK and native flags; final `rb_shade.c` recompiled after the semantic correction. This is not a full executable build. |
| Scoped ABI audit, stock/CODX | Four translation units, zero errors, mismatches or new ABI findings. |
| Scoped raw-offset audit | Four sources, 57 sites, zero unreviewed/unsafe sites. |
| Callback, strict AST, bit-conversion and import checks | 218 binding inventory entries, zero table/cast mismatches, unprototyped floating calls or new findings; zero bit-conversion violations; 513 import symbols, 115 active native sites, zero proven extra/missing dereferences. Missing callback definitions outside the scoped database are not validated. |
| Scoped stub gates, stock/CODX | Four existing candidates, zero new. |
| `tools/abi/legacy.py --base 0f07adc`, stock/CODX | Three files, five native-off configurations, zero mismatches; three actual SDK commands with the native definition removed, zero mismatches/errors. |
| `tests/fixes13/legacy.py --base 0f07adc` | Three files, ten legacy configurations, zero mismatches. |
| `git diff --check` | Pass. |

The CoD2x-only inactive checker was also run and **does not pass**: three files,
17 configurations, 21 differences. Its requirement that changes disappear with
CODX disabled does not apply to native fixes intentionally active in both stock
and CODX. It also synthesizes `COD2_X64=0`, which remains active for the
repository's `defined(COD2_X64)` convention. Actual CMake OFF omits this definition;
the native-off and actual SDK comparisons above pass. Neither the checker nor the
production gates was changed to conceal this result.

The legacy checks are preprocessing checks, not an i386 object/binary comparison.
No live game, original-binary execution, GPU context, material asset observation,
benchmark, or full-repository ABI audit was performed in this worktree.

Reproduce the new fixtures from the checkout containing this report:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_water.py \
  --compile-commands /Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_water.py \
  --compile-commands /Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos-codx/compile_commands.engine.json
taskpolicy -b nice -n 19 python3 tests/lp64/renderer/run_water.py \
  --compile-commands /Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json \
  --baseline 0f07adc
```

The last command is expected to fail the four fixed behavior groups. `--test`
selects individual groups. No source or report is staged or committed; integration
review, full builds, and live coverage remain with the parent task.
