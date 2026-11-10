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

# "-" signs ad hoc. A real identity, such as "Developer ID Application: Name (TEAMID)",
# also gets the hardened runtime and a secure timestamp, which notarization requires.
SIGN_IDENTITY = os.environ.get("COD2_SIGN_IDENTITY", "-")


# Voice chat records from the microphone. The hardened runtime refuses that
# without this entitlement, whatever the player answers when macOS asks.
ENTITLEMENTS = {"com.apple.security.device.audio-input": True}


def codesign(path, entitlements=None):
    command = ["codesign", "--force", "--sign", SIGN_IDENTITY]
    if SIGN_IDENTITY != "-":
        command += ["--options", "runtime", "--timestamp"]
    if entitlements:
        command += ["--entitlements", str(entitlements)]
    subprocess.run(command + [str(path)], check=True)


def make_portable(parser, machos, executables, frameworks):
    """Point each Mach-O at the bundle's own Frameworks folder and check that nothing else is needed."""
    bundle_rpath = "@executable_path/../Frameworks"
    for macho in machos:
        # Edit signed Mach-O files directly and re-sign afterwards. Stripping the
        # signature first leaves a __LINKEDIT gap that Xcode 16's install_name_tool
        # rejects, and no-op edits are skipped entirely.
        dependencies = subprocess.check_output(["otool", "-L", str(macho)], text=True).splitlines()[1:]
        for line in dependencies:
            dependency = line.strip().rsplit(" (", 1)[0]
            name = pathlib.PurePosixPath(dependency).name
            if (frameworks / name).is_file() and not dependency.startswith(("/System/", "/usr/lib/")) \
                    and dependency != "@rpath/" + name:
                subprocess.run(["install_name_tool", "-change", dependency, "@rpath/" + name, str(macho)], check=True)
        if macho not in executables:
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
        if not re.search(r"minos 26\.0(?:\.0)?\s", build):
            parser.error(f"{macho.name} must be built for macOS 26.0")


