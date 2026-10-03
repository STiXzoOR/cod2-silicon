#!/usr/bin/env python3
"""Bundle an arm64 COD2_CODX-enabled client and register its cod2x URL scheme."""
import argparse
import pathlib
import plistlib
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("app", type=pathlib.Path, help="new output path ending in .app")
    parser.add_argument("--game-dir", type=pathlib.Path, required=True,
                        help="user's licensed CoD2 directory; data remains outside the bundle")
    args = parser.parse_args()
    if not args.executable.is_file():
        parser.error("executable does not exist")
    if args.app.suffix != ".app" or args.app.exists():
        parser.error("output must be a new .app path")
    if not (args.game_dir / "main").is_dir():
        parser.error("game directory must contain a main/ folder")
    if shutil.which("lipo") is None:
        parser.error("lipo is required (provided by Xcode); no packages were installed")
    subprocess.run(["lipo", "-verify_arch", "arm64", str(args.executable)], check=True)
    contents = args.app / "Contents"
    macos = contents / "MacOS"
    macos.mkdir(parents=True)
    binary = macos / "cod2_macos"
    shutil.copy2(args.executable, binary)
    binary.chmod(binary.stat().st_mode | 0o111)
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleExecutable": "cod2_macos",
        "CFBundleIdentifier": "org.opencod2.cod2x.native",
        "CFBundleName": "CoD2x Native",
        "CFBundleDisplayName": "CoD2x Native",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.4.6.8",
        "CFBundleVersion": "1",
        "LSMinimumSystemVersion": "13.0",
        "NSHighResolutionCapable": True,
        "CoD2GameDirectory": str(args.game_dir.resolve()),
        "CFBundleURLTypes": [{
            "CFBundleURLName": "CoD2x server link",
            "CFBundleTypeRole": "Viewer",
            "CFBundleURLSchemes": ["cod2x"],
        }],
    }
    with (contents / "Info.plist").open("wb") as file:
        plistlib.dump(info, file)
    print(f"Created {args.app}. Launch it once to register cod2x:// links.")
    print("Build with COD2_X64=ON and COD2_FEATURE_CFLAGS=-DCOD2_CODX=1; game data stays outside the bundle.")


if __name__ == "__main__":
    main()
