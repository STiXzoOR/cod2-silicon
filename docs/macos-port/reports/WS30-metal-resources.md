# WS30 — Metal resources provider

Branch: `port/metal-resources`, based on `351fe824f8de95a4ce6aba5a339df74f5391455a`
from WS29. Resource operations and synthetic GPU tests are implemented against
ABI 1. No frozen integration headers, shared engine headers, GL method bodies,
remotes or sibling worktrees were changed. GL remains the default. This does
not enable engine geometry; WS33 still supplies draws.

## Implementation and decisions

`metal_resources.m` installs `MacMetalResourceOps` and keeps the C identity,
descriptor and lifetime in WS29 sidecars. Its producer-only registry holds raw
keys/values; the core payload owns each Objective-C resource. Address reuse is
checked when removing retired entries. The only existing-file change adds two
new native `.m` sources and ARC for the provider in `cmake/macos-arm64.cmake`.
The format converter uses a `.m` extension deliberately: legacy C source globs
must not acquire it.

**Buffers:** vertex, INDEX16 and INDEX32 buffers have a zero-initialized CPU
shadow and native shared/private backing. Locks validate offsets, lengths,
flags and nesting; size zero means the bounded remainder. Buffer bytes are
copied unchanged. In particular, producer RGBA/ARGB/BGRA color bytes remain as
documented in `lp64_color_order.h`; WS33 must decode them for each declaration
and producer rather than choose one universal color swizzle.

DISCARD rotates among three lazily allocated full buffer generations.
NOOVERWRITE rejects overlap with pending or uncompleted GPU ranges. Ordinary
writable overlap renames while preserving the shadow. Pending ranges use
`UINT64_MAX`; `submitted(value)` publishes the actual shared timeline value,
and `submitted(0)` cancels reservations. Collection uses GPU completion, both
on locks and subsequent bindings, so never-relocked buffers do not accumulate
historical ranges. Ring exhaustion returns `0x8876021c`, without a wait or
frame-age heuristic. Static-model cache locks at `r_staticmodelcache.c:890/1046`
use `0x1001`; the low modifier bit is accepted as the GL implementation does,
while the NOOVERWRITE range checks remain active.

Shared unlocks copy the bounded modified interval. Private unlocks stage the
bounded interval, using a full initialization upload for new/renamed backing.
If both producer slots are unavailable, unlock releases the CPU lock, preserves
the complete dirty shadow and defers the upload until binding. Engine unlock
callers do not retry, so leaving their lock held would fail the next write.
Explicit upload barriers order copies before later GPU use.

**Textures:** exact 2D, cube and volume dimensions, supplied/truncated or
natural mip chains, six cube faces and every volume slice have bounded CPU
layouts. Uploads accept padded row/slice pitches and reject overflow. Locks
return the actual byte/block offset and full parent pitches. Unlock stages the
modified region; initial data, cancelled uploads and palette changes require
a complete subresource upload. Binding flushes dirty mips/faces and rejects
locked subresources. Upload staging is immutable and retained to completion.

On the tested Apple M6, `supportsBCTextureCompression=1`; 2D/cube DXT1 uses
native BC1, DXT2/3 BC2 and DXT4/5 BC3. Metal does not admit compressed 3D
textures, so volume blocks are decoded into RGBA8. The same fallback is
available for a device without BC support. Only the BC-capable device was
available for execution; a BC-incapable device was not tested. BC4–7 inputs are
absent from the engine audit and are rejected.

Non-BC formats expand into linear `RGBA8Unorm`, preserving the measured GL
sampling semantics: RGB byte input for R8G8B8; BGRA input for A8/X8R8G8B8,
including the GL path's preserved X8 alpha; black RGB for A8; luminance replicated
to RGB; explicit packed-bit expansion. No sRGB conversion or DXT2/4
unpremultiplication is introduced. Palette entries are RGBA PALETTEENTRY bytes;
P8 uses entry alpha and A8P8 uses its explicit alpha byte. Sixteen palettes,
selection, getters and invalidation are implemented. The old GL palette setters
are stubs, so palette behavior is a separately tested compatibility extension.

