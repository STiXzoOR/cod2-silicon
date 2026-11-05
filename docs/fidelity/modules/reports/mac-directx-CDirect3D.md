# Mac/DirectX 9/CDirect3D reconciliation

Updated 2026-11-05.

31 assigned entries audited: 6 equal, 12 fixed, 3 intentional, 10 unchecked. Mac 1.3 i386 is
the reference. Current native GL remains selected. The ledger includes ten device-private
methods attributed to this translation unit.

The adapter used fixed modes, empty capabilities and unconditional format success. Mac 0xd750
accepts format 22 for mode enumeration; 0xd764 clears the mode and rejects an out-of-range
index; 0xd85c obtains the current mode and rejects a nonzero adapter. These now use the
existing SDL MacDisplay providers. Format/depth/usage gates at 0xd8c4, multisample rejection at
0xd98a and capability fields at 0xd9f8 are restored. GetAdapterIdentifier at 0xd748 leaves
output alone in Mac. Native retains its bounded zero initialization because the source scans
did not establish a selected slot 5 caller's output-storage initialization. Current r_init
creation uses slot 16; reset setup uses slot 11. This is an explicit defined-storage
adaptation.

QueryInterface 0xd6fc AddRefs its result. AddRef/Release 0xd6b2/0xd716 act on their argument,
and GetDirect3DInterface 0xf1d4 now acquires the native singleton rather than returning NULL
from link_stubs.c. Direct3DCreate9 no longer resets its reference count. Native singleton
storage remains static; its destructors clear active ownership without freeing static storage.
Native real texture binding ownership and limits at 0xe222 are restored, retaining the existing
non-COM native image-handle path.

The display dependency 5f652a7 was merged before this work, at local review base 0203fb8. The
native 14-word presentation protocol is windowed 7, interval 11, refresh 12. Startup r_init now
sets word 7 from r_fullscreen, and CreateDevice honors it while retaining the borderless
preference and interval propagation. The imported original struct offsets are not applied to
this private protocol.

CreateDevice remains unchecked as a whole. It selects pointer-aligned static 4096 storage, and
the actual native DeviceImpl size is 3416 bytes (0xd58); a compile-time bound now checks that
storage. SDL creates the context before Init makes GL calls. The typed attachment now creates
the native 24-byte swapchain around the backbuffer. This does not replace the full original
constructor/Reset contract: default depth surface creation and original stage/cache/stream
initialization remain incomplete.

Nine other unchecked entries are the original stream/VAO/declaration helpers,
lighting/transformation/rasterization validation, SetTransform and SetDepthStencilSurface.
Native indexed draw directly consumes CPU buffers and D3D elements, but this is only a partial
replacement for those responsibilities. Depth-surface reference ownership and complete
per-stage matrix/cache behavior remain unresolved. No equality or retirement is claimed for
absent private bodies.

The actual changed client objects compiled with the selected arm64 Release flags. The dedicated
Release target linked at low priority with two jobs. Existing 18-case depth-surface,
shader-raster and saved-screen fixtures passed. Adapter mode, capability, singleton and
creation API repairs were compiled and reviewed, not exercised by those fixtures. No new tests,
CI, replay or game run was performed. Five COD2_X64=OFF preprocess configurations preserve
every changed C body; no changed production file uses __LINE__. The checker also labels Linux
x64 as inactive, but that configuration deliberately enables these COD2_X64 changes.

Private evidence is under the recon-mac-device.noindex directory in ~/Library/Application
Support/CoD2-native-decomp/; original disassembly is not in Git.
