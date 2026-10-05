# WS38 — native LP64 raw-offset audit

2026-10-05. Worktree `lp64-audit`, branch `port/lp64-audit`, starting commit
`483a7d0` (`port/ui-lp64`, on WS36). Read the full PLAN before any other work.
Repository source contents and edits stayed in this worktree; documented private
references and the licensed install were read-only inputs. An initial delegated filename
search traversed sibling directories and returned filenames; no sibling file
contents were opened or modified. The search was then restricted to this worktree.
No package installs, remote changes, pushes or PRs occurred; no game data or
derived imagery was committed.

## Result

Both stock and CoD2x arm64 Release engines build. The new raw-offset checker
covers every `cod2_macos` translation unit in each engine compile database and
reports zero unreviewed/unsafe candidates. Every changed production C file has
an exactly identical `COD2_X64`-undefined source view to `483a7d0`.

Both final binaries pass the new menu acceptance test: 59 front-end menus,
73 menus after a local Toujane listen-server load, 46 handled UI script commands,
and 132 JPEG captures per build. No crash, `Sys_Error`, fatal diagnostic or
unexpected process exit occurred in either final run.

Both complete ABI gates pass: stock 621 TUs and CoD2x 638 TUs, with zero errors,
mismatches or new findings. Each has 218 renderer bindings with zero callback,
named-cast or unprototyped-floating findings. Retail import checks inspect 514
import symbols and report zero proven extra/missing dereferences in both builds
(1776 stock / 1778 CoD2x native accesses).
The actual i386 object/executable equality gate could not run on this arm64 Mac;
source-view equality is supplementary evidence, not a binary-equality claim.

## Inventory and checker

`tools/abi/raw_offsets.py` reads only `cod2_macos.dir` entries from the supplied
compile database. Clang preprocessing uses each entry's actual flags to select
active original source lines. This selects `COD2_CODX`, Apple SDK guards and
nested native conditions correctly, while original spelling preserves literals
hidden by macros. A simple `unifdef -DCOD2_X64=1 -U__EMSCRIPTEN__` view leaves
some derived Apple SDK conditions unknown; actual compiler selection avoids
classifying their inactive reconstructed layouts as native code.

The scanner covers byte/char/void-pointer literal arithmetic and dereference
casts, hexadecimal strides, the requested decimal strides, STRIDE/OFFSET macros,
archive helper offsets, literal allocation/copy/clear sizes, and named byte-buffer
cursors. It deliberately also captures scalar, text and serialized-data cases.
Each retained exact file/text entry in `tools/abi/raw-offsets.json` has a reason,
a classification and an occurrence bound. New text or extra occurrences fail.
Unknown classifications and missing reasons fail. The inventory command does
not generate approvals. The gate is part of `tools/abi/check.sh` before the
existing function/callback/import checks; its three synthetic tests cover raw
forms, negative offsets, duplicate/rejected entries and real native/CoD2x
preprocessor selection.

| Actual engine view | Compiled sources | Before candidates | After candidates | After safe | After dead/unreachable | Unreviewed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Stock | 390 | 1359 | 1196 | 1178 | 18 | 0 |
| CoD2x | 403 | 1365 | 1202 | 1184 | 18 | 0 |

Before means `483a7d0` source processed with the same engine flags and unchanged
headers/generated TUs. The identical checker rules were used for both snapshots.
Each configuration replaces/removes 171 original candidate occurrences (156
unique file/text keys) and adds eight reviewed native expressions. That count
includes dormant paths and allocator/clear literals, not 171 distinct live
crashes. Some additional mapped expressions, including computed centity strides,
were found during code review rather than by the literal patterns.

The combined manifest contains 1058 exact records: 1041 safe and 17
dead/unreachable, covering 1206 union occurrences (1188 safe, 18 dormant).
Safe means the listed expression is valid: identical pointer-free prefixes,
fixed-width wire/disk/bytecode/pixel records, character buffers, typed native
strides/offsetof, verified native object layouts, or verified spare allocation
capacity. It does not mean every match addresses a struct.