**Views:** owned mip/face surface identities use the core's external-reference
transfer. Releasing a texture while an external view survives retains its
parent; releasing that last view destroys both. The native texture accessor
unwraps a view and returns the owning texture; callers obtain attachment
mip/face from `MacMetal_SurfaceParent`. There is no additional native view
allocation. ABI 1 lacks a volume-view kind, so a surface identity carries the
parent/mip reference and a provider-local 13-slot IDirect3DVolume9 table supplies
the correct GetContainer/GetDesc/LockBox/UnlockBox methods. Fixtures execute
the actual volume table slots, rather than assuming surface LockRect layout.

**Nulls and residency:** typed 2D/cube/3D null textures contain opaque black,
including every cube face. The null buffer contains 256 zero bytes. Dynamic
buffers default to shared storage, other resources to private; provider-local
creation can explicitly select either. Every bound allocation and upload
staging buffer goes through `MacMetalGPU_Keep(..., allocation=1)`, joining the
shared transport's Metal 4 residency set and retaining the allocation until
completion. Resource COM retention and native-allocation retention coalesce
within a producer submission. Exact buffer reservations and a dirty-level
bitset remain available for submission/cancellation publication.

No shader, library or pipeline factory exists in the resource provider.
Synthetic fixture pipelines compile once during fixture setup.

`UpdateTexture` copies only matching texture kinds/descriptors and equal shadow
lengths, then flushes every dirty mip/face. It rejects buffer operands before
copying. A regression first reproduced an ASan 16-byte read past an 8-byte
buffer in that invalid call; the guard fixes it. Valid mixed-storage copies
are sampled across every mip, cube face and volume slice against GL.

## GPU fixture evidence

```sh
sh tests/metal/run_resources.sh
```

The fixture links the production provider, real ABI 1 core/dispatch/sidecars,
real shared GPU transport and production `MacOpenGLUtils_GetOpenGLTextureType`.
Only unrelated platform dependencies are stubbed. The GL reference uses a
private real CGL context, ARB texture sampling, RGBA8 offscreen rendering and
readback. The Metal fixture uses setup-time Metal 4 compute PSOs and GPU
readback. All 21 accepted texture formats run across both storage modes,
every mip, every cube face and every volume slice, with padded pitches and
partial updates. Native BC assertions also check the actual allocation format.
NPOT BC 2D/cube/volume chains and sub-4×4 final mips are covered.

| Format family | Reference and result |
| --- | --- |
| R8G8B8, A8/X8R8G8B8, R5G6B5, A1R5G5B5, A4R4G4B4, A8, L8, A8L8 | Original bytes through the production GL mapper and GL sample/readback; pass. |
| DXT1/3/5 | Original compressed blocks through GL sample/readback; native Metal 2D/cube and decoded volume slices pass. GL compressed 3D is unavailable, so each volume plane's GL reference uses the working 2D compressed path. |
| P8/A8P8 | Independent palette expansion, then real GL BGRA sample/readback; all subresources/storage/partial paths and palette revisions pass. This does not claim a functioning original GL palette path. |
| X1R5G5B5, X4R4G4B4 | Independent opaque-bit conversion into the corresponding working GL alpha-packed format; pass. |
| R3G3B2, A8R3G3B2, A4L4 | Independent packed-bit expansion into GL BGRA; pass. These lack a working original mapper entry. |
| DXT2/4 | GL BC2/BC3 storage-alias reference; pass. DXT2 is also an old GL alias; DXT4 lacks an original GL upload mapping. |

Final ASan/UBSan run: **1,635 comparisons, 168,832 channels**, MAE
**0.034910/255**, RMSE **0.187855/255**, p99 **1/255**, maximum **2/255**.
Each case independently enforces WS24's MAE/RMSE/p99 and bad-pixel thresholds;
no pixel exceeds 16/255 in any channel. Comparing all four channels is stricter
than RGB-only acceptance.

