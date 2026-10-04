# WS3 — native macOS platform and link closure

Worktree: `~/Projects/cod2-native-wt/platform`; branch: `port/platform`;
base: `bae83a6` (WS1 + WS5). Verified on the Apple M6/macOS 27/Xcode 27 machine
on 2026-10-03. No packages installed, sibling worktrees accessed, remote changes,
pushes, PRs, issues, game data, binary copies or decompiler dumps added.

The platform works in isolation. Both engine source sets compile, and every
remaining undefined symbol is reviewed data/import storage. The client still
cannot link or boot until WS2 supplies that storage. Physical raw mouse behavior
and the complete game remain unverified.

## Implemented behavior

**Window/GL.** `macos_display.c` replaces the temporary display bytes and AGL
stubs with SDL windows and a real legacy 2.1 context. `r_fullscreen` selects
windowed/exclusive; new archived `r_borderless=0` selects desktop fullscreen.
The chosen resolution renders into an RGBA8/depth-stencil FBO, with separate
AUX0/AUX1 attachments used by the existing blur/glow/shadow call sites. Native
`glDrawBuffer`/`glReadBuffer` routing applies only to `rb_state.c` and
`r_screenshot.c`. Presentation uses an aspect-preserving, centered nearest
blit to the drawable. Input applies its inverse, including Retina scale and
letterbox offsets. Resize replaces attachments while retaining the context
and updates the backbuffer surface dimensions. Native failure HRESULTs are
sign-extended through `int32_t` so the renderer's `hr<0` checks work on LP64.
Swap interval is 0; `com_maxfps` governs. `glVertexArrayParameteriAPPLE` is the
explicit unsupported-extension no-op requested in the brief.

Windowed, borderless and fullscreen requests pass the smoke test. This display
rejects exclusive switching with `CGDisplaySwitchToMode(): Unknown Error`;
the implementation reports it and uses desktop fullscreen with the same
chosen render size. Successful exclusive switching on a physical display is
not proven. MSAA is currently unavailable/reported as 0. Gamma uses SDL and is
not tested; video-memory size is unknown/reported as 0 rather than invented.

**Input.** The native `IN_Frame` uses the existing engine event path for key
codes, UTF-8 text scalars, mouse buttons and flipped multi-click wheel events.
Gameplay motion goes to `CL_MouseEvent`; menu motion uses the render-coordinate
transform. Focus loss clears keys and raw deltas; quit queues the engine quit
command. Actual SDL relative-mode state is checked, so a failed enable is not
recorded as success. `in_rawmouse=1` prefers `GCMouse.mouseInput.mouseMovedHandler`;
SDL relative mode is the fallback. The small Apple-only ARC `.m` adapter handles
connection changes, a serial callback queue, accumulated fractional deltas and
shutdown drainage. When raw input is active, SDL motion does not also affect aim.

**Audio.** `macos_audio.c` replaces the reconstructed MacMSS/AUGraph objects with
a software mixer and the SDK default-output AudioUnit. This retains the engine's
Miles call sites while avoiding the old fixed 32-bit object layouts and stub SDK
calls; it is less invasive than changing the engine to AVAudioEngine. PCM and
IMA WAV conversion, sample/stream playback, file callbacks, rate conversion,
looping, pause, buffer handoff and real output callbacks are tested. Streams
are predecoded with a 256 MiB bound. Volume, simple positional pan/attenuation and
feedback reverb are implemented; Miles/EAX quality parity is not claimed.
Native sound hunk allocations use `offsetof(MssSound,data)` and converted PCM
metadata; failed conversion does not expose partially written data.

The DSound voice PCM adapter also produces AudioUnit output. Microphone capture
is explicitly unavailable: recorder init fails, avoiding the legacy recorder's
hardcoded object address. Existing `Voice_Init` consequently disables in-game
voice, including its receive initialization. Voice reception must be separated
from microphone availability or real capture implemented in a later task.
PCM8/24/32 and all SDK-supported compressed stream formats are not comprehensively
tested with authentic assets.