Reproduce the current gate or the source-only historical count:

```sh
python3 tools/abi/test_raw_offsets.py
python3 tools/abi/raw_offsets.py build-macos/compile_commands.engine.json
python3 tools/abi/raw_offsets.py build-macos-codx/compile_commands.engine.json
python3 tools/abi/raw_offsets.py build-macos/compile_commands.engine.json \
  --inventory-only --base 483a7d0 --output output/ws38/baseline-stock.json
```

The checker is a conservative source inventory, not proof of arbitrary computed
pointer arithmetic. It scans main-TU source, not expanded SDK/project headers.
Changes to approved record definitions or activation of dormant code require
review even if the literal line remains identical.

## Fixed mappings

Every production change selects the replacement under `defined(COD2_X64)` and
retains the original text in the other branch. No shared header, CMake file,
legacy flag or generated typed-data snapshot changed. Decimal offsets below
are bytes unless a row says otherwise. Repeated sites using the same mapping
are grouped; all affected files are listed.

### UI, audio and script debug data

| Source / sites | Old i386 expression → field | Native replacement / measured layout |
| --- | --- | --- |
| `ui_mp/ui_main_mp.c`, server filter | `serverFilters + index*8 + 4` → `serverFilters[index].basedir` | Typed field; record 8→16, field 4→8. Only the native branch survives; the duplicate Emscripten site was also guarded without changing its legacy view. |
| Same, dormant find-player slots | `sharedUiInfo + 113140/113204/113276 + index*140` → pending server `adrstr/name/valid` | Native bases 120944/121008/121080, unchanged pointer-free slot stride140; typed array access. The unused old `slotBase` calculation is omitted natively. |
| Same, find-player results | `uiInfo + 0x860/0xc60 + index*64` → found-player server addresses/names | Declared i386 bases are2208/3232 (the old literals2144/3168 are also off by64); native bases3052/4076. Typed character arrays for copies and all progress/no-results messages. Nothing currently arms `nextFindPlayerRefresh`; this remains dormant but is now mapped. |
| `ui_mp/ui_shared_mp.c`, bind handler | Bind record stride20, command0, bind1+12, bind2+16 | Native record24, command0, bind1+16, bind2+20. Typed reads/writes; typed endpoint and sizeof iteration for all56 bindings. |
| Same, multi-choice numeric/string lookup and selection | `mDef+256+i*4` → dvarValue; `mDef+128+i*4` → dvarStr | Native dvarValue512, dvarStr256, pointer stride8; typed arrays. |
| Same, enum handler | `dvar+20` → `domain.enumeration.stringCount` | Native40; typed enum domain. |
| `ui/ui_shared_obj.c`, menu/item windows | Clear528 (`0x210`) bytes → embedded `window` | `sizeof(window)` =544 native; covers both menu and item window initialization. |
| `snd.c`, 13 expressions | Dvar current+8 (enabled/integer/value); modified+7 | Typed current at16, modified at11. Covers stream/2D/3D enable, pause, stat monitor, volume and slave fade time. |
| `script/scr_parser.c`, initial lookup allocation/clear | 65536 OpcodeLookup records at20 bytes (`0x140000`) | Allocate/clear count*sizeof record24; subsequent growth already used sizeof. SourceLookup's separate8-byte fixed-width table stays unchanged. |
| `script/scr_variable.c`, two dormant comparator counts | `ThreadDebugInfo+128` → posSize | Native256 after32 native code-position pointers. Typed counts; comparator has no native callers. Its unrelated inherited position-comparison algorithm is unchanged. |

### Cgame, scoreboard and HUD

