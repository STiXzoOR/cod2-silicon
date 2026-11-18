# CoD2 Silicon 0.3.4 verification

Recorded on 18 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK). GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.3.4 is 0.3.3 with one change: the release is built with a crash report
service, so **Settings → Privacy** offers "Send crash reports to CoD2
Silicon". The switch is off until it is turned on. The sender itself has
been in the launcher since 0.3.0 and its code did not change; nothing in
the game changed. [0.3.3's record](release-0.3.3-verification.md) and the
ones before it still describe the rest.

## What was run

- **The launcher.** Its unit tests pass at the release commit (`bf2dac99`),
  including the crash report tests: what a report holds, the user name and
  command lines taken out, a report that seems to hold a CD key refused, and
  each report sent once.
- **The report service.** Two reports, written by hand and labelled as
  tests, were sent through the launcher's own sender to the project's
  service and arrived, each with its attached text. The first showed that
  the service worked out a city from the connection although the address
  itself was not kept. The project now removes that location as well, and
  the second report was stored with no address and no location. Each
  carried the app's version, the macOS version, the Mac's model and which
  game crashed, and no name or account.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from `bf2dac99`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit,
  and holds the report service's address.
  `tests/packaging/release_smoke.py` started all four engines from it: both
  games reached the menu, loaded Toujane, played it and quit cleanly, and
  both servers answered a status query and stopped.

## What was not

- The switch was not turned on in a running launcher, and no real crash was
  sent from one: the reports above went through the same sender from a
  small test program.
- Install Update from 0.3.3 was not tried in a running 0.3.3. 0.3.3 accepts
  an update from this repository, with this archive name and this signing
  team; that was read from its source.
- The legacy source check and the rest of the merge gate in
  `CONTRIBUTING.md` were not run again; no game code changed.
