# REF-B: mtld3d Windows rendering reference

2026-10-23, branch `decomp/ref-mtld3d`.

**Result: mtld3d 0.14.2 renders CoD2's textured world and sky. Keep it as a
second reference.** The October 7 white-world failure does not reproduce on
this supported runtime. This does not establish the root cause of the old
DXMT 0.4/Wine 10 failure, or make either translator ground truth.

## Runtime and isolation

Private evidence and rerunnable installation:

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/REF-B.noindex"
```

- [athei/wine-build cx-26.3.0-9](https://github.com/athei/wine-build/releases/tag/cx-26.3.0-9),
  x86_64 under Rosetta, in `$E/runtime/wine`. Bundled x87sidecar 1.9.0;
  startup log confirms cooperative attachment for the i386 game. No fallback
  Wine combination was needed.
- [mtld3d v0.14.2](https://github.com/athei/mtld3d/releases/tag/v0.14.2),
  installed by its [builtin instructions](https://github.com/athei/mtld3d/blob/v0.14.2/INSTALL.md):
  replace the i386/x86_64 PE pairs and x86_64 Unix library under
  `runtime/wine/lib/wine/d3d9/mtld3d` **before** creating the win64 prefix.
  The bundled compatdb selects mtld3d by default. No game-side D3D9 override
  is present in this game tree. The live log confirms all three components
  are 0.14.2, i686 PE and x86_64 Unix under Rosetta.
- Independent D9VK control: recovery Highball Sikarugir Wine 10 r19,
  APFS-cloned into `$E/d9vk-runtime` from the read-only recovery engine,
  **new private prefix** `$E/d9vk/prefix`, official
  `v1.10.3-20250511-async (macOS)` D3D9 DLL and MoltenVK 1.4.1. The DLL
  matches the recovery game copy and official archive's x32 DLL exactly.
  This official DLL is already unmarked; it needs no marker clearing. The
  engine's bundled D9VK is a different file and was not used for captures.
- Two private APFS-cloned game asset trees, from read-only `~/Games/CoD2`;
  the diagnostic executable, CoD2x proxy/original Miles DLLs and renderer
  DLLs came from the recovered reference. Executable and CoD2x DLL byte
  comparisons pass. All accepted game logs contain `CoD2x 1.4.6.8 loaded`.
  Miles support files were cloned from the licensed install. No CD key,
  Steam credentials or account state was needed.

Published download digests, checked before extraction:

| Archive | Published SHA-256 | Result |
| --- | --- | --- |
| `wine-cx-26.3.0-9-macos-x86_64.tar.xz` | `af3682051414eeffa5be48584a671790fd3852e980ff5acd38facff6f4bff1e9` | Match |
| `mtld3d.tar.xz` | `8a3f159bd6f0042af106b16273cf417f9f8164acfbf9fe8be5ab31ca30ad7e2c` | Match |

These are GitHub release asset digests. Neither release publishes a detached
checksum/signature asset. Release API responses, installation instructions
and `setup-checks.txt` are private. Unneeded download archives were removed
after extraction; the installed runtime, notices and prefixes remain.

## Method

The private `run.py` adapts the earlier delayed CFG driver: after map load,
read `sv_serverId` and configstrings, derive the live menu indices, then send
literal `cmd mr` responses for `serverinfo_dm close`,
`team_britishgerman allies`, and `weapon_british enfield_mp`. Enable noclip,
hide HUD/gun/FPS, and use the recovery camera positions. It does not depend
on the unsuccessful `openscriptmenu` approach.

Both translators use 1280x720 Wine virtual desktops, DX9, fullscreen inside
that desktop, vsync off, maxfps 60, FOV 80, gamma 1, picmip/bump/spec 0,
manual picmip, AA 1, private `fs_homepath`, and loopback port 29263.
The same recovered profile configuration is copied into each private run.
Saved `r_*` configuration entries match exactly between the two normal runs.
No render option is changed for Black except `r_colorMap`.

| Pose | `setviewpos` | Logged view origin / yaw |
| --- | --- | --- |
| Courtyard | `2569 2274 181 180` | `(2569 2274 182) : 179` |
| Rooftop | `3051 2178 220 180` | `(3051 2178 221) : 179` |
| Sky | `3051 2178 350 0` | `(3051 2178 351) : 0` |

Sky uses held/released `+lookup` to reach the pitch clamp, as in recovery.
The original Windows `viewpos` prints origin and yaw, not pitch: the clamp
is inferred from the deliberately long angular input and matching image
orientation, not independently instrumented. This repairs the recovery's
unaccepted courtyard spawn capture by checking the actual new pose.

The D9VK frame-count pauses are shorter because the inherited pauses failed
to reach the readiness query within 180 seconds; camera coordinates and
render settings are unchanged. `cl_pitchspeed` is increased for D9VK so its
shorter lookup also reaches the clamp. Both variants of each translator use
the same scene commands apart from `r_colorMap`. Each capture requests
`screenshotJPEG` and uncompressed `screenshot` TGA. They are two frames
apart, so animated content need not be identical between formats.

Each launch takes the shared GUI lease atomically with exclusive creation,
records REF-B and the live runner PID, and removes only its own lease in
cleanup. Every game run has a 300-second limit (180 seconds for readiness)
and prefix-scoped shutdown.
Accepted D9VK GUI captures use the recovered launch path with nice 19; a
`taskpolicy -b` GUI attempt stalled before client initialization and was
rejected at the readiness timeout. Image analysis uses `taskpolicy -b nice
-n 19`. Initial mtld3d captures used the direct Wine launch; its repeat uses
nice 19. No builds were needed. The screen was locked
(`CGSSessionScreenIsLocked=Yes`); other user apps and background Wine
services remained active. These are engine-framebuffer
checks, not performance measurements or physical-display visibility tests.
No screen was unlocked and no unrelated process was stopped.

## White-world check and comparison

Both normal captures show world geometry, textured walls/ground, lighting
and cloud sky. The former 96.6%-white DXMT frame is not reproduced. On
mtld3d, `r_colorMap Black` makes the world predominantly black/dark while
leaving sky visible; it moves away from white, as a color-map replacement
should. D9VK shows the same dark-world/visible-sky response. Courtyard TGA mean
RGB moves from mtld3d `[65.0834,59.9781,49.8237]` / D9VK
`[65.0259,60.0594,50.0393]` to Black `[1.8841,1.9648,2.1174]` /
`[2.3086,2.4005,2.5894]`. Exact-white pixel fraction is zero in all four
courtyard TGAs; exact-black fraction rises to 91.03% / 88.25%. Black is a
diagnostic replacement, not a workaround needed to make mtld3d render.
Retire the old DXMT 0.4 combination as a failed rendering reference. This
experiment does not isolate which change removed its white-world failure:
frontend, Wine runtime and presentation setup changed together. The
responsible shader/state/readback defect remains unknown.

Full-frame mean absolute error, RGB byte values 0–255, no cropping,
registration, color adjustment or masking:

| Pose | TGA R / G / B | TGA mean | JPEG R / G / B | JPEG mean |
| --- | --- | ---: | --- | ---: |
| Courtyard | 0.3820 / 0.3371 / 0.3457 | 0.3549 | 0.8223 / 0.7325 / 0.8223 | 0.7924 |
| Rooftop | 0.1444 / 0.1503 / 0.1621 | 0.1522 | 0.5184 / 0.5045 / 0.5787 | 0.5339 |
| Sky | 0.0153 / 0.0069 / 0.0080 | 0.0100 | 0.0934 / 0.0792 / 0.0988 | 0.0905 |

Independent-process courtyard repeats (not two screenshots of one process):

| Translator | TGA R / G / B | TGA mean | JPEG R / G / B | JPEG mean |
| --- | --- | ---: | --- | ---: |
| mtld3d | 0.2996 / 0.2665 / 0.2889 | 0.2850 | 0.3741 / 0.3274 / 0.3793 | 0.3603 |
| D9VK | 0.1768 / 0.1391 / 0.1441 | 0.1533 | 0.2212 / 0.1675 / 0.1745 | 0.1877 |

The courtyard cross-translator error is close to mtld3d's measured run
variation. The amplified courtyard differences concentrate around ground
and wall haze and also appear in the mtld3d repeat. This is not a formal tolerance or a subtraction of noise from
error. Only courtyard was repeated; rooftop/sky have no measured repeat
floor. Runs use live local servers, not synchronized demo ticks. JPEG
compression contributes additional differences; TGA is the stronger pixel
comparison. Private `comparison/` contains `metrics.json`, raw absolute RGB
difference PNGs and 4x-amplified difference PNGs for each pose/format, plus
normal-vs-Black, cross-translator Black and repeat comparisons. D9VK has
a visibly wider bright halo at the courtyard sky opening in Black mode;
mtld3d keeps that edge tighter. Black cross-translator TGA mean MAE is
about 0.6473 courtyard, 0.7183 rooftop and 0.3754 sky. This diagnostic
difference does not identify which implementation is correct.

Visually, the two references agree on camera framing, surface detail,
lighting and sky; no white-world or missing-geometry defect distinguishes
them. Neither has an obvious visual advantage relative to the known final
WS15 appearance (textured world and cloud sky). The literal WS15 private image
directory is absent, so a fresh direct WS15 ranking is unavailable. As an
explicitly later proxy, the existing October 16 `native-codx-captures-03`
images give all-channel JPEG MAE of mtld3d/D9VK: courtyard 2.1656/1.9984,
rooftop 3.5677/3.4985, sky 0.5746/0.5767. D9VK is slightly numerically closer
to that native courtyard/roof; mtld3d is slightly closer to its sky. These
small differences do not establish which translator is more faithful.
`comparison/native-context.json` records this optional context check.
This is evidence for retaining a second translator, not evidence that either
is the original Mac renderer.

## Rerun and verify

Accepted captures are in `mtld3d/{normal2,black,repeat}` and
`d9vk/{normal3,black,repeat}`, each under `main/screenshots`. Validation
passed for all 28 captures, six successful exits, logged poses and color-map
settings, and all 56 generated difference PNGs. Every run has
its command/environment, stdout and engine log; successful runs have
`result.json` recording exit 0. The runtime's own mtld3d logs are beside its
game executable, in `mtld3d/game/mtld3d-logs`.

Recompute and validate existing results:

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/REF-B.noindex"
PY="$E/venv/bin/python"
unset REFB_SUFFIX
taskpolicy -b nice -n 19 "$PY" "$E/compare.py"
taskpolicy -b nice -n 19 "$PY" "$E/verify.py"
```

