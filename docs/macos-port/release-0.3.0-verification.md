# CoD2 Silicon 0.3.0 verification

Recorded on 10 November 2026, on a Mac mini with Apple silicon, macOS 27 and
the macOS 27 SDK. GitHub Actions were switched off for this cycle, so the
checks below were run on that Mac.

## What was run

- **The Mac CI job, locally.** Every step of `.github/workflows/ci.yml`'s
  `macos-arm64` job ran in a clean clone of `7ee1e700` and passed: both
  clients built from scratch (stock 1.3 and CoD2x), the ABI audits, the CoD2x
  suites, the sanitizer platform tests and the source fixtures.
- **The raw-offset gate** reports no unreviewed site in 417 sources.
- **The parser fuzz targets** (messages, the network channel, out-of-band
  packets, the tokenizer) pass their smoke run. It found one signed overflow,
  which is fixed and kept as a regression input.
- **Replay against the original.** A recorded demo was replayed in the
  original Windows 1.3 game (under Wine with DXVK) and in this port, and 20
  views on four maps were compared at 1920x1080 with anti-aliasing off. The
  mean difference per colour value, out of 255, is 0.11 on the default path,
  0.13 with the HUD on and 0.15 on the Direct3D 7 path. The engine inside the
  packaged app gives the same 0.11.
- **The package.** A package built with `scripts/package-release.sh` passed
  the bundle audit, the launcher core tests and the first-run test. Started
  with an empty home folder, it built the shaders from the game's files,
  verified all 834, opened the menu, loaded a map and quit cleanly.
- **Servers.** Earlier in this cycle, at `1f3d9e68`, the Mac client joined
  stock 1.3 servers and a CoD2x 1.4 server, downloaded their mods and played.

## What was not

- Voice chat with a real microphone. The sound path was tested with a
  recording in its place.
- Any Mac other than the one above, and macOS 26. The app's minimum is macOS
  26, but it only ran on macOS 27.
- Game Mode and frame pacing were not measured again.
- The original was not run on a Windows machine for the replay comparison.

Only aggregate results belong in public release material. Game content,
screenshots of it, match records and local profiles are left out.
