# WS6 — game and server LP64 correctness

Completed on 2026-10-03 in `lp64-game`, branch `port/lp64-game`, starting at
`a2f44778c8f76acedb6c74407f905a1681ffb732`. Native objects and isolated checks
pass. Neither executable links yet, so dedicated startup with `mp_toujane`,
real script execution, map loading, and a client connection remain unverified.

## Fixes

### Struct access and startup

- Server startup, resizing, spawn, and console commands read the actual
  `com_dedicated` dvar. The old `imp_com_dedicated + 8` expression reads beyond
  the pointer slot, rather than the dvar value.
- Server shutdown reads `com_sv_running->current.enabled`. The old expression
  dereferences the dvar's name and treats the string as a dvar payload.
- Game VM initialization clears the complete `client_t.gentity` pointer;
  timeout handling reads `bIsTestClient`; voice uses the real `sv_voice` dvar.
- Server spawn builds baselines through `svEntities[i].baseline`, copying the
  entity state and archive flags, masks, and bounds through their members.
- Snapshot traversal uses native `client_t` strides and whole `gentity`
  pointers. Profiling reads `svs.clients`, `svs.pOOBProf`, `netchan.pProf`, and
  `sv_maxclients`; `sv_ptr + 8` is a server timing field, not the client count.
- Game weapon model and shared ammo cap accesses use `WeaponDef` members.
  The hit-location parser indexes its pointer table by element, rather than
  using the four-byte float-table offset as a pointer-table byte offset.
- The existing `G_ENTITY`/entity stride helpers already use
  `sizeof(gentity_s)`. The inherited arm64 guard/alias fixes make those paths
  usable; they did not require another representation or stride constant.

### Pointer storage and call ABI

- Native game clients now have a local typed `gclient_t[64]` backing array.
  The inherited `g_clients` definition is still a 665,856-byte i386 blob;
  native clients require 667,136 bytes. Only `g_main_mp.c` consumes that raw
  global; other game code uses `level.clients`. Keeping the native storage in
  the owner avoids changing shared BSS generation. The unused old blob remains.
- Game initialization indexes `level.clients[i]`, and entity spawning registers
  `sizeof(gclient_t)` with the server instead of `0x28a4`.
- Native declarations preserve the pointer returns of `SV_AddTestClient`,
  `Scr_GetTypeName`, and `Com_FindSoundAlias`. `G_GetSavePersist` actually returns
  an integer, not an integer pointer. `nextmap` is a dvar pointer.
- `GScr_GetPartName` reads the typed model hierarchy directly. The out-of-scope
  `XModelBoneNames` implementation still returns a pointer as `int`; this
  removes the game-side round trip while that subsystem is migrated.
- The native server includes the real `getenv` declaration, eliminating its
  implicit integer return in the move diagnostic path.
- `g_banIPs` already has proper `const dvar_t *` storage in `src/blobs/bss.c`.
  The unresolved symbol came from that file's Darwin alias naming its target
  without the C underscore. A native Apple alias in `g_main_mp.c` resolves
  `g_banIPs` to `_g_banIPs`, without introducing duplicate data storage. `nm`
  confirms the indirect alias, and the final link has no unresolved `g_banIPs`.

### Sizes, field tables, and varargs

- Snapshot archive temporaries use native `msg_t` (32 bytes, formerly a
  24-byte buffer) and `archivedEntity_t` (276 bytes, formerly 240). Archive
  wraparound reads the message's data member rather than `msg + 4`.
- Cached-client ring limits and dvar reset flags use the verified retail value
  `0x1000`, not the relocated address of `__mh_execute_header`. The local retail
  symbol and the reference server's reset flag establish this value.
- Weapon animation string clearing covers all 128 native records using
  `sizeof`, instead of clearing only the i386 1,024 bytes.
- The weapon field table's included `.inc` still selected i386 offsets on
  arm64 despite its outer `.c` selecting typed records. Adding arm64 to that
  existing width guard selects all 366 `offsetof(WeaponDef, ...)` rows. Native
  string initialization now iterates those records directly. This is a map
  load hazard, not merely an initializer-warning cleanup.
- Network profile min/max arguments are integers for `%i`; the old calls
  passed doubles and therefore used the wrong arm64 varargs slots.

### Script and network representation

