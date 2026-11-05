# Renderer device streams reconciliation

Updated 2026-11-05.

**17/18 reconciled: 14 fixed, 2 intentional, 1 equal; 1 unchecked.** The original assignment contained fifteen gaps. The parent explicitly added the checked GetCode, equality and programmable-only texture-binding rows to document the changed native storage contract.

Mac 1.3 exact-entry disassembly is the authority. The coalesced SetStreamSource body was read in `__textcoal_nt`; raster combiner tables at 0x3266e0 and 0x326714 were decoded from the binary. All eight declaration table formats and all four recognized texture-coordinate generation modes were compared.

## Corrections

The missing declaration parser, stream selectors, stream-pointer collection, VAO eligibility checks, transformation selection, four packet builders, texture matrices/texgen and raster orchestration now have native bodies. Evidence addresses are 0xde60, 0xe29c, 0xe3ee, 0xe4b0, 0xea3e, 0xeb2e, 0xf228, 0xfbac, 0xff3e, 0x10ff2 and 0x19324. SetFVF at 0x1409a now emits the original tokens, invalidates both validation flags and prevents the retained explicit declaration from overriding native FVF element lookup. SetStreamSource at 0x2edb08 clears the locked byte and preserves the original same-buffer/same-offset fast return, including its retained-stride quirk.

Packet range start is a real native pointer shared by Device, COpenGLVAO, CVAOPacket and COpenGL. Every allocation and copy uses the widened owner. Native cache records, insertion, bounds and equality preserve the full uintptr_t key. COpenGL_SetVAO at 0x20b00 now copies that pointer directly. No scalar is reinterpreted as a pointer. The existing UINT32 GetCode facade remains ABI-compatible and intentionally narrows its result; neither active native cache producer calls it.

The existing opt-in ARB draw path now uses the restored declaration/packet/raster chain for the original eight declaration formats, including generic fallback, high/low range selection and 65,535-element chunks from 0x12094. Direct fallback writes are confined to APPLE VAO0 and invalidate the bound-ID cache. Converted native BGRA/ARGB colors use owned scratch storage and generic packets, so cached packets never retain scratch pointers. Raw RGBA streams retain their existing convention.

Modern GL exposed eight fixed-function units and sixteen image units in the existing depth fixture. Calling the original fixed-function target enables on image-only units produced GL_INVALID_OPERATION. The parent approved the native CTexUnit_SetTex adaptation at 0x2007a: preserve binding and sampler-cache copies, then skip fixed-function target enables/disables for programmable-only units. Fixed-unit behavior remains as read in Mac. The measurement establishes this context, not every hardware limit.

Native metadata is lazy-owned through the appended DeviceStreamsNative pointer. ResetStreamsNative frees its optional color allocations and metadata; stream/declaration COM pointers remain borrowed. The concurrently owned lifecycle integration must call this helper for reset/destruction. Malformed tokens, unsupported fixed registers and invalid texture-coordinate indices retain defined native handling instead of recreating out-of-bounds Mac writes. Valid original declarations preserve their operation order and selectors.

Two prerequisite composition errors were repaired: a duplicate CVAOPacket_CopyConstruct and an unterminated merged initializer. The initializer is exactly the reviewed 510b8a7 function, including the System tree helpers. These are parent seed repairs, not Mac behavior defects.

## Verification and limits

The dedicated Release target and actual Apple Device, COpenGL, COpenGLVAO, CVAOPacket and link-stub objects compile. The existing GL-state/layout fixture and all eighteen existing depth-surface cases pass under AddressSanitizer and UndefinedBehaviorSanitizer. The depth harness now links the actual packet/declaration/math/System providers and supplies the native default-texture and shader-prefix contracts; no cases or assertions were added. Legacy OFF preprocessing and actual line expansions are unchanged after accounting for the approved prerequisite initializer repair. Changed production files contain no __LINE__ uses.

DrawIndexedPrimitive remains unchecked as a whole. The CPU/fixed fallback and extended native declaration path still bypass the complete original orchestration, and original point/fence behavior is not closed. Nonzero-base range hints follow the literal Mac quirk: 0x120fc/0x12110 add base to low/high, 0xfc59 separately offsets stream pointers, and 0x12349/0x1236e pass raw indices and those hints unchanged. The existing fixture uses base zero and cannot establish other driver consequences. The original fixed packet builders, declaration/texgen branches, nonzero VAR/static-cache flush and cache-collision behavior were compiled and reviewed but are not established by the depth fixture. No full CI or replay was run.

Private evidence, source hashes, exact-symbol excerpts and build/fixture logs are under `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-device-streams.noindex/`.
