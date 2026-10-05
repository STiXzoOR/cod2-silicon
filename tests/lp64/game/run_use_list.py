#!/usr/bin/env python3
"""Check actual native use-list filtering, optionally against an older source."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, default=root / "build-macos")
parser.add_argument("--baseline")
args = parser.parse_args()
out = args.build.resolve() / "ws39-use-list"
out.mkdir(exist_ok=True)
source = "src/PC/game_mp/player_use_mp.c"
data = subprocess.check_output(["git", "show", args.baseline + ":" + source], cwd=root) if args.baseline else (root / source).read_bytes()
(out / "ws39_player_use.c").write_bytes(data)
entry = next(row for row in json.loads((args.build / "compile_commands.json").read_text()) if row["file"].endswith("/player_use_mp.c"))
flags = shlex.split(entry["command"])
flags = flags[:flags.index("-o")]
exe = out / "use-list"
with (out / "compile.log").open("w") as log:
    subprocess.run([*flags, "-UNDEBUG", "-fsanitize=address", "-ffunction-sections", "-fdata-sections", "-Wl,-dead_strip", "-I" + str(out), str(root / "tests/lp64/game/use_list.c"), "-o", str(exe)], cwd=root, stdout=log, stderr=log, check=True)
subprocess.run([str(exe)], check=True)
