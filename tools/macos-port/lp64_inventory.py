#!/usr/bin/env python3
"""Summarize Clang diagnostics and lexical LP64 review candidates, without compiling.

Run from the checkout root. Diagnostic identity is location + severity + flag +
message; identical header warnings and client/dedicated repeats count only once.
The lexical scan deliberately includes inactive preprocessor branches. It is a
review queue, not a bug detector or a substitute for inspecting native types.
"""
import argparse
import collections
import csv
import json
from pathlib import Path
import re

DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+): (warning|error|fatal error): (.*?)(?: \[(-W[^]]+)\])?$")
FOCUS = {
    "-Wshorten-64-to-32": "narrowing",
    "-Wpointer-to-int-cast": "pointer-to-integer",
    "-Wvoid-pointer-to-int-cast": "pointer-to-integer",
    "-Wint-to-pointer-cast": "integer-to-pointer",
    "-Wint-to-void-pointer-cast": "integer-to-pointer",
    "-Wint-conversion": "implicit-pointer/integer",
    "-Wincompatible-pointer-types": "pointer-type-mismatch",
    "-Wpointer-integer-compare": "pointer/integer-comparison",
    "-Wformat": "format/varargs",
    "-Wbuiltin-memcpy-chk-size": "constant-size-overflow",
    "-Wincompatible-library-redeclaration": "library-ABI",
}
# One record per source line/category. Expressions can match more than one category.
# All expressions are lexical: casts of scalars, valid file formats, comments and
# inactive branches can match, and macro-expanded/multiline constructs can escape.
SCANS = {
    "fixed-byte-access": re.compile(r"\*\s*\([^;]*?\*\s*\)\s*\([^;]*(?:\+|-)\s*(?:0x[0-9a-fA-F]+|[1-9][0-9]*)\b"),
    "constant-pointer-arithmetic": re.compile(r"\b[A-Za-z_]\w*(?:->\w+|\.\w+)?\s*\+\s*(?:0x[0-9a-fA-F]+|[1-9][0-9]*)\b"),
    "small-integer-cast": re.compile(r"\(\s*(?:unsigned\s+int|signed\s+int|int|unsigned|uint32_t|int32_t)\s*\)"),
    "fixed-size-memory-call": re.compile(r"\b(?:memcpy|memmove|memset|Com_Memcpy|Com_Memset|qsort|malloc|calloc|realloc|Hunk_AllocInternal|Z_MallocInternal)\s*\([^;]*(?:,\s*|\(\s*)(?:0x[0-9a-fA-F]+|[1-9][0-9]*)\s*\)"),
    "pointer-named-integer-field": re.compile(r"\b(?:unsigned\s+int|signed\s+int|int|unsigned|uint32_t|int32_t)\s+\w*(?:[Pp]tr|[Pp]ointer|[Aa]ddress|[Vv]ptr|[Hh]andle|[Nn]ext|[Pp]rev|[Hh]ead|[Cc]odePos)\w*\s*(?:;|\[|=)"),
    "x86-only-width-guard": re.compile(r"^\s*#\s*(?:if|elif).*defined\s*\(\s*(?:__x86_64__|_M_X64)\s*\)"),
    "integer-cast-hash-or-key": re.compile(r"(?:[Hh]ash|[Kk]ey).*\(\s*(?:int|unsigned(?:\s+int)?|u?intptr_t)\s*\)|\(\s*(?:int|unsigned(?:\s+int)?|u?intptr_t)\s*\).*(?:[Hh]ash|[Kk]ey)"),
}


def subsystem(name):
    bits = name.split("/")
    if len(bits) > 3 and bits[1] in ("PC", "Mac"):
        return "/".join(bits[:3])
    return "/".join(bits[:-1])


def table(rows, headings):
    return ["| " + " | ".join(map(str, headings)) + " |",
            "| " + " | ".join("---" for _ in headings) + " |"] + [
                "| " + " | ".join(str(v).replace("|", "\\|") for v in row) + " |" for row in rows]