Buffer GPU copies/readback prove distinct byte patterns, partial offsets,
zero-length remainder locks and shared/private vertex/16-bit/32-bit index
uploads, including a seven-byte update at unaligned offset 121.
`resource_gl_buffers.c` executes the original GL CPU constructors,
lock/unlock and destructor bodies as the readback reference, with fixture-only
constructor/vtable names and native dispatch disabled for that translation
unit. GL has no native GPU buffer backing here. Full, offset and partially
updated Metal GPU bytes compare exactly with this actual GL lock readback.
A real queue wait on an unsignaled shared event proves all three
generations remain unavailable under pending/in-flight submissions, overlapping
NOOVERWRITE fails, disjoint writes succeed, and completion permits wraparound.
Both storage modes run that blocked-queue case. Two exhausted producer slots
also prove deferred private writes merge correctly before later binding.
Repeated never-relocked bindings across 16 frames verify completed range
collection and one pending resource retention per submission. Cancellation
replays a private texture upload. An engine reference released before commit
remains alive until notification/retirement. External view transfer and typed
null texture sampling/null buffer GPU readback pass.

After drain, provider shutdown, core retirement, transport destruction and the
outer autorelease pool, counters report **resources=0, allocations=0,
pending=0, ranges=0**. The allocation counter inspects actual weakly tracked
native objects, including staging buffers; it is not an allocation-minus-free
guess. This is the requested Metal object-counter teardown proof, alongside
ASan/UBSan. It is not a claim that full drawn gameplay is leak-free.

## Builds, regressions and remaining gate limitations

Both Release configurations and native clients build with the exact
CONTRIBUTING settings, including arm64 and stock versus `-DCOD2_CODX=1`.
Builds/fixture compilation used `taskpolicy -b nice -n 19`; no packages were
installed. The real engine compile databases were saved as
`compile_commands.engine.json`; assertion-enabled copies were used for source
fixtures. Local logs and generated test outputs are ignored under
`output/ws30`; licensed logs, JPEGs, shader preparation and reference executable
copies remain external under `~/Library/Application Support/CoD2-native-ws30/`.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos --parallel 3
```

All CONTRIBUTING suites were invoked. Results:

| Commands/suites | Result |
| --- | --- |
| `tools/tests`, `tools/datagen/test_datagen.py`, `tools/abi/test_audit.py`, `test_imports.py` | Pass. |
| `tests/rendering/shader_setup.py`, Wine trace parser suite | Pass. |
| `COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh`, `run_native.sh`, `tests/online/sdk_identity.sh` | Pass. |
| `tests/online/run.py build-macos/compile_commands.json` | Pass. |
| `tests/online/run.py build-macos-codx/compile_commands.json` | Fails: timeout fixture omits `_Cod2x_Frame` and `_Cod2x_DemoClientFrame` link stubs, also reported by WS29. |
| `tests/fixes13/run.py --build build-macos-codx` | Pass. |
| `tests/fixes13/run.py --build build-macos` | Fails after earlier cases pass: stock preprocessing removes `Cod2x_LimitedFPS`, which the timing extractor requires; also reported by WS29. |
| `tests/perf/run.py`, LP64 game/script suites | Pass; perf fixtures are source correctness tests, not measurements. |
| Renderer `run.sh`, `run_shader_raster.sh`, `run_texture_mips.sh`, `run_volume_upload.sh` | Pass. |
| HUD matrices, impact marks, FX events/primitives/cloud, dedicated FX | Pass. |
| Platform CTest sanitized/plain configurations | **31/31 and 33/33 pass.** |
| `tools/datagen/roundtrip_test.sh` | Pass, including scalar/relocation comparisons and deterministic regeneration. |
| `tests/lp64/game/check_layouts.py --build build-macos` | Pass: 335 native fields, 292 retail network offsets/widths, 366 weapon mappings. |
| `tests/fixes13/reference.py` | Pass. |
| `tools/metal/run.sh` with the owned cache and external WS30 output directory | Pass: 417/417 programs and metadata, zero failures. |
| `tools/metal/add_dispatch.py --check`, `tests/metal/run_core.sh` | Pass: 470 preserved GL entry bodies; core fixtures pass. |
| Strict macOS 13 deployment syntax/availability check for `metal_resources.m` | Pass with `-Wall -Wextra -Werror` and both unguarded-availability errors. |
| Inactive-source guards against branch start | Pass: 10 legacy, 17 inactive CoD2x and 5 COD2_X64-off configurations, zero mismatches. |
| Full stock/CoD2x ABI gates | Pass: 628/645 TUs, zero errors/mismatches; 218 renderer bindings each, zero table/cast mismatches; retail imports zero errors. See launcher notes below. |

The full native renderer smoke passes with menu, `CS_ACTIVE`, controlled Toujane
screenshots and exit zero under **stock and CoD2x, GL and Metal**:

```sh
timeout -k 5 200 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer gl --home "$HOME/Library/Application Support/CoD2-native-ws30/stock-gl-update-final" \
  --port 29536
