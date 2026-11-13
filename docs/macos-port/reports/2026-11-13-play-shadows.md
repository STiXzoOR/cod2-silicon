# Character shadow cookies — 2026-11-13

Both native engines now schedule the original character shadow-cookie pass.
The visible Shadows control (`sc_enable`) produces soft character shadows.
The prior white-sampler safeguard remains the fallback when the current view
has no completed receiver target.

## Original behavior and implementation

| Native file/function | Original reference | Behavior |
| --- | --- | --- |
| `r_shadowcookie.c`, `R_GenerateShadowCookies` | Windows renderer 0x100280f0/0x10028330; Mac i386 0xe16f0 | Select at most 24 casters by type, render flags, distance/bounds weight; retain the original adaptive weight cap and fade. |
| `r_shadowcookie.c`, `R_AddShadowCookie` | Windows 0x100275a0–0x10027ac0; Mac 0xe08f0 | Sun axes, eight-corner projected bounds, padded orthographic projection, five frustum planes, caster and eligible receiver surfaces. |
| `r_shadowcookie.c`, `R_DrawShadowCookies` | Windows 0x10027d20; Mac 0xe1460 | 128×128 cookie, 126×126 inset, caster technique 31, explicit Windows shader blur, receiver technique 32. |
| `r_rendercmds.c`, cookie/blur producers | Windows 0x10021ee0/0x100225c0; Mac cookie 0xb0fb0 | Critical command 9 carries matrix/fade; ordinary command 28 schedules blur. |
| `r_scene.c`, `R_RenderScene` | Mac 0xaea20; Windows prepass 0x10024e80/0x10024f62 and sunlight 0x10025430 | Generate after main surface sorting; group 0 white/depth prepass, group 1 cookies, group 2 sunlight. Reuse receiver depth. |
| `rb_backend.c`, `rb_shade.c` | Original group ordering and native target aliases | Track filled/copied view identity; sampler 19 uses shadows only within that target's valid lifetime. |
| `lp64_device_targets.inc`, `DeviceTargets_Stretch` | Native AUX ownership during the original blur sequence | Copy current equal-size AUX content directly to the blur texture without overwriting the live receiver attachment. |

The OpenGL caster uses the Mac i386/PPC Y flip and positive lookup scaling.
The explicit blur follows Windows, with bottom-up texture T coordinates;
Aspyr's additional automatic copy blur is disabled for this sequence. The
caster's already-flipped Y needs a +0.5 GL raster center, while ordinary scene
and 2D drawing retain their existing correction. This preserves the original
cookie geometry and blur rather than introducing a replacement shadow method.

Native targets DYNAMICSHADOWS (3), RESOLVED_POST_SUN (1), and SAVED_SCREEN (7)
share storage. The receiver prepass fills AUX1; cookie rendering/blur uses AUX0
and its 128×128 textures without staging over AUX1. Returning from target 3 to
the framebuffer resolves receivers. Only then may sampler 19 use that view's
shadow texture. Early post effects and saved-screen writes invalidate it before
reusing the shared image. Command execution and a new receiver clear reset it.

Windows multiplayer does **not** submit the normal local first-person body as a
shadow caster. `CoD2MP_s.exe` CG_Player 0x4dc6fd–0x4dc717 returns before body
submission; the separate viewmodel's flags 0x8c are excluded by the renderer's
bit-8 check at 0x10028147. Mac CG_Player 0x1dd25f–0x1dd283 agrees. The retained
Linux definitions name the local/followed-player mask bits. Third-person body
submission remains eligible. The downward reference therefore verifies absence
of an own-body shadow, while showing another character's shadow.

## Compared frames

Fresh Windows 1.3 frames were made under Wine from a locally recorded Toujane
demo with three grounded test clients (`developer 0`). Native replay uses the
established `--shift -6`. Numbers below are mean absolute RGB differences out
of 255 from lossless TGA inputs; retained review images are JPEGs. The engines
are pixel-identical to each other on all six character frames.

| View | 1920×1080, 2×AA MAD | Pixels >16 | Signed native−Windows | 5120×2880, 4×AA MAD | Pixels >16 | Signed native−Windows |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Third-person own shadow | 0.273987 | 0.312982% | +0.009586 | 0.249759 | 0.281467% | +0.009207 |
| Other characters | 0.130412 | 0.122251% | −0.003973 | 0.111260 | 0.077901% | −0.004321 |
| First-person downward | 0.146993 | 0.016975% | +0.001832 | 0.084753 | 0.002808% | +0.001535 |

Subtracting each renderer's matched Shadows-off image isolates the shadow
change. At 1080p, native/original integrated darkening ratios are 1.003842,
0.996122 and 0.991861 respectively; weighted centroid differences are 0.054,
0.171 and 0.754 pixels. At 5K the ratios remain within 0.83% of one. These are
aggregate measures, not a claim that every silhouette pixel matches: the
third-person shadow-region error is 1.69/255 at 1080p (1.64 at 5K), and visible
body/animation residuals also exist with Shadows off.

The five existing Toujane views remain at 0.118524–0.284152/255 with Shadows on
and 0.118524–0.273028 with Shadows off. The original on/off reference itself
changes slightly in overview, gun and third-person views. The port's five
Shadows-off frames, three new 1080p frames and three new 5K frames are all
pixel-identical to the pre-change build.

With glow, distortion and soft particle edges enabled, the additional three
Shadows-off frames also match the pre-change build exactly. That configuration
has a pre-existing Windows/port residual of up to 2.372263/255; enabling shadows
makes it 2.374780. Its isolated third-person and other-character shadow changes
match the post-effects-off measurements. General post-effect parity is not
established by this unit.

## Cost and checks

On this Mac at 1080p/2×AA, uncapped CoD2x with three visible characters took
2.6796 ms/frame with shadows off and 3.4629 with shadows on: +0.7833 ms (+29.2%).
This is one 12-second live run per setting with one second trimmed at each end;
3,740/2,897 frames remain. A short 115-frame timedemo, three alternating runs per
setting, had medians of 2.8694 and 4.2790 ms (+1.4095 ms). Its off runs ranged
343.3–393.8 fps and on runs 228.2–241.6 fps, so it is less stable than the live
measurement. No claim is made for a full server or other hardware.

Both Release engine builds passed. The existing 26-fixture ASan/UBSan renderer
suite and CGL raster/saved-screen checks passed. The sampler fixture now covers
unfilled/filled/invalidated target selection. The outdoor matrix fixture gained
the tessellation stub needed by the caster matrix path.

The aggregate ABI gate remains red at its raw-offset stage: 51 rejected keys
with CoD2x, one with stock. Independently archived/configured main fa15e5b7 and
starting branch 3cdcf869 have the same rejected keys and occurrence multisets.
The allowlist is unchanged; moved line numbers were not the cause.
The later ABI stages were run separately for both engines: function, callback,
bit-conversion, strict and licensed-reference import checks all passed. The
function audit covered 682/643 TUs with zero new mismatches; import checks found
zero proven extra or missing dereferences among 531 import symbols.

Private evidence: sibling `_agents/play-shadows-evidence/` and
`_agents/play-shadows.report.md`, including original addresses/disassembly,
frame hashes, JPEGs, numeric JSON, replay plans, capture scripts and run logs.
No licensed binary, shader cache, game data or build scratch is committed.
