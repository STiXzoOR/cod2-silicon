# WS9 — 1.3 correctness and 333 fps blockers

Date: 2026-10-03. Worktree: `fixes-13`. Branch: `port/fixes-13`.
Starting point: `4802e924f22b15beacc5139a1ac8e239940f7272`.

The native fixes and focused arm64 tests are complete. The client compiles in
Release, but its link still fails on the typed-data `_dlText+0xC` alignment issue.
No engine startup, live server connection, download, timedemo, sustained 333 fps,
or full CoD2x gameplay parity is claimed.

## Reference and scope

Read all of `docs/macos-port/PLAN.md` and these reference reports:

- `~/Projects/cod2-native-refs/research/engine-server-reconstructions.md`
- `~/Projects/cod2-native-refs/research/mac-client-reconstructions.md`
- `~/Projects/cod2-native-refs/research/tools-ecosystem.md`

Checked the actual local 2006 Mac 1.3 i386 STABS binary, the licensed Steam Mac
2013 binary, and Windows 1.3 `CoD2x/bin/windows/CoD2MP_s.exe`. Read cod2engine's
1.2c-to-1.3 notes and its server implementation to identify comparisons; decisions
below use retail types/instructions rather than assuming the reconstruction is
correct. No reference binaries, game data, or decompiler dumps were added.
No packages were installed. No sibling worktree was accessed, remote changed,
branch pushed, PR opened, or issue created.

All production behavior changes are under `COD2_X64`, or in the native-only CMake
file. Existing CoD2x policy remains under `COD2_CODX`; its competitive FPS limit
is respected. The legacy branches and declarations remain intact.

## 1.3 receive and storage

The receive buffer and `MSG_Init` capacity now both use `MAX_MSGLEN` on LP64:
128 KiB in 1.3 mode. Mac `Sys_GetEvent` starts at `0xC3C6E`; instructions at
`0xC3D82` and `0xC3D8A` pass `0x20000` and `sys_packetReceived` (`0x1229F20`),
then call `MSG_Init` at `0xC3D98`. The receive test writes the complete 128 KiB
buffer with ASan and verifies the queued packet's last payload byte.

The audit distinguished debug-type sizes from linker symbol spans, which include
padding. Copying the span blindly would invent extra array elements.

| Object | Mac 1.3 address | Retail type / storage | Native result |
|---|---:|---:|---|
| `sys_packetReceived` | `0x1229F20` | 131,072 bytes; span 131,168 | 128 KiB buffer and receive capacity |
| `g_largeLocalBuf` | `0x377F00` | 1,048,576 bytes | Actual static arena in `com_memory.c` and blob mirror are 1 MiB |
| `svs` | `0x185D280` | 110,844 bytes; span 110,848 | Existing typed WS6 storage is 119,096 bytes on LP64; no additional resize |
| `cls` | `0x13158E0` | 2,837,904 bytes; span 2,837,920 | Add missing 1.3 `serverInfo_t.punkbuster`; typed `cls` becomes 3,000,008 bytes |
| `ucmds` | `0x34B880` | 13 entries / 104 bytes; span 128 | Restore `wwwdl` at index 9 and terminator at index 12 |
| `serverStatusDvars` | `0x34AEE0` | 24 entries / 288 bytes | Restore `sv_punkbuster` row at index 22 and terminator at 23 |

Native `serverInfo_t` is 148 bytes; retail Mac is 140. This difference includes the
port's deliberate 20-byte `netadr_t`, versus retail Mac's IPv4-only 12 bytes.
Native `requestCount` and `hostName` offsets are 34 and 42. `CL_SetServerInfo`
also populates the restored PunkBuster byte. The 20,000 global server records
explain most of `cls` growth. Both 33-byte PunkBuster GUID fields in `challenge_t`
and `sv_lastTimeMasterServerCommunicated` already match 1.3 in the starting branch.
Native challenge size is 116, versus retail 108, again including `netadr_t`.

