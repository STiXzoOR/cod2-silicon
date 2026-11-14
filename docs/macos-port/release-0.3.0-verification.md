# CoD2 Silicon 0.3.0 verification

Recorded on 14 November 2026, on a Mac mini with Apple silicon, macOS 27 and
the macOS 27 SDK. GitHub Actions were switched off for this cycle, so the
checks below were run on that Mac.

## What was run

- **The merge gate.** All 100 steps passed at `74217baf`, the engine source
  this release is built from: both clients built (original 1.3 and CoD2x),
  the fidelity and ABI audits, the raw-offset gate, the CoD2x suites with the
  sanitizers on, and the online, movement, weapon, sound, collision, camera
  and script fixtures for both engines.
- **A parity audit.** The port was read against the original in seven parts:
  the Windows 1.3 game for everything a player sees or hears, the Mac 1.3
  game for Mac-only behaviour and CoD2x's own source for the CoD2x layer. It
  produced 173 findings. 119 are fixed. 36 are not faults: the original does
  the same, or the difference is a recorded decision. 5 cannot be provided.
  12 are still open and 1 is left as it is; both are listed below.
- **Replay against the original.** A demo recorded in the original Windows
  1.3 game was replayed there (under Wine with DXVK) and in both engines of
  this port at 1920x1080, and the pictures were compared view by view. The
  figure is the mean difference per colour value, out of 255; thin
  anti-aliased edges alone account for 0.1 to 0.3. Every stock multiplayer
  map was compared during the cycle. On the final code:
  - Toujane, three views, effects off: 0.12 to 0.27.
  - Toujane, the same views with a new profile's settings (glow, heat
    distortion, soft smoke and shadows on): 0.14 to 0.27.
  - Trainstation, six views: 0.02 to 0.28.
  - Rhine, five views: 0.12 to 0.30.
  - Harbor, seven views: 0.09 to 0.26. The water alone: 0.035, with the wave
    pattern matching (correlation 0.995 to 0.998).

  The figures are the original 1.3 engine's. CoD2x gives the same picture
  except that it outlines players during demo playback, as CoD2x does.
- **A new profile.** Each engine was started with an empty home folder and
  the launcher's first-run values. Glow, heat distortion, soft smoke and
  shadows were on in the running game and saved to its config in both.
- **The launcher.** Its full test suite passed: 202 checks, among them 182
  screens captured in dark and light, again at the smallest window size and
  under Increase Contrast, Reduce Transparency and Reduce Motion, with every
  control read through the accessibility interface. The run was finished in
  parts. The captures were taken at `e36dfe22`; three later commits changed
  one note in the Hosting form, one test's timing and one review screen, and
  were checked on the final source with the click-driven suites, the
  accessibility pass over every screen and the Hosting screens captured
  again.
- **Hosting a server.** The launcher's hosting code started both dedicated
  servers, CoD2x and the original 1.3, from the notarized app's own copies.
  Each wrote its `server.cfg`, answered a status query, moved to the next map
  when asked, refused a wrong admin password and shut down when stopped,
  leaving its port free. zPAM 4.12 and zPAM 3.36 were downloaded from the zPAM
  project's releases, matched the checksums published there, and a match
  server ran with each: 4.12 on CoD2x, 3.36 on the original 1.3. CoD2x's
  update service was replaced by a stand-in on this Mac for these runs.
- **The config checker and player setups.** The checker's tests cover every
  kind of finding and every fix. The 68 setups prepared for the catalogue were
  each read by the launcher's own import review in both game versions: none
  holds a line the review blocks, and names, chat lines, passwords, server
  settings and timed macros were removed before that. The catalogue is
  published, and its signed index, fetched from GitHub, verifies with the key
  built into the app.
- **Config Studio's catalogue.** The release build checks the bundled
  settings catalogue against the engine source. It was out of date and was
  regenerated: 852 settings, with no differences in either engine mode.
