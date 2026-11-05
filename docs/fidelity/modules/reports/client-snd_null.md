# client/snd_null module reconciliation

Updated 2026-11-05.

**33/33 reconciled: 1 equal, 1 fixed, 31 intentional.** Mac 1.3 is the authority.
The Linux module is a dedicated-server null implementation, while its two named
Mac matches are full client engine functions in `src/PC/snd.c`. The legacy source
is unchanged. The 31 numbered Linux rows are intentionally omitted anonymous
dedicated stubs, not claims of recovered Mac identity or semantic equality.

## Correction and comparison

`SND_Init` at Mac 0x4b496 registers eleven dvars, initializes the environment,
returns on driver failure, then initializes master/channel volume, clocks,
slave interpolation, the 0.75 volume scale and eleven pause settings. It registers
both environment commands, prints its success message and initializes voice.
Those statements, constants and their order match native. Native's two console
banners differed: the opening omitted a newline and both used different text.
The native branch now uses the strings referenced at 0x4b49e and 0x4b7a0.

`SND_ShutdownChannels` at 0x4ce3c calls `SND_StopSounds(0)` before zeroing all 53
channel records. Native preserves that order and clears the complete typed
array. Mac's 0x1090 byte count covers 53 ILP32 records; native's `sizeof` includes
the wider alias pointers. The stop operation reaches sample-end and stream-close
wrappers before their engine channel metadata is cleared.

## Native boundary

The configured client compile database includes `snd.c`, `snd_driver.c` and
`src/platform/macos_audio.c`. It excludes the old MacMSS family and
`src/stubs/audio_stubs.c`. The WS3 replacement retains the engine's AIL call sites
and implements an SDK default-output AudioUnit, software mixer, sample handles
and stream decoding. I read the driver's initialization and stop paths and their
native adapter consumers: driver format bits, success/failure return, synthetic
provider enumeration, sample limits, allocation, end status and stream closure.
Bundled Miles and Miles DSP parity are outside this comparison.

The configured dedicated target includes none of those three audio sources.
`common.c` guards out initialization and channel shutdown; the client restart
callers are absent. Dedicated sound-alias metadata remains, but
`com_sndalias.c` also guards out sample loading and the `snd_errorOnMissing`
dereference. Thus it does not require Linux's null-init dvar registration to
service those consumers. No null wrapper was substituted for an active client
engine function. The existing voice playback adaptation is documented separately
in WS110; `SND_Init` continues to call `Voice_Init` in the Mac order.

## Dedicated stub provenance

The follow-up checked the origin of the rows, rather than inferring equality
from missing names. All 31 numbered names and the unrecoverable-name comment
already occur in cod2engine's first reconstructed Linux 1.0 source commit,
`d5ecaf8`, and were carried into the 1.3 archive by `81ddb00`. The current
1.3 file has the same SHA-256 as the private Linux inventory input. That
inventory's `libclang-definition-cursor` records classify these definitions as
`game_owned` from their directory. `tools/fidelity/module_inventory.py:79`
retains unmatched definitions as engine rows; it does not establish original
symbol provenance. AST extraction established source definitions, not that
their invented names or parameterless signatures are original sound APIs.

