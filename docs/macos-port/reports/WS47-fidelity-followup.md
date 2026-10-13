# WS47: dvar, command and profile fidelity follow-up

> Current-release integration scope (2026-10-07): the full WS47 unit below was
> recorded on `codex/fidelity-followup` (`0dab850`). Its CoD2x profile storage
> mapping change is parked with Config Studio until migration can ship. This
> integration retains the released shared `main/config_mp.cfg` mapping, all
> other corrections, and writable-home profile creation/deletion. The current
> profile fixture explicitly checks that retained mapping. The original report
> below describes the broader historical unit, not the trimmed integration.
> Parent verification of this trimmed integration: all six ASan/UBSan fixtures
> pass in both stock and CoD2x modes using actual Release compile commands;
> eight changed production files pass all ten inactive legacy configurations.

Date: 2026-10-07. Isolated branch: `codex/fidelity-followup`. Baseline:
`0e46885152b77b5bd380c1b356400fc7d3878f17`. This wave implements verified
registration/parser/profile corrections and supplies behavioral fixtures. It
does not establish whole-game fidelity or authorize integration/release. The
changes remain separate from the 0.2.1 gate/playtest baseline; no git staging,
commits, pushes or integration occurred here.

The governing inputs were CONTRIBUTING, the complete macOS PLAN, the WS44 audit,
and machine-local research `dvars/port-diffs.md` plus
`grammar/{grammar,roundtrip}.md` under
`/Users/stix/Projects/cod2-native-wt/_agents/research/`. Research claims were
checked against active source, actual Mac binary branches/table bytes and
private CoD2x 1.4.6.8 source. The read-only Mac authority is
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Addresses below are original i386 virtual addresses. No licensed source was
copied into this implementation, and no reference binary was executed or
modified. No disassembly/decompiler dumps or game data were added.

## Resolved contracts

| Contract | Independent evidence | Native correction |
| --- | --- | --- |
| LOD cheat protection | Mac renderer registration sites `0xc923b` (`r_lodScale`) and `0xc9264` (`r_lodBias`) pass `0x2080`. CoD2x `src/mss32/hook.cpp:149–153` changes only `r_lodScale` to ARCHIVE, default 1, range 0–1. | Stock `r_lodScale` and both modes' `r_lodBias` use CHEAT rather than ARCHIVE. CoD2x's documented `r_lodScale` override remains. |
| Seven light debug dvars | Mac sites `0xc96a3`, `0xc96ce`, `0xc96f9`, `0xc9728`, `0xc9757`, `0xc9786`, `0xc97b9` pass `0x2280`; source-wide CoD2x review found no override. | Add CHEAT `0x80` to existing SAVED/RENDERER bits for Ambient, DiffuseFraction, SunLight, AmbientColor, SunColor, SunDiffuseColor and SunDirection. |
| Config-created typed dvars | Mac bool/int/float handlers `0x4fd98`, `0x4fe4c`, `0x4ff56` test high byte bit `0x40` at offset +5 (`0x4fe28`, `0x4ff2b`, `0x50066`), meaning EXTERNAL word bit `0x4000`. | Permit external string promotion; reject engine-owned string types. The old test checked ROM `0x40` instead. |
| Joined set value | Mac `Dvar_GetCombinedString`, `0x4f798`, passes capacity `0x1000` and stops before an argument makes argument-length total exceed `0xffd`. Existing native callers already allocate `0x1000`. | Raise concatenation capacity from 1023 to 4096; retain the original whole-argument cutoff and space insertion. |
| Network lower bound | Mac client registration call `0x14889f` is int 30, range 10–100, `0x1001`; CoD2x competitive disable restores minimum 10. | Native stock/CoD2x minimum 10; existing defaults and upper bounds unchanged. |
| Language index 14 | Mac initial registration `0x3170f` and restart `0x14b5a3` allow 14. Actual language table has Czech at 14; selection/name/index/refresh code (`0x315ca`, `0x3163c`, `0x3168c`, `0x31880`) uses fifteen languages. | Correct initial, refresh and video-restart domains; restore Czech and fifteen-entry lookup/selection/asset loops. A restart-only range fix would leave the earlier registration at 13. |
| Missing client dvars | Mac calls `0x148c09` and `0x148e9e` register `cl_profileTextHeight` int 16, 1–100, `0x1000`, and `motd` string empty, `0x1000`. | Register both in native client initialization. |
| Quick Chat command | Mac cgame table `0x36e740` contains `mp_QuickMessage` handler `0x1d2fcc` and `mr` with NULL handler. The handler's existing snapshot/pm-flag gate agrees with actual Mac branches. | Replace native `quickmessage` registration with `mp_QuickMessage`; add `mr`; preserve dispatch and popup conditions. |
| Escaped quotes/token overflow | Mac tokenizer `0x4e914` emits one quote for `\"`; boundary sites `0x4e98c`/`0x4e998` return argc 0 instead of wrapping. CoD2x common tokenizer retains those semantics. | Restore quoted quote byte and whole-command rejection beyond the exact last-byte 512-token boundary. |
| CoD2x parser parity | CoD2x `src/shared/common.cpp:28–174` retains signed-byte whitespace, overlapping `/*/` terminator, verbatim final limited token and colon-before-`//` exception. | Native CoD2x tokenizer follows these contracts, retaining the existing capacity-error return. |
| Native profile directory owner | Profile creation/deletion use `fs_basepath` in both original Mac and baseline port. Native config writers and the mandated read-only base/writable-home contract use `fs_homepath`. | Intentional native adaptation: create/delete profile directories under writable `fs_homepath`, using the current game directory. |
| CoD2x profile config path | CoD2x `src/shared/iwd.cpp:710–724` changes `game` to `main` for the active profile config and preserves `qpath`. It does not flatten to a bare file. | Preserve `players/<profile>/config_mp.cfg` through OS path building, and stop redirecting bare `config_mp.cfg` as a profile config. |

