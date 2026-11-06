# system/basic_string.h attribution reconciliation

Updated 2026-11-05.

1 equal, 11 fixed, 21 intentional, 1 unchecked (34 entries).

The real owners include material loading, ARB shader programs, StringEd, native startup/display and cursor services, and shader-cache metadata. Material_LoadPassShader had a reversed version clamp and wrong 16-bit size-shift order. ARB constructors now restore the prior binding, fragment texture-use masks handle indices 10–15, and current-program destruction disables the corresponding target. Native vertex cleanup is reached once through its owning shader destructor. Raw ARB/cache/declaration storage remains the selected native replacement for old name::program strings and stream descriptors. SE_GetFoundFile matches its 64-byte split-and-consume algorithm. Carbon display/cursor structures and the old runtime shader string-map have specific replacements recorded per entry, rather than a system-header exemption. CreatePixelShaderOpenGL remains unchecked: ignoring ShaderType and substituting PixelShaderStub has no established ATI/NV/failure equivalence.

Changed or unresolved examples: `CDirect3DDevice_CreatePixelShaderOpenGL` at 0x0001b0da, `COpenGLARBFragmentProgram` at 0x0001c8a0, `COpenGLARBFragmentProgram` at 0x0001ca34, `COpenGLARBFragmentProgram` at 0x0001cbc8, `COpenGLARBFragmentProgram_COpenGLARBFragmentProgram` at 0x0001bdf8, `COpenGLARBFragmentProgram_COpenGLARBFragmentProgram` at 0x0001c34c, `COpenGLVertexProgram` at 0x00023538, `COpenGLVertexProgram` at 0x000236b4, `COpenGLVertexProgram` at 0x00023830, `COpenGLVertexProgram_COpenGLVertexProgram` at 0x00022de0, `COpenGLVertexProgram_COpenGLVertexProgram` at 0x0002318c, `Material_LoadPassShader` at 0x001039b6.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. The arm64 Release cod2_macos_ded target builds. Changed client sources were compiled separately because the dedicated target excludes them. No new tests, full CI or replays ran; changes are compiled and reviewed, not exercised. All COD2_X64=OFF bodies pass the baseline guard comparison, and no changed file uses __LINE__.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-system-runtime.noindex/`. The per-entry unit ledger is maintained outside the repository under the assigned orchestration directory.

Native shader-blob cleanup remains unconditional after device creation. Mac 0x103e50 bypasses the release on failure; retaining the native release prevents a leak of the separately owned CD3DXBuffer and its data. Independent review identified this lifetime boundary; copying shader bytes into material storage does not transfer blob ownership. Compiled and reviewed only.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
