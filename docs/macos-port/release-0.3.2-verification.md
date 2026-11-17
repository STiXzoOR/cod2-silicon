# CoD2 Silicon 0.3.2 verification

Recorded on 17 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK, a 6016x3384 display). GitHub Actions were still switched off, so the
checks below were run on that Mac.

0.3.2 is 0.3.1 with three commits on top: the launcher no longer leads with
frame rate, the in-game console is enlarged above 1080 lines, and the version
and notes. This record covers those; [0.3.1's record](release-0.3.1-verification.md)
and [0.3.0's](release-0.3.0-verification.md) still describe the rest.

## What was run

- **The console.** The packaged CoD2x game was started at 1920x1080 and at
  5120x2880 with the console open. Its input line covers the same share of
  the screen height in both (2.2%). The full console, with the output window,
  was compared the same way from a build of the same source and matches.
- **The original game is touched only where intended.** With the Mac port's
  switches off, the three changed source files give the same text as before
  the change in all ten configurations the legacy check covers: 0 mismatches.
- **The launcher.** Its unit tests pass at the release commit (`17c9d207`).
  The screens changed by the frame rate work were captured and reviewed when
  that change was made.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from `17c9d207`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit.
  `tests/packaging/release_smoke.py` started all four engines from it: both
  games reached the menu, loaded Toujane, played it and quit cleanly, and
  both servers answered a status query and stopped. The packaging script's
  catalogue check passed in both modes.

## What was not

- The rest of the merge gate in `CONTRIBUTING.md` was not run again for this
  release; no game code changed apart from the console's drawing.
- The console's suggestion list while typing, and scrolling the output with
  the keyboard, were not checked: the test cannot press keys. They use the
  same drawing calls as what was checked.
- The full launcher capture suite was not run again after the version and
  notes change.
- The console was not checked at 6016x3384 itself: the test's own start
  settings gave 5120x2880 on that display.
- The in-app update from 0.3.1 to 0.3.2 was not exercised before publishing.