**System/network.** Home is `~/Library/Application Support/CoD2-native`, with
`getpwuid` fallback when HOME is absent. Clocks use `mach_absolute_time`; QPC is
nanoseconds at 1e9 Hz; Windows-style millisecond clocks retain 32-bit wrap behavior.
Native thread IDs use `pthread_mach_thread_np`. Threads have typed native mutex,
handle and lifecycle storage, real virtual destructors, cooperative stop and
join; concurrent stop calls are serialized independently of the worker mutex.
Ring buffers now write bounded data and wrap correctly instead of doing nothing.
Callers must release `StThreadLock` before joining/stopping a worker that needs it.

UDP uses SDK sockaddr lengths,4-byte IPv4 addresses, real fd sets and `-1` socket
sentinels, accepting fd0. Interface enumeration uses `getifaddrs`. Packet events
pack and unpack all 20 bytes of native `netadr_t` (port at18), replacing the corrupt
12-byte copy. Native crash/hunk diagnostics use backtrace/dladdr and builtin
return addresses; the Windows module/CRT diagnostic references are gone. The
SIGABRT test prints arm64 registers and a symbolized stack. Signal reporting
remains best effort, not fully async-signal-safe or stack-corruption recovery.
SOCKS wire-width fixes compile, but authentication/partial-read behavior is
unverified; the inherited 64-byte credential buffer and offered-method byte need
separate hardening before claiming SOCKS support.

**Runtime/link/dedicated.** `macos_cpp_abi.cpp` implements the old GCC COW string,
red-black tree and list operations with native pointers/sizes. It does not cast
those objects to libc++ containers. Native allocation, exceptions and casts use
libc++/libc++abi. StringEd's empty-string representation and tree-header setup
are updated for this native ABI. GNU `--wrap` and its override source are removed;
the existing engine `R_Error` is used directly.

Mutable yacc/lex and `winvoice_*` state have real definitions; const parser tables
remain WS2 data. Arm64 uses the existing offset-encoded script-arena macros.
The foreign X11 `_Linux_PollInputEvent` path is excluded on native Apple.
Read-only reference `nm -a` shows `__mh_execute_header` is the i386 absolute value
4096, so affected dvar flag arithmetic preserves that number explicitly.
The native client links SDK libcurl for the actual download functions.

Dedicated excludes renderer, cgame, UI, client, input and audio source families.
It retains CPU FX parsing, lengths and server visibility, model bone/collision
loading and sound-alias metadata. GPU material/surface and sample loading are
excluded at their call sites. The client prediction slot is null, matching the
reverse server; slot1 retains real server traces. Unknown commands and network
profiling report to the console. Darwin `-dead_strip` discards unreachable draw
methods in the retained CPU units. Its link command has only SDK zlib and libc++:
no SDL, OpenGL, GameController or audio framework. Some discarded objects still
contain unused renderer references; new WS2 function-pointer roots require a
fresh closure check after integration.

## Commands and observed results

```sh
cmake -S . -B build-macos -DCOD2_X64=ON
cmake --build build-macos -j12 -- -k
# exit2 at data-only links; both target source sets compile with zero errors

cmake --build build-macos --target cod2_macos -j12 -- -k > build-macos/ws3-client-final.log 2>&1
cmake --build build-macos --target cod2_macos_ded -j12 -- -k > build-macos/ws3-dedicated-final.log 2>&1
python3 tools/macos-port/check_platform_link.py \
  --client build-macos/ws3-client-final.log \
  --dedicated build-macos/ws3-dedicated-final.log
# 1275 client /348 dedicated; zero unreviewed names; zero compile errors

cmake -S tests/platform -B build-macos/platform-tests
cmake --build build-macos/platform-tests -j12
ctest --test-dir build-macos/platform-tests --output-on-failure
# 14/14 pass, including all three window modes

cmake -S tests/platform -B build-macos/platform-asan -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build build-macos/platform-asan -j12
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-macos/platform-asan \
  --output-on-failure -E '^smoke_'
# 9/9 pass under ASan/UBSan; deliberate crash/guard tests excluded

sh tests/platform/run_dedicated_fx.sh
# ASan CPU parser: 450 ms lifetime, cache, channels/media growth and cleanup pass
python3 tools/macos-port/check_legacy_guards.py --base bae83a6
# 46 modified existing C/header files, 6 inactive configurations, 0 mismatches
```

