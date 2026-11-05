# Unmatched import and executable-entry reconciliation

Updated 2026-11-05.

**657 rows audited: 650 intentional, 5 fixed, 2 unchecked.** There are 631 external-import rows and 26 lower-address entries. No external library implementation is described as equal to reconstructed engine code.

## Import identity and providers

`otool -l -Iv` identifies `__IMPORT,__jump_table` at 0x01acc000, size 0xc5d, with 633 five-byte symbol slots. Its first indirect-table entry is 9. Each assigned address, symbol and nlist index was checked against that table. These addresses are real Mach-O import slots, not synthetic function addresses. All 3,165 on-disk bytes are 0xf4 placeholders; the file contains no library body to disassemble there. The two unassigned jump-table symbols are `__cxa_begin_catch` and `__cxa_rethrow`.

The assigned imports originate in Carbon (243), libSystem (154), OpenGL (125), libstdc++ (36), QuickTime (27), AudioToolbox (16), AGL (11), AudioUnit (10), CoreAudio (4), IOKit (3), and libgcc_s (2). Each verdict names its original symbol/index and selected provider or retired caller boundary. The audit used original direct-call owners, preprocessed native client source references, native object undefined symbols, linked symbol providers and current SDK export declarations. Original calls attributed to SDK headers were traced by their actual enclosing symbols, such as `MacDisplay::Initialize` and `UserPaneDrawProc`.

Native libc, math, pthread, dyld and GL APIs use SDK declarations and runtime providers. An SDK export with no current native call is recorded as an available provider, without approving any missing engine-side call. The retained ASCII rune shims are identified separately from system ctype calls.

The active GCC COW string, list and red-black-tree adapters are in `macos_cpp_abi.cpp`, not the excluded argument-dropping trampoline or malloc-based C++ aliases. Their pointer/size_t fields were compared with StringEd, COpenGLVAO and CVAOPacket caller storage. Native allocation and exception adapters forward to libc++/libc++abi. This does not close C callers' missing allocation-exception cleanup.

Carbon retirement is specific to callers. Legacy MacDisplay, MacMSS and CAudioRecorder sources are excluded by the selected build; SDL display/input, native AIL mixing/ExtAudioFile decoding and the explicit unavailable-microphone boundary replace them. Retained CAE, MacBuilder, MacFolders and custom-cursor helpers have no incoming native engine function calls. Native terminal console and file-preference paths do not construct Carbon controls or preference objects. This does not retire active rendering or every resource helper.

Pending platform sources were read independently where this worktree predates them: recon-unix-runtime e423a2a supplies typed SDK directory deletion, bundle/URL launch and user activity plus its precise stub guards; recon-mac-smalltools 3d0c2c1 supplies SDK MacStrings storage and its stub guard; recon-mac-lifecycle ec76b27 supplies typed SDK GPU model queries. Those source-dependent verdicts explicitly identify the pending provider and are not represented as linked in this worktree.

## Correction

**A rectangle comparator was intercepted by a zero stub.** Mac import 0x01acc2fd is `CGRectEqualToRect`. Native `link_stubs.c` defined an unconditional zero-returning function, shadowing CoreGraphics for `MacMetal_UpdateGeometry`. The placeholder is now excluded only on Apple COD2_X64. After rebuilding, `nm` reports `_CGRectEqualToRect` undefined from CoreGraphics. This repairs an existing provider boundary and adds no Metal behavior. The legacy body is unchanged.

The local `glVertexArrayParameteriAPPLE` no-op is a separate, intentional boundary: its sole engine call requests `GL_VERTEX_ARRAY_STORAGE_HINT_APPLE` / `GL_STORAGE_SHARED_APPLE`; native draws submit CPU arrays without registering an APPLE VAR range. It does not establish general GL or VAO equivalence.


**The retained timing caller used an incompatible structure.** Parent review found that MacTools_Sleep passed and returned a reconstructed AbsoluteTime containing two unsigned-long words (16 bytes on Apple LP64), while the selected macos_compat.c helpers used two unsigned-int words (8 bytes). Both reconstructed native definitions now match the provider’s eight-byte lo/hi layout. Actual caller and provider object compilation confirms size8/alignment4 on both sides; legacy definitions remain unchanged. The native monotonic clock/nanosleep implementation remains an explicit Carbon replacement.

This closes the three UpTime/AddDurationToAbsolute/MPDelayUntil import boundaries and the additional MacTools_Sleep row at 0xa48c. Its exact Mac body passes both returned words through the same three-call sequence. The import module still contains 657 rows; the submitted ledger has one additional cross-module Sleep correction. Sleep timing and failure paths were not exercised. Both native client and dedicated targets rebuilt successfully; fresh source-only follow-up review is pending.

## Lower-address entries and remaining gaps

The low-address FUN rows were located inside their enclosing symbols. Four are MacDisplay initialization cleanup fragments; 0xc157 belongs to the unused cursor vector's static cleanup. TextPool 0xabd76 uses a native fatal-allocation boundary, and screenshot 0xe8b1a uses C allocation with explicit frees. Score 0x112c31 has no native heap deque to destroy. D3DX compiler tails 0x13ce0c/0x13ce4e destroy GCC strings absent from the selected C buffer/cache paths; those paths use malloc/calloc and stdio, not throwing C++ allocation. This does not approve generic shader output or unchecked calloc failure behavior.

Five coal-section texture/cube/vertex-shader thunks preserve their destructor targets with native subobject offsets. The remaining PIC, CRT and loader helpers are owned by the arm64 compiler/linker/runtime, including three real 25-byte `__picsymbolstub2` imports.

The key-resource follow-up closes 0x01acc42e `CFBundleCopyLocalizedString`, 0x01acc429 `CFStringGetPascalString`, and 0x01acc424 `StringToNum` as intentional. At native base `6ae3bd57`, the retained VK translator is reached only by unregistered Carbon key callbacks; the selected SDL key/text path queues engine events directly, and the four native generated-data payloads contain no callback/resource reference. This supersedes the earlier pending-VK_Map gap, without approving the zero language helper, placeholder CF providers or keyboard-layout parity. Original import identities and consumers, including the separate key-alert consumer, are documented in [Mac key-resource imports](mac-key-resource-imports.md). This follow-up is report/ledger-only source proof; it ran no build or runtime check.

Two import-unit rows remain unchecked:

- 0x00113119 and 0x00113166: Optimize's triangle-list and deque exception cleanup. The restored native normal path still allocates with throwing `__Znwm` from C and lacks matching unwind cleanup.
The texture follow-up closes0x002f1830: the native constructor installs the volume secondary vtable and its deleting-destructor slot subtracts one pointer to reach the primary object, then follows D1/free ownership. Exact Mac coalesced thunk adjusts this by four bytes and jumps to D0; the native pointer-sized adjustment is verified against the actual adjacent fields. This boundary is fixed, compiled and reviewed; no secondary-interface runtime coverage is claimed.

## Verification and limits

Both `cod2_macos` and `cod2_macos_ded` configured and built as arm64 Release, at low priority with at most two build jobs. Both relinked after the comparator change. The original comparator guard preserves all six inactive configurations. The subsequent AbsoluteTime type repair changes only the native port-on configuration; all five legacy configurations are identical. `git diff --check` passed. No new tests, full CI, replay, or runtime presentation was run. The comparator change is compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-unmatched-imports.noindex/`. All 657 import-module IDs plus the MacTools_Sleep correction are covered in the submitted unit ledger; global routing files were not changed.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
