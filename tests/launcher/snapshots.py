#!/usr/bin/env python3
"""Render every launcher screen through the review harness and sanity-check the images.

Needs a login session whose window server supplies rendered window pixels (macOS 27
can return transparent captures while locked). Uses only the original fallback art
and fake documentation-range servers; no game data is read.
"""
import json
import pathlib
import plistlib
import shutil
import struct
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / "output/ws28/unit/snapshots"
build = out.parent / "snapshot-harness"
subprocess.run([str(root / "scripts/build-launcher.sh"), str(build), "--snapshots"], check=True)
# A normal bundle gives the review host a stable application identity for native
# window activation and external interaction with AppKit's remote Save panel.
app = build / "LauncherSnapshots.app"
macos = app / "Contents/MacOS"
resources = app / "Contents/Resources"
macos.mkdir(parents=True, exist_ok=True)
resources.mkdir(parents=True, exist_ok=True)
shutil.copy2(build / "LauncherSnapshots", macos / "LauncherSnapshots")
for name in ["Fonts", "Assets.car", "ConfigCatalog.json", "ConfigRulesets.json", "ConfigOptimizer.json"]:
    source = build / name
    if source.is_dir():
        shutil.copytree(source, resources / name, dirs_exist_ok=True)
    elif source.exists():
        shutil.copy2(source, resources / name)
info = plistlib.loads((build / "Info.plist").read_bytes())
info.update(CFBundleExecutable="LauncherSnapshots", CFBundleName="CoD2 Launcher Review", CFBundlePackageType="APPL")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
harness = macos / "LauncherSnapshots"


def run(target, *extra):
    target.mkdir(parents=True, exist_ok=True)
    # Python's own timeout: GNU timeout is not on a default non-interactive macOS PATH.
    subprocess.run([str(harness), str(target), *extra], check=True, timeout=300)


# The window server captures at the display's scale: 1440x900 points is 2880x1800 on a Retina display.
WINDOW = {(1440, 900), (2880, 1800)}
# The launcher's minimum window, 1100x760 points.
MINIMUM = {(1100, 760), (2200, 1520)}


def size(png):
    data = png.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", png
    return struct.unpack(">II", data[16:24]), len(data)


run(out)
configs = ["configs-guided", "configs-guided-aim", "configs-expert", "configs-text", "configs-history", "configs-running",
           "discover-unavailable", "discover-players", "discover-saved", "discover-player", "configs-import",
           "configs-import-everything", "servers-ready", "servers-ready-fixed",
           "configs-binds-list", "configs-binds-advanced", "configs-binds-visual", "configs-checkup",
           "configs-guided-rules", "configs-expert-rules", "configs-binds-verify", "configs-binds-learn", "configs-expert-player",
           # Check-up with summaries measured, turned off and unreadable (the plain screen above has none yet).
           "configs-checkup-measured", "configs-checkup-off", "configs-checkup-error",
           # Import from a link or a Windows profile, the review while it reads, a Saved bookmark the
           # signed index dropped, and a save that failed. Synthetic data in a private home only.
           "configs-import-link", "configs-import-windows", "configs-import-loading", "discover-saved-removed", "configs-save-failure",
           # The Windows profile picker over synthetic candidates (no drive is scanned), and the
           # Settings page held while its catalog loads.
           "configs-import-windows-profiles", "configs-catalog-loading"]
screens = ["home", "home-first", "servers", "servers-empty", "settings", "library-demos", "library-screenshots", "about",
           "setup-data", "setup-data-missing", "setup-key", "setup-shaders", "setup-ready"] + configs
for screen in screens:
    shots = {}
    for appearance in ["dark", "light"]:
        png = out / f"{screen}-{appearance}.png"
        (width, height), length = size(png)
        assert (width, height) in WINDOW, (png, width, height)
        assert length > 60_000, f"{png} looks blank ({length} bytes)"
        shots[appearance] = png.read_bytes()
    assert shots["dark"] != shots["light"], screen

fallback = out / "fallback"
run(fallback, "--fallback", "--only", "servers")
for appearance in ["dark", "light"]:
    (width, height), length = size(fallback / f"servers-{appearance}.png")
    assert (width, height) in WINDOW and length > 60_000
    assert (fallback / f"servers-{appearance}.png").read_bytes() != (out / f"servers-{appearance}.png").read_bytes()