Rerun all six game captures into new private directories, sequentially. The
runner obtains/releases the GUI lease itself; no existing Highball prefix is
used. Do not run the two translators concurrently:

```sh
export REFB_SUFFIX="-rerun-$(date -u +%Y%m%dT%H%M%SZ)"
python3 "$E/run.py" mtld3d normal2
python3 "$E/run.py" mtld3d black
python3 "$E/run.py" mtld3d repeat
python3 "$E/run.py" d9vk normal3
python3 "$E/run.py" d9vk black
python3 "$E/run.py" d9vk repeat
taskpolicy -b nice -n 19 "$PY" "$E/compare.py"
taskpolicy -b nice -n 19 "$PY" "$E/verify.py"
unset REFB_SUFFIX
```

The suffix selects new capture homes and lets comparison/validation use
them. Comparison output in `comparison/` is regenerated. The installed
runtimes and prefixes require no download or installation to rerun. The
retained evidence tree is about 11 GiB by `du -sh`, including shared APFS
clones; that is not an exclusive physical-storage measurement. mtld3d Wine
is about 1.2 GiB and its prefix 993 MiB; D9VK runtime is about 1.2 GiB and
its prefix 337 MiB. Unused installer payloads were removed, with notices
and instructions retained. Windows sound was initialized for these runs; audible sound, input, combat,
multiplayer authentication and long-run stability were not tested.

