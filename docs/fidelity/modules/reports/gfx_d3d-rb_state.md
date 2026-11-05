# gfx_d3d/rb_state module reconciliation

Updated 2026-11-05.

**36/39 reconciled: 33 equal, 3 fixed, 3 unchecked.** Mac 1.3 i386 is the authority.

Compared named native cache fields, device slots/arguments, matrix copies and validity invalidation, stream/index sources, viewport/depth state, texture stages, render targets, and DX7 transforms. The original texture/sampler/stencil/blend tables and the vertex-copy jump table were decoded from the binary.

RB_ChangeState_0 now changes alpha-test function/reference only at the original changed-bit gates (0xced0a/0xced68), and ORs cached function bits when alpha testing is disabled (0xcf176). RB_SetRenderTarget now defines the thirteenth viewport behavior explicitly: Mac's 12-word table at 0x32ab20 reads the following zero at 0x32ab50 for target12. RB_SetVertexData, implemented in rb_shade.c, restores offset-dependent DISCARD/NOOVERWRITE and skips unsupported strides as the table at 0x32b5c0 does.

Three rows remain unchecked: RB_SetAnisotropy, RB_SetSampler and RB_SetInitialState. Mac capability bytes at dx+0x2d65/+0x2d66 and MIN/MAG filter caches at +0x2d68/+0x2d6c are absent from native DxGlobals. Current native code hardcodes MAG linear and derives MIN only from anisotropy. The header and capability producer need coordinated reconstruction; neither was edited here.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