run(fallback, "--fallback", "--only", "configs-guided")

# Config Studio under Increase Contrast and Reduce Transparency (set for the capture only).
# Increase Contrast changes every screen's palette. Reduce Transparency swaps glass for
# opaque fills, so only screens whose shape audit lists glass must change; a screen of
# opaque panels (Guided, the read-only banner) can capture identically.
audit = {entry["screen"]: entry["items"] for entry in json.loads((out / "shape-audit.json").read_text())}
# Behind the import sheet the window shows no translucent surface of its own, and the sheet's
# material belongs to the system, so Reduce Transparency can leave those captures unchanged.
sheets = {"configs-import", "configs-import-everything", "configs-import-link", "configs-import-windows", "configs-import-loading",
          "configs-import-windows-profiles"}
for variant in ["increase-contrast", "reduce-transparency"]:
    folder = out / variant
    run(folder, "--group", "configs", f"--{variant}")
    for screen in configs:
        for appearance in ["dark", "light"]:
            png = folder / f"{screen}-{appearance}.png"
            (width, height), length = size(png)
            assert (width, height) in WINDOW and length > 60_000, png
            if variant == "reduce-transparency" and screen in sheets:
                continue
            glass = any("glass" in item["name"] for item in audit[f"{screen}-{appearance}"])
            if variant == "increase-contrast" or glass:
                assert png.read_bytes() != (out / f"{screen}-{appearance}.png").read_bytes(), f"{png} matches the standard capture"

# Config Studio at the launcher's 1100x760 minimum: actual window images, both appearances, with the
# same shape audit (the harness fails on a finding).
minimum = out / "minimum"
run(minimum, "--group", "configs", "--minimum")
for screen in configs:
    shots = {}
    for appearance in ["dark", "light"]:
        png = minimum / f"{screen}-{appearance}.png"
        (width, height), length = size(png)
        assert (width, height) in MINIMUM, (png, width, height)
        assert length > 40_000, f"{png} looks blank ({length} bytes)"
        shots[appearance] = png.read_bytes()
    assert shots["dark"] != shots["light"], f"minimum {screen}"

# Reduce Motion, set in the view environment for the capture only (never the Mac's setting).
# Stills can match the standard captures; each must still be a whole window.
motion = out / "reduce-motion"
run(motion, "--group", "configs", "--reduce-motion")
for screen in configs:
    for appearance in ["dark", "light"]:
        (width, height), length = size(motion / f"{screen}-{appearance}.png")
        assert (width, height) in WINDOW and length > 60_000, (screen, appearance, width, height, length)

# Real clicks on Config Studio controls (including a Binds key capture and Check-up Apply and
# Undo) write exact bytes and History rows in a temporary home.
subprocess.run([str(harness), str(out / "actions"), "--actions"], check=True, timeout=300)

# Every Config Studio control has a name in the accessibility tree VoiceOver reads; the trees
# are written in reading order for review.
subprocess.run([str(harness), str(out), "--accessibility"], check=True, timeout=300)
for screen in configs:
    assert (out / "accessibility" / f"{screen}.txt").stat().st_size > 1000, screen

# Sheets, popovers and alerts, opened by real clicks and captured over their window.
overlays = ["configs-jump", "configs-new-loadout", "configs-expert-note", "configs-binds-keyboard", "configs-binds-pointer", "configs-binds-confirm",
            "configs-import-confirm"]
subprocess.run([str(harness), str(out / "overlays"), "--overlays"], check=True, timeout=300)
for overlay in overlays:
    for appearance in ["dark", "light"]:
        (width, height), length = size(out / "overlays" / f"overlay-{overlay}-{appearance}.png")
        assert width >= 1440 and height >= 900 and length > 60_000, (overlay, appearance, width, height, length)

# The harness exits non-zero on any shape-audit finding, so reaching here means none.
print(f"PASS: {len(screens) * 2} launcher screens at 1440x900 points in dark and light, plus the material fallback, with bundled fonts"
      " and a clean shape audit (capsule buttons, concentric nesting, equal insets); Config Studio under Increase Contrast,"
      f" Reduce Transparency and Reduce Motion, and {len(configs) * 2} captures at the 1100x760 minimum; Config Studio clicks"
      " write exact bytes; every Config Studio control is named for VoiceOver;"
      f" {len(overlays) * 2} sheet, popover and alert captures")
