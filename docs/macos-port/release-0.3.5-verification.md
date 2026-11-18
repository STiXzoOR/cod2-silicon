# CoD2 Silicon 0.3.5 verification

Recorded on 18 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK). GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.3.5 is 0.3.4 with two additions to setup: **Download from Steam…** on the
Game data step, for people who own the game on Steam and have it installed
nowhere, and a step that asks whether to send crash reports. Nothing in the
game changed. [0.3.4's record](release-0.3.4-verification.md) and the ones
before it still describe the rest.

## What was run

- **The launcher.** Its unit tests pass at the release commit (`0b6d6310`).
  The new ones drive the whole Steam download with a stand-in for the
  download tool and no network: a wrong checksum, so the tool never runs; a
  code that is never approved; an account without the game; a lost
  connection; the tool crashing; Steam finishing without the language files;
  Stop, then Resume to the end; Back from each Steam screen; quitting during
  sign-in; a half-downloaded folder refused by the folder picker; a reopened
  launcher offering to finish the download; and the crash reports answer
  surviving a relaunch. After each of them the tool's folder, and with it
  the stored sign-in, is gone.
- **The screens.** The new and changed Setup screens were drawn in both
  appearances and passed the layout check. The accessibility check found
  every control named and each step's action and progress read out, and the
  click-through walked Setup with the new steps.
- **The real download tool.** The launcher's own code fetched
  DepotDownloader 3.4.0 from its release page, checked it against the pinned
  SHA-256 and started it; Steam's sign-in code arrived as a 29 by 29 code
  after about four seconds and was renewed once. Nobody signed in during
  that run, and nothing was left behind.
- **A real download.** The project's owner then ran a test build from the
  first commit of this work as a new player: scanned the code with the Steam
  app, downloaded the whole game (3.70 GB, sixteen archives, English) into
  an empty folder, and went on through setup. That run is what the second
  commit answers: folder paths shortened to `~/`, one "Skip for Now" label,
  and a launcher that remembers an unfinished download.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from `0b6d6310`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit,
  and holds the report service's address.
  `tests/packaging/release_smoke.py` started all four engines from it: both
  games reached the menu, loaded Toujane, played it and quit cleanly, and
  both servers answered a status query and stopped.

## What was not

- A download that is interrupted and then finished was not tried against
  Steam itself, and neither was the reopened launcher's offer to finish
  one: both are covered only by the stand-in.
- The languages other than English were not downloaded. Their depot numbers
  come from Steam's public listing of the game.
- An account that does not own the game was not tried against Steam; the
  stand-in prints what the tool prints in that case, read from the tool's
  source.
- The full capture suite, which draws every screen of every page in both
  appearances, was not waited for. The Setup screens, where this release
  changes things, were drawn and checked.
- No crash report was sent from this build. The sender is 0.3.4's, and the
  new step only sets its switch.
- Install Update from 0.3.4 was not tried in a running 0.3.4. 0.3.4 accepts
  an update from this repository, with this archive name and this signing
  team; that was read from its source.
- The legacy source check and the rest of the merge gate in
  `CONTRIBUTING.md` were not run again; no game code changed.
