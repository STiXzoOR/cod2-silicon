# Renderer device lifecycle reconciliation

Updated 2026-11-05.

**11/11 reconciled: 8 fixed, 2 equal, 1 intentional.** The eleven assigned lifecycle entries were read from exact Mac symbols. No existing module report or central ledger was changed.

Mac CreateDevice at 0xdc82 calls the complete constructor. Both device constructors (0x1a7ba and 0x1ab62) finish through Reset; the native constructor previously installed only its vtable and reference count. Native creation now reaches typed Reset, with the SDL context current before GL initialization. The selected renderer retains its aligned 4096-byte singleton, rejects another live device and leaves that storage intact in D0. Failed creation uses D0 so the shim forgets the reused address.

Reset at 0x19c7c now initializes the actual COpenGL provider, releases owned resources, recreates the backbuffer/swapchain/default texture, resets stream metadata, stage/render/material/light/transform caches and preserves the live device reference count. The Mac viewport cache is zero even though GL receives the presentation dimensions; that quirk is retained. The default texture is format 21 (`D3DFMT_A8R8G8B8`), and Mac 0x1a03b writes exactly `0x000000ff` into its pixel. No color interpretation was substituted.

The native presentation producer publishes fourteen 32-bit words, not the imported pointer-bearing struct. Windowed, depth format, interval and refresh are words 7, 8, 11 and 12. The decoded Mac depth table at 0x327034 selects 16 bits for formats 70/80, 32 for 71 and 24 for 75/77. SDL retains its context for native shader objects; depth changes replace only the depth renderbuffer. Failure restores the old complete attachments and framebuffer/renderbuffer bindings. Color and presentation resources remain owned by the display provider. The accepted display capability boundary advertises no multisampling: the renderer negotiation falls back to zero samples, and direct nonzero sample/quality requests now return not-available instead of silently losing FSAA. No multisample parity is claimed.

Default depth is implicit context storage. The Mac constructor zeros its default COM depth pointer and Reset copies that null pointer into current depth. Native `depthStencilDetached = 0` deliberately keeps the selected SDL depth attachment active initially, including when the COM pointer is null; an explicit null setter detaches it. This is the established native boundary, not literal Mac pointer-based mask behavior. Native owning target setters require separate renderTarget/backBuffer references. A native backbuffer staging texture is retained for selected scaled framebuffer copies; the Mac default surface is ownerless under `g_NoTextureID`.

COpenGL Init at 0x21c10 now has a live lifecycle caller. Its AUX/front/back clears and the debug viewer's `D3DXTex_SetupView` use the existing Apple FBO buffer mapping. The two COpenGL constructors already match the mapped array, identity matrix, list, empty texture-vector and sixteen generic-array defaults. COpenGL+0x800 is the ATI reversed-components capability. The original 96 command-number cache records are not emulated as dead state: the selected ARB fragment and vertex setters really upload each write, matching Mac 0x1b778 and 0x2edf24. Retained-context Init resets supported GL environment registers explicitly.

Present at 0x157c0 now publishes the debug texture ID and invokes both viewers with the device context current. Screenshot and trace consumers remain selected; swap still precedes deferred memory update and the frame increment. D1/D2 now perform resource and COpenGL teardown before releasing the SDL context. D0 intentionally releases contents without freeing static device storage.

## Verification and limits

The arm64 Release dedicated target built with two low-priority jobs. All five affected Apple objects compiled: CDirect3D, CDirect3DDevice, COpenGL, D3DXTex and macos_display. The existing GL layout fixture passed under ASan/UBSan. All eighteen existing depth cases passed with current Device/COpenGL sources and private exact-hash pending array/math providers; only the fake fixture math import was replaced. No test cases or assertions were added or changed. The 35 inactive source comparisons passed. None of the changed files uses `__LINE__`.

Full creation, Reset/depth-format attachment replacement, Present/debug viewers and complete teardown were compiled and reviewed, not exercised. The streams unit supplies `CDirect3DDevice_ResetStreamsNative`; isolated depth fixtures used only its authorized null-metadata substitute. A complete client link requires that real helper and the coordinated owning-target/provider changes. No full CI or replay was run.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-device-lifecycle.noindex/`.
