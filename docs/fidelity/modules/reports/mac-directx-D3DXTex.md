# Mac/DirectX 9/D3DXTex reconciliation

Updated 2026-11-05.

5 entries fixed. ViewSingleTexture at Mac 0x23e6e and ShowShadowCookies at 0x2433e had no-op native bodies. They now save attributes, disable the original vendor/program state, set their distinct orthographic projections, save and reset texture matrices in descending unit order, set texgen/texture/combiner state, draw their quads, restore matrices in ascending unit order and pop saved state. The single viewer also preserves client attributes.

The shadow viewer preserves the original 1024x768 RGBA upload, 0xc00000-byte cookie spacing, eight-column placement with 0.125 spacing and 0.09375 quad size, reversed Y projection, and final cookie-count reset. Native cookie-vector pointers use native width. Matrix storage is sized for the current GL unit count, preserving defined storage rather than writing beyond the original eight-matrix stack array on newer hardware.

The static initializer at 0x24994 now has its priority/initialize branching, zeroes vector begin/end/capacity and deletes non-null owned storage at destruction. Its four-byte elements have no observable destruction work. Constructor/finalizer entries 0x24a04/0x249f4 are present in Mac __mod_init_func/__mod_term_func slot six; native constructor/atexit registration restores that lifecycle.

The arm64 Release dedicated build and actual Apple D3DXTex object compilation passed at low priority. Inactive source-body checks pass and this file contains no __LINE__. No new tests, full CI or replays were run. The viewers and their lifetime code were compiled and reviewed, not exercised with a GL context or cookie allocation. Private evidence: ~/Library/Application Support/CoD2-native-decomp/recon-mac-color-math.noindex/.
