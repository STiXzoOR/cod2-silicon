# cgame/cg_newDraw_mp module reconciliation

Updated 2026-11-05.

**24/24 reconciled: 19 equal, 5 fixed.** Every assigned body was compared by hand with Mac 1.3, including the 105-entry ownerdraw dispatch table at 0x33ba90 and its inlined draw paths. Supporting scripts compare calls/strings, literals, all 21 dvar registrations (type, name, default, bounds, flags), the four pulse magnitudes and all 54 stance command pointers. The five inactive preprocessing configurations retain the original engine source body. The source does not use `__LINE__`.

## Corrections

**Hint colors and health pulse magnitudes were missing.** Native's tentative declarations produced zero arrays. Mac stores white for both hint colors at 0x33ba40 and 0x33ba60 and `{1, 0.8, 0.6, 0.3}` at 0x33ba50. `CG_DrawHoldBreathHint`, `CG_DrawMantleHint` and `CG_PulseLowHealthOverlay` now use those values. The pulse-index bound also follows Mac's unsigned comparison at 0x18b343.

**The ammo display omitted several draw paths.** Mac 0x18c520–0x18c559 uses the next snapshot's entity weapon, with a range-checked selected-weapon override under the 0x800000 flag. Negative reserve ammo suppresses its display; clip-only weapons suppress the clip. The two ownerdraw IDs share the combined layout. Native now preserves those branches, centers a lone clip/reserve at the original baseline, draws the low-clip flash over the normal clip, updates its 800 ms clock, and caps its alpha, as at 0x18c5d8–0x18c666 and 0x18c903–0x18cc26. The reserve and separator use their own low-ammo color.

**Ownerdraw geometry, dispatch and inline state were incomplete.** Mac 0x18cc3f–0x18cc5d copies the input rectangle directly; it never reads `text_x` or `text_y`. The two plain score cases are 67 and 68, not 66 and 67. The team background takes the valid local client's team at 0x18e07a–0x18e0d7. The shader uses `CL_DrawStretchPic`, while backdrop, weapon-name background and mode-icon paths retain `UI_DrawHandlePic`.

The health bar now applies Mac's tint, texture coordinates, delayed red health segment and frame-time decay (0x18cc76–0x18cf3a, 0x18e689–0x18e6c6, 0x18e868–0x18e88d). Its background now draws the injured/critical pulse and updates its pulse time (0x18d793–0x18da58). The full-screen health overlay follows its own nonzero-health gate and interpolation (0x18db6a–0x18dc78), without the unrelated health-bar dvar gate. Weapon-name text reserves 28 pixels at the right, its foreground uses the direct selection fade, and its background/mode icon use the 1,800 ms menu fade. Missing mode icons do not draw the fallback. The ammo backdrop gets its low-ammo tint, compass alpha updates its supplied color, and compass friendlies retains the callee's own gating.

Equal examples include `CG_CalcPlayerHealth`'s next-snapshot health/max-health ratio and dead-player gate; the reserve and clip warning thresholds; server material range/length validation; all three low-health pulse phases apart from the missing table and unsigned bound; scoreboard key handling; archive field order and sizes; and every stance command search, timing and draw branch. `CG_ApplySplitScreenCompassScale` is also empty in Mac.

## Verification and limits

The arm64 Release dedicated target builds with two low-priority jobs. Both changed engine source objects compile. Existing ownerdraw groups run with ASan/UBSan in stock and CoD2x modes: all ten groups pass in each mode. They exercise the existing objective and location paths and the corrected rectangle origin; they do not exercise the restored ammo, hint, health-bar, pulse or weapon-name paths. Those changes are compiled and reviewed, not rendering- or replay-tested. No new cases or tests were added.

History and `UI_OwnerDraw` were checked before changing fixture expectations: there is no documented native placement adaptation, and `UI_OwnerDraw` applies text offsets only to its later UI-specific dispatch after forwarding the unmodified rectangle to `CG_OwnerDraw`. Existing fixture expectations moved by (-3, -5), retaining the nonzero offsets as conflicting inputs. The fixture initially failed to link because its extraction omitted the existing `AngleNormalize360Native` helper; its runner now conditionally extracts that helper before its callers, including compatibility with older baselines without it.

Existing native validation around absent snapshots, invalid client indices, missing fonts/materials and weapon metadata is retained where present. It defines storage/interface behavior outside the normal valid engine contracts rather than reproducing invalid Mac memory accesses. Existing CoD2x compass visibility/offset controls remain intentional product extensions.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-hud.noindex/`.
