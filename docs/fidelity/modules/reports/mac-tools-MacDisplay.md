# Mac/Tools/MacDisplay module reconciliation

Updated 2026-11-05.

**47/47 reconciled: 2 equal, 1 fixed, 44 intentional.** Mac 1.3 i386 is the authority. Each selected provider and engine caller was inspected. `cmake/macos-arm64.cmake` excludes legacy `MacDisplay.c` from both native targets and selects `src/platform/macos_display.c` and `macos_window.m` for the client. Current GL rendering remains in scope; these verdicts do not depend on future Metal integration.

The original context destructor at 0x526e calls `glFinish` at 0x527e before destruction. Native release omitted completion. The native GL release path now finishes the context only after `SDL_GL_MakeCurrent` succeeds, before deleting framebuffer/renderbuffer/gamma resources and the SDL GL context. Optional native renderer contexts have no GL context and do not enter this branch. This restores the explicit GPU completion call; it makes no claim about whole-frame GPU synchronization callers audited elsewhere. The original audit treated the odd-count final flip as an intentional AGL replacement. That verdict was unsupported and has been withdrawn; the exact unresolved behavior is recorded below.

The intentional verdicts identify concrete replacements rather than exempting renderer code. SDL owns window creation, centering, exclusive/borderless transitions and context lifetime; the fixed render framebuffer supplies engine dimensions. Mode enumeration uses SDL records with fixed 32-bit color instead of the old CDisplayInfo vector and old 64-MiB/OS-version admission rules. This validates the MacDisplay provider and native mode-name path, not the separate CDirect3D adapter API: its selected mode count still returns one and EnumAdapterModes does not fill an output. GL vendor, renderer, extension strings, texture-unit limits and anisotropy are queried from the selected current context. Unified-memory budgeting returns MiB for the adapted `R_CheckDxCaps` threshold and bytes for the texture budget used by `GetAvailableTextureMem`. Apple uses the generic card path, GL 2.1/pixel-shader 2.0 capability contract, and no multisample buffers. Those are explicit native capability choices, not reproductions of obsolete ATI/Nvidia hardware probes.

The native SDL event pump and `MacPlatform_MapWindowPoint` replace Carbon port-coordinate and hit-testing helpers; no live native caller requests a QuickDraw device handle. The main-window getter returns an SDL object and the main-port compatibility getter is not a QuickDraw drawable. Original `DrawSplashScreen` drawing is not provided by that getter and remains a separate lifecycle gap. The old display-selection user pane is replaced by native mode/window configuration. The vector helper at 0x2ebd4a was read from `__textcoal_nt`; its 100-byte element growth and string/vector ownership are retired with CDisplayInfo. Startup/teardown now belong to explicit native initialization/release, not the removed i386 C++ vector constructors.

Fullscreen display-wide CoreGraphics fades and QuickTime movie recording are absent from the selected native reset/presentation path. Their compatibility entry points do no work; there is no native capture-movie owner to finalize. Gamma instead belongs to presentation-local storage and is discarded at release. The equal rows are the zero-returning, unused BuilderCallback and the fullscreen predicate, not the unsupported Carbon APIs.

