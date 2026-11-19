#!/usr/bin/env python3
"""Write the platform-neutral engine source of this repository to another folder.

The engine source here also carries this project's own app: the launcher, the Mac
packaging, the Metal renderer path, and its names. A copy without those is what a
plain reconstruction of the game needs. This tool makes that copy, in one of two
forms:

    plain   no CoD2x layer at all (every COD2_CODX block and CoD2x file is left out)
    codx    with the CoD2x layer, still switched by COD2_CODX

It is one-way. Change the source here and export again; never edit the copy.

    tools/export/export.py DEST [--profile plain|codx]          write the copy
    tools/export/export.py DEST [--profile ...] --check         report drift, write nothing
    tools/export/export.py DEST [--profile ...] --prune         also remove exported files that are gone here

DEST may also come from the environment variable COD2_EXPORT_DIR. Files in DEST that
this tool does not manage are never read, changed or removed.

How a file is made neutral, in this order:

  1. paths.txt decides whether it is copied at all.
  2. In C, Objective-C and C++ files, the conditional blocks that depend on the
     symbols in symbols.txt are resolved (conditionals.py): COD2_ENGINE_ONLY counts
     as defined, so `#if !defined(COD2_ENGINE_ONLY)` marks code that is this
     project's own; the other symbols count as undefined. `#include` lines for
     files that are not copied go with them.
  3. In other text files, the lines between `# export-begin: app` (or `codx`) and
     `# export-end` are removed; the marker lines themselves always are. Lines
     written as `#| ...` after an `# export-else` take their place in the export.
  4. In JSON lists of records with a "file" key, records for files that are not
     copied are removed.
  5. rewrites.txt replaces this project's names with neutral ones.
  6. The result must not match any pattern in forbidden.txt, or the export fails.
"""
import argparse, fnmatch, json, os, pathlib, re, shutil, subprocess, sys

sys.dont_write_bytecode = True
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import conditionals  # noqa: E402

ROOT = HERE.parents[1]
C_FAMILY = (".c", ".h", ".inc", ".m", ".mm", ".cpp", ".cc", ".hpp")
DROPS = {"plain": ("app", "codx"), "codx": ("app",)}
MARK_BEGIN = re.compile(r"^\s*#\s*export-begin:\s*(\w+)\s*$")
MARK_END = re.compile(r"^\s*#\s*export-end\s*$")
MARK_ELSE = re.compile(r"^\s*#\s*export-else\s*$")
MARK_ALTERNATIVE = re.compile(r"^(\s*)#\| ?(.*)$", re.S)


def sections(name):
    """Read a list file: '[section]' headers, '#' comments, one entry per line."""
    out, current = {}, None
    for line in (HERE / name).read_text().splitlines():
        line = line.split(" #", 1)[0].rstrip() if not line.startswith("#") else ""
        if not line.strip():
            continue
        if line.startswith("[") and line.endswith("]"):
            current = out.setdefault(line[1:-1], [])
        elif current is not None:
            current.append(line)
    return out


def matches(path, pattern):
    if pattern.endswith("/"):
        return path.startswith(pattern)
    return fnmatch.fnmatchcase(path, pattern)


def select(profile):
    lists = sections("paths.txt")
    tracked = subprocess.check_output(["git", "-C", str(ROOT), "ls-files"], text=True).splitlines()
    chosen = []
    for f in tracked:
        if not any(matches(f, p) for p in lists["include"]):
            continue
        if any(matches(f, p) for p in lists["exclude"]):
            continue
        if profile == "plain" and any(matches(f, p) for p in lists["codx only"]):
            continue
        chosen.append(f)
    return tracked, chosen


def drop_marked(text, drops, path):
    """Apply the export markers of a text file.

        # export-begin: app      the lines up to export-else (or export-end) stay out of
        ...                      every export; "codx" ones stay out of the plain export
        # export-else
        #| ...                   kept, without the "#| ", only where the lines above are out
        # export-end
    """
    while True:
        out, frames, again = [], [], False
        for line in text.splitlines(keepends=True):
            begin = MARK_BEGIN.match(line)
            if begin:
                frames.append([begin.group(1), False])
            elif MARK_ELSE.match(line):
                if not frames:
                    sys.exit(f"{path}: export-else without export-begin")
                frames[-1][1] = True
            elif MARK_END.match(line):
                if not frames:
                    sys.exit(f"{path}: export-end without export-begin")
                frames.pop()
            elif all((name in drops) == in_else for name, in_else in frames):
                alternative = MARK_ALTERNATIVE.match(line)
                if alternative and frames:
                    line = alternative.group(1) + alternative.group(2)
                    again = again or bool(MARK_BEGIN.match(line))
                out.append(line)
        if frames:
            sys.exit(f"{path}: export-begin without export-end")
        text = "".join(out)
        if not again:
            return text