| Source / sites | Old expression → field | Native replacement / measured layout |
| --- | --- | --- |
| `cg_players_mp.c`, DObj/reset/player/turret client lookup | CI_STRIDE1208; `cg.bgs.anim_user + i*1208 + 20` aliases clientinfo | Typed `cg->bgs.clientinfo[i]`, native base1255200 vs i386919828, stride1232. `anim_user` starts at1255164 vs919808; its gap is not a portable20. |
| Same, reset legs/torso and turret frame | `ciBase+0x394/+0x3c4`,48-byte lerp frames | Typed legs/torso with sizeof56. Relative CI legs896 on both; torso944→952. Typed legs pointer for turret animation. |
| Same, sprites | `cg+i*1208+0xe0914/+0xe0940` → infoValid/team | Typed clients for remote/local players; CI infoValid0/team44 remain scalar offsets but the containing array base/stride moved. |
| Same, corpse rendering | `cgs - 0x6bec + entity*1208` → corpseinfo[entity-64] | Typed corpse array; i3861.0 base49684 (matches the old negative alias for entity64), i3861.3 probe164756, native1.3 base172168, stride1232. |
| `cg_snapshot_mp.c`, player reset | CI base-minus20 aliases, old move/lean992/996 and angle alias1020-minus20 | Typed lerpMoveDir1008, lerpLean1012, playerAngles1016 (i3861000). Sources are typed nextState.angles2[1]/leanf and lerpAngles. |
| Same, client clear / corpse reset-copy | `cgs+entity*1208-0x6bf0+4`; tagSrc `corpseBase+0x204` | Typed corpse array and full sizeof1232 corpse copies; regular-client clears also use the native-sized record. Typed attachTagNames; actual char-only tag/model arrays still at512/128 on both ABIs. |
| Same, corpse animation preservation | Fixed CI tree1188 and legs.animationNumber912 | Typed tree at1208 and typed legs animation member; CI_STRIDE selects sizeof for native copy/clear. |
| `cg_event_mp.c`, obituary | Fixed cg client base plus1208 stride; name[20]/name[8] intermediate aliases and oldteam-minus20 | Typed target/local/attacker client records, name and oldteam; no shifted intermediate client pointer. CI name12/oldteam48 are unchanged but the array moved. |
| Same, pickup/ammo sounds | itemInfo stride36, pickup+28/ammo+32 | Typed itemInfo stride72, sound pointers56/64. |
| Same, weapon pickup | gitem stride44, giType+28/giTag+32 | Typed gitem stride72, fields52/56. |
| Same, turret event | Computed `ep + (ep*16+ep)*8`, then *4 = entity stride548 | Typed `cg_entities[ep]`, native centity stride568. |
| `cg_hudelem_mp.c`, player name | Snapshot+`0xe0920+i*1208` incorrectly treats snapshot as cg | `cg->bgs.clientinfo[i].name`, including native negative-index rejection. |
| `cg_main_mp.c`, local sound entity/origin | cg+36 pointer slot, then +216/+32 without dereference | `cg->nextSnap->ps.clientNum/origin`; nextSnap moves36→40. Snapshot fields216/32 themselves remain fixed-width. Mac1.3 reference confirms the dereference. |
| `cg_scoreboard_mp.c`, scroll step/client records | Truncated dvar-pointer load then+8; cg client array base`0xe0914` | Typed dvar current.integer and typed clientinfo array. |
| `cg_draw_mp.c`, stat monitor | 8-byte records, material cast from int slot+4 | Typed16-byte records with full pointer material+8. |

### Core, parsing, filesystem and FX