The bodies themselves are not invented missing code. The original stripped
Linux 1.3 binary, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`,
contains the same ordered block at 0x080d6bd0–0x080d6cdd: 22 anonymous empty
returns, nine anonymous zero returns, the dvar initializer at 0x080d6c7c and
the named empty shutdown at 0x080d6cac. Examples are the first empty body at
0x080d6bd0, first zero return at 0x080d6be2, last empty body at 0x080d6cce and
last zero return at 0x080d6cd4. A conservative raw rel32 call/jump and absolute
address scan finds no candidate reference to any numbered body. Shutdown has
the two calls at 0x080628a4/0x08062919; the initializer's only absolute address
occurrence is in `.eh_frame`. The binary has no sound dynamic exports.
This confirms the reconstruction comment's anonymous, unused boundary; it
does not recover original function names or make Linux behavioral authority.

Mac export/STABS metadata instead assigns `SND_Init` at 0x4b496 and
`SND_ShutdownChannels` at 0x4ce3c to `PC/snd.cpp`. Their prior fixed/equal
verdicts and evidence are preserved exactly. No mapping from a numbered
placeholder to an exported or inlined Mac operation is established or claimed.
The explicit native decision is to omit
this unused dedicated no-op block, whose only indexed identities are secondary
reconstruction names. The configured dedicated graph also removes live output
callers and client import retention roots, rather than needing substitute fake
helpers: `cmake/macos-arm64.cmake:65`, `tools/server/dedicated_data.py`,
`common.c` initialization/shutdown guards and `com_sndalias.c` sample-loading
and `snd_list` guards implement the documented WS3/WS26 boundary.

Real server sound work retains typed owners. `SV_SpawnServer` loads alias
metadata with system 2 in `src/PC/server_mp/sv_init_mp.c:1152`;
`G_SoundAliasIndex` in `src/PC/game_mp/g_utils_mp.c:290` uses the sound
configstring range; `G_PlaySoundAlias` at line 514 publishes entity/player
events. `src/PC/universal/com_sndalias.c` retains alias parsing and lookup.
These are distinct from device playback, and no anonymous null query services
them. Client playback remains owned by the real `snd.c` wrappers,
`snd_driver.c` and `macos_audio.c`; those functions retain their own fidelity
obligations. All 31 numbered rows are therefore intentional omissions of the
specific unused dedicated block. Their historical names remain unknown.

The provenance follow-up changes no engine source. Its own arm64 Release
dedicated build and symbol/dependency checks are recorded below. No new tests,
cases or assertions, full CI, replay, client build or runtime audio exercise
were added for this classification-only change.

## Verification and limits

Ran the prescribed triage and constant checks, then read both Mac functions
with symbol-start `otool` disassembly. After the correction, triage reports no
`SND_Init` call/string difference. Its shutdown-only `memset` flag comes from the
SDK's `__builtin___memset_chk` expansion. The compiled native wrapper calls
`SND_StopSounds(0)`, then tail-calls `bzero` for 0x13e0 bytes at `g_snd+0x348`,
covering the 53 native channel records.

Configured an arm64 Release build and built `cod2_macos_ded` at background
priority with two jobs. Since dedicated excludes the changed client file,
compiled `snd.c`, `snd_driver.c` and `macos_audio.c` separately with their actual
configured client commands. The dedicated binary has no `SND_`, `AIL_` or
`MacAudio` symbols. The five
`COD2_X64=OFF` guard configurations and exact SDK preprocessing both pass, with
zero mismatches. `snd.c` has no `__LINE__` diagnostics. No new tests, full CI,
replays, game, physical audio or runtime initialization were run; the banner
correction is compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-sound.noindex/`.

The 2026-11-05 provenance follow-up configured and built its own arm64 Release
`cod2_macos_ded` with `taskpolicy -b nice -n 19` and two jobs. The build passed.
Its configured source graph includes all four metadata/event owners above,
while `snd.c`, `snd_driver.c` and `macos_audio.c` occur only in client commands.
The linked dedicated binary retains `Com_LoadSoundAliases`, `Com_FindSoundAlias`,
`G_SoundAliasIndex` and `G_PlaySoundAlias`, has no `SND_`, `AIL_` or `MacAudio`
symbols, and links no AudioToolbox/CoreAudio framework. These checks establish
the selected native dependency boundary, not runtime audio or original-name
recovery. No production source changed or required a new client compile.
Private follow-up evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/recon-null-sound-provenance.noindex/`.

## Integrated batch verification

Integrated source a90c5b7 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. At that acceptance, the sound module retained 31 unresolved identities rather than claiming equality from missing symbols; the provenance follow-up above records their later intentional omission boundary.

Both 2,000-frame stock and CoD2x replays exactly match both fresh d873794 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