`reference.py` verifies these retail sizes and audits the active native byte-array
declarations against unambiguous STABS types: **262 checked, no undersized arrays**;
34 unavailable/ambiguous types are explicitly excluded. This is a bounded audit,
not proof that every opaque BSS fallback is correct. The native storage test checks
the active struct sizes/offsets and fills eight simultaneous 128 KiB LargeLocal
allocations through the final byte of the real 1 MiB arena, then releases them.

## Snapshot downloads, overflow recovery, and duplicate defaults

`SV_SendClientSnapshot` now calls `SV_WriteDownloadToClient` once after the snapshot
for each non-zombie state, including active clients. Its old inner snapshot-helper
download call is suppressed on LP64 to avoid sending twice. Mac state comparison
is at `0x1964EE`; zombie branch at `0x1964F1`; download tail is `0x196017` reached
from `0x1964F9`. The test exercises states 0–4 with both open and absent download
handles, using both real snapshot functions and mocked message/transport services.

Overflow recovery now reserves `strlen(command) + 6` bytes before the EOF and uses
`reliableSequence` as the loop bound, as retail does. Mac `0x196A99` performs the
NUL-inclusive string scan, `0x196AAA` adds five to that count, and `0x196AAE`
compares against `0x1FFFF`; this is strlen+6 with room for the EOF. Retail client
offsets are `reliableSequence=0x2080C`, `reliableSent=0x20814`. The test checks the
exact fit, one-byte overflow boundary, and pending commands beyond reliableSent.

The compiler's extra nonzero-key condition is removed on LP64. Default labels have
key zero and now participate in duplicate detection. In Mac `EmitSwitchStatement`
(`0x99626`), `0x99AF1` loads the key, `0x99AF6` compares the next key, and
`0x99AF8` jumps directly to the duplicate error at `0x99B64`; there is no zero
exception. The native test uses the real validation block/comparator and checks
two defaults, two equal nonzero cases, and a valid default plus a distinct case.
It does not run the complete script parser.

## Diagnostics and entity 439

New `src/PC/qcommon/port_debug.h` defaults `COD2_PORT_DEBUG` to **0 on LP64**, **1 on
legacy builds**. `COD2_DEBUG_ONLY` removes instrumentation at preprocessing time;
`COD2_DEBUG_ENV` resolves to a constant null pointer when disabled. This gates the
G_RunFrame checkpoints, per-frame entity scans/watchpoint checks, per-opcode VM
probes/ring recording, and repeated debug environment checks in client, cgame,
renderer, game, server, xanim, and Direct3D diagnostic paths. Error-path reporting
remains available. Re-enable the original probes with `-DCOD2_PORT_DEBUG=1`.

Tests compile seven complete game, VM, client-screen, and renderer translation
units at `-O0` and verify absence of `getenv`, entity-439 probe, opcode-recording,
and renderer trace-helper symbols. Renderer code relocations must not reference
surface counters, volatile per-surface crash records, or disabled diagnostic calls.
The final audit also gated the frame-120 diagnostic, stretch-picture counters,
and v60 unmapped-import diagnostic. These counters/records retain their storage
for debug/crash tooling but perform no per-draw writes when disabled.
All changed diagnostic translation units also passed
native syntax checks with debugging explicitly enabled. The functional renderer
option `D3D_PROG` remains available, sampled once instead of once per indexed draw;
its focused test checks 1,000 calls with the option absent and present. Existing
`LMAP_SCALE` was already cached; initialization-only `COD2_NOXSURF` stays unchanged.

Default native `BG_EvaluateTrajectory` now reports `ERR_DROP` on an invalid type,
matching retail, instead of returning a plausible stationary result. Retail
function `0x6B262` checks the supported range at `0x6B276`, branches to the error
at `0x6B279`, passes error code 1 at `0x6B28C`, and reaches `Com_Error` at
`0x6B29A`. The test checks valid linear/gravity evaluation at 3 ms and invalid-type
failure. The legacy/debug fallback remains available for upstream investigation.