def tsv(path, headings, rows):
    with path.open("w", newline="") as out:
        writer = csv.writer(out, delimiter="\t", lineterminator="\n")
        writer.writerow(headings)
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path, help="logs or directories of per-TU .log files")
    parser.add_argument("--output", type=Path, default=Path("docs/macos-port/lp64-logs"))
    parser.add_argument("--inventory", type=Path, default=Path("docs/macos-port/lp64-inventory.md"))
    parser.add_argument("--commands", type=Path, default=Path("build-macos/compile_commands.json"))
    args = parser.parse_args()
    root = Path.cwd()
    files = sorted({p for item in args.logs for p in (item.rglob("*.log") if item.is_dir() else [item])})
    if not files:
        parser.error("no diagnostic logs found")
    unique = collections.Counter()
    for logfile in files:
        for line in logfile.read_text(errors="replace").splitlines():
            match = DIAGNOSTIC.match(line)
            if match:
                name, row, col, severity, message, flag = match.groups()
                name = name.removeprefix(str(root) + "/")
                unique[(name, int(row), int(col), severity, flag or "unclassified", message)] += 1
    warnings = [d for d in unique if d[3] == "warning"]
    errors = [d for d in unique if d[3] != "warning"]
    focus = [d for d in warnings if d[4] in FOCUS]
    compiled = set()
    if args.commands.exists():
        compiled = {str(Path(c["file"])).removeprefix(str(root) + "/") for c in json.loads(args.commands.read_text())}
    candidates = []
    scanned = 0
    for path in sorted((root / "src").rglob("*")):
        if path.suffix not in (".c", ".h") or "blobs" in path.parts:
            continue
        scanned += 1
        name = str(path.relative_to(root))
        for row, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            for category, regex in SCANS.items():
                if regex.search(line):
                    candidates.append((name, row, category, "listed-TU" if name in compiled else "header-or-unlisted", line.strip()))
    args.output.mkdir(parents=True, exist_ok=True)
    tsv(args.output / "diagnostics.tsv", ["file", "line", "column", "severity", "flag", "message", "occurrences"],
        [(*d, unique[d]) for d in sorted(unique)])
    tsv(args.output / "scan-candidates.tsv", ["file", "line", "category", "compile-database", "source"], candidates)
    flag_counts = collections.Counter(d[4] for d in warnings)
    group_counts = collections.Counter(subsystem(d[0]) for d in warnings)
    focus_counts = collections.Counter(subsystem(d[0]) for d in focus)
    candidate_groups = collections.Counter(subsystem(c[0]) for c in candidates)
    categories = collections.Counter(FOCUS[d[4]] for d in focus)
    by_category = collections.Counter((subsystem(d[0]), FOCUS[d[4]]) for d in focus)
    category_names = sorted(categories)
    tsv(args.output / "counts-by-category.tsv", ["directory", *category_names],
        [(g, *(by_category[g, category] for category in category_names)) for g in sorted(focus_counts)])
    worst = collections.Counter(d[0] for d in focus)
    summary = ["<!-- BEGIN GENERATED LP64 COUNTS -->", "## Measured diagnostic and scan counts", "",
        f"Input: {len(files)} log files, {sum(p.stat().st_size for p in files):,} bytes. "
        f"{sum(unique.values()):,} diagnostic occurrences reduce to {len(warnings):,} unique warnings and "
        f"{len(errors):,} unique compile errors. {len(focus):,} warnings fall in the LP64/ABI review categories below.", "",
        "Identity is `(file, line, column, severity, flag, message)`: repeated headers/targets are deduplicated; "
        "distinct messages at one location remain distinct. Counts measure diagnostics, not confirmed defects. "
        "Narrowing includes bounded lengths and indexes; pointer casts include intentional integer IDs. "
        "Compile errors must reach zero before treating warning coverage as complete.", "", "### By directory", ""]
    summary += table([(g, group_counts[g], focus_counts[g], candidate_groups[g]) for g in sorted(set(group_counts) | set(candidate_groups))],
                     ["Directory", "All unique warnings", "LP64/ABI subset", "Lexical candidates"])
    summary += ["", "### LP64/ABI categories", ""] + table(categories.most_common(), ["Category", "Unique warnings"])
    summary += ["", "### All warning flags", ""] + table(flag_counts.most_common(), ["Flag", "Unique warnings"])
    summary += ["", "### Worst files by LP64/ABI warning count", ""] + table(worst.most_common(25), ["File", "Unique warnings"])
    summary += ["", "### Lexical scan categories", "",
        f"Scanned {scanned:,} `.c`/`.h` files under `src`, excluding `src/blobs`; found {len(candidates):,} line/category candidates. "
        "The scan is intentionally not preprocessed and includes inactive `#else`, optional features, historical headers "
        "and unused sources. A line can appear in multiple categories. `listed-TU` means only that CMake lists the file, "
        "not that the matching branch executes. These are review candidates, not confirmed bugs.", ""]
    summary += table(collections.Counter(c[2] for c in candidates).most_common(), ["Heuristic category", "Line/category matches"])
    summary += ["", "Full locations/messages: [diagnostics.tsv](lp64-logs/diagnostics.tsv). "
        "Full lexical queue: [scan-candidates.tsv](lp64-logs/scan-candidates.tsv).", "<!-- END GENERATED LP64 COUNTS -->"]
    generated = "\n".join(summary) + "\n"
    (args.output / "summary.md").write_text(generated.replace("(lp64-logs/", "("))
    if args.inventory.exists():
        old = args.inventory.read_text()
        block = r"<!-- BEGIN GENERATED LP64 COUNTS -->.*?<!-- END GENERATED LP64 COUNTS -->\n?"
        if re.search(block, old, flags=re.S):
            args.inventory.write_text(re.sub(block, lambda _: generated, old, flags=re.S))
    print(f"{len(warnings)} unique warnings; {len(focus)} LP64/ABI; {len(errors)} errors; {len(candidates)} lexical candidates")


if __name__ == "__main__":
    main()