The arm64 Release `cod2_macos_ded` target compiled at background priority with two jobs; it intentionally does not compile the client display provider. The changed provider was separately compiled using its actual client compile command, with only OpenGL deprecation diagnostics. Five `COD2_X64=OFF` configurations match the baseline; there are no `__LINE__` expansions. No new test, replay, runtime context teardown, window transition, display enumeration, gamma operation or full client build was run. The completion fix is compiled and reviewed, not exercised. Follow-up static coverage and the two unchecked rows are recorded below.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-display.noindex/`.

## Presentation follow-up

Fresh caller review found that swap interval was not controlled elsewhere: `CreateScreenContext` discarded its interval argument and forced SDL interval zero, while native `CDirect3D_CreateDevice` passed literal zero. Mac 0x57d5..0x57f6 maps `0x80000000` (immediate) to zero and every other interval to one, then ignores the AGL setter result. Native now shares that mapping between initial creation and `CDirect3DDevice_Reset`, after making the selected GL context current. The native helper accepts either window mode; the original AGL assignment was on its fullscreen branch. This restores the supplied presentation request rather than introducing a Windows-specific arithmetic decision.

The presentation ABI was checked at both ends. Original Mac Reset loads interval from byte offset 0x34 at 0x19e30. Current native `r_init.c` supplies a private 14-int record with interval at word 11 (byte 0x2c), windowed at word 7 and refresh at word 12. The imported `D3DPRESENT_PARAMETERS` pointer-bearing struct describes neither that private record nor its LP64 padding. The native callers therefore continue using the actual word storage. Creation now forwards word 11; reset applies word 11 even when size is unchanged. The imported struct was not edited. Current startup still supplies the constant one, so it now requests interval one; only its reset construction reads `r_swapInterval`. That caller preference choice remains for the renderer's `r_init` audit.

The OpaqueContextRef destructor and SwapContext rows are now **unchecked**. Original 0x5283/0x52c7..0x52e7 performs an extra default-drawable swap when the global count is odd and swapping/double buffering are enabled. Native always renders into an offscreen FBO, then copies it to the SDL back buffer at presentation; it has no corresponding parity tracking. A raw final SDL swap could show a previous or undefined default buffer, while another FBO presentation would show current content. Source inspection does not establish the original drawable retention/visible result well enough to choose either adaptation. No speculative swap was added. The earlier `glFinish` correction remains, but it does not resolve this final-swap gap.

All three changed client translation units (`CDirect3D.c`, `CDirect3DDevice.c`, `macos_display.c`) compiled using their actual arm64 Release commands in separately configured stock and CoDx builds, at background priority and with at most two concurrent jobs. Native assembly confirms the +0x2c caller loads, creation/reset calls, successful-MakeCurrent guard and exact immediate-versus-one mapping. The dedicated Release target also builds. The four modified source/header files preserve all five OFF configurations and contain no `__LINE__` expansions. Diagnostics were existing GL deprecations and the existing vtable/function-pointer warnings. No client linking, context creation, SDL swap-interval request/query, frame pacing, reset, teardown, optional Metal presentation or final visible buffer was executed. No new tests, full CI or replay ran.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.


## Window retirement follow-up

The two remaining rows, SwapContext at Mac 0x42fa and the context destructor at
0x526e, now have an explicit native window-lifetime contract. This replaces the
preceding unchecked verdicts; it does not claim equivalence between AGL and SDL
front/back-buffer retention or the image produced by the original final flip.

Fresh original inspection confirms the enable-byte test, swap and counter
increment at 0x4300/0x4313/0x4318, and the destructor's completion call, odd-count,
double-buffer and enable tests, final swap and increment at
0x527e/0x5283/0x52c7/0x52cd/0x52db/0x52e0. The counter is process-global BSS, not
a context field. The ordinary text contains only those two increments and the
parity read; context creation and reset do not reset it. The enable word starts
at one, with no assignment found in that text. Neither value supplies a native
frame number, resource identity or render-target index.

The selected native producer retains the engine image in offscreen color
attachment zero. Every regular GL present explicitly reads that attachment,
clears the SDL default back buffer, and draws or blits the complete image into
it before swapping; AUX0/AUX1 are separate attachments. This establishes normal
presentation ownership, not retention of the default back buffer after a swap.
The existing smoke fixture verifies the offscreen image and both AUX images
survive presentation. A second raw swap at teardown would not perform this copy.

Because the teardown difference is platform-specific and can affect the final
visible window, Windows was checked directly. In the supplied
`gfx_d3d_mp_x86_s.dll`, R_Shutdown at 0x10013180 reaches teardown at
0x10013285. That helper calls resource release at 0x10012e70, then tests each
HWND through IsWindow at 0x10012ea0 and destroys it through DestroyWindow at
0x10012eb6, before releasing the device at 0x10012ee3 and D3D interface at
0x10012f03. The import slots were independently decoded. There is no final
Present or parity-dependent call in this helper. Its partial shutdown branch at
0x10013283 bypasses the helper. Native follows this visible retirement boundary,
not an inferred Windows driver back-buffer policy.

Previously native ReleaseContext exited fullscreen and deleted its context/FBO
but left the SDL window alive and visible until a separate ReleaseDisplay or
process exit. Now, only for the active `screenContext`, it removes the borderless
resign observer and hides its owned drawable before fullscreen/context/resource
teardown. After deleting the GL context or stopping the native renderer, it
destroys that drawable and clears the matching `sdl_gl_window` global. No final
swap or FBO presentation is introduced. Once the window is retired, its old
default buffers are no longer an observable presentation owner. This is an
intentional native lifetime adaptation, not a reconstruction of the AGL flip.
The previous glFinish correction remains.

The exact caller boundary was traced. CL_ShutdownRef synchronizes the renderer
and calls R_Shutdown(true); that clears stored engine HWNDs before device
Release -> deleting destructor -> ReleaseContext. R_Shutdown(false) retains the
device. ResetNative retains the same context and only changes SDL mode and FBO,
so reset does not retire the window. Successful CreateDevice publishes its new
window into dx.windows after creation; its constructor-failure destructor also
releases the active context. CreateScreenContext replacement first releases the
old active context, then creates a fresh window. Detached or unpublished failure
wrappers do not retire the shared window. The rest of the device destructor uses
no window after release. ReleaseDisplay sees the cleared global, preventing a
second SDL_DestroyWindow. No selected engine caller creates an additional AGL
shared context. The native Metal stop/presenter destruction still precedes window
destruction; its backend's subsequent ReleaseDisplay is likewise safe.

The installed SDL2 provider is sdl2-compat 2.32.74 over SDL3 3.4.18, confirmed from
Homebrew receipts. The [compatibility adapter](https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.74/src/sdl2_compat.c)
forwards swap and deletion directly. The [current Cocoa context owner](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/video/cocoa/SDL_cocoaopengl.m)
flushes at swap and detaches its view during context destruction; it supplies no
engine parity operation. The [window owner](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/video/cocoa/SDL_cocoawindow.m)
closes its NSWindow on destruction. These concrete owners support explicit window
retirement; they do not promise default-buffer retention or final pixel equality.

Validation: fresh background-priority builds, at most two jobs, compiled the
unchanged existing smoke/fullscreen targets and the selected provider using both
actual stock/CoDx arm64 Release commands. All five existing cases passed:
windowed/borderless/fullscreen smoke and borderless/exclusive backing/display
release. They exercise ordinary FBO/AUX presentation, size reset, context release
and subsequent display release. No new test, case or assertion was added. The
five inactive source configurations remain identical and the changed file has
no __LINE__ expansions. A debugger-only inspection attempt failed to attach/pause
on this Mac and supplies no runtime evidence. No full client link, full CI or
replay was run. Device-constructor failure, direct context replacement, optional
Metal teardown and engine-level vid_restart were compiled/reviewed but not
exercised. The fixtures do not measure the final composited pixel or compare an
original Mac teardown image. Fresh source-only review is owned by the parent.

The resolved ReleaseContext row `mac:0x000052ea` and ReleaseDisplay row
`mac:0x0000532c` keep their intentional status, with evidence updated for this
specific movement of window ownership. All other existing resolved verdicts and
evidence are retained. Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-display-remaining.noindex/`.