- **The package.** `scripts/package-release.sh` built both engines, both
  dedicated servers and the launcher from `c0b1de4c`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit.
  Each engine in the archive, started on its own with an empty home folder,
  reached the game's menu and quit cleanly, and both servers were hosted
  from it as described above. The engine source is the same as in an earlier
  package, built from `00ccf9a1`, which was also started through the
  launcher with an empty home folder: it built the shaders from the game's
  files, verified all 834 and reached the game's menu, and each of its
  engines loaded a map. Those two steps were not repeated on the final
  package.

## Known differences from the original

Still different, or never measured:

- **Water in the Direct3D 7 renderer.** The near water on Harbor is lit
  differently: 2.04 of 255 on that view. Only players who choose the
  Direct3D 7 renderer see it; the default renderer's water matches.
- **Near clip plane.** Geometry between 2 and 4 units from the eye is drawn
  here and clipped on Windows. It was not measured, and needs something
  almost touching the camera.
- **Menu panel edges.** The top and bottom one-pixel edges of dense menu
  panels are the other way round.
- **Voice codec.** Decoded sound differs from Windows by 13 to 82 steps of
  32768, which is inaudible. Original 1.3 voice chat only.
- **Tank machinery loop.** Up to 0.18 dB different, which is inaudible.
- **Wireframe diagnostic.** A developer view; its lines differ by 4.3 and
  7.1 of 255 in its two modes.

Corrected in code, but not seen beside the original in a running game:

- Soft smoke edges at 5K. They match at 1080p (0.001 to 0.011), the test
  harness cannot use a 5K mode, and one distant fire differs by 0.69 without
  a known cause.
- The frame shellshock saves. The Windows game faults under Wine when
  shellshock is triggered.
- Scenes dense enough to drop models, and a frame with two heat-distortion
  materials. No recorded scene reaches either.
- Three developer features with no trigger in normal play: shadow debug
  colours, developer points and cubemap export.

Cannot be provided: PunkBuster, IPX LAN play, the alternate 3D sound
providers and hearing your own microphone. Left as they are: the old patch
button in the original 1.3 menu does nothing (its update server is retired),
and CoD2x's `r_autopriority` setting is not included.

## What was not

- A full match played by a person on the final build. A person played a
  session earlier in the cycle, and its findings are fixed in this release.
- Public servers on the final build. Both engines last joined public servers
  on 10 November, at `7ae595cd`: the original 1.3 loaded a stock 1.3
  server's map, and CoD2x joined a CoD2x 1.4.6.8 server, downloaded the two
  zPAM files it needed and loaded its map.
- CoD2x's services against the CoD2x project's servers. zPAM updates, script
  web requests, WebSocket connections, the match service, vMix control, the
  update check's UDP fallback, and crash and error reports were run against
  stand-ins on this Mac. The one exception is the start-up update check:
  test runs of the CoD2x engine made it against the CoD2x project's server,
  as every CoD2x client does, and it answered that no update was available.
- No player joined a hosted server. Kick, the invite link, joining from
  another machine, a server listed publicly and a router's port forward were
  not tried, and no zPAM match was played on one. A released CoD2x 1.4.6.8
  client was not tried against a hosted CoD2x server; by the server's own
  check it is turned away.
- The launcher was not driven past starting the game in the packaged app:
  Setup, Settings and Launch were exercised by the launcher's tests, not by
  hand in the notarized build.
- Installing an update from a real release, and sending a report to a real
  report service.
- Encrypted WebSocket connections, and the refusal of a bad certificate by
  the CoD2x services.
- Voice chat with a real microphone. The sound path was tested with a
  recording in its place.
- Mouse sensitivity with a physical mouse, and CoD2x key authorisation
  against a real key server. The code follows CoD2x's; the end result was
  not compared.
- Any Mac other than the one above, and macOS 26. The app's minimum is macOS
  26, but it only ran on macOS 27.
- Game Mode and frame pacing were not measured again.
- The original was not run on a Windows machine for the replay comparison.

Only aggregate results belong in public release material. Game content,
screenshots of it, match records and local profiles are left out.
