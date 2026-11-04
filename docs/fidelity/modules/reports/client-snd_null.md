# client/snd_null module reconciliation

Updated 2026-11-04.

**2/33 reconciled: 1 equal, 1 fixed, 31 unchecked.** Mac 1.3 is the authority.
The Linux module is a dedicated-server null implementation, while its two named
Mac matches are full client engine functions in `src/PC/snd.c`. The legacy source
is unchanged.

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

## Unresolved identities

I read all 33 ledger rows and every body of the read-only Linux `snd_null.cpp`.
Its 22 `SND_NullStub*` bodies are empty; its nine `SND_NullQuery*` bodies return
zero. For example, `SND_NullStub22` has no statements and `SND_NullQuery9` returns
zero. The reconstruction explicitly says these names are synthetic, unreferenced
and unrecoverable from that binary. Native source and the Mac symbol table have
no definitions under those names, and the ledger supplies no Mac address.
Their 31 rows remain unchecked because there is no verified mapping to a Mac
engine function. Build exclusion and an audio replacement cannot establish
their missing identities or justify equality.

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
