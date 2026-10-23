---
title: Faithful CoD2 decompilation
date: 2026-10-23
status: active; supersedes the validated-reconstruction plan (2026-10-15)
---

# Faithful CoD2 decompilation

## Why this replaces R1–R6

The [validated-reconstruction plan](2026-10-15-validated-reconstruction.md) ran
from 2026-10-15 to 2026-10-23. It produced 60 R-phase commits, of which four
changed engine code (about 40 lines). The R3 phase closed unknown bytes in the
original binary, including bundled libjpeg, zlib, libwww and padding, patched
Ghidra's decompiler and built strict metadata contracts, without changing the
engine. The earlier WS44/WS47 fidelity audit found about fifteen confirmed
player-visible differences in two days.

On 2026-10-23 the user restated the goal: a proper decompilation so the native
port is as faithful as possible, after many renderer back-and-forths. Speed
matters. This plan keeps that goal and drops proof work that does not change
what a player sees.

## Definition of done (multiplayer)

Every game-owned function behaves as in Mac 1.3, plus CoD2x 1.4.6.8's
intentional changes in the CoD2x build:

- Shared game code (bgame, game, script, server, qcommon, universal, xanim,
  EffectsCore, botlib) has been reconciled function by function with the
  Linux 1.3 source below. Mac-only differences are checked against the Mac 1.3
  binary.
- Client-only code (renderer, the Direct3D 9-to-OpenGL wrapper, cgame, client,
  ui, sound, Mac platform) has been reconciled function by function with the
  Mac 1.3 decompilation.
- The server replay oracle shows no unexplained divergence from the original
  server over the replay corpus, and renderer captures match the reference
  within recorded tolerances.

Bundled libraries come from upstream releases. Platform services are
intentional replacements. Neither is re-derived from the original binary.

## Sources of truth

| Source | Role | Use |
| --- | --- | --- |
| Mac 1.3 i386 binary with STABS, private Ghidra export of all 7,481 MP bodies | Authority for all Mac behavior | Private; read |
| [cod2engine](https://github.com/nawaftahir/cod2engine) `archive/lnxded-1.3` | Claimed byte-identical source of the Linux 1.3 dedicated server, Mac-named | Read-only reference; no licence, so nothing is copied |
| [CoD2rev_Server](https://github.com/voron00/CoD2rev_Server) | Linux 1.0 server reconstruction (AGPL-3.0) | Read-only guide: `cm_trace`, data tables, `XAnim_GetTimeIndex*` |
| CoD2x 1.4.6.8 source | CoD2x build behavior | Read |

The port stays MIT. Moving to AGPL-3.0 to copy CoD2rev code is deferred
(user, 2026-10-23). Ported behavior is re-derived and checked against the Mac
binary, as `NOTICE.md` already requires.

CoD2rev differs from 1.3 in netcode (`msg_mp`, `sv_client_mp`, `net_chan_mp`),
enables libcod behavior changes by default, uses x87 float literals (Mac 1.3 is
SSE) and stores script values at pointer width. None of that is imported.

## Oracles

- **Server replay.** An ARM64 Linux VM in UTM runs Docker with `linux/386`
  emulation. It rebuilds cod2engine 1.3 and confirms the SHA-256 against the
  original binary, then builds an instrumented copy. The instrumented server
  and the native server replay the same recorded user commands on the same map
  and seed, and print per-frame state lines in the `src/unix/sysdiff_statehash.c`
  format, extended with player and script state. The first diverging frame
  names the area to fix. Emulated x87 matches hardware for basic arithmetic;
  transcendental results can differ in the last bit, so divergences are
  confirmed before being treated as engine bugs.
- **Client reference.** The Windows CD 1.3 executable with CoD2x 1.4.6.8 under
  Highball's Wine 10 r19 engine and D9VK 1.10.3, launched by
  `tools/wine/play-cod2x.sh --reference --renderer d9vk` (see
  [reference recovery](../macos-port/reports/2026-10-16-reference-recovery.md)).
  It is a translated renderer with known artifacts. UTM's Windows 11 graphics
  (UTM 5.0.8) is not used: it targets Direct3D 11/12, and Direct3D 9 runs only
  through a September 2026 fallback with no reported working game.
- **Mac renderer reference.** No Intel Mac is available. An emulated
  Mac OS X 10.6 guest with software OpenGL is an optional spike for still
  captures of the original Mac binary.

## Phases

| Phase | Work | Done when |
| --- | --- | --- |
| F0 — oracles | UTM VM, cod2engine rebuild check, instrumented build, user-command recorder and replay for both servers | A replay of the native server and the original server compares frame by frame, with the comparison commands documented |
| F1 — shared code | Reconcile shared modules against cod2engine 1.3, module by module; port `cm_trace` traversal with CoD2rev as a guide | Every function in a module is marked equal, fixed, or an intentional Mac/CoD2x difference; replay agrees |
| F2 — client code | Reconcile the renderer and Direct3D 9 wrapper first, then cgame, client, ui and sound, against the Mac decompilation | Every function is marked; capture comparisons recorded |
| F3 — single-player | Same loop on the Steam SP binary; Windows SP under Wine as reference | Separate plan |

Metal U2–U7 continue in parallel under the
[0.3 readiness plan](2026-10-13-03-metal-readiness-plan.md).

## Working rules

- One worktree and branch per unit; one commit per reconciled batch; one short
  report per module, not per function.
- Fix candidates come from source comparison and a fresh WS44 audit, not from
  byte accounting.
- No sealed inventories and no duplicate re-runs. The merge gates, the replay
  oracle and capture comparisons are the checks. Agents do not edit an oracle
  inside a unit that is measured by it.
- `COD2_X64=OFF` sources stay byte-identical, as `CONTRIBUTING.md` requires.
- Original binaries, decompiler output and cod2engine source stay outside Git.

The R1–R6 private evidence stays frozen under the analysis workspace until the
user decides whether to delete it.