## Scope and merge notes

Only this report is committed. The private scripts/runtime/prefixes/images
remain outside Git. No native engine source, build flags, data snapshots,
existing reference prefix or Highball installation changed. There are no
engine files requiring `unifdef -UCOD2_X64` comparison. Native builds and the
engine merge-gate suite were not run for this report-only unit. They are
not claimed green: `legacy-abi` retains its 10 known differences,
`tests/cod2x/check_inactive_gates.py` its 436 known differences, and x86 Linux
byte comparison remains unavailable.

Initial failed trials are retained separately: mtld3d `normal` omitted the
Miles support directory and returned to the menu after sound initialization
failed; D9VK `normal` loaded the map but exhausted the readiness timeout in
an inherited frame-count pause. D9VK `normal2` was the rejected background-
policy trial and also produced no comparison imagery. None supplies
comparison results. Shortening the initial query delay exposed an early
configstrings query; the driver now repeats that query until menus exist.
The mtld3d Black run was recovered by updating its waiting CFG before
readiness expired; its accepted scene/capture commands were unchanged.
Inherited missing CoD2x menu-material and beep warnings remain visible in
the logs. They were not treated as rendering failures or repaired here.
The first boot also logged and ignored a redundant malformed compatdb
entry; accepted game runs use the documented bundled default instead.

The REF-B GUI lease is released and no REF-B Wine/game process remains.
No GUI `.app` bundle was created, registered or installed. No global package,
configuration, remote, push, tag or PR operation was performed. All temporary
material is inside the evidence directory; there are no external scratch
builds to remove. The original binary and native renderer were not modified,
so there is no Mac assembly behavior change to substantiate.
