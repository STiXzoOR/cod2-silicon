# win32/snd_driver module reconciliation

Updated 2026-11-05.

**55/55 reconciled: 45 equal, 7 fixed, 3 intentional.** Mac 1.3 is the
authority. All assigned rows and active engine bodies were read. No bundled
Miles implementation was reconstructed. Legacy source is unchanged.

## Corrections

`SND_StartAlias3DSample` retained the original playback rate when computing the
channel duration. Mac 0x579bf-0x57a3b divides by the rounded rate after pitch and
timescale are applied. Native now uses that rate. The existing timeline fixture
plays 10 ms of source PCM at pitch 2 after a 32 ms delay: its return value is 37,
while the old fixture expected 42. That existing assertion was corrected; it
failed before the production correction and passes afterwards.

The same Mac duration path multiplies sample count by 1000 with unsigned 32-bit
wrap before converting to float. Native used signed multiplication, which could
overflow for a long sample. Native now preserves the wrap and unsigned
conversion. The byte-offset calculation at 0x57a8e-0x57a9d also converts an
unsigned 32-bit payload length. `SND_StartAlias2DSample` similarly uses unsigned
metadata rate at 0x574df-0x574ee rather than the reconstructed signed cast.

The 2D, 3D and stream save functions convert position and length as unsigned
32-bit values before division: Mac 0x56196-0x561b4, 0x56288-0x562a2 and
0x563b4-0x563d2. Native used a signed 3D length and widened unsigned-long casts
for the other two functions. The native branch now retains the Mac widths.

Shutdown at 0x55b32 and initialization failure at 0x5894d clear the live handle
tables inside `milesGlob`. Native had a separate 2D handle array outside that
reset. Its native-only name now aliases `milesGlob.handle_2D`, so the existing
typed reset clears the table used by allocation and channel wrappers.

## Engine comparison and native boundary

File callback seek modes map 0/1/2 to engine modes 2/0/1; invalid modes return
zero. The raw path retains its 32 buffers, 8192-byte minimum, wait/update loop,
single-buffer handoff and float-to-double bytes-per-ms calculation. Channel
pause, delay, free-status, save/restore, volume and reverb branches were compared
with their Mac bodies. The loader's PCM/ADPCM format decision tree, conversion
rate selection and ownership paths were read as well.

Mac `MSS_SpatializeStream` at 0x56a5a takes a stream slot and adds 32 channel
records through its scaled address bias. The active native implementation is
`MSS_SpatializeStreamImpl`, which takes the complete channel index. Its callers,
normalized distance, listener axis, attenuation and pan agree with that mapping.
The unused two-argument placeholder is not the implementation used by native
callers. Listener position transforms and left/right output arguments match.

The WS3 replacement in `src/platform/macos_audio.c` retains the AIL call surface
and uses SDK decoding, a software mixer and the default-output AudioUnit. Its
documented PCM metadata and widened sound-header contract are preserved:
`SND_GetSoundFileSize` uses `offsetof(MssSound,data)`, the loader validates native
conversion and rewrites output metadata, and the open callback uses a typed
file-handle temporary before writing its wider callback handle. These are the
three intentional rows. The native digital-driver API takes bits, and its
distance API takes min/max, while the Mac Miles calls take bytes and max/min.
Those adapter contracts are preserved. No shared header or backend changed;
Miles/EAX DSP and decoder parity remain outside this audit.

## Verification and limits

Configured arm64 Release and built `cod2_macos_ded` at background priority with
two jobs. Dedicated excludes this client driver, so its actual configured client
translation unit was also compiled separately. It compiles with seven inherited
warnings. Generated arm64 assembly shows 32-bit unsigned float conversions,
32-bit sample-count multiplication, the adjusted duration divisor, allocation
into `milesGlob`, and a complete 536-byte reset with no separate native 2D table.

Existing audio output fixtures pass with ASan/UBSan: 18 listener directions,
moving-emitter position, pitch/delay/stop/resume timing and 138 surface events.
The existing environment command fixture passes, and all ten existing reverb
reference cases agree. No new tests were added. Only the existing timeline
expectation changed.

The prescribed triage and constant checks ran, including an explicit comparison
of the active stream helper with its Mac identity. Five inactive preprocessing
configurations match the base. `unifdef -UCOD2_X64` output is byte-for-byte
identical, and the driver contains no `__LINE__` diagnostics.

The high-bit metadata/save inputs, wrapped long-sample duration, complete driver
shutdown/reinitialization and actual device initialization/failure are compiled
and reviewed but not exercised by these fixtures. No full CI, replays, game,
physical output, new tests or pushes were run. Nothing remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-platform-audio.noindex/`.
