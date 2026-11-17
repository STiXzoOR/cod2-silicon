# CoD2 Silicon 0.3.3 verification

Recorded on 17 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK). GitHub Actions were still switched off, so the checks below were run on
that Mac.

0.3.3 is 0.3.2 with one change: the project's addresses. The repository is
now `cod2unbound/cod2-unbound` and the config catalogue
`cod2unbound/cod2-configs`. Nothing in the game changed.
[0.3.2's record](release-0.3.2-verification.md) and the ones before it still
describe the rest.

## What was run

- **The addresses in the app.** The packaged launcher holds the new
  addresses for the update check, What's New, the catalogue and its links,
  and accepts update downloads from the repository's current address and
  from the two it had before (`STiXzoOR/cod2-silicon` and
  `cod2unbound/cod2-silicon`).
- **The launcher.** Its unit tests pass at the release commit (`742b0c9e`),
  including the update offer from each of the three addresses and its
  refusal of another host or another project.
- **The catalogue.** Revision 2 of the index lists every config at the new
  address. It was generated from the same 68 entries, checked byte for byte
  and signed with the same key; the public key is unchanged. Downloaded
  back from both the new and the old address, the index verifies against
  that key, and a config it lists is served.
- **The old addresses.** After the move, the old update-check address
  answers with a redirect that works, and the old news and catalogue
  addresses still serve their files.
- **The package.** `scripts/package-release.sh` built both games, both
  dedicated servers and the launcher from `742b0c9e`, signed the app with a
  Developer ID and had Apple notarize it; the ticket is stapled. Unpacked
  from the archive, the app passes its checksum, the signature check,
  Gatekeeper's assessment ("Notarized Developer ID") and the bundle audit.
  `tests/packaging/release_smoke.py` started all four engines from it: both
  games reached the menu, loaded Toujane, played it and quit cleanly, and
  both servers answered a status query and stopped.

## What was not

- **Install Update from 0.3.2 or earlier does not work, by design of those
  versions.** They accept an update only from the old address, and GitHub
  now reports the new one, so they show that 0.3.3 exists and open its
  release page. This was read from their source, not tried in a running
  0.3.2.
- Install Update from 0.3.3 to a later version cannot be tried until there
  is one.
- The launcher was not opened by hand to browse the catalogue or What's New
  at the new addresses; the addresses and the signature were checked
  outside it.
- The legacy source check reports one file, a Cocoa header of the Mac port
  whose link changed; the original game does not build it. The rest of the
  merge gate in `CONTRIBUTING.md` was not run again; no game code changed.
