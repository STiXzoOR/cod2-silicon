# cgame/cg_main_mp module reconciliation

Updated 2026-11-04.

**29/29 reconciled: 23 equal, 5 fixed, 1 intentional.** The Mac 1.3 bodies are the authority. The graphics body lives in `cg_registergraphics_new.c`; it is selected by the native client build. No new Windows platform decision was needed in these ordinary client paths.

The review covered all assigned bodies, including the inline subtitle, localization, smoke selection, callback and animation-tree helpers. A script checked the types, names, defaults, bounds and flags of all 152 Mac dvar registrations, and decoded all three enum string tables. The tables match. The duplicate allocator name in the binary was checked at the assigned cgame address, 0x14444e, rather than relying on the helper's first symbol match.

## Corrections

**Graphics registration continued past the first empty configstring.** Mac 0x143bc8 and 0x143c01 terminate the model and effect loops. Native continued scanning. The model loop also omitted the screen update at 0x143b9b. Inline-model midpoints now follow 0x143d3a–0x143d63: subtract the endpoints in float, then add half that span to the minimum in double before converting to float.

**Entity sound used an interpolated position.** Mac 0x144c30 passes `nextState.pos.trBase`, at centity offset 0x108. Native passed `lerpOrigin`, the field used separately by the orientation callback. The existing audio output fixture now initializes both positions for its stationary emitter.

**Smoke replay used a different empty-selection sentinel.** Mac 0x14468b and 0x144791 test whether the best time is zero. Native tested whether any candidate had been found. The difference matters when a qualifying grenade has time zero; the Mac can replace it with a later candidate. Both searches now retain that quirk.

**Sound registration ran in a different order.** Mac 0x144cf8 registers the two initial aliases, then fourteen sets of 23 surface aliases, then the fifteen remaining aliases. Native registered every single alias before the surface sets. The native typed pointer arrays and bounded formatting remain; the registration order now matches.

**Initialization registered an invented dvar and evaluated two empty debug prints.** Mac 0x146668 registers `cg_shock_sound` immediately after `cg_shock_viewKickRadius`; it has no `cg_shock_viewKickFadeTime` registration. The native shellshock parameter loader already fixes that fade to 3000 ms, as documented in WS9 and WS41. The extra registration has no native or CoD2x consumer. Mac 0x146ecd–0x146ed2 also calls weapon setup and animation loading without the native empty prints and their script-memory reads. The native path now follows that sequence. The existing CoD2x radar registration is retained.

## Intentional native behavior

`CG_GetDObjOrientation` retains the native camera-origin adaptation from commit 19c0151. Mac 0x144569 copies `cgArray.viewModelOrigin`, while native viewmodel effects use the typed `cg->refdef.vieworg`. Entity orientation, handle bounds and viewmodel axes were compared with the Mac body. This review preserves that established native camera contract and changes no renderer or asset provider.

## Verification and limits

The Release arm64 `cod2_macos_ded` target built and linked with two low-priority jobs. Since it excludes cgame, both changed client translation units were also compiled through their actual `cod2_macos` CMake object rules. Both compiled again with the same client flags plus `COD2_CODX=1`.

The existing cgame layout test passed with assertions enabled, checking 72 animation trees, typed sound arrays and 1024 entity handles. The existing ASan/UBSan audio output fixture initially failed because it populated only the old interpolated sound position. After correcting that setup, all 138 run/walk/prone first/third-person surface events reached native Render PCM with the expected output. No new tests were added.

All ten source-body comparisons in the five `COD2_X64=OFF` configurations passed. Neither changed production file uses `__LINE__`. No full CI, replays or live client session were run. Graphics loading, the smoke time-zero case and full initialization remain compiled and reviewed, not exercised in a game session. The registration fixture checks final sound storage rather than alias lookup order.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-main.noindex/`.