DYLD_INSERT_LIBRARIES="$PWD/output/ws30/no-gl-guard/no_gl_guard.dylib" \
  timeout -k 5 200 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer metal --home "$HOME/Library/Application Support/CoD2-native-ws30/stock-metal-update-final" \
  --port 29537
```

CoD2x used `build-macos-codx/cod2_macos`, separate homes and ports 29538/29539.
The Metal interposer covers **1,342 GL/CGL symbols, including all 126 engine
imports**; neither Metal smoke triggered it. Metal screenshots are still the
core's clear-only scene, not rendered geometry. Decoded 1280×720 GL menu and
controlled courtyard JPEGs match the branch-start stock executable with
**zero RGBA bytes different**. FX/entities were disabled by the smoke's static
golden setup.

The CONTRIBUTING parity recorder was also invoked for 100 frames. A local
external wrapper only adds the unique port, explicit GL mode and window setup;
it execs the built CoD2x client. The engine exits zero, but trace validation
fails at row 9 with `noncontiguous frames or map restart`. The same invocation
against the external branch-start stock executable reproduces the failure.
Neither the parity tools nor unrelated engine frame-numbering code was changed.

The benchmark lock was held for all game captures/recording and released when
they ended. `pgrep -fl cod2_macos` / exact-name checks preceded launches; an
observed other game was allowed to exit before proceeding. All runs used unique
homes/ports and `timeout -k`; no process belonging to another agent was killed.
The screen was **locked**. Other autonomous workstreams, background apps/system
processes and low-priority compilers/Python audits were running. All numbers
here are correctness evidence, not FPS, pacing or performance acceptance.

## WS33 handoff and merge requirements

Use `metal_resources.h`, a provider-local header; it does not extend ABI 1:

1. `MacMetalResources_Buffer(side, submission, byteOffset, byteLength, &binding)`
   validates the interval and returns a borrowed native buffer, native byte
   offset, length and monotonically increasing generation. It reserves that
   exact interval and retains its residency/lifetime. Apply index/base/stride
   validation in WS33; INDEX32 resource support alone is not an indexed draw
   implementation. No hidden stream/base/index rebasing occurs. CPU index
   validation can use ABI readonly lock/unlock of the CPU shadow, finishing
   that lock before binding; private `MTLBuffer.contents` is not a CPU view.
   Check return codes and handle ring BUSY without a wait.
2. `MacMetalResources_Texture(side, submission)` flushes dirty data and returns
   a borrowed typed native texture, or NULL on a lock/busy/upload error. Surface
   sides unwrap to their owning texture. Honor its actual pixel format and
   dimension. All non-BC uploads are RGBA8; engine vertex colors are unchanged.
3. Use `MacMetalResources_NullTexture(kind, submission)` and
   `MacMetalResources_NullBuffer(submission)` for unbound slots. Default disabled
   vertex attributes still belong to WS33; a zero buffer is not their complete
   semantic default.
4. Encode on the one active **producer** submission. Commit/cancel through the
   existing backend/core path, which must call `MacMetal_ProducerSubmitted(value)`
   exactly once, including value zero for cancellation. Do not have two
   independently unnotified producer batches: ABI 1's callback has no submission
   identifier. Do not retain returned raw pointers outside their submission.
5. The integration owner must call `MacMetalResources_Shutdown()` after drain
   and engine resource release at renderer shutdown, and at device replacement.
   This releases typed-null caches and the empty registry. ABI 1 has no provider
   shutdown callback, and frozen core headers/lifecycle code were intentionally
   not changed here. Fixtures exercise this ordering. Null caches are lazy, so
   clear-only game smoke does not exercise that future draws shutdown seam.

There is only **one installed ResourceOps table**. WS34 must compose target/post
shim handling with `MacMetalResources_Ops()` instead of replacing it. Resource
shim returns zero for unhandled target/device operations so the compositor can
delegate. Keep WS30's upload/lock/unlock/submitted callbacks. Standalone color/
depth targets, D24S8, target GPU readback, attachment hazards, copy/stretch and
post are WS34's work; CPU shadow locks are not render-target GPU readback. R32F,
D16/D32 and BC4–7 have no proved active texture-loader request and are rejected
by this provider rather than guessed from memory-accounting declarations.
The provider currently auto-installs at construction. Install the composed
table explicitly afterward during integration setup; do not rely on relative
constructor/link order, which could otherwise overwrite either provider.

**ABI proposals:** no ABI change is needed for the current engine upload/draw
handoff. Two integration limitations are explicit: the shutdown call above must
be wired by the lifecycle owner; volume views currently use surface kind. If a
future ABI wants an explicit volume identity, append `MAC_SHIM_VOLUME_VIEW` and
teach object creation, view binding/parent access and dispatch its exact volume
vtable/descriptor, preserving existing enum values. This would remove the local
volume table workaround. No such change was made to ABI 1.

Merge `c72ff8b`, `3d98ba2`, `940c734`, `4554e13`, `929c73c`, `4c8475e`,
`9417aea`, `34ff090`, `4ba6a8d`, then this report commit in branch order.
Only the two small native CMake additions overlap integration work. No push, PR, issue, remote change,
package installation or proprietary/derived content publication occurred.

## Final ABI / legacy evidence

Commands for the complete ABI checks:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws30/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws30/abi-codx-prepared
```

