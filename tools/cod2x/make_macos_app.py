#!/usr/bin/env python3
"""Bundle an arm64 COD2_CODX-enabled client and register its cod2x URL scheme."""
import argparse
import pathlib
import plistlib
import shutil
import subprocess
import re
import tempfile
import os


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("app", type=pathlib.Path, help="new output path ending in .app")
    parser.add_argument("--game-dir", type=pathlib.Path, default=None,
                        help="optional developer override; omitted in release bundles")
    parser.add_argument("--frameworks", type=pathlib.Path, help="pinned SDL install prefix for a self-contained bundle")
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--resolution", default="1920x1080")
    parser.add_argument("--borderless", action="store_true", help="desktop fullscreen instead of an explicit display mode")
    parser.add_argument("--no-game-mode", action="store_true", help="disable eligibility for a controlled comparison")
    parser.add_argument("--bundle-id", default="io.github.stixzoor.cod2silicon")
    parser.add_argument("--replace", action="store_true", help="replace an existing bundle with the same ID after verification")
    args = parser.parse_args()
    if not args.executable.is_file():
        parser.error("executable does not exist")
    if args.app.suffix != ".app":
        parser.error("output must end in .app")
    if args.app.exists():
        if not args.replace:
            parser.error("output exists; use --replace to rebuild it")
        with (args.app / "Contents/Info.plist").open("rb") as file:
            if plistlib.load(file).get("CFBundleIdentifier") != args.bundle_id:
                parser.error("refusing to replace a different application")
    if args.resolution not in {"1920x1080", "2560x1440", "3008x1692", "3840x2160", "5120x2880", "6016x3384"}:
        parser.error("resolution must be one of the measured 1080p through 6K modes")
    game_dir = str(args.game_dir.resolve()) if args.game_dir else None
    if game_dir and any(c in game_dir for c in '\";+\r\n'):
        parser.error("game directory cannot contain quotes, semicolons, plus signs, or newlines")
    if not re.fullmatch(r"[A-Za-z0-9.-]+", args.bundle_id):
        parser.error("bundle ID must contain only letters, numbers, dots, and hyphens")
    if shutil.which("lipo") is None:
        parser.error("lipo is required (provided by Xcode); no packages were installed")
    subprocess.run(["lipo", str(args.executable), "-verify_arch", "arm64"], check=True)
    symbols = subprocess.check_output(["nm", "-g", str(args.executable)], text=True)
    if "_Cod2xNativeApp_Arguments" not in symbols:
        parser.error("client needs COD2_CODX=1 and bundle argument support; rebuild the native target")
    root = pathlib.Path(__file__).resolve().parents[2]
    destination = args.app.expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = tempfile.TemporaryDirectory(prefix=".cod2x-build-", dir=destination.parent)
    app = pathlib.Path(staging.name) / destination.name
    contents = app / "Contents"
    macos = contents / "MacOS"
    macos.mkdir(parents=True)
    binary = macos / "cod2_macos"
    shutil.copy2(args.executable, binary)
    binary.chmod(binary.stat().st_mode | 0o111)
    resources = contents / "Resources"
    resources.mkdir(parents=True)
    if args.frameworks:
        frameworks = contents / "Frameworks"
        frameworks.mkdir()
        for source in sorted((args.frameworks / "lib").glob("libSDL*.dylib")):
            target = frameworks / source.name
            if source.is_symlink():
                target.symlink_to(os.readlink(source))
            else:
                shutil.copy2(source, target)
        if not (frameworks / "libSDL3.dylib").is_file() or not (frameworks / "libSDL2.dylib").is_file():
            parser.error("SDL install prefix must contain SDL3 and sdl2-compat dylibs")
        shutil.copytree(args.frameworks / "licenses", resources / "licenses")
        machos = [binary] + sorted(p for p in frameworks.glob("*.dylib") if not p.is_symlink())
        bundle_rpath = "@executable_path/../Frameworks"
        for macho in machos:
            # Edit signed Mach-O files directly and re-sign afterwards. Stripping the
            # signature first leaves a __LINKEDIT gap that Xcode 16's install_name_tool
            # rejects, and no-op edits are skipped entirely.
            dependencies = subprocess.check_output(["otool", "-L", str(macho)], text=True).splitlines()[1:]
            for line in dependencies:
                dependency = line.strip().rsplit(" (", 1)[0]
                name = pathlib.PurePosixPath(dependency).name
                if name.startswith("libSDL") and (frameworks / name).is_file() and dependency != "@rpath/" + name:
                    subprocess.run(["install_name_tool", "-change", dependency, "@rpath/" + name, str(macho)], check=True)
            if macho != binary:
                current = subprocess.check_output(["otool", "-D", str(macho)], text=True).splitlines()[1:]
                if [line.strip() for line in current] != ["@rpath/" + macho.name]:
                    subprocess.run(["install_name_tool", "-id", "@rpath/" + macho.name, str(macho)], check=True)
            commands = subprocess.check_output(["otool", "-l", str(macho)], text=True)
            rpaths = re.findall(r"cmd LC_RPATH\n.*?path (.*?) \(offset", commands, re.DOTALL)
            for rpath in rpaths:
                if rpath != bundle_rpath:
                    subprocess.run(["install_name_tool", "-delete_rpath", rpath, str(macho)], check=True)
            if bundle_rpath not in rpaths:
                subprocess.run(["install_name_tool", "-add_rpath", bundle_rpath, str(macho)], check=True)
        for macho in machos:
            dependencies = subprocess.check_output(["otool", "-L", str(macho)], text=True).splitlines()[1:]
            for line in dependencies:
                dependency = line.strip().rsplit(" (", 1)[0]
                if not dependency.startswith(("/System/", "/usr/lib/", "@rpath/", "@executable_path/")):
                    parser.error(f"non-portable dependency in {macho.name}: {dependency}")
            build = subprocess.check_output(["vtool", "-show-build", str(macho)], text=True)
            if not re.search(r"minos 13\.0(?:\.0)?\s", build):
                parser.error(f"{macho.name} must be built for macOS 13.0")
        for dylib in machos[1:]:
            subprocess.run(["codesign", "--force", "--sign", "-", str(dylib)], check=True)
    iconset = pathlib.Path(staging.name) / "Native.iconset"
    subprocess.run(["swift", str(root / "tools/cod2x/app_icon.swift"), str(iconset)], check=True)
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(resources / "Native.icns")], check=True)
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleExecutable": "cod2_macos",
        "CFBundleIdentifier": args.bundle_id,
        "CFBundleName": "CoD2 Silicon",
        "CFBundleDisplayName": "CoD2 Silicon",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": args.version,
        "CFBundleVersion": "1",
        "CFBundleIconFile": "Native.icns",
        "LSMinimumSystemVersion": "13.0",
        "NSHighResolutionCapable": False,
        "LSApplicationCategoryType": "public.app-category.action-games",
        "LSSupportsGameMode": not args.no_game_mode,
        "CoD2AutomaticShaderSetup": True,
        "CoD2LaunchArguments": f'+set r_mode {args.resolution} '
            f'+set r_fullscreen 1 +set r_borderless {int(args.borderless)} +set com_maxfps 333 '
            '+set r_swapInterval 0 +set in_rawmouse 1 +set m_filter 0 +set cl_mouseAccel 0 '
            '+set logfile 0 +set developer 0 +set com_introPlayed 1',
        "CFBundleURLTypes": [{
            "CFBundleURLName": "CoD2x server link",
            "CFBundleTypeRole": "Viewer",
            "CFBundleURLSchemes": ["cod2x"],
        }],
    }
    if game_dir:
        info["CoD2GameDirectory"] = game_dir
    with (contents / "Info.plist").open("wb") as file:
        plistlib.dump(info, file)
    subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    if destination.exists():
        previous = pathlib.Path(staging.name) / "previous.app"
        os.replace(destination, previous)
        try:
            os.replace(app, destination)
        except OSError:
            os.replace(previous, destination)
            raise
    else:
        os.replace(app, destination)
    staging.cleanup()
    print(f"Created {destination}. Launch it once to register cod2x:// links.")
    print("Build with COD2_X64=ON and COD2_FEATURE_CFLAGS=-DCOD2_CODX=1; game data stays outside the bundle.")


if __name__ == "__main__":
    main()
