# Getting your Call of Duty 2 data

CoD2 Unbound needs data from a copy of Call of Duty 2 that you legally own,
plus your own CD key. Nothing in this repository supplies the game. Steam's
[game is app 2630](https://store.steampowered.com/app/2630/Call_of_Duty_2/).
Keep downloads, Steam credentials and manifests outside the source checkout.

## Use an existing installation

Choose the folder containing `main/` on first launch. For the tested English
Windows 1.3 installation, that folder contains:

```text
CoD2/
  main/
    iw_00.iwd … iw_15.iwd
    localized_english_iw00.iwd … localized_english_iw11.iwd
```

Windows 1.3 archives were used for native map loading and public-server play
in [WS14](reports/WS14-online.md). Use matching localized content for your
language. Preserve names, case and archive contents; do not unpack/repack IWDs
or combine different platform installs to fill gaps. Pure-server checksums
still apply. If `main/` is nested inside an old Mac app bundle, select its
parent data folder or copy the complete `main/` directory to a separate data
root. Retain the unmodified original install.

## Download your licensed Steam content

### In setup

If you own the game on Steam, setup's Game data step has **Download from
Steam…**. Choose a language and a place for the game (about 3.7 GB; the
suggestion is `~/Games/CoD2`), then scan the QR code with the Steam app on
your phone: tap the shield at the bottom, then **Scan a QR Code**. Your phone
names the sign-in "DepotDownloader", after the tool that does the download.
No Steam name or password is typed on the Mac.

What setup does for you:

- It fetches [DepotDownloader](https://github.com/SteamRE/DepotDownloader)
  3.4.0 for Apple silicon (32 MB) from the tool's own release page and checks
  it against a known SHA-256 checksum. A file that does not match is deleted
  and never run. The tool is free and open source (GPL-2.0) and is not made
  by Valve or by this project; it is never part of the app.
- It runs the tool with a home folder of its own inside the launcher's
  support folder, so whatever the tool stores about the sign-in lands there.
- It asks for the Windows edition of app 2630, naming each depot: 2631, 2632,
  2633 and the one for your language (English 2634, German 2636, French 2637,
  Spanish 2638, Italian 2639).
- When the download ends, stops or is cancelled, and when the launcher
  quits, it deletes that home folder, which signs the tool out. When the game
  is complete, or you leave the Steam screens, it also deletes the tool; a
  finished download has the tool's notes (`.DepotDownloader`) removed from
  the game folder.
- What the tool printed in its last try is kept in the launcher's `logs`
  folder as `steam-download.log`, without the sign-in code. It stays on your
  Mac; crash reports never include it.

If the download stops, the files stay. **Resume** shows a new code, because
the sign-in is not kept between tries, and the tool then checks what is on
disk and fetches only what is missing. A folder with an unfinished download
is not accepted as game data until the download is finished. Closing the
launcher mid-download stops the tool and signs it out; the next time, the
Game data step shows **Finish the Download…** for the same folder and
language.

Your CD key is in Steam: right-click the game, then **Manage › CD keys**.

### By hand

[DepotDownloader](https://github.com/SteamRE/DepotDownloader) has a native
macOS arm64 release. Version 3.4.0 was checked during development. Download
that tool from its own Releases page and keep it outside this repository,
for example in `~/Tools/DepotDownloader/`.

Run it yourself in Terminal. QR login avoids a password in arguments:

```sh
"$HOME/Tools/DepotDownloader/DepotDownloader" \
  -app 2630 -os windows -language english -qr -dir "$HOME/Games/CoD2"
```

Download the Mac edition separately for the original shaders:

```sh
"$HOME/Tools/DepotDownloader/DepotDownloader" \
  -app 2630 -os macos -language english -qr -dir "$HOME/Games/CoD2-mac-bin"
```

Use your own entitled Steam account and complete Steam Guard yourself.
Anonymous login does not replace ownership. Never put credentials in scripts
or commit downloader output. Keep the two downloads separate.

The development depot map was Windows **2631–2634** (English: 2634), macOS
**231931–231933** (English: 231933). Steam selects content according to account,
platform and language; inspect your own manifests rather than assume every
depot or historical version is available. Valve's
[SteamCMD reference](https://developer.valvesoftware.com/wiki/SteamCMD) describes
an alternative downloader on a compatible machine. Its macOS archive does
not prove that the executable runs on arm64 or supports old 32-bit programs.

## Original shaders

The Windows game compiles its shaders from HLSL sources in `main/iw_07.iwd`
(`materials/shaders/*.hlsl`) each time it starts. CoD2 Unbound does the same
work once, at setup, with the bundled `cod2-shadergen`:

1. It reads the 242 HLSL files from your game's `.iwd` archives.
2. It makes the one edit OpenGL needs: vertex shaders write their clip position
   with an OpenGL projection matrix. Everything else compiles as shipped.
3. It compiles each shader's `vs_main` and `ps_main` to Direct3D 9 bytecode with
   [vkd3d-shader](https://gitlab.winehq.org/wine/vkd3d), and translates the
   bytecode to the OpenGL programs and constant tables the renderer loads.

The result is 834 files in a local cache with a SHA-256 manifest. It takes a
few seconds and is reused on later launches. A new app version rebuilds the
cache if its generator changed. Nothing derived from the game is bundled or
downloaded by this project. The generator's sources are in
[`tools/shadergen`](../../tools/shadergen); `scripts/build-vkd3d.sh` builds the
compiler library from pinned sources.

These shaders follow Windows. The Mac 1.3 game shipped precompiled shaders
with three 2006 workarounds (a scaled `normalize`, a series approximation of
`sin` and `cos` that is visibly off on flags, and power-of-two render-target
clamps); the generated ones leave those out.

### Mac 1.3 shaders as a fallback

If the shaders cannot be built from your game files, the app can take the
precompiled ones from a Mac copy. The Steam Mac multiplayer executable tested
in [WS18](reports/WS18-ship.md) is at:

```text
~/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer
```

It contains the Mac ARB programs and constant metadata. The app extracts
834 files into the same local cache and validates their manifest. It
continues with approximate shaders if neither source is available.

Extraction is native code inside the app; no Python or developer tools are
needed at runtime. The app looks for the Mac executable in your game folder,
Steam libraries (including those listed in `libraryfolders.vdf`),
`/Applications` and `~/Games/CoD2-mac-bin`, and otherwise lets you choose the
Mac game folder.

Building from source does not need any Mac executable: arm64 builds compile
the committed typed-data snapshot. The Steam executable has incomplete STABS
type information; regenerating that snapshot additionally needs the
full-STABS 1.3 reference. See [typed-data generation](../../tools/datagen/README.md).

## First-run paths and privacy

The app finds your game folder or asks for it, and asks for a CD key if none
is stored. The key lives in `~/.cod2/preferences`; configs, logs, demos,
screenshots and the shader cache live in
`~/Library/Application Support/CoD2 Unbound`. The key prompt checks the key's
checksum before saving it with owner-only permissions. Data from an earlier
`CoD2x Native` install is migrated once.

If `default_mp.cfg` is missing, check that you selected the parent of `main/`
and that the base and matching language archives are complete. Keep source
data read-only and use a separate writable home for development/parity runs:

```sh
./build-macos-codx/cod2_macos \
  +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$HOME/Library/Application Support/CoD2 Unbound"
```

Never post your CD key, Steam login, credentials, HWID, key digest, demos or
game archives in a bug report. Review and redact console logs before sharing.
