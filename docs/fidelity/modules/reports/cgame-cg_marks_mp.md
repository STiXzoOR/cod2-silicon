# cgame/cg_marks_mp module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 2 equal, 1 fixed.** Compared full Mac bodies. Legacy source is unchanged under COD2_X64=OFF.

Restored the 0x15 diagnostic prefix in the free-list error at Mac 0x17c44b.

Compared mark-limit clearing and free links, complete active traversal and sphere culling, impact axes and four corners, 1024-point/384-fragment limits, four color conversions and byte order, oldest-frame eviction, list insertion and vertex copies. The zero-limit behavior is retained.

The active native cg alias is cgArray[0], so the initialization sentinel matches Mac. Generated bss owns 1024 native MarkPoly objects. GfxWorldVertex remains 68 bytes, matching the Mac copy stride; the existing native 1024-vertex assembly buffer and contiguous axis array are preserved. Renderer culling and polygon submission remain behind their established interfaces.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. It excludes cgame, so all five assigned client translation units were also compiled individually with this worktree's actual `cod2_macos` compile commands. OFF preprocessing matches the base with real diagnostic line expansions preserved. No new tests, full CI or replays were run. Changes are compiled and reviewed, not exercised in a running client.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-world.noindex/`.