Game debug reads use `scrVmPub.top` and `function_frame` rather than i386
offsets 16 and 12. The inherited `SCR_ARENA_*`, code-position/vector encodings,
and entity handle scheme are unchanged. Tests cover all 1,024 entity handle
round trips, free-list reuse, and script-arena tag handles.

No protocol table, scalar width, or bit encoding was changed. The existing
player/entity/client/archive field tables in `qcommon/msg_mp.c` use
`__builtin_offsetof` on real structs. The layout check validates 254 such
entries against retail STABS offsets and member widths. It also validates
38 existing numeric HUD/objective entries: those records contain no pointers
and retain their retail offsets. They are outside this source ownership and
remain numeric. The isolated stock encoder test pins entity, removal, client,
and archive byte fixtures and exercises a changed-field round trip; the actual
netchannel XOR implementation passes a fixed byte fixture and decode check.
Full packet parity against a running i386 server still requires WS5 reference CI.

## Layout evidence and shared header change

`tests/lp64/game/check_layouts.py` reads the local retail binary through the
existing `tools/datagen` STABS reader. It derives native layout by retaining
field order/scalar widths, widening pointers, and applying natural alignment,
then compiles 335 field offset/width assertions using the CMake arm64 flags.
No reference dump or binary is committed.

| Type | Retail i386 bytes | Native bytes |
| --- | ---: | ---: |
| `gentity_t` | 560 | 568 |
| `gclient_t` | 10404 | 10424 |
| `client_t` | 725084 | 725144 |
| `serverStatic_t` | 110844 | 119096 |
| `server_t` | 390452 | 399680 |
| `entityState_t` | 240 | 240 |
| `playerState_t` | 9896 | 9896 |
| `clientSnapshot_t` | 9924 | 9924 |
| `cachedClient_t` | 9992 | 9992 |
| `archivedEntity_t` | 276 | 276 |
| `msg_t` | 24 | 32 |

Explicit existing layout exceptions: `gentity_t` parent/chain/tag/free-list
fields remain four-byte encoded handles; `VoicePacket_t` stays packed; the
engine's `netadr_t` retains IPX storage and is 20 bytes versus retail Mac's
12 bytes. The latter also explains the enlarged challenge array in the server
types; it is not pointer widening. The script accounts for these exceptions
and checks every named top-level field in the selected complete STABS records.
It also checks all 366 weapon rows against their unchanged i386 counterparts.

The **only shared header hunk** is three lines in
`src/headers/PC/server_mp/sv_types.h`: add the retail 1.3
`sv_lastTimeMasterServerCommunicated` field immediately before `pOOBProf`,
guarded by `COD2_X64 && COD2_IS_PATCH_13`. Its omission failed the STABS-derived
assertions. Native `pOOBProf` moves from 118952 to 118960; header layout remains
unchanged when the port is off. Rebuild generated native data after merging.

## Verification commands and results

```sh
cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY=$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386
cmake --build build-macos -j12 -- -k
python3 tests/lp64/game/replay.py
python3 tests/lp64/game/run.py
python3 tests/lp64/game/check_layouts.py
python3 tests/lp64/game/check_legacy.py
git diff --check
```

- Configure succeeds; real object emission succeeds. Build exits **2 at link**
  for both executables. Remaining references include old-ABI libstdc++ strings,
  tree/list helpers, C++ runtime, SDL/display/input, crash handling, and Windows
  diagnostic shims such as `GetModuleHandleA`/`GetProcAddress`. These depend on
  WS3/platform integration. Build log: ignored `build-macos/ws6-after-build.log`.
- Replay: **90 commands, zero failures** (45 owned source files, each compiled
  for client and dedicated using its actual CMake flags).
- All **seven AddressSanitizer tests pass**: dedicated/listen allocations;
  game script pointer APIs; snapshot indexing/copy/archive wrap; profiles/XOR;
  weapon field offsets/string initialization; spawning/handles; stock wire
  fixtures. They link actual engine source with isolated service stubs. They
  do not constitute a full map-load or gameplay test.
- Layout check: **335 native fields, 292 network entries, 366 weapon mappings**.
- Legacy check: **17 source files, ten inactive configurations, zero
  mismatches** against the starting commit, including the weapon `.inc`.
  Linux/Apple/MinGW/wasm/clang bodies are checked with both patch settings.
  CMake OFF omits `COD2_X64`; defining it to zero is not the upstream OFF
  convention. Includes are excluded from this source-body comparison, so it
  establishes guard discipline, not byte-identical i386 object output. Actual
  i386 binary parity remains a reference CI check; this host cannot run i386.
