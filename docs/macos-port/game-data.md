# Getting your Call of Duty 2 data

CoD2 Silicon needs data from a copy of Call of Duty 2 that you legally own,
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

## Original Mac shaders

The Steam Mac multiplayer executable tested in [WS18](reports/WS18-ship.md)
is at:

```text
~/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer
```

It contains the original ARB programs and constant metadata. The app extracts
834 files into a local cache, validates their manifest and reuses them on
later launches. It continues with approximate shaders if extraction is
unavailable. No original shader is bundled or downloaded by this project.

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
`~/Library/Application Support/CoD2 Silicon`. The key prompt checks the key's
checksum before saving it with owner-only permissions. Data from an earlier
`CoD2x Native` install is migrated once.

If `default_mp.cfg` is missing, check that you selected the parent of `main/`
and that the base and matching language archives are complete. Keep source
data read-only and use a separate writable home for development/parity runs:

```sh
./build-macos-codx/cod2_macos \
  +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$HOME/Library/Application Support/CoD2 Silicon"
```

Never post your CD key, Steam login, credentials, HWID, key digest, demos or
game archives in a bug report. Review and redact console logs before sharing.