Renderer CHEAT enforcement was tested through real `Dvar_SetCommand`, including
color/vector values and vector contents, while cheats are disabled. Fresh LOD
and light registrations lack ARCHIVE. Existing config strings are also tested:
`cl_maxpackets 10` and `loc_language 14` retain valid values during registration;
preexisting cheat values reset to reference defaults. The engine's existing
merge rule keeps an earlier ARCHIVE bit on an already archived name; this wave
does not add a special archive-removal rule or migrate old config contents.

The wrong typed-command flag test has an ownership consequence beyond failed
conversion: it can add EXTERNAL to an engine-owned ROM string with a borrowed
name. The first baseline ASan characterization caught an invalid free at
shutdown. The final fixture asserts the wrong type/flags, then restores its
test dvar's flags solely for valid cleanup and complete baseline coverage.

## Final parser contract for Config Studio

The executable assertions are in `tests/fidelity/tokenizer.c` and `dvars.c`;
the runner exercises active production implementations in both modes. The
machine-local grammar research remains the detailed two-stage specification.
Its roundtrip section 7 row showing native `main/config_mp.cfg` describes the
old implementation and is superseded by the profile correction here.

Stage 2 operates on bytes. Outside quotes, bytes 0x80–0xff are whitespace, as
are non-NUL bytes at or below 0x20. Inside quotes, high bytes and backslashes
are data, except `\"`, which emits `"`. A UTF-8 BOM outside quotes is skipped
as three whitespace bytes; the engine is not a UTF-8 decoder. Empty quoted
tokens remain tokens; adjacent quoted and bare portions form separate tokens.
The block-comment scanner accepts the overlapping terminator in `/*/`.
An unterminated quote consumes to NUL. These behaviors are inherited from the
original tokenizer; this wave does not introduce a modern grammar.

In limited-token mode, the final token is the verbatim remaining input after
exactly one separator after the previous token has been consumed. Additional
spaces, comments and quote spelling remain in that token. Exactly 512 tokens
are accepted when token 512 ends at the last byte of input; a trailing
separator or any continued input causes argc 0 for the whole command. CoD2x
preserves `//` immediately after `:` anywhere in the input; stock recognizes
it as a comment. CoD2x's existing bounded-buffer `-1` result is preserved.

Joined dvar values use a 4096-byte output buffer and count argument bytes toward
the 4093 limit. Spaces are inserted separately, so the exact output may contain
4094 bytes at the tested boundary; an argument that crosses the limit is
dropped whole. `Cmd_Args` remains independently capped at 1023 bytes.