| Source / sites | Old expression → field | Native replacement / measured layout |
| --- | --- | --- |
| `qcommon/cm_load.c` | Brush allocation48; clip-map clear272 (`0x110`) | sizeof(cbrush_t)=56 and sizeof(cm)=456. |
| `qcommon/cm_trace.c`, two accesses | Thread-local value3 +20 (`0x14`) → box_model | Typed TraceThreadInfo.box_model at40. |
| `universal/com_files.c` | searchpath+4 → pack; pack+784 (`0x310`) → referenced | Typed traversal/field; native8/788. |
| `qcommon/files.c`, FS_GetModList merge | `(n0+n1+n2+1)*4` pointer array | count*sizeof(*pFiles), native pointer slots8. |
| `client_mp/cl_parse_mp.c`, three array helpers | Client-active bases864480/1356000/300512 → parseEntities/parseClients/snapshots | Typed arrays, native bases979560/1471080/415592. Default1.3 i386 declarations are979552/1471072/415584; COD2_PATCH_10 i386 declarations exactly match the old literals. The115072-byte feature-size difference plus native pointer alignment explains the shift. Element records240/92/9944 remain fixed-width. |
| `client_mp/cl_console_mp.c` | keys+`0x780` = key160*12 → down | Typed qkey_t[0xa0].down, native record stride16. |
| `qcommon/net_chan_mp.c` | incoming raw+12 and dropped raw+8 are swapped | Typed incomingSequence+8/dropped+12. Both ABIs have these actual offsets; this is an inherited reconstruction error fixed only natively. |
| `game_mp/g_scr_main_mp.c`, three ScriptIO helpers | level bases13832/13836/13840 (`0x3608/c/10`) → handles/buffers/marks | Typed arrays at14496/14504/14512. Old literal macros become legacy-only. |
| `EffectsCore/FxPrimitives.c`, Emitter archive | 588/600/612/624/628/632/644/660/664 → emitPos/initialVel/velocityDelta/emitLastTime/emitStep/spawnSize/spawnDensity/spawnVariance/_tail | Native offsetof888/900/912/924/928/932/944/968/972. Archive scalar/vector payload widths stay unchanged; model check compares the full native pointer instead of its low32 bits. |

### Renderer and D3D shim

| Source / sites | Old expression → field | Native replacement / measured layout |
| --- | --- | --- |
| `r_bsp.c`, release/shutdown/bounds/light updates | World+48 → vd.worldVb; +316/+328 → mins/maxs; +180 → sunLight | Typed fields at88,432/444,224. Both VB clear sites and both sun-light calls covered. |
| `r_bsp_load_obj.c`, occluder allocation/construction | Native objects allocated/indexed as36-byte occluders/16-byte edges | sizeof64/32; typed arrays and plane/vertex/edge pointers. Old relative-to-edges writes -12/-8/-4/0/+4/+8/+16/+20 map to planeCount/planes/edgeCount/edges/vertexCount/vertices/ignoreStackLevel/viewPlaneCount, native offsets0/8/16/24/32/40/48/52. Edge pointers old0/4/8/12 become typed pointer pairs. Disk occluder20, edge4, plane20 and vertex12 formats stay fixed. |
| `r_sky.c` | SunFlareDynamic stride48 | Typed increment, native64, preserving both query pointers across four states. |
| `r_sky_load_obj.c` | sunflare clear96 | sizeof native112. |
| `rb_shade.c`, DX7 pass | raw stateMap+8, grid+4, objective+7, fog+8 | Typed stateMap0/grid8/objective11/fog12 natively. The stateMap+8 interpretation was already wrong against the i386 declared pointer-at0 contract; the scalar flags i3864/7/8 were correct. |
| `rb_light.c` | Technique+14 → first DX7 ambient flag | Typed passArray.dx7[0].ambientLighting, native26. |
| `r_model.c`, debug boxes/axes | Scene entity cent+8; entity axis+20/origin+60 | Typed cent16, axis24, origin64; both functions covered. |
| `rb_tess.c`, DX7 and DX9 lines | Entity endpos+72 | Typed endpos76. |
| `r_utils.c`, material picker | trace scratch36; infoParm stride20, table entry22*20, flags+8/contents+12 | Typed trace40; infoParm24, typed scalar fields12/16 and sizeof iteration. Name loads remain correct offset0 relative to native-sized entries. |
| `r_image_load_obj.c` | Four-byte texture clear at image+4 | Full typed image.texture.basemap pointer clear. |
| `rb_shade.c`, texture prebind | Texture GL-name+84 (`0x54`) | Existing typed 2D/cube getters; native GL-name storage104. |
| `Mac/DirectX_9/CColorConverter.c` | Vtable address point+8 | Skip two native pointer entries:2*sizeof(void*)=16. |
| `CDirect3DDevice.c` / `CDirect3DVertexShader.c` | Active vertex program read shader+4 | Getter reads the local native baseState at16; its first GLuint is ABI-independent. Getter is available under COD2_X64 and fallback draw calls it. |
| `CDirect3DCubeTexture.c` / `CDirect3DVertexShader.c` | Six secondary-base destructor thunks subtract4 | Full native pointer slot / offsetof secondary vtable8. |
| `CDirect3D.c` | Adapter identifier clear1024 | sizeof declared native identifier1168; no current engine GetAdapterIdentifier caller. Shared reconstructed GUID/LARGE_INTEGER defects are documented below. |
| `D3DXShader.c` | Constant-table descriptor clear32 | sizeof declared descriptor16 natively, preventing overwrite of caller-sized storage. |

