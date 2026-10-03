# Owned CoD2 data for the native port

CoD2 is Steam app **2630**. Download it using your own account with a license
for the game. Keep the downloaded game, credentials and Steam manifests outside
this repository. The port contains reconstructed engine code, not game assets.

## Recommended: DepotDownloader (native arm64)

[DepotDownloader](https://github.com/SteamRE/DepotDownloader) 3.4.0 ships a
native `macos-arm64` build, so it runs on this Mac without Rosetta. Checked on
2026-10-03: the binary is `Mach-O 64-bit executable arm64` and prints its usage.
It is kept outside the repository in `~/Projects/cod2-native-refs/tools/`.

Run it yourself in a terminal. `-qr` shows a login QR code to scan with the
Steam mobile app, so no password is typed or stored in a script:

```bash
~/Projects/cod2-native-refs/tools/DepotDownloader/DepotDownloader \
  -app 2630 -os windows -language english -qr -remember-password \
  -dir "$HOME/Games/CoD2"
```

`-os windows` picks the Windows depots, which put the IWDs directly in
`main/`. Use `-os macos` instead to get the Mac depots. Either way, point
`fs_basepath` at the directory that contains `main/`, as described below.

## Alternative: SteamCMD

These commands are for the user to run. No SteamCMD process or authenticated
download was run while preparing this guide. The macOS bootstrap URL returned
HTTP 200 on 2026-10-03; that verifies reachability, not compatibility with this
Mac or access to the depots.

```bash
mkdir -p "$HOME/Tools/steamcmd" "$HOME/Games/CoD2-steam-macos" \
  "$HOME/Games/CoD2-steam-windows"
cd "$HOME/Tools/steamcmd"
curl --fail --location --output steamcmd_osx.tar.gz \
  https://steamcdn-a.akamaihd.net/client/installer/steamcmd_osx.tar.gz
tar -xzf steamcmd_osx.tar.gz
# Inspect the downloaded tool before trying to execute it.
find . -type f -name steamcmd -exec file {} \;
```

This project's Apple Silicon machine has no Rosetta and cannot execute 32-bit
x86 programs. Downloading a macOS archive does **not** establish that its
SteamCMD executable runs natively on arm64. If `file` reports an unsupported
architecture, use a compatible machine to download your licensed content and
copy the data back. Do not install translation tools as part of the port.
The platform override selects **game content**, not SteamCMD's CPU architecture.

On a Mac that can execute SteamCMD, replace `YOUR_STEAM_LOGIN` below. Supply the
password and Steam Guard code only at the interactive prompts; do not put them
in command arguments, scripts or logs. Use separate directories for each
platform so `validate` cannot replace the other platform's files.

```bash
cd "$HOME/Tools/steamcmd"
./steamcmd.sh +@sSteamCmdForcePlatformType macos \
  +force_install_dir "$HOME/Games/CoD2-steam-macos" \
  +login YOUR_STEAM_LOGIN +app_update 2630 validate +quit

./steamcmd.sh +@sSteamCmdForcePlatformType windows \
  +force_install_dir "$HOME/Games/CoD2-steam-windows" \
  +login YOUR_STEAM_LOGIN +app_update 2630 validate +quit
```

Use the [Valve SteamCMD reference](https://developer.valvesoftware.com/wiki/SteamCMD)
for installation and cross-platform download syntax. If the account is not
entitled, resolve the license with Steam; anonymous login is not a substitute.
If the installation is not English, select English for the app in Steam before
downloading or inspect the language/depot configuration in SteamCMD. Do not
rename another language's archives to make them appear English.

## Depot map and provenance

The workstream brief supplies this depot map; it has not been revalidated by
an authenticated `app_info_print` or an actual download in this workstream:

| Platform | Depots | English content depot |
| --- | --- | --- |
| macOS | 231931, 231932, 231933 | 231933 |
| Windows | 2631, 2632, 2633, 2634 | 2634 |

The other listed depots cover the platform's remaining executable/base content;
this guide does not assign individual files to them without their manifests.
`app_update 2630` selects the depots allowed for the account, chosen platform
and language. The override does not request every language depot. In an
interactive authenticated SteamCMD session, `app_info_update 1` followed by
`app_info_print 2630` lets the user inspect the current platform/language rules.
Keep the resulting install manifest (normally `appmanifest_2630.acf`) privately
alongside a SHA-256 inventory to identify the precise content used in tests.

opencod2 was reconstructed from the **Mac MP 1.3 engine**, so use the Mac content
as the first native-port reference and retain a separate Windows 1.3 install
for the real Windows CoD2x client and Linux reference server. This provenance
does not mean the native executable loads the old Mac application or its
libraries. The engine opens platform-neutral IWD archives: `FS_Startup("main")`
in `src/PC/universal/com_files.c` enumerates `.iwd` files, processes language
prefixes, and requires `default_mp.cfg` inside the search paths.

Windows CoD2 1.3 IWD data is a useful compatibility candidate for this loader,
and the CoD2x README explicitly uses it. However, no comparison of owned Mac
and Windows depots was possible here. Do not assume that their entire contents
or IWD checksums match, mix the two installations to fill gaps, or interpret a
successful directory scan as successful map loading. Pure-server IWD checks
still apply. Preserve platform/version/language provenance with every run.

## Directory layout and first checks

`fs_basepath` must point to the directory **containing `main/`**:

```text
CoD2-native-data/                 <-- fs_basepath
  main/
    iw_*.iwd                     stock/base content, including patch content
    localized_english_*.iwd      matching English assets, when using English
```

Preserve the archive names, case and content; do not unpack and repack IWDs.
The CoD2x reference README lists `iw_01.iwd` through `iw_15.iwd` and
`localized_english_iw00.iwd` through `localized_english_iw11.iwd` for its Windows
1.3 developer setup. Treat that as a Windows reference inventory, not a proven
manifest for every macOS depot/language.

The exact macOS Steam layout has not been downloaded here. Locate `main` in
the install instead of guessing an application-bundle path:

```bash
find "$HOME/Games/CoD2-steam-macos" -type d -iname main -print
```

If `main` is already directly inside `CoD2-steam-macos`, use that parent as
`fs_basepath`. If it is nested inside a bundle, copy the complete `main` folder
to a separate data root, setting `COD2_SOURCE_MAIN` to the path found above:

```bash
COD2_SOURCE_MAIN='/absolute/path/to/the/downloaded/main'
mkdir -p "$HOME/Games/CoD2-native-data/main"
rsync -a "$COD2_SOURCE_MAIN/" "$HOME/Games/CoD2-native-data/main/"
```

Keep unmodified source downloads for comparison. Check that base and localized
IWDs exist and capture their identity before running:

```bash
COD2_DATA="$HOME/Games/CoD2-native-data"
find "$COD2_DATA/main" -maxdepth 1 -type f -name '*.iwd' -print
(cd "$COD2_DATA" && find main -type f -name '*.iwd' \
  -exec shasum -a 256 {} \;) > "$HOME/Games/cod2-native-iwd.sha256"
tools/macos/run.sh "$COD2_DATA" +set loc_language 0
```

`loc_language 0` selects English in the source; its matching localized content
must be present. Set a writable `fs_homepath` separately when isolating test
configs/logs, and use the same clean home-directory contents for parity runs.
No old Mac application executable, Windows DLL, or SteamCMD binary is needed
in the native build tree. A missing `default_mp.cfg` error means the filesystem
could not find required game content; check the parent directory, archive
inventory and selected language before diagnosing the renderer.

Native boot, actual depot availability, Mac/Windows archive equality and
successful pure-server connections remain unverified until licensed data and
a working native engine are available.
