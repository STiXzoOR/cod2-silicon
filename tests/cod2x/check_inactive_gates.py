#!/usr/bin/env python3
"""Check CoD2x changes disappear with either gate inactive (not a binary ABI test)."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("legacy_guard", root / "tools/macos-port/check_legacy_guards.py")
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    paths = subprocess.check_output(["git", "diff", "--diff-filter=M", "--name-only", args.base], cwd=root, text=True).splitlines()
    paths = [p for p in paths if p.endswith((".c", ".h", ".m"))]
    configs = {}
    for name, defines in guard.CONFIGURATIONS.items():
        if not any(value.startswith("COD2_X64=") for value in defines):
            configs[name + "-codx-on-x64-zero"] = defines + ["COD2_X64=0", "COD2_CODX=1"]
            configs[name + "-codx-on-x64-undefined"] = defines + ["COD2_CODX=1"]
        configs[name + "-codx-zero"] = defines + ["COD2_CODX=0"]
    configs["arm64-codx-off"] = ["__GNUC__=4", "__APPLE__=1", "__aarch64__=1", "__arm64__=1", "COD2_X64=1", "COD2_CODX=0"]
    failures = []
    for path in paths:
        original = subprocess.check_output(["git", "show", args.base + ":" + path], cwd=root, text=True)
        current = (root / path).read_text()
        original = re.sub(r"^\s*#\s*import\b[^\n]*", "", original, flags=re.M)
        current = re.sub(r"^\s*#\s*import\b[^\n]*", "", current, flags=re.M)
        for name, defines in configs.items():
            if guard.body(original, defines) != guard.body(current, defines):
                failures.append((path, name))
    print(f"{len(paths)} files; {len(configs)} inactive CoD2x configurations; {len(failures)} mismatches")
    for path, name in failures:
        print(path, name)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
