# opencod2 → native macOS arm64, with CoD2x client compatibility

Shared brief for every agent working on this port. Read it fully before starting.

## Goal

A native Apple Silicon (arm64, macOS 27) build of the opencod2 multiplayer
client that can join CoD2x 1.4 servers at a stable 250 fps with true raw mouse
input. Changes should be clean enough to offer back to opencod2 upstream.

## Machine

Mac mini, Apple M6 (12 cores), 24 GB, macOS 27.0.1, Xcode 27 / Apple clang 21,
CMake, SDL3 with `sdl2-config` (sdl2-compat) from Homebrew. No Rosetta, no
Docker, no QEMU, no ninja. The machine cannot run 32-bit x86 code at all.

## Facts established on 2026-10-03

- **opencod2** (upstream `opencod2/opencod2`, base commit `410342a`) is a
  source-level reconstruction of the CoD2 MP 1.3 engine, rebuilt from the
  Aspyr/i5works Mac binary (it carries the Mac platform layer in `src/Mac` and
  the Mac D3D9-on-OpenGL renderer in `src/Mac/DirectX_9`). It builds and boots
  as a 32-bit (ILP32) program on Linux, Windows (MinGW/MSVC) and WebAssembly.
  It is work in progress and still crashes on its own platforms.
- **`COD2_X64`** (CMake option) is upstream's in-progress LP64 port. It
  configures and compiles but "does NOT link/run yet — the typed-data migration
  (Stage 2) is the prerequisite". The 15 Sep commits started that migration
  ("retype blobs", "x64 context/display safety", "differential statehash for
  x86/x64 cross-architecture parity verification").
- **Data blobs** still encode the 32-bit layout: `src/blobs/data.S` (3,068
  `.long <symbol>` pointer relocations), `src/blobs/data.c` (byte arrays that
  embed raw addresses, e.g. `___progname`), `literals.S`, `import_pointers.*`,
  plus generated `build/native_gen/data32.c` and `literals32.c`.
- **arm64 compile probe** (`clang -arch arm64 -fsyntax-only -DCOD2_X64=1`):
  2,678 errors, almost all from `src/headers/cod2_defs.h` redefining Darwin SDK
  types (`_opaque_pthread_*`, `__darwin_*`, `rlimit`, `rlim_t` — they came from
  the Mac binary's STABS) and from x86-only `regparm`. Guarding those dropped it
  to 413, led by 300 × "field has incomplete type". The real LP64 problems sit
  underneath and have not been inventoried yet.
- **OpenGL on the M6**: the legacy 2.1 profile ("2.1 Metal - 91.7") supports
  every extension the renderer uses — `ARB_vertex_program`,
  `ARB_fragment_program`, `APPLE_fence`, `APPLE_vertex_array_object`,
  `EXT_texture_compression_s3tc`, `EXT_blend_func_separate`,
  `EXT_framebuffer_object`, `ARB_multitexture`, `ARB_texture_env_combine` —
  except `APPLE_vertex_array_range` (one call, `glVertexArrayParameteriAPPLE`;
  stub it).
- **Float semantics**: the Mac i386 original used SSE scalar math (the Darwin
  i386 default), not x87. arm64 IEEE single/double matches that as long as FP
  contraction is off. Build with `-ffp-contract=off`; never assume x87 extended
  precision.
- **Inline x86 asm**: about 32 sites in the engine (`src/PC/script/scr_compiler.c`,
  `scr_vm.c` incl. `rdtsc`, `src/PC/universal/com_sndalias_load_obj.c`, label
  emitters in `sv_client_mp.c`, `sv_main_mp.c`, `g_main_mp.c`).

## Game data (local, never commit)

The user's licensed Steam copy (Windows depots 2631–2634, downloaded 2026-10-03)
is at `~/Games/CoD2`. Use `+set fs_basepath ~/Games/CoD2` (expanded); `main/`
holds `iw_00`–`iw_15.iwd` and `localized_english_iw00`–`iw11.iwd`. Treat it as
read-only: point `fs_homepath` somewhere else for configs and logs.

## Reference material (read-only) — `~/Projects/cod2-native-refs/`

- `CoD2x/` — the CoD2x patch source (v1.4.6.8). `src/shared` is server/client
  shared code and shows exactly what a CoD2x 1.4 server expects; `src/mss32` is
  the Windows client hook. Servers reject clients without a 32-char HWID2.
  CoD2x clients connect to 1.3 (protocol 118) and 1.4 (protocol 120) servers.
  `src/other/Call_of_Duty_2_Multiplayer_MAC_1.3.c` is a symbolized decompile.
- `macbin/cod2mp_mac_1.3_i386` — the Mac MP 1.3 binary, i386 slice, with full
  STABS debug info (≈397k stab entries, 465 source files, 6,511 functions,
  global variable types). Inspect with `nm -a`, `otool`, `llvm-objdump`.
- `CoD2rev_Server/` — callofduty2x fork of voron00's reversed dedicated server
  (shallow, May x64 work + CoD2x protocol; lacks voron00's September LP64 fixes).
  Prefer the full upstream clone in `research-clones/CoD2rev_Server-voron00`
  (AGPL-3.0, Linux 1.0/protocol 115, 64-bit by default since 30 Sep 2026; its
  LP64 approach: pointer-width script values, 32-bit bytecode offsets,
  `offsetof`/`ptrdiff_t` field tables). Its `-mfpmath=387` (added 3 Oct) is for
  x87 Linux parity and does not apply to us: the Mac original is SSE.
- `research-clones/cod2engine` — nawaftahir's byte-identical rebuild of the
  Linux dedicated server 1.0/1.2c/1.3 (server, game, bgame, script, qcommon,
  xanim, EffectsCore; no client/renderer), names from the Mac 1.3 STABS. Its
  1.3 target is the same `cod2_lnxded` CoD2x ships. **Use it as the reference
  for server/game/script behaviour**: compare, then settle disagreements against
  the Mac disassembly. No license and derived from AGPL code: read, don't paste.
  32-bit only.
- `research/` — in-depth reports on related projects
  (`engine-server-reconstructions.md`, `mac-client-reconstructions.md`,
  `tools-ecosystem.md`).
- `xtnded-cod2/` — an older reconstruction from the Mac STABS; useful for types.

## Known upstream opencod2 issues (verified against the Mac 1.3 disassembly)

From `research/engine-server-reconstructions.md`; cod2engine matches the Mac
binary in each case.

- `SV_SendClientSnapshot` never sends download data to active clients (Mac
  0x1964eb does).
- The overflow-recovery size check uses `+5` where the original uses strlen+6.
- The switch compiler has an extra `value != 0` test, so it accepts two
  `default:` labels that stock 1.3 rejects.
- Debug instrumentation in hot paths: `G_RunFrame` prints five `[ckpt]` lines
  and calls `getenv` twice per server frame; the script VM runs a debug check on
  every opcode; `BG_EvaluateTrajectory` hides bad trajectory types. Together they
  mask an unresolved corruption of entity 439. Gate this before any 250 fps or
  parity measurement.
- Seven places hand-declare `qsort`/`memcpy`/`memset`/`malloc` with 32-bit size
  parameters; use the system headers on LP64.
- Good news: all six network field tables and the Huffman table match 1.3.

## Rules for every agent

1. Work only inside your own git worktree and branch. Do not touch other
   worktrees. Commit in small, focused steps with clear messages. Never push,
   never open issues or PRs, never change git remotes.
2. Keep the original 32-bit build unchanged: with `COD2_X64=OFF` the result must
   be byte-for-byte what it was. Put 64-bit and macOS work behind `COD2_X64`,
   `__APPLE__`, `__aarch64__`/`__arm64__` guards, or make it genuinely
   architecture-neutral.
3. Match upstream's style (see `.clang-format`, comment density, naming). No
   drive-by reformatting.
4. Never add game data, Activision binaries or decompiler dumps to the repo.
5. Do not install system packages or change global config. If you need a tool
   that is missing, stop and say so in your report.
6. Verify claims with real commands. If something could not be verified, say so.
7. Finish by writing `docs/macos-port/reports/<workstream>.md`: what you did,
   what works (with the commands that show it), what is left, and any blockers.
   Commit it.

## Workstreams

| ID | Branch | Wave | Scope |
|----|--------|------|-------|
| WS1 | `port/macos-build` | 1 | Whole source set compiles for `arm64-apple-macos` with `COD2_X64=ON`; CMake macOS target; header hygiene; inline x86 asm gated with portable C; LP64 warning inventory by subsystem. |
| WS2 | `port/datagen` | 1 | Generator that turns the data blobs into typed, architecture-neutral C using the Mac binary's STABS, with an i386 round-trip test. |
| WS4 | `port/cod2x` | 1 | CoD2x client compatibility in source, behind a feature gate. |
| WS5 | `port/verify` | 1 | Verification harness: x86 reference CI, smoke and parity tests, Mac benchmark scripts. |
| WS3 | `port/platform` | 2 | macOS platform layer: SDL window and GL 2.1 legacy context, raw mouse, audio, UDP, paths. |
| WS6 | `port/lp64-*` | 2 | LP64 runtime fixes, fanned out by subsystem, driven by the WS1 inventory and WS2 output. |

The orchestrator merges finished branches into `port/main`, resolves conflicts
and runs the integrated build.

## Definition of done (whole project)

1. `cod2_macos` (arm64) launches from a CoD2 install on this Mac, reaches the
   menu and loads a map.
2. It connects to a stock 1.3 server and to a CoD2x 1.4 server and plays a full
   round without desync or crashes.
3. It holds `com_maxfps 250` with a stable frame time on the M6 (timedemo plus
   live play).
4. Raw mouse input bypasses macOS pointer acceleration.
5. With `COD2_X64=OFF` the 32-bit Linux and Windows builds still build unchanged.