Smoke reports `GL_VERSION=2.1 Metal - 91.7`, renderer Apple M6, SDL2 runtime
2.32.72/cocoa, swap interval0, all nine brief-required extensions plus framebuffer
blit. Vertex-array-range is absent. At chosen 640x480, windowed drawable is 640x480
and desktop fullscreen drawable 1280x960; FBO/viewport remain 640x480. Main/AUX
colors remain independent across swaps; synthetic 1920x1080 logical/3840x2160
Retina letterbox coordinates map correctly; resize to 800x600 passes. Tone output
observed 24 callbacks, 12288 frames, 11978 audible frames and peak 0.152588.

The suite additionally exercises 16 real UDP/event checks with stdin closed and
HOME unset; actual guard-page faults and SIGABRT reporting; 20 thread restarts
with dual concurrent stops, virtual destruction and native IDs; 10000 wrapped
ring bytes; 12000 differential string operations; randomized trees plus 14400
small-tree erase permutations; lists, exception payload/destructors, multiple
inheritance casts and the actual StringEd file-list builder.

Red evidence exists in ignored `build-macos`: system worker baseline failures
for sockaddr/sender/home/fd0 and packet packing; AUX smoke GL failure before
attachment isolation; and the CPU FX negative control reverting primitive
allocation to 0x2a4, which ASan detects as a heap overflow. No earlier Apple-only
platform suite existed. Source/stub characterization was used for other units,
then the adapter tests were run independently by the root agent. Detailed unit
receipts and exceptions are in [WS3-verification.json](WS3-verification.json).

The legacy check now skips new files that have no baseline body. It confirms
guard discipline; it does **not** prove byte-identical 32-bit executables. This
Mac has no 32-bit execution/toolchain, so reference CI must perform that check.
The top-level `CMakeLists.txt` is untouched; native additions are confined to
its already-guarded Apple module.

## Raw-mouse evidence and limit

Run `build-macos/platform-tests/macos_platform_smoke` with a physical mouse
connected and move it during the three-second capture. It prints both GCMouse
and SDL deltas/event counts. The actual runs here reported devices 0 and both
counts 0, so hardware raw deltas and an acceleration curve cannot be measured.
Synthetic input tests exercise the SDL fallback and engine mapping, not physical
raw input or desktop focus ownership.

