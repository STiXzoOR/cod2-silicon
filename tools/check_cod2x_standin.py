#!/usr/bin/env python3
"""Every script that starts the game engine points CoD2x's services at a stand-in.

A CoD2x engine asks CoD2x's own services at every start. A script under
tests/, tools/ or scripts/ that starts an engine must therefore set
COD2X_TEST_SERVICE_URL itself or source tools/cod2x/standin_env.sh. This names
each script that does neither. tools/cod2x/standin-review.txt lists the
scripts the check cannot classify from their text.

    python3 tools/check_cod2x_standin.py [--root <checkout>]
"""
import argparse
import pathlib
import re
import sys

FOLDERS = ('tests', 'tools', 'scripts')
REVIEW = 'tools/cod2x/standin-review.txt'
# An engine command line: '+set', "+map", `+set fs_basepath ...` and the like.
COMMAND_LINE = re.compile(r"""['"]\+(?:set|map|devmap|exec|connect|demo|timedemo)['" ]|\+set (?:fs_|dedicated|sv_|developer|r_)""")
COVERED = re.compile(r'COD2X_TEST_SERVICE_URL|standin_env\.sh')


def review(root):
    """The reviewed list: ({paths that start an engine}, {exempt path: reason}, [what is wrong with the list])."""
    starts, exempt, problems = set(), {}, []
    for number, line in enumerate((root / REVIEW).read_text().splitlines(), 1):
        words = line.split(None, 2)
        if not words or words[0].startswith('#'):
            continue
        if words[0] == 'starts' and len(words) == 2:
            starts.add(words[1])
        elif words[0] == 'exempt' and len(words) == 3:
            exempt[words[1]] = words[2]
        else:
            problems.append(f'{REVIEW}:{number}: expected "starts <path>" or "exempt <path> <reason>"')
            continue
        if not (root / words[1]).is_file():
            problems.append(f'{REVIEW}:{number}: {words[1]} does not exist')
    for path in sorted(starts & set(exempt)):
        problems.append(f'{REVIEW}: {path} is listed as both starting and exempt')
    return starts, exempt, problems


def check(root):
    """Every problem found, as lines to print. Empty when the tree is clean."""
    starts, exempt, problems = review(root)
    scripts = sorted(path for folder in FOLDERS if (root / folder).is_dir()
                     for path in (root / folder).rglob('*') if path.suffix in ('.py', '.sh') and path.is_file())
    for path in scripts:
        name = path.relative_to(root).as_posix()
        if name == 'tools/check_cod2x_standin.py':
            continue
        text = path.read_text(errors='replace')
        builds = bool(COMMAND_LINE.search(text))
        if name in exempt:
            if not builds:
                problems.append(f'{REVIEW}: {name} is exempt but builds no engine command line; remove the line')
            continue
        if (builds or name in starts) and not COVERED.search(text):
            problems.append(f'{name}: starts the engine without the CoD2x stand-in '
                            '(source tools/cod2x/standin_env.sh, or set COD2X_TEST_SERVICE_URL)')
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--root', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    problems = check(parser.parse_args().root.resolve())
    for problem in problems:
        print(f'FAIL: {problem}')
    if problems:
        return 1
    print('PASS: every script that starts the engine points CoD2x at a stand-in')
    return 0


if __name__ == '__main__':
    sys.exit(main())
