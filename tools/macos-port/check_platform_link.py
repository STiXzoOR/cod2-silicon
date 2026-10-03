#!/usr/bin/env python3
"""Check target-specific Darwin link logs against the reviewed WS2 data inventory.

This never suppresses linker errors. New undefined names require review; it does
not guess that an unfamiliar function is engine data. The TSV inventories list
names only, and contain no reference binary contents or serialized game data.
"""
import argparse
from collections import Counter
from pathlib import Path
import re


def check(target, log_path):
    root = Path(__file__).resolve().parents[2]
    inventory = root / "docs/macos-port/reports" / ("WS3-undefined-" + target + ".tsv")
    reviewed = {}
    for line in inventory.read_text().splitlines()[1:]:
        category, symbol = line.split("\t")
        reviewed[symbol] = category
    log = log_path.read_text()
    names = set(re.findall(r'^  "([^"]+)", referenced from:', log, re.M))
    compile_errors = [line for line in log.splitlines()
                      if "error:" in line and "linker command failed" not in line]
    unknown = sorted(names - reviewed.keys())
    counts = Counter(reviewed[name] for name in names if name in reviewed)
    print(f"{target}: {len(names)} undefined, {len(unknown)} unreviewed, "
          f"{len(compile_errors)} compile errors; {dict(sorted(counts.items()))}")
    for line in compile_errors + unknown:
        print(line)
    if "Undefined symbols" not in log and "Built target cod2_macos" not in log:
        print("Log does not prove a linker attempt or completed build")
        return False
    return not unknown and not compile_errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--dedicated", type=Path, required=True)
    args = parser.parse_args()
    client_ok = check("client", args.client)
    dedicated_ok = check("dedicated", args.dedicated)
    return not (client_ok and dedicated_ok)


if __name__ == "__main__":
    raise SystemExit(main())