Both complete checker runs produced all three reports. Stock: **628 TUs**,
CoD2x: **645 TUs**; each has **0 compile errors, 0 function ABI mismatches,
0 unprototyped floating calls**, and **218 renderer bindings with 0 table/cast
mismatches**. Retail inventories cover **515 symbols and 185 placeholders**
each, **1,777 stock / 1,779 CoD2x active sites**, and **0 errors**, including
zero proven extra/missing dereferences.

Final-source stock function/callback audits were refreshed after the copy guard
using the completed cache; CoD2x's later audit also refreshed all 645 TUs and
218 bindings. They again report all mismatch/error counts zero:

```sh
python3 tools/abi/audit.py build-macos/compile_commands.engine.json \
  --output output/ws30/abi-stock-final/functions.json \
  --cache output/ws30/abi-stock/audit-cache --baseline tools/abi/baseline.json
python3 tools/abi/callback_tables.py build-macos/compile_commands.engine.json \
  --audit output/ws30/abi-stock-final/functions.json \
  --baseline tools/abi/callback-baseline.json \
  --output output/ws30/abi-stock-final/callbacks.json
```

Two launcher details matter when inspecting local logs. The scratch gate
driver's redundant CoD2x import repeat was deliberately stopped (143) after
its final-source function/callback passes; the already-running complete
`abi-codx-prepared` scan supplies retail validation for the identical compile
database and unchanged source. That complete check produced every success
summary/report, then its outer zsh bookkeeping failed (1) because `status` is
read-only. This happened after the checker; it did not interrupt an audit.
The committed fixture launcher now uses a task-specific exit variable and
passes `sh -n`, so it does not repeat that launcher mistake.
The report uses the complete checker artifacts and verified zero counts,
not the scratch launcher status as evidence of a passing test.

```sh
python3 tests/fixes13/legacy.py --base 351fe824f8de95a4ce6aba5a339df74f5391455a
python3 tests/cod2x/check_inactive_gates.py --base 351fe824f8de95a4ce6aba5a339df74f5391455a
python3 tools/abi/legacy.py --base 351fe824f8de95a4ce6aba5a339df74f5391455a
bash tools/ci/compare-x86.sh 410342a HEAD output/ws30/x86-reference-final
git diff --check
```

The three source guards pass with zero mismatches. The x86 comparison exits 2
with `Requires x86_64 Linux with the README multilib dependencies.` This Mac
cannot prove i386 artifact identity; the orchestrator must run the real Linux
gate. No packages were installed to bypass that prerequisite. Source isolation
and the passing OFF guards supplement that requirement, not replace it.
