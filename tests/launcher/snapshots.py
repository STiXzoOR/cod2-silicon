#!/usr/bin/env python3
"""Render every launcher screen through the review harness and sanity-check the images.

Needs a login session with a window server (a locked screen is fine). Uses only the
original fallback art and fake documentation-range servers; no game data is read.
"""
import pathlib
import struct
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / "output/ws28/unit/snapshots"
build = root / "output/ws28/harness"
subprocess.run([str(root / "scripts/build-launcher.sh"), str(build), "--snapshots"], check=True)
harness = build / "LauncherSnapshots"


def run(target, *extra):
    target.mkdir(parents=True, exist_ok=True)
    # Python's own timeout: GNU timeout is not on a default non-interactive macOS PATH.
    subprocess.run([str(harness), str(target), *extra], check=True, timeout=300)


# The window server captures at the display's scale: 1440x900 points is 2880x1800 on a Retina display.
WINDOW = {(1440, 900), (2880, 1800)}


def size(png):
    data = png.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", png
    return struct.unpack(">II", data[16:24]), len(data)


run(out)
screens = ["home", "home-first", "servers", "servers-empty", "settings", "library-demos", "library-screenshots", "about",
           "setup-data", "setup-data-missing", "setup-key", "setup-shaders", "setup-ready"]
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
# The harness exits non-zero on any shape-audit finding, so reaching here means none.
print(f"PASS: {len(screens) * 2} launcher screens at 1440x900 points in dark and light, plus the material fallback, with bundled fonts"
      " and a clean shape audit (capsule buttons, concentric nesting, equal insets)")