- Tests exposed the original failures before their fixes. Baseline replays
  remain available through `run.py --baseline a2f4477 --test NAME`; for example
  `--test spawn` fails its client-stride assertion with the original source.
  Baseline replay uses current headers and service stubs, not an old executable.
- `git diff --check` passes. No system package was installed; no tool was missing.

## Warnings before and after

Unique client diagnostics from the owned directories, with dedicated commands
also replayed. Baseline is `a2f4477`; counts include included `.inc` diagnostics.
The nested weapon-table warnings were absent from WS1's directory totals but
appear after the inherited arm64 outer guard fix. These are warnings, not a
count of defects.

| Directory | Before | After |
| --- | ---: | ---: |
| `src/PC/server_mp` | 35 | 10 |
| `src/PC/game_mp` | 66 | 63 |
| `src/PC/game` | 5 | 5 |
| `src/PC/bgame` | 746 | 14 |
| `src/PC/botlib` | 9 | 9 |
| **Total** | **861** | **101** |

732 integer-conversion, six integer-to-pointer, one integer-to-void-pointer,
20 pointer-to-integer, and one implicit-function diagnostics were removed by
the fixes. No warning suppression flags were added. Remaining categories:
79 narrowing, nine ignored attributes, eight missing returns, two implicit
`Com_Printf` declarations, one array-bounds, one pointer-to-char, and one
pointer-to-int return. The local before/after logs remain ignored; reproduce
current counts with `replay.py`, or count a retained replay with
`replay.py --logs build-macos/ws6-before`.

## Remaining risks and first-launch watch points

- The dedicated binary cannot run until the link blockers are resolved.
  Watch `G_SetupWeaponDef`, `BG_LoadAnim`, `Scr_LoadLevel`/`Scr_LoadGameType`,
  baseline construction, then the first `SV_ArchiveSnapshot`. Their local
  storage/layout hazards are fixed, but their asset, collision, script,
  allocator, and platform dependencies were not exercised together.
- Model loading/animation still depends on the xanim migration, including
  `XModelBoneNames`'s integer return outside this workstream. Game avoids that
  call; other subsystems may still truncate it.
- Botlib's native allocation header was already correctly padded to 16 bytes.
  Its `unsigned long` allocation lengths still narrow to the engine allocator's
  integer size. Normal token/struct allocations are small; huge or malformed
  inputs need allocator range validation before a broad large-file test.
- Several narrowing diagnostics are bounded client/entity indexes or short
  strings. Ban-file pointer differences are bounded by the integer-sized file
  service. They were retained without warning-silencing casts. Unbounded file
  sizes and allocation arithmetic remain a wider engine contract to validate.
- `G_RunFrame` returns `(int)imp_bgs`; all source callers discard that result.
  It is not a pointer round trip, but the historical signature remains a
  diagnostic. Eight other legacy non-void functions have missing returns;
  validate their caller contracts before depending on their values.
- `game/g_weapon_load_obj.c` has `header[14] = 0` on a 14-byte array in the
  unused static `G_ParseWeaponAccuracyGraph` helper. The called plural parser
  uses an 8192-byte buffer. This existing overrun is outside the active startup
  path; repair it before reviving that helper. The remaining pointer-to-char
  warning in `BG_LoadPlayerAnimTypes` converts a literal zero and produces the
  correct terminator; it does not truncate a live pointer.
- Network wire layouts checked here are pointer-free and unchanged. The
  `netadr_t` retail/engine ABI difference still warrants a real UDP handshake
  and cross-architecture packet capture during integrated verification.

## Merge notes

Merge the focused commits in order: `b5d93e5`, `3a931f9`, `d5ddc37`, `1a6785a`,
`8b78b79`, `2d5416e`, `3fa275c`, followed by this report commit. Source changes
are confined to the owned game/server/bgame directories plus the single
`sv_types.h` hunk described above. Botlib was reviewed but not changed.
No CMake, shared `cod2_defs.h`, protocol table, generator, remote, or game data
change is included.

Coordinate the `g_banIPs` alias and temporary local native client backing with
any later BSS/generator cleanup. Replacing the raw global with authoritative
typed storage can remove that local workaround once all aliases agree.
Rerun the checks after the other subsystem branches are integrated, regenerate
native data, then launch with the licensed data read-only and a separate
`fs_homepath`, as required by `PLAN.md`.