Native layouts were measured with clang using actual headers and Apple SDK
selection, with i386 measurements for the UI/cgame mappings and final dual-ABI
core probes (output/ws38/core-dual-layout.json). A separate COD2_PATCH_10 i386
probe confirms the old client-active bases and negative corpse alias against
the 1.0 layout; native engines use the 1.3 feature sizes. These feature differences
must be distinguished from pointer-width differences when mapping literals.
Compile-only offset probes either
emit constant assembly or the requested char-array diagnostic sizes; unrelated
i386 SDK errors do not change the reported offsets. STABS-backed datagen round
trip, game layout and fixes/reference checks also pass. Local machine-readable
probes/reviews are under ignored `output/ws38/`; no reference binary, decompile,
AST or generated licensed content was added to git.

## Retained dormant code and safe examples

Eighteen occurrences remain classified dormant, with exact reasons in the manifest:
legacy Carbon mouse callbacks (2); unused opaque CoreAudio constructor (1);
unused cgame stride/address macros and clear helper (7); unreachable stock-shim
device-loss recovery (5); unused render-target width/height macros (2); and an
unused samplerDef calculation in an empty loop (1).

The device-loss conclusion follows the actual call/assignment chain: native
Device TestCooperativeLevel and Device/SwapChain Present always return0; only
failed HRESULTs from those calls arm deviceLost. Recovery's old DxGlobals and
truncated dvar reads would be unsafe if device-loss support is implemented.
Both stock and CoD2x use this same native shim. The Carbon/CoreAudio classes
remain opaque and have no native callers/instances; their old object offsets
must not be treated as future layout contracts.

Reviewed safe examples include listBox endPos16/cursorPos36 and scrollInfo.item24
before any widened field; snapshot/player/entity/client-state records with no
native pointers; serialized script handles and bytecode (VariableValue 8,
VariableValueInternal 16, memory-tree nodes 8) which intentionally stay 32-bit;
BSP/RIFF/SOCKS/Mach-O32 payloads; and local D3D objects fitting their existing
padded allocations. SE_Init's 40-byte block safely overallocates its 8-byte active
native string package. No broad replacement of four-byte wire/scalar counts
with pointer size was made.

## Tests and observed failures