**The corrupting write to entity 439 is not identified or fixed.** Static inspection
checked entity allocation/free, script entity references, entity handles, entity
strides, and the active BSS. `g_entities[1024]`, `level`, and `level_bgs` are already
typed in the starting branch; entity allocation and clearing use native
`sizeof(gentity_t)` (568 versus retail 560), and WS6's layout test passes. No live
reproduction or writer trace is possible with the current link failure. The
runtime watchpoint/probes are retained behind the debug flag for that next step.
Restoring the retail error exposes corruption; it does not establish its cause.

## LP64 libc declarations and extraction fixes

System `<stdlib.h>` declarations replace 32-bit qsort/malloc declarations on LP64
in the seven reported locations and further instances found during the audit:
`scr_compiler.c`, `cl_main_pc_mp.c`, `r_scene.c`, `r_staticmodel_load_obj.c`,
`gfx_v60_threads.c`, `cl_console_mp.c`, `net_chan_mp.c`, `FxUtil.c`, and the two
script debugger UI files. `cg_main_mp.c` already used system memcpy/memset headers
on arm64, so needed no edit. The test force-includes libc headers while compiling
all eleven relevant complete translation units; function-type assertions verify
64-bit `size_t` for qsort, malloc, memcpy, and memset.

Restored all 20 key names as single Latin-1 bytes, including their reused French
number-key strings. These are engine byte encodings, not UTF-8 strings. Retail
`keynames_localized` is at `0x34A680` and `frenchNumberKeysMap` at `0x34A640`;
Steam tables are `0x37E3A0` and `0x37E360`. The reference test independently reads
both binaries and confirms the same 20 bytes and four French keys. Native table
tests verify the production strings. The old garbled legacy strings remain intact.

Removed the invented `cg_shock_viewKickFadeTime` entry from the native 29-entry
shellshock-name table. Retail `cg_shock_dvar_names` is at `0x34D680`, size 116.
Index 4 is `cg_shock_sound`, and index 28 is `cg_shock_mouse_fadeTime`. The existing
29-iteration callers now reach the last name instead of skipping it. Legacy keeps
its original 30-entry table.

## 333 fps timing and performance readiness

The existing frame loop already implements Windows 1.3 timing. No replacement
fractional accumulator, new physics quantum, or frame-cap arithmetic was needed.
Windows `Com_Frame` at `0x434F70` loads 1000 at `0x434FD3`, performs signed integer
division at `0x434FD9`, falls back to one millisecond at `0x434FDF`, loops through
events at `0x434FF0`, compares elapsed time at `0x435010`, and calls Sleep(0) at
`0x435016`. Native `Com_Frame_Try_Block_Function` uses the same integer minimum
and `NET_Sleep(0)` while waiting. Tests also pass on the starting commit's timing
code, confirming that preserving it is deliberate.

| Requested FPS | Ideal repeated frame/command interval | `cl_maxpackets 125` interval |
|---:|---:|---:|
| 125 | 8 ms | 8 ms |
| 250 | 4 ms | 8 ms |
| 333 | 3 ms | 9 ms |
| 333 with CoD2x competitive limit | 4 ms (250 fps) | 8 ms |

Each sequence is checked for 1,000 frames through the real frame loop and client
command/packet scheduling functions. At 333 the nominal rate is 333.333 frames/s;
there is no intentional alternating 3/4 ms sequence. `cl_maxpackets` constrains
network sends, not command creation or rendering. Its integer 8 ms minimum is
reached every third 3 ms frame, yielding about 111 packets/s. Stock's lower packet
dvar limit is unchanged. The test also checks a renderer hitch passes real elapsed
time through to server/client frames. Actual clock and rendering hitches can
produce longer frames; this simulation is not a performance measurement.

Native `Sys_Milliseconds` uses the existing monotonic mach clock converted to
integer milliseconds, with a uint32 base; the frame loop uses zero-timeout polling.
`Pmove` accepts 1–66 ms steps and computes `pml.frametime = msec * 0.001f`, so 3,
4, and 8 ms commands are not rounded to a different physics tick. No movement code
or floating-point contraction policy changed. Native presentation defaults to
swap interval zero; user-enabled vsync can still cap presentation to display Hz.
Complete cross-platform physics parity still requires live state/trajectory tests.

