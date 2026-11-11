#!/usr/bin/env python3
"""Render every launcher screen through the review harness and sanity-check the images.

Needs a login session whose window server supplies rendered window pixels (macOS 27
can return transparent captures while locked). Uses only the original fallback art
and fake documentation-range servers; no game data is read.
"""
import atexit
import os
import time
import json
import pathlib
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / "output/ws28/unit/snapshots"
build = out.parent / "snapshot-harness.noindex"
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
atexit.register(lambda: subprocess.run(["/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister", "-u", str(app)], check=False))


lease = root.parent / "_agents/bench.lock"
lease_owned = False


def release_lease():
    if lease_owned and lease.exists() and lease.read_text().startswith(f"{root.name} {os.getpid()} "):
        lease.unlink()


atexit.register(release_lease)


def take_lease():
    global lease_owned
    # Parallel worktrees share one desktop. Standalone checkouts have no agent bench.
    if not lease.parent.exists() or lease_owned:
        return
    while True:
        try:
            with lease.open("x") as stream:
                stream.write(f"{root.name} {os.getpid()} {time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}\n")
            lease_owned = True
            return
        except FileExistsError:
            owner = lease.read_text()
            try:
                os.kill(int(owner.split()[1]), 0)
            except ProcessLookupError:
                if lease.read_text() == owner:
                    lease.unlink()
                print(f"Reclaimed inactive window lease: {owner.strip()}", flush=True)
                continue
            print(f"Waiting for window lease: {owner.strip()}", flush=True)
            time.sleep(60)


def run(target, *extra):
    take_lease()
    target.mkdir(parents=True, exist_ok=True)
    # LaunchServices activates the real app; a shell-launched process may have no key window.
    # `open -W` masks app exit codes, so require the harness's own success result as well.
    for attempt in range(3):
        with tempfile.TemporaryDirectory(prefix="run-", dir=build) as temporary:
            folder = pathlib.Path(temporary)
            result, stdout, stderr = [folder / name for name in ["result", "stdout", "stderr"]]
            try:
                subprocess.run(["open", "-n", "-W", str(app), "--stdout", str(stdout), "--stderr", str(stderr),
                                "--args", str(target), *extra, "--result-file", str(result)], check=True, timeout=600)
            except subprocess.TimeoutExpired:
                if result.exists():
                    try:
                        os.kill(int(result.read_text().splitlines()[0]), 15)
                    except ProcessLookupError:
                        pass
                raise
            finally:
                logs = "".join(log.read_text() for log in [stdout, stderr] if log.exists())
                print(logs, end="", flush=True)
            if result.exists() and result.read_text().splitlines()[1] == "0":
                return
            # macOS can refuse activation even through LaunchServices. Retry only this
            # environmental failure; shape, accessibility and action failures still fail.
            if "did not become active and key" not in logs or attempt == 2:
                raise AssertionError(f"Review harness failed: {target} {extra}")
            print("Retrying the review app after macOS refused key-window activation", flush=True)


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
           "home-notice", "home-game-running", "setup-data", "setup-data-missing", "setup-folder-error", "setup-key", "setup-key-invalid", "setup-key-stored", "setup-key-replace", "setup-shaders-ready",
           "setup-shaders", "setup-ready", "setup-summary-ready", "setup-summary-approximate", "setup-summary-link", "setup-cancelable"] + configs
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

# Every page at the launcher's 1100x760 minimum: actual window images, both appearances, with the
# same shape audit (the harness fails on a finding).
minimum = out / "minimum"
run(minimum, "--minimum")
for screen in screens:
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

# Setup keyboard, stored keys, cancellation, inline validation and completion.
run(out / "shell-actions", "--shell-actions")
run(out / "shell-accessibility", "--shell-accessibility")
for variant in ["increase-contrast", "reduce-transparency", "reduce-motion"]:
    run(out / variant, "--only", "setup-summary-approximate", f"--{variant}")

# Real clicks on Config Studio controls (including a Binds key capture and Check-up Apply and
# Undo) write exact bytes and History rows in a temporary home.
run(out / "actions", "--actions")
# A Mouse speed keyboard step or typed value never lands on the loadout chosen before it saved, and
# Show Controls with a text draft asks to Apply, Discard or Keep Editing; each with a positive save.
run(out / "actions", "--actions", "--only", "review-controls")

# Every Config Studio control has a name in the accessibility tree VoiceOver reads; the trees
# are written in reading order for review.
run(out, "--accessibility")
for screen in configs:
    assert (out / "accessibility" / f"{screen}.txt").stat().st_size > 1000, screen

# Sheets, popovers and alerts, opened by real clicks and captured over their window.
overlays = ["configs-jump", "configs-new-loadout", "configs-expert-note", "configs-binds-keyboard", "configs-binds-pointer", "configs-binds-confirm",
            "configs-import-confirm"]
run(out / "overlays", "--overlays")
for overlay in overlays:
    for appearance in ["dark", "light"]:
        (width, height), length = size(out / "overlays" / f"overlay-{overlay}-{appearance}.png")
        assert width >= 1440 and height >= 900 and length > 60_000, (overlay, appearance, width, height, length)

# Expert's group rail as navigation in a real window: active and inactive, with the window's sidebar
# hidden and shown by its own toolbar button, and resized to the minimum and back.
navigation = out / "navigation"
run(navigation, "--navigation")
for step in ["active", "inactive", "sidebar-hidden", "sidebar-shown", "minimum", "minimum-sidebar-hidden", "minimum-sidebar-shown", "resized-back"]:
    for appearance in ["dark", "light"]:
        (width, height), length = size(navigation / f"navigation-{step}-{appearance}.png")
        assert (width, height) in (MINIMUM if step.startswith("minimum") else WINDOW) and length > 40_000, (step, appearance, width, height, length)

# The harness exits non-zero on any shape-audit finding, so reaching here means none.
print(f"PASS: {len(screens) * 2} launcher screens at 1440x900 points in dark and light, plus the material fallback, with bundled fonts"
      " and a clean shape audit (capsule buttons, concentric nesting, equal insets); Config Studio under Increase Contrast,"
      f" Reduce Transparency and Reduce Motion, and {len(screens) * 2} captures at the 1100x760 minimum; Config Studio clicks"
      " write exact bytes; every Config Studio control is named for VoiceOver;"
      f" {len(overlays) * 2} sheet, popover and alert captures")
