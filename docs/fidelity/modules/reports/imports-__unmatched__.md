# Unmatched import and executable-entry reconciliation

Updated 2026-11-05.

**657 rows audited: 650 intentional, 1 fixed, 6 unchecked.** There are 631 external-import rows and 26 lower-address entries. No external library implementation is described as equal to reconstructed engine code.

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

## Lower-address entries and remaining gaps

The low-address FUN rows were located inside their enclosing symbols. Four are MacDisplay initialization cleanup fragments; 0xc157 belongs to the unused cursor vector's static cleanup. TextPool 0xabd76 uses a native fatal-allocation boundary, and screenshot 0xe8b1a uses C allocation with explicit frees. Score 0x112c31 has no native heap deque to destroy. D3DX compiler tails 0x13ce0c/0x13ce4e destroy GCC strings absent from the selected C buffer/cache paths; those paths use malloc/calloc and stdio, not throwing C++ allocation. This does not approve generic shader output or unchecked calloc failure behavior.

Five coal-section texture/cube/vertex-shader thunks preserve their destructor targets with native subobject offsets. The remaining PIC, CRT and loader helpers are owned by the arm64 compiler/linker/runtime, including three real 25-byte `__picsymbolstub2` imports.

Six rows remain unchecked:

- 0x01acc42e `CFBundleCopyLocalizedString`, 0x01acc429 `CFStringGetPascalString`, and 0x01acc424 `StringToNum`: Mac `GetLanguageCode` at 0x9cbc uses these, and pending VK_Map still calls native `MacResources_GetLanguageCode`, which returns zero. The retained key-language resource path has no complete native provider/storage replacement.
- 0x00113119 and 0x00113166: Optimize's triangle-list and deque exception cleanup. The restored native normal path still allocates with throwing `__Znwm` from C and lacks matching unwind cleanup.
- 0x002f1830: the volume-texture secondary destructor thunk has no native body. The native constructor leaves its secondary vtable null, but exhaustive secondary-interface/cast caller retirement has not been established.

## Verification and limits

Both `cod2_macos` and `cod2_macos_ded` configured and built as arm64 Release, at low priority with at most two build jobs. Both relinked after the comparator change. The inactive guard checker passed all six configurations with zero mismatches; `git diff --check` passed. No new tests, full CI, replay, or runtime presentation was run. The comparator change is compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-unmatched-imports.noindex/`. All 657 exact IDs are covered in the submitted unit ledger; global routing files were not changed.
