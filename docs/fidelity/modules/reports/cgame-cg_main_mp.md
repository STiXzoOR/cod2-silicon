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

## Integrated batch verification

Integrated source ed4d921 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh 819d282 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
