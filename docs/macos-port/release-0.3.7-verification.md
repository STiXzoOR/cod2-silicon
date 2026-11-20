# CoD2 Unbound 0.3.7 verification

Recorded on 20 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK). GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.3.7 changes how the launcher looks and reads, and nothing else. Every page
now follows the one set of design rules settled on 19 November: the same page
titles, group labels, buttons, switches and fields everywhere. No file under
`src/` changed, so the game, both dedicated servers and CoD2x are the code
0.3.6 shipped. [0.3.6's record](release-0.3.6-verification.md) and the ones
before it still describe the rest.

## What was run

- **The launcher's tests.** At `fd4bc558`, the commit the package was built
  from: the unit tests, the config, hosting and Steam download tests, the
  font check, the check that every button takes its style from `Theme.swift`,
  and the typecheck of the fallback rendering path. All passed.
- **The capture suite.** It draws every screen of every page in both
  appearances, at full and at minimum size and with the accessibility
  settings, and checks the pictures, the accessibility trees and a set of
  real clicks. Run in full at `06ef777e`, it passed 12 of its 17 phases. The
  other five were followed up, fixed in `fd4bc558`, and each was run again
  and passed:
  - *Accessibility trees* found a real fault. The Rules menu in Check-up,
    Expert and Edit as Text had lost its VoiceOver name when it was redrawn
    as a capsule. It reads "Rules to check against" and the chosen rules
    again.
  - *Minimum size* found a real fault. In the narrowest window the bar above
    the keyboard drawing sat 16 points from a corner rounded at 20. It keeps
    20 now. In the same pictures "Tune for" broke onto two lines; the words
    now stay on one line, or step aside when there is no room for them.
  - *Overlays* failed on two of the suite's own expectations. The Jump
    button now shows its shortcut and is read with it, and the mouse chip in
    Binds moved below the fold, where a click could not reach it. The suite
    now scrolls to it first.
  - *Config actions* failed on two more. The rename sheet's field is called
    "Loadout name", and the sample player is "vargr" instead of a
    placeholder, so the imported loadout's folder has that name.
  - *Increase Contrast* reported one main button as a rounded rectangle. It
    is a capsule: the button sits on a picture, and the check compared its
    corner with the picture beside the button's middle. It now reads the
    background beside the corner.

  The whole suite was not run again after `fd4bc558`. Its five failed phases
  were, and the changed Configs screens were drawn once more at both sizes.
- **Against the design.** While the work was done, each area's screens were
  compared with their boards on the design canvas. Afterwards these were
  looked at in the suite's pictures of the finished branch: Home, Servers,
  Library (Demos, Matches, the Trash banner, the rename error), Settings
  (General, Display, About), Setup's welcome and CD key steps, Player Setups,
  a player's page, Guided and Check-up in Light; Servers, Expert, Binds, the
  import review and Check-up in Dark; and Servers, Home, Guided, New Server,
  Check-up, Expert, Matches and a player's page at minimum size.
- **Behaviour.** The changes outside the views were read for anything that
  alters what the app does. One control changed kind: the mouse's up and down
  direction is a switch now, and writes the same values the two-way control
  wrote. Three things moved without changing what they do: while there are
  no servers, Hosting leaves New Server and Export to the page instead of
  the toolbar; History's source filter is a pop-up button; and the Rules
  menu is drawn by the app.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from `fd4bc558`, signed the app with a
  Developer ID and had Apple notarize it, then built the disk image, signed
  it and had Apple notarize that too; both tickets are stapled. The app taken
  out of the ZIP and the app inside the disk image each pass the checksum,
  the signature check, Gatekeeper's assessment and the bundle audit, and each
  carries the crash-report address.
  `tests/packaging/release_smoke.py` started all four engines from the app in
  the disk image: both games reached the menu, loaded Toujane, played it and
  quit cleanly, and both servers answered a status query and stopped.

## What was not

- The whole capture suite was not repeated on the final commit, as said
  above.
- Not every screen was compared with its board by eye after the last edits.
  Not looked at: the Add a Config menu open, most Setup steps in Light, and
  the crash report sheet.
- Where the app and a board still differ on purpose (the system's toolbar
  grouping, its pop-up chevrons, its sidebar rows, and wording the app has
  more accurately), the app was left as it is. Things a board shows that the
  app cannot do yet were not built: the render scale and the Metal choice in
  Display belong to 0.4.
- Many states have no board (error, loading and empty states, mostly). The
  rules were applied to them, and nobody has approved those screens on the
  canvas.
- VoiceOver itself was not run. The suite reads the accessibility tree the
  way VoiceOver does, which is how it found the unnamed menu.
- Install Update from 0.3.6 to 0.3.7 was not tried. It needs the published
  release.
- The Steam download test failed once on a timing check, on the branch
  before the version commit and while other builds were running. It passed
  when run again and passed at `fd4bc558`; nothing in 0.3.7 touches that
  code.
- The disk image's window was not opened in Finder this time; the script
  that builds it did not change.
- The rest of the merge gate in `CONTRIBUTING.md` was not run again; no game
  code changed.