def without_comments(text):
    """The text with comment bodies blanked, line count unchanged."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0)) if match.group(0).startswith("/") else match.group(0)
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text, flags=re.S)


def filter_records(text, kept):
    try:
        records = json.loads(text)
    except ValueError:
        return text
    if not (isinstance(records, list) and records and all(isinstance(r, dict) and "file" in r for r in records)):
        return text
    left = [r for r in records if r["file"] in kept]
    return text if len(left) == len(records) else json.dumps(left, indent=2, ensure_ascii=False) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dest", nargs="?", default=os.environ.get("COD2_EXPORT_DIR"))
    parser.add_argument("--profile", choices=tuple(DROPS), default="plain")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--prune", action="store_true")
    parser.add_argument("--quiet", action="store_true", help="print the summary and the problems only")
    args = parser.parse_args()
    if not args.dest:
        parser.error("give a destination folder, or set COD2_EXPORT_DIR")
    dest = pathlib.Path(args.dest).resolve()
    if dest == ROOT or ROOT in dest.parents:
        parser.error("the destination must be outside this repository")

    drops = DROPS[args.profile]
    symbol_lists = sections("symbols.txt")
    undefined = {s for mark in drops for s in symbol_lists[mark]}
    defined = set(symbol_lists["defined"])
    forbidden_lists = sections("forbidden.txt")
    names = [re.compile(p) for p in forbidden_lists["names"]]
    engine = [re.compile(p) for p in forbidden_lists.get(args.profile + " engine code", [])]
    allowed = set(forbidden_lists.get("allowed", []))
    rewrites = [tuple(part.strip(" ") for part in line.split(" => ", 1)) for line in sections("rewrites.txt")["rewrite"]]
    tracked, files = select(args.profile)
    kept_names = {os.path.basename(f) for f in files}
    left_out = {os.path.basename(f) for f in tracked} - kept_names

    results, dropped, problems = {}, [], []
    for path in files:
        source = (ROOT / path).read_bytes()
        if b"\0" in source:
            results[path] = source
            continue
        text = source.decode("latin1")
        try:
            if path.endswith(C_FAMILY):
                text = conditionals.strip(text, undefined, lambda name: os.path.basename(name) in left_out, path, defined)
            else:
                text = drop_marked(text, drops, path)
        except conditionals.Unsupported as error:
            problems.append(f"{error}")
            continue
        if text.encode("latin1") != source and not text.strip():
            dropped.append(path)  # nothing left once this project's blocks are out
            continue
        results[path] = text
    kept = set(results)
    for path, text in results.items():
        if isinstance(text, bytes):
            continue
        if path.endswith(".json"):
            text = filter_records(text, kept)
        for old, new in rewrites:
            text = text.replace(old, new)
        code = None
        for number, line in enumerate(text.splitlines(), 1):
            if any(p.search(line) for p in names):
                problems.append(f"{path}:{number}: {line.strip()[:110]}")
            elif engine and not path.startswith(("tests/", "tools/")) and any(p.search(line) for p in engine):
                if code is None:
                    code = without_comments(text).splitlines() if path.endswith(C_FAMILY) else text.splitlines()
                if number <= len(code) and any(p.search(code[number - 1]) for p in engine) and \
                        f"{path}: {line.strip()}" not in allowed:
                    problems.append(f"{path}:{number}: {line.strip()[:110]}")
        results[path] = text.encode("latin1")

    changed, missing = [], []
    for path, data in results.items():
        target = dest / path
        if not target.exists():
            missing.append(path)
        elif target.read_bytes() != data:
            changed.append(path)
        else:
            continue
        if not args.check and not problems:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            shutil.copymode(ROOT / path, target)
    listing = dest / ".exported-files"
    before = set(listing.read_text().splitlines()) if listing.exists() else set()
    gone = sorted(before - kept)
    if not args.check and not problems:
        if args.prune:
            for path in gone:
                if (dest / path).exists():
                    (dest / path).unlink()
        dest.mkdir(parents=True, exist_ok=True)
        listing.write_text("\n".join(sorted(kept)) + "\n")
    verb = "would be" if args.check or problems else "were"
    print(f"{args.profile}: {len(kept)} files; {len(changed)} changed and {len(missing)} new {verb} written; "
          f"{len(gone)} exported before and gone here{'' if args.prune or args.check or not gone else ' (use --prune)'}")
    if not args.quiet:
        for path in changed: print(f"  changed  {path}")
        for path in missing: print(f"  new      {path}")
        for path in gone: print(f"  gone     {path}")
    for problem in problems:
        print(f"  NOT NEUTRAL  {problem}")
    if problems:
        sys.exit(f"{len(problems)} lines are not neutral and nothing was written: guard them (symbols.txt), "
                 "mark them (export-begin), or add a rewrite (rewrites.txt)")
    if args.check and (changed or missing or gone):
        sys.exit(1)


if __name__ == "__main__":
    main()