Stage 1 command-buffer semantics remain unchanged: LF/CR always split commands,
semicolon splitting depends on quote parity, and escaped quote bytes still
participate in that parity. Config Studio must preserve original bytes and
spans, including comments/endings/BOM/NUL tails, and account for those traps.
The new fixtures prove the corrections above; they do not implement or verify
the editor's complete lossless serialization contract.

## Profile behavior: resolved and retained

Resolved: a selected profile's config path remains
`players/<profile>/config_mp.cfg`. Stock builds resolve it under
`<fs_homepath>/<fs_gamedir>`; native CoD2x resolves it under
`<fs_homepath>/main`. New/delete profile operations use writable home ownership.
`Com_InitPlayerProfiles` still reads the first token of `players/active.txt`;
profile change still disconnects, resets dvars and executes the selected path.
The fixture observes those exact requests without touching real profiles.

Retained: directory listing and `players/active.txt` follow the current game
directory/search paths. Upstream redirects the active profile config only; it
does not redirect every profile-related operation. A mod-specific active file
therefore remains a distinct input from the CoD2x main profile config. Bare
`exec config_mp.cfg` after a filesystem/mod restart remains a bare lookup in
the requested game directory. No automatic migration reads or deletes the
previous flattened `main/config_mp.cfg`; migration needs a separate stored-data
decision. The existing policy helper recognizes any matching profile config
path, whereas upstream compares against the current profile's exact path;
non-active cross-profile direct exec behavior remains a policy approximation.

Actual Mac New/Delete (`0x111c08`/`0x111802`) load import slot `0x1accd31`,
whose stored address is `fs_basepath` at `0x10d63fc`. This home-path change is
therefore an intentional native ownership adaptation, not a restoration of
the Mac implementation. It is required by the native read-only base/writable
home split: using base creates/deletes a different tree from the writable
profile configs and can target the licensed install. That split must remain
coherent even when the traditional base and home paths are no longer equal.

The old `imp_fs_gamedir` cast was checked rather than assumed broken: Mac slot
`0x1accf01` points directly to character array `0x10d6420`, and the native
wrapper likewise stores the direct array address. That original indirection
is valid. The native path uses the equivalent direct `fs_gamedir` symbol.

## Network defaults: source verification, no policy change

CoD2x does set 40/125 in competitive mode. Source `src/mss32/competitive.cpp:54`
conditions the branch on `g_competitive`; lines 69–71 set the snaps minimum to
40 and force its current value to 40; lines 74–76 set the maxpackets minimum to
125 and force its current value to 125. Disable lines 109–111 restore minima
1 and 10, leaving current values intact. Separately,
`src/mss32/cgame.cpp:241–245` patches only the upper bounds, 30→40 and 100→125.
README lines 127–128 describe those maximum increases. Source-wide review did
not find an unconditional reset/default patch to 40/125.

Thus a screenshot showing current 40/125 is consistent with competitive mode
or a config, and alone does not distinguish registration defaults from current
values. The screenshot content was not available to this worker. Native CoD2x
registration defaults remain snaps 40/maxpackets 125 as requested; the verified
minimum correction is independent. Stock defaults remain 20/30. No change was
made to `qcommon/common.c`, competitive enforcement or network timing.

## ISO §: investigated, mapping retained

Actual Mac `TranslateKeyCodeToVK` (`0xccd4`) and the ANSI/French/German 128-byte
key tables map keycode `0x32` to VK `0xc0`; ISO keycode `0x0a` is unmapped.
Actual key-down (`0x10bb1e`) and key-up (`0x10b912`) handlers special-case VK
`0xc0` to engine `0x7e` (tilde) before their extended-character lookup. The
MacRoman section-sign byte `0xa4` is absent from that lookup. A literal SDL
section-sign symbol is `0xa7`; assigning that number directly would conflict
with engine F1 key number `0xa7`.

No engine mapping was changed. This preserves the reference-supported mapping,
and leaves a user experience gap for a literal ISO § binding or the extra ISO
physical key. Config Studio must distinguish physical key identity, displayed
glyph and engine key number; this report does not claim runtime keyboard
coverage or select a new mapping without evidence.

## Validation and limits

All focused builds and fixture compilation used `taskpolicy -b nice -n 19`;
build concurrency was at most three. Evidence is local in ignored
`output/ws47-stock` and `output/ws47-codx`. See
[the fixture README](../../../tests/fidelity/README.md) for reproduction and
the precise mocked boundaries.