def build_icon(root, output):
    """Compile CoD2 Silicon.icon, or draw the static fallback without Xcode's actool."""
    result = subprocess.run([str(root / "scripts/compile-launcher-icon.sh"), str(output)])
    if result.returncode == 0:
        with (output / "icon-info.plist").open("rb") as file:
            keys = plistlib.load(file)
        files = [output / "Assets.car", output / (keys["CFBundleIconFile"] + ".icns")]
        if set(keys) != {"CFBundleIconFile", "CFBundleIconName", "NSAccentColorName"} or not all(path.is_file() for path in files):
            raise SystemExit("actool did not produce the expected icon files")
        return files, keys
    if result.returncode != 3:
        raise SystemExit("actool failed to compile CoD2 Silicon.icon")
    print("Using the static icon fallback; install Xcode for the layered macOS 26 icon.")
    iconset = output / "CoD2 Silicon.iconset"
    subprocess.run(["swift", str(root / "tools/cod2x/app_icon.swift"), str(iconset)], check=True)
    icns = output / "CoD2 Silicon.icns"
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(icns)], check=True)
    return [icns], {"CFBundleIconFile": "CoD2 Silicon"}


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("app", type=pathlib.Path, help="new output path ending in .app")
    parser.add_argument("--launcher", type=pathlib.Path, help="prebuilt Swift 6 launcher; built locally if omitted")
    parser.add_argument("--engine-only", action="store_true", help="legacy engine-only bundle for native engine fixtures")
    parser.add_argument("--game-dir", type=pathlib.Path, default=None,
                        help="optional developer override; omitted in release bundles")
    parser.add_argument("--frameworks", type=pathlib.Path, help="pinned SDL install prefix for a self-contained bundle")
    parser.add_argument("--shadergen", type=pathlib.Path, help="cod2-shadergen executable to bundle")
    parser.add_argument("--vkd3d", type=pathlib.Path, help="libvkd3d-shader install prefix from scripts/build-vkd3d.sh")
    parser.add_argument("--version", default=(root / "RELEASE_VERSION").read_text().strip())
    parser.add_argument("--resolution", default="1920x1080")
    fullscreen = parser.add_mutually_exclusive_group()
    fullscreen.add_argument("--borderless", dest="borderless", action="store_true", default=True,
                            help="desktop fullscreen (default)")
    fullscreen.add_argument("--exclusive", dest="borderless", action="store_false",
                            help="select an explicit display mode")
    parser.add_argument("--no-game-mode", action="store_true", help="disable eligibility for a controlled comparison")
    parser.add_argument("--bundle-id", default="io.github.stixzoor.cod2silicon")
    parser.add_argument("--replace", action="store_true", help="replace an existing bundle with the same ID after verification")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version):
        parser.error("version must be N.N.N")
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
    destination = args.app.expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = tempfile.TemporaryDirectory(prefix=".cod2x-build-", dir=destination.parent)
    app = pathlib.Path(staging.name) / destination.name
    contents = app / "Contents"
    macos = contents / "MacOS"
    macos.mkdir(parents=True)
    game_contents = contents if args.engine_only else contents / "Helpers/CoD2 Game.app/Contents"
    game_macos = game_contents / "MacOS"
    game_macos.mkdir(parents=True, exist_ok=True)
    binary = game_macos / "cod2_macos"
    shutil.copy2(args.executable, binary)
    binary.chmod(binary.stat().st_mode | 0o111)
    resources = contents / "Resources"
    resources.mkdir(parents=True)
    if args.frameworks:
        frameworks = game_contents / "Frameworks"
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
        make_portable(parser, machos, {binary}, frameworks)
        for dylib in machos[1:]:
            codesign(dylib)
    if args.shadergen:
        # The shader generator and its HLSL compiler travel in the game app, where both
        # the game and the launcher find them. The compiler stays a separate library.
        if not args.vkd3d:
            parser.error("--shadergen needs --vkd3d, the libvkd3d-shader install prefix")
        frameworks = game_contents / "Frameworks"
        frameworks.mkdir(exist_ok=True)
        helpers = game_contents / "Helpers"
        helpers.mkdir(exist_ok=True)
        tool = helpers / "cod2-shadergen"
        library = frameworks / "libvkd3d-shader.1.dylib"
        shutil.copy2(args.shadergen, tool)
        shutil.copy2(args.vkd3d / "lib/libvkd3d-shader.1.dylib", library)
        (resources / "licenses").mkdir(exist_ok=True)
        for licence in sorted((args.vkd3d / "licenses").iterdir()):
            shutil.copy2(licence, resources / "licenses" / licence.name)
        make_portable(parser, [tool, library], {tool}, frameworks)
        codesign(library)
        codesign(tool)
    icon_files, icon_keys = build_icon(root, pathlib.Path(staging.name) / "icon")
    entitlements = pathlib.Path(staging.name) / "entitlements.plist"
    with entitlements.open("wb") as file:
        plistlib.dump(ENTITLEMENTS, file)
    for icon_file in icon_files:
        shutil.copy2(icon_file, resources / icon_file.name)
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleExecutable": "cod2_macos" if args.engine_only else "CoD2Launcher",
        "CFBundleIdentifier": args.bundle_id,
        "CFBundleName": "CoD2 Silicon",
        "CFBundleDisplayName": "CoD2 Silicon",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": args.version,
        "CFBundleVersion": "1",
        **icon_keys,
        "LSMinimumSystemVersion": "26.0",
        "NSHighResolutionCapable": not args.engine_only,
        # macOS shows this when voice chat first opens the microphone.
        "NSMicrophoneUsageDescription": "Voice chat sends your microphone to the players on your server while you hold the voice chat key.",
        "LSApplicationCategoryType": "public.app-category.action-games",
        "LSSupportsGameMode": not args.no_game_mode,
        "CoD2AutomaticShaderSetup": True,
        # No frame cap here: the engine defaults to 250 and the player's own setting (the launcher's,
        # or config_mp.cfg) decides.
        "CoD2LaunchArguments": f'+set r_mode {args.resolution} '
            f'+set r_fullscreen 1 +set r_borderless {int(args.borderless)} '
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
    if not args.engine_only:
        launcher = args.launcher
        if launcher is None:
            launcher_output = pathlib.Path(staging.name) / "launcher-build"
            subprocess.run([str(root / "scripts/build-launcher.sh"), str(launcher_output)], check=True)
            launcher = launcher_output / "CoD2Launcher"
        if not launcher.is_file():
            parser.error("Swift launcher executable does not exist")
        subprocess.run(["lipo", str(launcher), "-verify_arch", "arm64"], check=True)
        launcher_build = subprocess.check_output(["vtool", "-show-build", str(launcher)], text=True)
        if not re.search(r"minos 26\.0(?:\.0)?\s", launcher_build):
            parser.error("launcher must be built for macOS 26.0")
        for line in subprocess.check_output(["otool", "-L", str(launcher)], text=True).splitlines()[1:]:
            dependency = line.strip().rsplit(" (", 1)[0]
            if not dependency.startswith(("/System/", "/usr/lib/")):
                parser.error(f"non-system launcher dependency: {dependency}")
        shutil.copy2(launcher, macos / "CoD2Launcher")
        game_info = dict(info)
        game_info.update(CFBundleExecutable="cod2_macos", CFBundleIdentifier=args.bundle_id + ".game",
                         NSHighResolutionCapable=False, CoD2AutomaticShaderSetup=False, CoD2LaunchArguments="")
        game_info.pop("CFBundleURLTypes", None)
        game_info.pop("CoD2GameDirectory", None)
        game_resources = game_contents / "Resources"
        game_resources.mkdir()
        for icon_file in icon_files:
            shutil.copy2(icon_file, game_resources / icon_file.name)
        with (game_contents / "Info.plist").open("wb") as file:
            plistlib.dump(game_info, file)
        codesign(game_contents.parent, entitlements)
        info["LSSupportsGameMode"] = False
        info["CoD2AutomaticShaderSetup"] = False
        info["CoD2DefaultResolution"] = args.resolution
        info["CoD2DefaultFullscreen"] = "borderless" if args.borderless else "exclusive"
        # SIL OFL 1.1 typefaces travel with their licence texts; the launcher also
        # registers them at runtime when run outside a bundle.
        fonts = root / "launcher/Resources/Fonts"
        subprocess.run([str(root / "scripts/fetch-launcher-fonts.sh"), "--check"], check=True, stdout=subprocess.DEVNULL)
        shutil.copytree(fonts, resources / "Fonts", ignore=shutil.ignore_patterns(".*"))
        info["ATSApplicationFontsPath"] = "Fonts"
        for policy in ["ConfigCatalog.json", "ConfigRulesets.json", "ConfigOptimizer.json"]:
            shutil.copy2(root / "launcher/Resources" / policy, resources / policy)
        for name in ["LICENSE", "NOTICE.md", "CREDITS.md"]:
            shutil.copy2(root / name, resources / name)
    with (contents / "Info.plist").open("wb") as file:
        plistlib.dump(info, file)
    codesign(app, entitlements)
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
