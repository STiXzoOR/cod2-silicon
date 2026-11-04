# sound/snd module reconciliation

Updated 2026-11-05.

**46/46 reconciled: 39 equal, 6 fixed, 1 intentional.** Mac 1.3 is the
authority. All rows have named Mac engine bodies in `src/PC/snd.c`; none was
exempted because the native client uses a replacement audio backend. The earlier
sound-init banner correction and client null-symbol verdicts are unchanged.

## Corrections

`SND_StartAliasStream` used the wrong end-time subtraction when all candidate
streams were busy. Mac 0x4b31e–0x4b336 subtracts the best end time from the
candidate end time and replaces the best on a negative result. Native did the
reverse. The native branch now prefers the earlier-ending candidate, as the
2D/3D selectors already do. Its no-listener diagnostic at 0x4b2f0 is also complete.

`SND_Update` now refreshes channel reverbs only when a dry or wet fade rate
entered the update nonzero, matching Mac 0x4e143–0x4e474. The unconditional call
came from the initial reconstruction, commit `63de35a`, before native audio.
The 2D, 3D and stream start paths already set each handle's wet state; the native
adapter retains that state under its mutex and the mixer reads it without
resetting it. No documented replacement-provider initialization needs the extra
per-frame refresh. Fade completion still refreshes once after setting the rate
to zero.

The same update body now preserves the unordered SSE branches for volume-rate
signs at 0x4e02b/0x4e21f, timescale selection at 0x4e0dc and slave interpolation
clamping at 0x4e347–0x4e37e. `SND_StopMusic` now returns for an unordered goal
volume comparison, as the branch at 0x4ca56–0x4ca59 does.

The environment command bodies at 0x4cf46 and 0x4d196 now print the original
usage, argument diagnostics and invalid-priority list. `SND_StartBackground`
also restores the complete 3D-channel and non-streamed-track errors referenced
at 0x4c7c6 and 0x4c645. All changes are under native guards.

## Comparison and native boundary

I read every assigned function against the Mac disassembly, including the
static bodies missed by automatic extraction and the three overlay helpers
inlined in the Mac overlay function. Examples include the channel replacement
entity preference, all loop-reuse scans, ambient track alternation, pause order,
save sentinels and signed entity/byte/float serialization. The 32 room-table
slots at 0x3434c0 and all fourteen blend-validation diagnostics match.

`SND_SetChannelInfo` retains one specific native adaptation: a null origin
produces a zero entity offset. Mac dereferences that pointer for a spatial
channel on entity 0 through 1023. The existing guard preserves defined native
behavior. Valid-origin dot products, metadata and update consumers match Mac;
the output fixture exercises their moving-entity path. No new null access was
introduced to claim equality.

The configured client retains the engine wrappers and `snd_driver.c`, backed by
the documented WS3 `macos_audio.c` software mixer and default-output AudioUnit.
I inspected the provider enumeration and consumers: one synthetic positional
provider, its 256-sample capability clamped to 32 engine 3D channels, typed
handle allocation, channel-start state, room/master setters and stream closure.
Bundled Miles, discarded MacMSS objects and Miles/EAX acoustic parity remain
outside this reconciliation. The dedicated target excludes these audio sources.

## Verification and limits

Configured an arm64 Release build and built `cod2_macos_ded` at background
priority with two jobs. Since it excludes sound, also compiled the production
`snd.c`, `snd_driver.c` and `macos_audio.c` with their actual configured client
commands. The final `snd.c` compile exits zero without diagnostics.

Both stock and CoD2x existing environment fixtures pass; each reports ten reverb
reference cases with zero differences. Both existing output fixtures pass all
18 listener directions, the moving entity offset, sample delay/rate/cursor
duration, and 138 first/third-person surface events through the native render
callback, under their existing sanitizer settings. No tests were added or
changed. These fixtures do not cover the corrected busy-stream selection,
update reverb gate, unordered-float cases or diagnostic wording; those changes
are compiled and reviewed, not exercised.

The prescribed triage and constant passes completed. The six changed functions
were also checked with exact-symbol-start `otool` output. Five inactive guard
configurations and exact SDK preprocessing with the port disabled have zero
mismatches against the assigned base. `snd.c` has no `__LINE__` diagnostics.

No full CI, replay, game session, audio-device initialization or physical sound
output was run. There are no unchecked identities in this module. The prior
31 synthetic Linux null identities belong to `client/snd_null` and remain
unresolved in their established ledger; this work does not infer their mapping.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-sound-runtime.noindex/`.
