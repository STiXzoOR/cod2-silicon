# CoD2 Unbound 0.3.6 verification

Recorded on 18 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK). GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.3.6 is 0.3.5 under a new name. The app is CoD2 Unbound, with a new bundle
identifier, a new icon, a renamed data folder and a disk image to install
from, and one layout fix on Home. Nothing in the game changed.
[0.3.5's record](release-0.3.5-verification.md) and the ones before it still
describe the rest, under the name the app had then.

## What was run

- **The launcher.** Its whole test suite was run at `78a07183`. The unit
  tests, the config, hosting and Steam download tests and the typecheck of
  the fallback rendering path passed. The capture suite, which draws every
  screen of every page in both appearances and checks the pictures, passed
  15 of its 17 phases, and the other two were followed up:
  - *Minimum size* found a real fault on Home. In a small window the chips
    under the buttons take a second row when one is too wide, and the extra
    height slid them up over Launch and Find a Server. `44707ae0` fixes it;
    Home's screens were then drawn again at full and at minimum size with no
    findings.
  - *Page actions* failed its first Library check because that run kept its
    private home under `/private/tmp`, where a folder listing and a path
    built from the home spell the same file two ways. Run again with the
    home inside the checkout, and with the fix in, all 20 of its checks
    passed.

  The whole suite was not run again after the fix, which touches Home's
  layout and nothing else.
- **The new mark.** The sidebar, Setup's welcome and Settings › About were
  looked at in both appearances with the star, the 2 and the drawn name in
  place.
- **The data folder.** `tests/packaging/first_run.py` covers the takeover:
  an old `CoD2 Silicon` folder is renamed once, and when both folders exist
  neither is touched. The packaged stock dedicated server was also started
  once in a private home that held an old folder with a file in it; afterwards
  the folder was `CoD2 Unbound` and the file was inside.
- **The legacy source check.** `tests/fixes13/legacy.py --base main` reports
  ten mismatches, all in `src/platform/cod2x_native_setup.h`. That file is an
  Objective-C header which only Mac code includes (the launcher, the CoD2x
  wrapper and the first-run test), so no 32-bit build reads it. The two
  window titles in `macos_display.c`'s legacy branches still say what they
  said.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from the release commit (`44707ae0`), signed the app with a
  Developer ID and had Apple notarize it, then built the disk image, signed
  it and had Apple notarize that too; both tickets are stapled. The app
  taken out of the ZIP and the app inside the disk image each pass the
  checksum, the signature check, Gatekeeper's assessment and the bundle
  audit. `tests/packaging/release_smoke.py` started all four engines from
  the app in the disk image: both games reached the menu, loaded Toujane,
  played it and quit cleanly, and both servers answered a status query and
  stopped.
- **The disk image's window.** Opened in Finder on this Mac in Dark, with
  Finder's tab, path and status bars on: the picture, both icons at their
  places, both names readable on it. A see-through test picture showed that
  Finder paints such a window light in both appearances, which is why one
  picture serves both.

## What was not

- The disk image's window was not looked at in Light, on macOS 26, or with
  Increase Contrast on.
- Nobody installed 0.3.6 over a real CoD2 Silicon with real settings. The
  takeover was tried in private homes only, and never against the folder of
  the Mac this was built on.
- What a running 0.3.5 shows when it finds 0.3.6 was read from its source,
  not watched: it lists the update and opens the release page, because the
  archive has a name it does not accept.
- Install Update from 0.3.6 to a later version cannot be tried until there
  is one. The updater's tests pass with the new archive name and identifier.
- The microphone and notification prompts that macOS shows again for the
  renamed app were not walked through, and neither was typing a remembered
  server password again.
- A dedicated server installed with the old helper script was not carried
  over; the changelog says to remove it with the old script.
- The rest of the merge gate in `CONTRIBUTING.md` was not run again; no
  game code changed apart from the names.
