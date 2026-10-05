# WS38 — core native offset audit

Audited the compiler-selected stock native sources in `src/PC/qcommon`,
`src/PC/universal` (excluding `snd.c`), `src/PC/client_mp`, `src/PC/server_mp`,
`src/PC/stringed`, `src/PC/xanim`, `src/PC/EffectsCore`, `src/PC/game*`,
`src/PC/bgame`, and `src/Mac/Main`. No game data was read or launched.

## Changes

All changed expressions are selected with `defined(COD2_X64)`. Their original
source text remains in the other branch.

| Path | Wrong native operation | Replacement |
| --- | --- | --- |
| `qcommon/cm_load.c` | Allocates 48 bytes then copies a native 56-byte brush; clears only 272 of the 456-byte clip map | `sizeof(cbrush_t)` allocation and `sizeof(cm)` clear |
| `qcommon/cm_trace.c` | Reads temporary box-model pointer at byte 20, while native offset is 40 | Typed `TraceThreadInfo.box_model` |
| `universal/com_files.c` | Reads search-path pack pointer at 4 instead of 8; clears pack byte 784 instead of referenced byte 788 | Typed search-path traversal and `pack->referenced` |
| `qcommon/files.c` | Allocates four-byte slots for a merged array of native pointers | `sizeof(*pFiles)` |
| `client_mp/cl_parse_mp.c` | Uses stale client-active array bases 864480/1356000/300512 instead of native 979560/1471080/415592 | Typed parse-entity, parse-client and snapshot indexing |
| `client_mp/cl_console_mp.c` | Uses 12-byte key stride for modifier key 160, whose native stride is 16 | Typed `qkey_t[0xa0].down` |
| `qcommon/net_chan_mp.c` | Raw incoming-sequence and dropped-count fields are swapped | Typed fields; incoming offset 8 and dropped offset 12 |
| `game_mp/g_scr_main_mp.c` | ScriptIO bases use 13832/13836/13840, while native arrays start at 14496/14504/14512 | Typed level ScriptIO arrays |
| `EffectsCore/FxPrimitives.c` | Emitter archive helpers receive old offsets 588 through 664 for fields whose native offsets are 888 through 972; model check reads only low 32 pointer bits | `offsetof(Emitter, ...)` and native pointer null comparison |

The net-channel field swap also exists in the preserved nonnative code. Both
native and i386 header-layout checks confirm offsets 8/12. This work fixes the
native path and leaves the original build source unchanged as required.

## Retained sites

The untracked `output/ws38/core-review.json` contains exact source text, count,
classification and reason for 335 retained records / 364 occurrences from the
initial and expanded compiler-selected scans. Removed offset text is tracked
separately in `core-fixed.json` (15 occurrences); it is not retained approval.

Native layout checks confirm the following unchanged fixed-width records:
entity state 240 bytes, client state 92, client snapshot 9944, cached client
9992, archived entity 276, archived snapshot 8, cached snapshot 28, message line
164, FX curve keys at 8, backwards-compatible FX channel 64 (24 channels occupy
1536), effect visibility 20, animation notify 8, and test LOD 8. Character,
wire and asset byte cursors retain their byte counts. The local rune table
stub deliberately places 256 32-bit masks after thirteen 32-bit words.

`SE_Init` allocates and clears the same 40-byte buffer. The active string
package is a single native pointer (8 bytes), and the native parser stores
entries separately; this historical over-allocation is safe.

The two Carbon mouse callbacks remain unchanged. The engine class is an opaque
four-byte placeholder, and there are no native callers or reconstructed native
instances. The native SDL/raw-mouse bridge sends input events directly. Their
old object offset cannot be validated for a future reconstructed Carbon engine.

## Verification

- Arm64 syntax checks use each changed source's actual entry in
  `build-macos/compile_commands.json`, replacing object output with
  `-fsyntax-only`. All nine sources pass. Existing narrowing warnings remain in
  the game script source.
- For every changed source, `git show 483a7d0:<path>` and the current file are
  each processed by `unifdef -UCOD2_X64`, then compared with `cmp`: nine passes.
  Local outputs and per-source syntax logs are in `output/ws38/core-*`.
- The standalone native layout probe includes the production headers and uses
  the native database flags. Results are in `core-layout.txt`.
- i386 target syntax checks of production-header net-channel `offsetof`
  assertions pass; the preprocessed declaration probe is in
  `core-i386-layout.i` and its compiler log in `core-i386-layout.log`.
- `core-netchan-test.c` includes the actual production net-channel source and
  stubs the byte-message/environment interfaces. Sequences 10, 14, 15 produce
  incoming sequences 10, 14, 15 and gap counts 9, 3, 0; duplicate 10 and older
  13 are rejected. The current source passes. The same fixture including
  baseline `483a7d0` fails the first incoming-sequence/gap assertion. The fixture
  uses dead stripping and dynamic lookup for uncalled unrelated functions;
  it is not an integrated engine test.

The parent owns integrated builds, ABI gates and menu sweeps. No game runtime,
network server session or performance claim is made here. The source checker
is a conservative inventory; it cannot establish correctness for arbitrary
pointer arithmetic or callbacks without reconstructed object layouts.
