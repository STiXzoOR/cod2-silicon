# CoD2 Unbound 0.4.1 verification

Recorded on 21 November 2026, on the same Mac as 0.4.0: a Mac mini (Apple M6,
macOS 27, the macOS 27 SDK) with one 5K display that runs at 60 Hz and is
16:9. GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.4.1 is a round of fixes found by going over 0.4.0 after it shipped (issues
5, 27 to 39, 56 and 57), and one new thing: sharing a loadout with other
players from Configs.

**Still nobody has played a whole match on Metal for a release record, and
nobody has spoken into a real microphone.** Everything below is a replay, a
listen server with one player, a measurement run, an automated start or a
test. Several fixes are for hardware this Mac does not have; they are listed
under "What was not".

## What was run

- **The builds.** Clean rebuilds of the Metal game, both OpenGL games and
  both dedicated servers. Every source file the release touches was compiled
  again in 0.4.0's tree and in this one: no warning is new and none is gone.
  The dedicated servers import exactly the symbols 0.4.0's servers import,
  and no audio framework.
- **The engine checks.** `python3 tests/fixes13/legacy.py --base main`
  reports 0 mismatches over 13 changed source files. The byte-offset check
  reports nothing unreviewed on the Metal build (1,303 sites), the OpenGL
  build (1,237) and the OpenGL reference build (1,299).
- **The picture, against 0.4.0.** The Toujane replay at 1920 × 1080 in a
  window with 4x anti-aliasing: all five frames are identical, pixel for
  pixel, to 0.4.0's on Metal, on OpenGL with the original game and on OpenGL
  with CoD2x. The Carentan replay with no smoke is identical too on Metal
  with 4x, without anti-aliasing, with the DirectX 7 renderer and at a 67%
  render scale. The engine logs differ from 0.4.0's only in the lines the
  fixes are meant to change. So nothing below changed a picture it was not
  meant to change.
- **Smoke** (issues 33 and 56). With a frozen cloud at 1920 × 1080 and 4x
  anti-aliasing, pixels along roof and wall edges that differ from the
  full-size smoke layer by more than 12 of 255 went from 217 to 0 at quarter
  size and from 180 to 0 at half size; what is left is no more than two
  full-size frames differ from each other. At a 67% render scale the same
  count went from 1,580 to 0. Inside the cloud with soft particles off, the
  weapon was clearer than at full size by 33.6 of 255 on average; it is now
  within 0.02. The step that skips hidden smoke no longer lets the scene
  through after the smoke has moved: after the change, no measured view has
  more pixels showing the scene through smoke than before, and the worst
  case went from 782 pixels to 0. Drawing smoke at quarter size costs 3 to 5%
  more frame time in a 5120 × 2880 window.
- **Shaders on a first visit** (issue 27). With nothing saved, 0.4.0 left 23
  to 42 draws out of their frames on Toujane while shaders compiled; this
  build leaves out none. The first frame of a map takes 10 to 25 ms longer
  the first time; later starts are unchanged.
- **Sun glare with anti-aliasing** (issue 28). Looking straight at the sun
  on Toujane at 4x, the picture is now as bright as without anti-aliasing
  and as OpenGL's, and the glare rises over time the same way. A partly
  hidden sun was not run.
- **Display modes** (issue 29). On this display the refresh rates offered
  went from "60 Hz" alone to the display's own list, and the game logs which
  display and rate it paces to. The function that picks the picture's shape
  was run on real sizes through a temporary switch: 1024 × 768 and 1280 ×
  1024 give 4:3 and 1920 × 1080 gives 16:9. A 16:10 size could not be run
  here; by the arithmetic it gives 16:10.
- **`r_gpuSync`** (issue 30). A probe counted how many frames pass before a
  finished frame is seen as shown, in a 5120 × 2880 window inside four smoke
  grenades: 4 in 0.4.0, 3 now. The cost there is 3 to 12% of frame time,
  measured while other work was running; where the frame cap holds there is
  none. The delay from mouse to screen itself was not measured.
- **CoD2x's film colour settings** (issue 31). Twelve changes of a setting
  in a running game compiled 13 shaders in 0.4.0 and compile 1 now. Over
  five settings and three conditions the pictures are identical to 0.4.0's,
  except 6 to 13 pixels one level apart with one setting.
- **Large sizes with anti-aliasing** (issue 32). With creation made to fail
  by a temporary switch, the game started with 2 samples, then with none,
  drew normally and survived a renderer restart. Without the buffer it no
  longer makes, the game uses 65 to 85 MB less in a 5120 × 2880 window.
- **The screen after a grenade kills you** (issue 57). It was drawn upside down on
  Metal, at Full and at a 67% render scale; it is now upright and framed as
  OpenGL frames it. A blast you survive was not triggered.
- **Select Profile** (issue 34). With two profiles in a private game folder,
  the Metal game started as the launcher starts it opens on the main menu,
  and started without that opens Select Profile listing both. The launcher
  line that says a loadout was chosen is in the launcher this record's suite
  built; a start through the launcher with two profiles was not run.