The installed Homebrew SDL3 receipt is 3.4.16, behind SDL2-compat 2.32.72. Its
[release Cocoa source](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/video/cocoa/SDL_cocoamouse.m#L601-L612)
reads NSEvent deltaX/deltaY and submits them as relative motion; it contains no
GCMouse movement handler. SDL's current [macOS backend documentation](https://github.com/libsdl-org/SDL/blob/main/docs/README-macos.md)
distinguishes GameController raw movement from accelerated NSEvent fallback.
Thus the installed fallback is an accelerated system-event path by code-backed
inference, not an empirical measurement on this host. Disabling relative system
scale alone cannot substitute for a missing raw backend in that release.
Apple documents [GCMouseInput](https://developer.apple.com/documentation/gamecontroller/gcmouseinput)
as providing raw movement deltas. This port uses that API directly and disables
SDL's newer GCMouse hint to avoid two owners if SDL is upgraded. No global mouse
acceleration settings were changed.

The fixture initially found an unrelated installed SDL2-compat injection defect:
[Event2to3/SDL_PushEvent](https://github.com/libsdl-org/sdl2-compat/blob/main/src/sdl2_compat.c)
rejects a synthetic text-input event by returning NULL, which SDL_PushEvent
forwards into SDL3. ASan localized that crash. The input fixture feeds the poll
boundary and explicitly controls focus while retaining the production event
translation. Native OS text reception is not proven by synthetic injection.

## Undefined counts and WS2 handoff

Before counts include all lines in WS1's committed inventories, including
25 demangled C++ function/data lines that a leading-underscore-only count misses.
After counts use separate target link logs, avoiding interleaved linker output.

| Category | Client before | Client after | Dedicated before | Dedicated after |
| --- | ---: | ---: | ---: | ---: |
| Non-data functions/macros |49|0|55|0|
| Native platform/parser/voice state |41|0|44|0|
| Absolute4096 constant |1|0|1|0|
| Import slots |417|417|417|70|
| Vtable/RTTI/COW data |2|2|2|0|
| Address-named literals |1|1|1|2|
| Other engine data/aliases |855|855|855|276|
| **Total** |**1366**|**1275**|**1375**|**348**|

Full names with categories: [client TSV](WS3-undefined-client.tsv) and
[dedicated TSV](WS3-undefined-dedicated.tsv). The audit script accepts only these
reviewed data names; unfamiliar future symbols fail review rather than silently
being called data. Read-only reference nm classification finds no text symbols
among remaining names. Invented pointer/dvar aliases were also checked against
their source declarations. Client metadata names are `___ZTV12IncludeClass`
and `___ZTIl`; the latter now appears because the old blanket C++ throw shim
has been replaced. Its pointer must route to native long RTTI if legacy Carbon
code is retained. That code still allocates 4-byte exception payloads and is a
specific WS6 hazard; no successful Carbon exception path is claimed.

WS2 must avoid duplicate native owners: mutable yacc/lex globals, `winvoice_*`
state, native SDL/window globals and the four GCC COW exports
`_ZNSs4_Rep20_S_empty_rep_storageE`, `__ZNSs4_Rep20_S_empty_rep_storageE`,
`_ZNSs4_Rep11_S_terminalE`, `__ZNSs4_Rep11_S_terminalE` (C identifiers, before
Mach-O prefixing). The first empty-rep export is actual native storage; the second
is the reconstructed pointer-style slot. Do not serialize old display/thread/
ring/audio object bytes into the new platform objects. Script `VariableUnion`
remains an offset representation, not widened raw pointers. Native COW string,
tree and list layouts are described in `macos_cpp_abi.h`; remaining raw container
node/GUI offsets belong to WS6.

## Merge notes

Merge the entire branch into `port/main`; commits are focused by runtime ABI,
system/network, audio, GL/input/thread, parser/constants and dedicated closure.
Code commits: `4b0be37`, `d8eaefc`, `e33ffdc`, `dfb7cf9`, `f555aab`, `db92422`.
Verification/inventory/report follow in the final commit. No remotes were touched.

`CMakeLists.txt` has no WS3 hunk. `cmake/macos-arm64.cmake` adds C++/Objective-C,
client-only frameworks/SDL/curl, native adapter sources, renderer draw/read macro
properties, source exclusions and the dedicated CPU FX list/dead stripping.
Reconcile those edits with WS2/WS4's data/compatibility additions. Do not restore
GNU wrapping or let legacy stubs intercept SDK audio/AGL calls.

Shared header changes are small Apple-only blocks: 32-bit GL enum/size typedefs,
real mutex/thread/lock/ring layouts, and arm64 use of the existing script-arena
macros. Preserve the original else bodies. `common.c` contains the paired native
packet unpacking and dedicated client/renderer guards; `mac_main.c` packs the
native address and excludes dedicated input/renderer lifecycle. Keep those
changes together.

After WS2 merge, rerun both full target links and the dedicated library/symbol
check: data function-pointer roots may expose previously dead code. Then address
WS6 pointer/layout hazards before claiming menu/map boot, authentic game audio,
voice, server connections, 250 fps or full simulation parity. A connected physical
GCMouse is needed for the remaining raw-motion/acceleration verification.
