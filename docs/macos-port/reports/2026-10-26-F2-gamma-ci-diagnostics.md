# Gamma presentation CI diagnostics

The intermittent platform gamma sanitizer failure remains unresolved. This
change adds diagnostics to the existing fixture; it does not change production
rendering, assertions, failure status, test selection or retry behavior.

## Evidence

The failure occurs after seven successful RGB checks, when the fixture returns
from adjusted gamma to identity presentation after resizing. The next operation
uses `glBlitFramebufferEXT`, whereas adjusted gamma uses a shader. ASan reports a
1280-byte write through Apple's `libGLImage` / `GLRendererFloat` into a freed
399360-byte region at offset 397696. Reporting the free stack then fails inside
ASan's StackDepot with corrupted metadata. That prevents identifying the owner
or proving an application free caused the error.

The three relevant production/test files (`macos_display.c`, `macos_gamma.h`,
`gamma.c`) were unchanged from `715b45f` through investigation base `5ce6fb0`.
Downloaded complete job logs establish this dependency/outcome matrix:

| Commit / Actions run | SDL2 compatibility / SDL3 | Sanitized default / software gamma |
| --- | --- | --- |
| `7e1e610` / `40296490956` | 2.32.74 / 3.4.18 | fail / pass |
| `4818b1a` / `40297984747` | 2.32.74 / 3.4.18 | pass / pass |
| `c2360e9` / `40299290723` | 2.32.74 / 3.4.18 | pass / pass |
| `5ce6fb0` / `40308572709` | 2.32.74 / 3.4.18 | pass / fail |

The final row comes from the signed-in GitHub live log, transcribed while the
job was still running. All three downloaded runs use macOS 26.6.2 (25G83);
the first and third also use the same runner image. A dependency upgrade between
these runs does not explain the alternating outcomes.

## Context-update hypothesis

[Apple's context guide](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_contexts/opengl_contexts.html)
requires geometry updates before drawing. However,
[SDL's Cocoa window code](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/video/cocoa/SDL_cocoawindow.m)
immediately updates the current context when changing window size or position.
Its Cocoa window and OpenGL files are byte-identical between 3.4.16 and 3.4.18.
The compatibility library's intervening changes do not change this GL path.

A private runtime probe invoked and counted the real context update method.
Default and software runs, with and without Retina scaling, observed 7, 12, 19
and 24 completed updates after the four mode changes, with no pending dirty flag
before rendering or the final blit. This contradicts a missing-update hypothesis
on the tested machine; no additional context-update bridge was added.

The initial local dependency inspection found compatibility 2.32.72 / SDL3
3.4.16. During investigation, the installed libraries changed externally: current
receipts record 2.32.74 / 3.4.18 at 10:57 UTC. No install was performed by this
unit. Because that transition overlaps the private probe period, those probe
logs alone do not establish their exact runtime library versions. The final
fixture diagnostics explicitly report runtime SDL 2.32.74; linked-library and
receipt checks confirm SDL3 3.4.18. Local software OpenGL remains
`APPLE-24.0.2`, compared with CI's `APPLE-23.1.1`.

## Diagnostic and validation

The fixture now prints runtime SDL and GL identity. Test-local wrappers log
entry and return for the final real clear and blit, with render/window/drawable
sizes, read/draw framebuffer and buffer selections, viewport, fixed-backing
state and exact blit arguments. They forward the same calls and preserve all
existing checks. Queries and logging can affect timing; a passing diagnostic run
will not establish that the original problem is fixed.

Allocator hooks were omitted: stack printing inside allocation callbacks can
allocate or recurse, and querying corrupted ASan allocation metadata can hit
the same reporting failure. Allocation/free ownership remains an explicit gap.

Focused Debug builds with `COD2_PLATFORM_SANITIZERS=ON` and `OFF` each passed
`macos_gamma_test` and `macos_gamma_test --software`, four runs total. The new
logs show the clear and blit return, expected 320×240 source/destination geometry,
read FBO 1, draw FBO 0 and fixed backing disabled. No broader production checks
are claimed for this test-only diagnostic.

Private logs, source comparisons, runtime probes and exact run drivers are under
`~/Library/Application Support/CoD2-native-decomp/F2-gamma-recurrence.noindex/`.
The next useful evidence is the new boundary output from a failing CI invocation,
not more unchanged local repetitions. Graphics runs held the exclusive lease
and restored the reference VM to its prior started state.

## Integration review

Completed local and independent Claude review `20261026-131913-2a8c4a34`
verified unchanged assertions, original operation ordering and SDK signatures.
All five simplification rubrics produced no edits. Two P3 diagnostic-coverage
suggestions were applied: flushed boundary markers now also bracket readback,
actual swap and the outer final presentation call. These additions change no
arguments or expected results. Claude Opus 5.5 was confirmed; high effort was
requested, while actual effort was unverified.

The final integrated diagnostic passed both default/software runs in sanitized
and plain Debug builds (four runs). Build trees were removed after preserving
executables and logs privately in `F2-gamma-integration.noindex`. The graphics
lease was released and the VM restored to started.

Run `40308572709` subsequently completed: the source-based game/script/network
fixtures and both Release builds/ABI audits passed. The only failing Apple-job
step was platform CTest. Its complete raw job log was downloaded and confirms
the gamma failure transcribed above. The separate x86 reference job remains
nonblocking and fails in the upstream baseline. This diagnostic is not a fix
for either failure, and a future passing run alone will not establish one.