- Both current stock and CoD2x runs pass all six fidelity fixtures with
  assertions and ASan/UBSan enabled. The same acceptance assertions fail
  against the baseline in every suite.
- Existing `tests/cod2x/test_policy.c` passes against the actual changed
  native CoD2x policy TU with assertions and ASan/UBSan. The existing tokenizer
  `infostring-setkey-overflow` regression passes in both modes. Bounded
  SanitizerCoverage campaigns (seed 47, max length 4096, twenty seconds each)
  completed 271,616 stock and 172,288 CoD2x inputs without a crash or sanitizer
  failure. These campaigns test memory safety, not grammar equivalence.
- All changed active engine translation units compile with their actual
  Release commands: eight stock and nine CoD2x, no errors.
- Scoped ABI audit: twelve stock/thirteen CoD2x TUs, zero errors, function
  mismatches or new unreviewed signatures. Renderer callback audit checks 218
  bindings with zero table/named-cast mismatches or unprototyped float calls.
  Raw-offset audit reports 54/52 selected sites and zero unsafe/unreviewed
  sites. Import audit reports zero proven missing/extra dereferences.
  Strict float/integer bit-conversion audit reports zero violations.
- Legacy source comparison: nine changed files across five `COD2_X64`-off
  configurations, zero differences; thirteen actual SDK compile commands with
  the native definition removed, zero preprocessing differences/errors.
  `tests/fixes13/legacy.py` also passes nine files across ten legacy configs.
  The two CoD2x-only changed sources pass six inactive configurations,
  including native with CoD2x disabled. Original i386 objects/binaries were
  not built locally; this is source preservation evidence, not the Linux
  multilib artifact gate required by CONTRIBUTING.
- `git diff --check` passes. No shared layouts, headers, generated snapshots,
  launcher, shared network or movement files were changed.

The final baseline reruns exit 1 as expected; current runs exit 0:

| Fixture | Stock baseline failed assertions | CoD2x baseline failed assertions | Current stock / CoD2x |
| --- | ---: | ---: | --- |
| tokenizer | 2 | 134 | 0 / 0 |
| dvars | 8 | 8 | 0 / 0 |
| registrations | 26 | 25 | 0 / 0 |
| language | 5 | 5 | 0 / 0 |
| profiles | 2 | 4 | 0 / 0 |
| commands | 5 | 5 | 0 / 0 |

No full linked client/server build, game launch, playtest, benchmark, actual
disk rewrite, localized UI screenshot or physical keyboard run was performed
in this wave. Runtime validation belongs to the release gate after approved
integration; these fixture and ABI results do not replace it.

## Changed paths and remaining scope

Production: `src/PC/cgame_mp/cg_consolecmds_mp.c`,
`client_mp/cl_main_mp.c`, `gfx_d3d/r_dvars.c`, `qcommon/cmd.c`,
`qcommon/cod2x_policy.c`, `qcommon/com_playerprofile.c`,
`qcommon/dvar_cmds.c`, `stringed/stringed_hooks.c`,
`universal/com_files.c` (all under `src/PC`). New fixtures:
`tests/fidelity/{run.py,check.h,dvar_services.h,tokenizer.c,dvars.c,registrations.c,language.c,profiles.c,commands.c,README.md}`.
This report is the only documentation outside that fixture directory.

FA05 overlap is resolved for maxpackets's lower bound and the full language-14
registration/table/selection contract. The twenty client owner-bit `0x1000`
differences listed by WS44 remain outside this bounded wave; this report does
not mark all FA05 closed. The informational supported-language printer still
lists fourteen entries; Czech asset selection itself uses fifteen. WEB_BUILD
language storage/loops remain unchanged and are inactive in native builds.
`tcmd`, `tell_target`, `ui_sweep`, missing `unskippablecinematic`, the shock-view
dvar and other research-only command/dvar differences were considered but not
implemented without their own source/runtime contracts.

No fixes were started for FA01 message masks, FA02 mantle weapon decisions,
FA04 fog/objective initialization, FA06 reverb, FA07 stubs, FA15 collision,
HUD/view or scripting. Parent-owned subsequent waves handle those independent
audits. Network default policy, old flattened-config migration, cross-mod
active-profile UI, whole-language UI layout and ISO binding UX remain explicit
decisions or runtime checks.