Build commands, run with `taskpolicy -b nice -n 19`:

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos --parallel 3
```

Saved the real Release engine databases as `compile_commands.engine.json`;
fixture copies replace NDEBUG with UNDEBUG as CONTRIBUTING requires. Final
build logs are `output/ws38/final-build-{stock,codx}.log`.

| CONTRIBUTING command family | Result |
| --- | --- |
| tools/tests; datagen unit tests; ABI audit/import/raw checker unit tests; shader setup; Wine trace parser | PASS |
| COD2X_SANITIZERS=1 run_full.sh; run_native.sh; online/sdk_identity.sh | PASS |
| online/run.py and fixes13/run.py on both build databases | PASS |
| perf/run.py (fixtures, not benchmark); LP64 game/run.py; script/run.py | PASS |
| LP64 renderer run.sh, shader raster, texture mips, volume upload | PASS |
| Platform HUD matrices, impact marks, FX events/primitives/cloud, dedicated FX | PASS; final FX-primitives rerun after Emitter change also PASS |
| tests/platform sanitized configure/build/CTest | PASS25/25 |
| tests/platform plain configure/build/CTest | PASS27/27, including crash/diagnostic tests |
| Datagen roundtrip; game/check_layouts.py; fixes13/reference.py | PASS against external private references |
| New tests/lp64/ui/run.py on stock and CoD2x | PASS ASan/UBSan; baseline483a7d0 fails the binding handler on misaligned20-byte native stride |
| New raw checker, stock and CoD2x | PASS0 unreviewed/unsafe; baseline source inventory is rejected for156 removed/replaced exact keys |
| Final full tools/abi/check.sh, stock and CoD2x | PASS: 621/638 TUs, 218 bindings each, zero findings; both retail import checks zero proven extra/missing dereferences |
| git diff --check | PASS |

The main sequential gate's first ABI command reached its 600-second harness cap.
Its earlier suites all passed. That duplicate runner was stopped using only
its owned PIDs, and both final ABI gates were run separately with the new checker;
those final results, rather than the truncated duplicate, are authoritative.

The completed final commands were:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json \
  output/ws38/final-abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json \
  output/ws38/final-abi-codx
```

Both exit 0. Logs are `output/ws38/final-abi-{stock,codx}.log`; the respective
output directories contain the raw-offset, function, callback and import JSON
results. The private Mac1.3 STABS reference was present, so the import stage ran
and was not skipped.

Additional legacy evidence:

- For all 38 changed C files: `git show 483a7d0:path` and current source each run
  through `unifdef -UCOD2_X64`, then actual `cmp`: 38 equal, no `-t` fallback
  needed in the final consolidated run. `output/ws38/legacy-cmp.json` records
  every filename and exit code.
- `python3 tests/fixes13/legacy.py --base 483a7d0`: 38 files, 10 legacy
  configurations, 0 mismatches.
- `python3 tools/abi/legacy.py --base 483a7d0`: 38 files, 5 option-off
  configurations, 0 mismatches.
- `python3 tests/cod2x/check_inactive_gates.py --base 483a7d0`: **fails**, 264
  body differences across 17 configurations. This feature-only comparator
  demands every changed native body equal the baseline when CoD2x is off,
  including arm64+COD2_X64=1. WS38 intentionally fixes stock native code too;
  it also checks COD2_X64=0 even though CMake OFF must omit the definition.
  No new CoD2x-only feature bodies were introduced. The tool was not changed
  or its failure relabeled as a pass; the valid option-undefined comparisons
  above are the legacy evidence.
- `bash tools/ci/compare-x86.sh 410342a HEAD output/ws38/x86-reference`:
  exit 2, `Requires x86_64 Linux with the README multilib dependencies.` No
  tool/package was installed. The orchestrator still needs that artifact gate
  on a compatible host (the repo separately documents upstream GCC14 failure).

Temporary actual-source ASan/UBSan probes fail before and pass after for
client 63/corpse 7 and full lerp/corpse copies, and four sun states/two occluders
with preserved 64-bit pointers. The local-sound snapshot/client 63/origin-pointer
probe was verified passing after; no baseline run was performed for that probe. Actual
Netchan_Process gap/duplicate/older-sequence assertions pass after and fail at
the baseline's first incomingSequence/dropped check. These isolated local
probes are evidence, not added tracked suites or a gameplay/performance claim.

## Menu sweep

`tests/rendering/menu_sweep.py` reuses stdin-console driving. It discovers menu
names from the running UI rather than committing extracted menu data. An
opt-in native diagnostic command (`COD2_UI_SWEEP=1`, `ui_sweep`) lists menus,
executes the production UI_RunMenuScript, selects a map and acknowledges existing
openmenu/closemenu operations. It is registered only in the opt-in environment;
its source is wholly COD2_X64-gated. The normal UI flow is unchanged otherwise.

