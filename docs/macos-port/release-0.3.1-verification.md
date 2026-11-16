# CoD2 Silicon 0.3.1 verification

Recorded on 16 November 2026, on the same Mac mini as 0.3.0 (Apple silicon,
macOS 27.0.1, the macOS 27 SDK, a 1920x1080 display). GitHub Actions were
still switched off, so the checks below were run on that Mac.

0.3.1 is 0.3.0 with 39 commits on top. This record covers what changed and
what was run again; [0.3.0's record](release-0.3.0-verification.md) still
describes the rest.

## What was run

- **The merge gate.** `CONTRIBUTING.md` lists 88 commands. 79 of them passed
  at `f2eeefa7`, the last commit that changes the game or the launcher: both
  clients and the dedicated server built (original 1.3 and CoD2x), the
  fidelity and ABI audits with zero mismatches, the raw-offset gate, the
  CoD2x suites with the sanitizers on, the online, movement, weapon, sound,
  collision, camera, script, renderer and effects fixtures for both engines,
  Config Studio's catalogue against the source in both modes, the input
  bridge's self-tests and a recorded 100-frame run of the CoD2x game in a
  window. Three were started again after a first attempt that failed on a
  leftover output folder. The launcher suite and the live hosting test are
  two more of the 88 and are described below. The other seven are under
  "What was not".
- **The original game is touched only where intended.** The original-1.3
  build's 653 compiled units (the game and the dedicated server) were
  preprocessed at 0.3.0 and at `f2eeefa7`, with the version number set back
  so that only source changes show, and compared. 13 differ, each from a
  change listed in the changelog: the server list's PunkBuster field, flood
  protection, the PunkBuster rows, the Sound Provider row, a dedicated
  server's wait rounding, menu panel edges and Direct3D 7 fog. With the Mac
  port's switches off, the changed files give the same text as at 0.3.0 in
  all 15 configurations checked: 0 mismatches.
- **CoD2x as the released 1.4.6.8.** A started CoD2x server reports revision 6
  and "1.4.6.8", the values a released client checks, and the CoD2x suites
  pass at that revision. Coloured text codes, x-ray outlines and killfeed
  icons, which only CoD2x's unreleased source has, are off in the game and in
  demo playback.
- **Differences settled in the original binaries.** 13 places where this
  source read differently from another project's account of the game were
  looked up in the original game's own binaries. In 10 this
  source was right. 3 were faults and are fixed: the server list reply's
  PunkBuster field, flood protection reading the wrong field, and a dedicated
  server's wait of exactly half a tick.
- **The hosted server against the original, on the wire.** This port's
  original-1.3 server and the 2005 Linux server were each joined by the same
  scripted client on this Mac. They sent the same 282 game strings, the same
  baselines and the same snapshots. With the player spawned, 23 of 24
  snapshots with content are identical and the other differs in the random
  spawn point and soldier model. One difference is the original's own: the
  Linux server sends its "player connected" line without its command letter,
  so clients drop it.
- **Fuzzing.** Five targets cover what a server reads from the network:
  connectionless packets, the packet channel, message fields, command text,
  and a connecting or connected player's messages. Each ran for 30 minutes
  on both game versions with the address and undefined-behaviour sanitizers
  on and found no crash. After the move to CoD2x 1.4.6.8 the connectionless
  and player targets ran 30 minutes again on CoD2x: 5,133,312 and 2,796,544
  inputs, no crash.
- **Menu panel edges.** On the dense-menu fixture at 1920x1080, against the
  original's picture, the top edge row differed by 9.14 of 255 before and
  0.05 after, the bottom by 9.18 and 0.07.
- **The near clip plane.** 0.3.0 listed it as a suspected difference. It was
  measured in the running game by moving the camera towards a wall with
  `r_znear` at 1 and at 16: the wall is cut exactly 15 units apart. The plane
  is where the setting puts it, and nothing was changed.
- **Fog in the Direct3D 7 renderer.** Windows works fog out at each vertex
  and this port did it at each pixel. After the fix, against the original's
  frames at 1920x1080 (mean difference per colour value, of 255): Harbor's
  near water 4.56 before and 0.57 after, its distant water 4.10 and 1.89, the
  whole view 2.60 and 0.90. Twenty recorded Direct3D 7 views of Toujane,
  Carentan, Burgundy and Breakout: 0.132 before and 0.089 after on average,
  none worse. The default renderer is untouched: ten views of Toujane and
  Carentan are byte-identical before and after.
- **The launcher.** Its full test suite was run at `2713e7da`. Every unit
  suite passed, and 16 of the 17 capture phases: 434 pictures of its screens
  in dark and light, the same set at the smallest window size, the pages
  again under Increase Contrast, Reduce Transparency and Reduce Motion, every
  shape checked with no findings, and the suites that drive the pages with
  real clicks. The 17th phase reads every control through the accessibility
  interface, and it failed on its own expected text: the test still named
  0.3.0 on the Updates pane, where the launcher correctly says 0.3.1. The
  test was corrected (`83d058b9`, which changes nothing else) and the phase
  passed on its own: 126 screens, every control named. An earlier full run
  this cycle found two faults, both fixed before this one: Library's tabs
  losing their scroll position, and two rows squeezed in Report a Problem at
  the smallest window size.
- **Hosting a server.** The launcher's hosting code started both dedicated
  servers, CoD2x and the original 1.3, from the notarized app's own copies.
  Each wrote its `server.cfg` and a log of its own for that start, answered a
  status query, moved to the next map when asked, refused a wrong admin
  password and shut down when stopped. zPAM was not downloaded again; its
  checks are covered by the launcher's tests.
- **The package.** `scripts/package-release.sh` built both engines, both
  dedicated servers and the launcher from `83d058b9`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit.
  `tests/packaging/release_smoke.py` then started all four engines from it,
  each with an empty home folder of its own: both games reached the menu,
  loaded Toujane, played it and quit cleanly, and both servers answered a
  status query with the map and stopped. The games ran with the approximate
  shaders, because no set-up had run in those homes. The first attempt
  failed in the test itself, which brought no player profile, and the game
  loads no map without one; the test was corrected (`7cbc52df`).

## Known differences from the original

Still different:

- **Distant water in the Direct3D 7 renderer.** 1.89 of 255 on Harbor after
  the fog fix. The cause is not known. Only players who choose that renderer
  see it.
- **Rounding of exact halves.** Windows rounds a value that falls exactly
  between two whole numbers to the even one. 57 places in the game do the
  same now and 68 do not; none of the 68 sets a HUD or menu position.
- **Voice codec, tank machinery loop, wireframe diagnostic.** As in 0.3.0:
  inaudible, inaudible and developer-only.

Corrected in code, but not seen beside the original in a running game:

- The PunkBuster row in the original 1.3 game's Options, and the Sound
  Provider row. Their rules are tested; nobody looked at the menus.
- A dedicated server's wait rounding. It was read from the original
  dedicated servers' instructions, not seen in a running Windows server.
- Menus and the HUD in the Direct3D 7 renderer. Only 3D views were compared.
- Everything 0.3.0 listed here: soft smoke edges at 5K, the frame shellshock
  saves, scenes dense enough to drop models, and three developer features.

Cannot be provided, as before: PunkBuster, IPX LAN play, the alternate 3D
sound providers and hearing your own microphone.

## What was not

- **A released CoD2x 1.4.6.8 game on Windows or Linux joining a hosted
  server.** This is what 0.3.1 is for, and it rests on the server reporting
  the released version and on the admission tests, not on a friend joining.
- **A 0.3.1 player on a server hosted from 0.3.0.** By the code it is turned
  away; the changelog says so. It was not tried.
- A full match played by a person, a real microphone, a physical mouse, and
  public servers on this build. None of these changed since 0.3.0's record.
- **Real-input scenarios and the menu sweep** (five gate commands). They
  were run for the first time this cycle and both stop in their own harness
  on this display: the input run in the server browser, where four
  quarter-steps of the wheel did not move the selection by one, and the
  sweep because it expects 59 menus and finds 58. The engine built for 0.3.0
  finds the same 58, so the count is not new. Scrolling the server browser
  with a wheel and a trackpad, and opening each Options page, are left as
  checks by hand.
- **The byte-for-byte comparison of the untouched x86 build** (one gate
  command). It needs an x86-64 Linux machine. The preprocessor checks above
  stand in for it.
- The launcher's one-page capture variant (one gate command); the full run
  covers it.
- Wi-Fi freezes with AirDrop and Handoff on. The Hosting note and the server
  guide cite another project's measurement, 96 ms against 14 ms; it was not
  measured on this Mac, and no real change of network was made while a
  server ran.
- The descriptions of CoD2x's 42 settings were written from the source, not
  checked one by one in the running game.
- Installing this update from 0.3.0 through the launcher's own updater, and
  sending a report to a real report service.
- The packaged app started through the launcher with an empty home folder,
  which builds the shaders from the game's files. 0.3.0's record describes
  that run; it was not repeated.
- The original was not run on a Windows machine. Every reference picture
  comes from the Windows game under Wine with DXVK.
- Any Mac other than the one above, and macOS 26.

Only aggregate results belong in public release material. Game content,
screenshots of it, match records and local profiles are left out.