- **Three CoD2x settings** (issue 36). `con_printDoubleColors 0` showed a
  killfeed name without its colours; `r_autopriority 1` lowered the game's
  priority when another app came to the front and restored it after;
  `com_affinity` is accepted without an error.
- **Voice chat** (issue 5), without a microphone. The capture tests ran
  under the address, undefined-behaviour and thread sanitizers with no
  report: opening the microphone no longer blocks a frame (the slowest frame
  during a 0.4-second open took under 0.11 ms; 0.4.0's code fails this
  test), and capture reopens after access is granted, after the device is
  lost and after the default input changes. The playback, relay and queue
  tests pass (55 of 55 checks; 90 queue cases).
- **The launcher's fixes** (issue 35). Each has a check in the
  launcher's tests, and the two that could be shown failing first were:
  the ping test stopped on 0.4.0's code, and the catalogue test failed with
  the server's refusal before the fix.
- **Share your setup** (issue 37). The launcher's cleaning is the catalogue
  tool's own, moved to one place: on 639 configs the old and the new give
  identical output. `tests/launcher/config_share.sh` covers the cleaning,
  the name rules, the request, every reply the service can give, the saved
  record and Stop Sharing, against a local stand-in. The website's service
  has 42 tests of its own, and all 68 configs in the catalogue pass its
  rules. The twelve Share screens were compared with their boards on the
  design canvas, in both appearances; the differences found were corrected
  in the app or on the boards.
- **The launcher's tests and the capture suite.** At `f5ec87a7`, the commit
  that sets the version: every check passed and the capture suite passed all
  17 of its phases the first time: 436 screens in both appearances, at full
  and at minimum size and with the accessibility settings, their
  accessibility trees, the real clicks, and the shape audit with no finding.
  The package was built one commit later, at `f69e300c`. The release build's
  own check had stopped the first package: the launcher's settings catalogue
  did not list the three CoD2x settings this release adds to the game. That
  commit regenerates the catalogue from the 0.4.1 source. The three settings
  are the only entries added, and every other entry differs in its source
  line numbers alone. On it, the catalogue matches the source for both game
  versions, and every launcher check that opens no window passed again; one
  Steam download check that depends on timing failed in the first run, while
  other work was running on the Mac, and passed when run again. The capture
  suite was not run again: it draws nothing from those three entries.
- **The package.** `scripts/package-release.sh` built both games twice (for
  Metal and for OpenGL), both dedicated servers and the launcher from
  `f69e300c`, signed the app with a Developer ID and had Apple notarize it,
  then built the disk image, signed it and had Apple notarize that too; both
  tickets are stapled. The app taken out of the ZIP and the app inside the
  disk image each pass the checksum, the signature check, Gatekeeper's
  assessment and the bundle audit (twelve programs and libraries, all signed
  by the same Developer ID), and each carries the crash-report address.
  `tests/packaging/release_smoke.py` started all six engines from the app in
  the disk image: both games, with Metal and with OpenGL, reached the menu,
  loaded Toujane, played it and quit cleanly, and both servers answered a
  status query and stopped.

## Known differences

- **Render scale is a plain stretch**, as in 0.4.0.
- **With the DirectX 7 renderer, smoke drawn at half or quarter size** can
  show a sharp-edged patch whose edges sit about a pixel off. It was seen in
  one of two runs, is the same in 0.4.0, and was not looked into.
- **In the two Share sheets that show text you can copy**, VoiceOver reads
  the text as one item with a name and cannot select inside it. Copy and
  View Request sit beside them.
- **The Stop Sharing question** has its two buttons side by side, as macOS
  lays out short titles; Delete Loadout's are stacked because its title is
  longer.

## What was not

- A whole match with Metal, as before.
- **Voice with a real microphone.** The permission prompt and the name it
  shows, the menu bar's microphone indicator, two players hearing each
  other, a change of device while talking, a Bluetooth headset, and
  `winvoice_mic_mute 0` by ear.
- **A display that is not 16:9, a display faster than 60 Hz, and a second
  display.** The display-mode fix was run on one 16:9, 60 Hz display.
- **A Mac with 8 GB of memory** at a large size with 4x anti-aliasing. The
  step down to fewer samples was shown only by making creation fail.
- Whether the mouse feels closer to the screen with `r_gpuSync` on.
- **A real send.** The launcher has only met the stand-in, and the service
  has only met its tests. The first setup sent from the published app is the
  first real one.
- In Share: the items of the More menu and of the Player Setups button's
  menu, the Left Out tab of See What's Sent, and the Stop Sharing question
  in the light appearance, which the suite cannot capture properly.
- Whether sheets look the same in a window that has the keyboard as in the
  suite's captures, where they are flatter than the boards draw them.
- A start through the launcher with two game profiles.
- Spectating and the killcam with smoke or a render scale, as in 0.4.0.
- Install Update from 0.4.0 to 0.4.1. It needs the published release.
- A Mac other than this one.
- VoiceOver itself, as before.

Everything in [0.4.0's record](release-0.4.0-verification.md) that this
record does not replace still applies.