The test checks `pgrep -fl cod2_macos` before launch, refuses competing games,
uses a new external fs_homepath, private net_port, fullscreen0,640x480,60fps,
and `timeout -k 5 180`. It opens each menu by name, lets onOpen commands/rendering
advance, captures screenshotJPEG, closes it, and checks liveness/fatal output.
Then it executes handled script names with safe isolated preconditions and
loads mp_toujane once through StartServer before repeating every in-game menu.

Final acceptance commands:

```sh
python3 tests/rendering/menu_sweep.py build-macos/cod2_macos \
  --output "$HOME/Library/Application Support/CoD2-native-ws38/menu-stock-final" \
  --port 29938
python3 tests/rendering/menu_sweep.py build-macos-codx/cod2_macos \
  --output "$HOME/Library/Application Support/CoD2-native-ws38/menu-codx-final" \
  --port 29939
```

Both report PASS: main59 + in-game73,46 distinct handled script commands,
132 nonempty JPEGs, completed=true while the engine is alive. All screenshots,
console logs and result manifests are inside those external output directories;
images are at `home/main/screenshots/`. Each test terminates only its own
process group after the live acceptance assertion, avoiding the known inherited
listen-server script-cleanup shutdown hang. It does not test orderly shutdown.

There are51 handled names.45 run in the script loop plus StartServer once.
Skipped: Quit (would end process), RunMod (changes mod/restarts), JoinServer
(needs selected external server), playMovie (no licensed ROQ in the test install),
startSingleplayer (launches a separate app). StartServer is skipped only in the
loop and covered with the real map load. Extra update subcommands and explicit
loadGameInfo/loadArenas preparation also run.

During test development the first stock run crashed in the newly introduced
UI_Sweep_f: Cmd_Argv lacked a visible prototype and its returned pointer was
truncated. Added its actual char* prototype; final binaries do not reproduce
that instrumentation error. The second stock run completed all59 front-end
captures but the resetDefaults acknowledgement timed out because that script
mutates console argument storage. Saving the script name before invocation
fixed the harness acknowledgement. The successful stock3/CoD2x1 preliminary
runs and both final runs found no further crash or Sys_Error. No SDL UI-probe
helper was needed or linked.

## Remaining limits and merge notes

- Live acceptance covers local menus and a local listen-server session, not
  internet gameplay, every asset/map, renderer visual parity or333fps. No
  benchmark was run and no performance claim is made. Known listen-server
  shutdown cleanup remains outside this workstream.
- Dormant recovery/Carbon/CoreAudio assumptions and unused raw macros must be
  revisited before activation. Find-player raw sites were fixed proactively,
  but this test does not enable the currently unarmed refresh path.
- Shared reconstruction defects remain: GUID.Data1 is unsigned long (8 bytes
  native), and LARGE_INTEGER is an unrelated pointer-bearing64-byte struct.
  The declared D3DADAPTER_IDENTIFIER9 is1168 rather than standard1104; no current
  engine GetAdapterIdentifier call exists. No shared-header correction was made.
- The checker has the finite scope described above. It is a gate against new
  candidate expressions, not a substitute for reviewing pointer truncation,
  type definitions or calculated indices.
- Actual Linux i386 binary equality remains unverified here. The feature-only
  CoD2x inactive-body comparator is not a passing branch-wide gate for these
  intentional stock-native changes; its failed invocation is retained above.

Merge this branch after `483a7d0`; it depends on that sharedUiInfo correction.
No CMake/shared headers were changed. Shared-file edits are small: two ABI shell
steps and CONTRIBUTING suite/command additions. UI changes extend the already
corrected ui_main file. Source changes preserve original else text and should
merge independently with sibling workstreams; no sibling file contents were read
or modified (the initial filename-only search is disclosed above).
The additional `WS38-core.md` records the delegated core audit.

Focused signed-off source commits: 37348e1,8c0bbe5,608a41a,ca7037a,68f0d80,
2ad3691,023327c,9dc5772,0f7b0a7,6d336b3,1839fdb. Checker/docs:
393f96d,3a4f319. Core report commits:f3bcb88,6ec39c5. No push occurred.