Found and removed native CMake's unconditional trailing `-O0`, which overrode
Release's `-O3`. Release now actually compiles optimized; Debug/unspecified builds
retain normal unoptimized defaults. `-ffp-contract=off` and `-fno-strict-aliasing`
remain. A compile-command test verifies both native targets retain Release
optimization. This native-only change is one small hunk in `cmake/macos-arm64.cmake`.

## Verification commands and results

Run from the worktree; the local binaries remain outside the repository:

```sh
cmake -S . -B build/ws9 -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release
python3 tests/fixes13/run.py
python3 tests/fixes13/build_flags.py
python3 tests/fixes13/run.py --test debug_enabled
python3 tests/fixes13/reference.py
python3 tests/fixes13/legacy.py
python3 tests/lp64/game/check_layouts.py --build build/ws9
bash tests/cod2x/run.sh
cmake --build build/ws9 --target cod2_macos -j8
```

- Configure succeeds. Ten focused test groups pass natively; executable tests use ASan.
- Release flag check and retail/Steam reference audit pass.
- Legacy token comparison: **51 changed source files × 10 configurations, zero mismatches**.
  This covers patch 1.0/1.3 under legacy Linux, Apple, MinGW, Clang, and wasm configurations.
- WS6 layout check: 335 native fields, 292 retail network offsets/widths, 366 weapon mappings pass.
- Existing CoD2x protocol, identity, runtime policy, stance/posture, and state-reset suites pass.
- Native client compiles, then link exits 2 on `_dlText+0xC` alignment. Full all-target
  build also exposes pre-existing dedicated unresolved client/renderer imports in
  typed import/vtable data; those were present before the Release optimization edit.
- Baseline `--baseline 4802e924 --test snapshot` and `--test switch` reproduce their
  assertion failures; baseline `--test timing` passes with the same Windows sequence.
- Debug opt-in syntax checks pass for 34 native diagnostic translation units.

The original 32-bit build was not produced here: the project-specific i386 build
and byte-comparison toolchain is not available in this native worktree. There is
no local `toolchain/` directory; `command -v i686-linux-gnu-gcc` and
`command -v i686-w64-mingw32-gcc` find neither compiler. Token
identity is evidence for guard correctness, not a claim of executable byte identity.
No tool or package was installed to bypass that constraint.

## Merge and remaining work

Implementation commits, in order:

1. `a644187` — receive buffer, snapshot downloads, overflow recovery.
2. `0ecb3a9` — duplicate defaults and LP64 libc declarations.
3. `5dac564` — remaining 1.3 storage, command/status/key/shellshock tables.
4. `afed0d3` — default-off port diagnostics and retail trajectory error.
5. `8bd2f3d` — Release optimization, cached renderer option, frame/packet timing tests.
6. `96d7b31` — renderer counter/trace elimination and stronger hot-path verification.

Merge the branch in order. The only edit under `src/headers/` is the guarded
PunkBuster byte in active `cod2_defs.h`; `common_types.h` and `CMakeLists.txt` are
untouched. Native CMake changes only the optimization option line. `src/blobs/bss.c`
changes only receive/arena storage under the native guard. The debug change touches
many source files mechanically and adds one header, so preserve that header and its
includes if resolving conflicts. Regenerate typed data after merging to pick up
the BSS/type changes; do not merge generated build artifacts.

After WS2 resolves the client link, test large gamestates, active-client downloads,
full script compilation, browser/server status, stock protocol 118 and CoD2x
protocol 120 connections, and real optimized 333 fps gameplay. Respect server FPS
restrictions. Reproduce entity-439 corruption with debug probes/watchpoint enabled
and identify the first writer before calling it fixed. Run the actual i386 binary
comparison in the orchestrator's reference toolchain. Thirty-four ambiguous BSS
types and full client/renderer LP64 correctness remain outside this bounded audit.
