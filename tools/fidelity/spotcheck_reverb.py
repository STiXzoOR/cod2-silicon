#!/usr/bin/env python3
"""Observe real reverb functions with synthetic channel state and captured AIL output."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LEX = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def extract_function(source, name):
    clean = LEX.sub(lambda m: ' ' * len(m[0]), source)
    start = re.search(r'^(?:static\s+)?(?:void|int)\s+' + re.escape(name) + r'\s*\([^;{]*\)\s*\{', clean, re.M)
    if not start:
        raise ValueError('Missing function: ' + name)
    begin = clean.index('{', start.start())
    depth = 1
    end = begin + 1
    while depth and end < len(clean):
        depth += (clean[end] == '{') - (clean[end] == '}')
        end += 1
    if depth:
        raise ValueError('Unterminated function: ' + name)
    return source[start.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-difference', action='store_true', help='characterize audit finding instead of require fidelity')
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error('Keep observation evidence outside git')
    source_path = ROOT / 'src/PC/win32/snd_driver.c'
    source = source_path.read_text()
    rows = json.loads((args.build / 'compile_commands.engine.json').read_text())
    entry = next(r for r in rows if Path(r['file']) == source_path and 'cod2_macos.dir' in r['command'])
    command = shlex.split(entry['command'])
    flags = command[:command.index('-o')]
    # Reference truth is the control-flow path at 0x56897..0x568d8 and
    # 0x56919..0x56952, verified against Mac 1.3; not another implementation.
    prelude = '''#include "common_types.h"
#include "imports.h"
#include <stdio.h>
snd_local_t g_snd;
void *imp_snd_enableReverb;
static struct MssLocal milesGlob;
static HSAMPLE handle_2D_004a3ad4[8];
#define SND_2D_HANDLE(channel) handle_2D_004a3ad4[(channel) - 45]
static float observed;
void AIL_set_sample_reverb_levels(void *s, float dry, float wet) { observed = wet; }
void AIL_set_3D_sample_effects_level(void *s, float wet) { observed = wet; }
'''
    body = '\n'.join(extract_function(source, name) for name in ('SND_Update2DChannelReverb', 'SND_Update3DChannelReverb'))
    driver = '''
int main(void) {
    dvar_t enabled = {0};
    const dvar_t *slot = &enabled;
    snd_alias_t alias = {0};
    snd_enveffect effect = {0};
    effect.wetlevel = 0.75f;
    g_snd.effect = &effect;
    imp_snd_enableReverb = (void *)&slot;
    for (int mode=0;mode<2;mode++) {
        int channel = mode ? 0 : 45;
        for (int on=0;on<2;on++) for (int flags=0;flags<=16;flags+=16) {
            enabled.current.enabled = on;
            alias.flags = flags;
            g_snd.chaninfo[channel].pAlias0 = &alias;
            if (mode) SND_Update3DChannelReverb(channel); else SND_Update2DChannelReverb(channel);
            printf("%d %d %d %g\\n", mode, on, flags, observed);
        }
        g_snd.chaninfo[channel].pAlias0 = 0;
        if (mode) SND_Update3DChannelReverb(channel); else SND_Update2DChannelReverb(channel);
        printf("%d -1 -1 %g\\n", mode, observed);
    }
    return 0;
}
'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as tmp:
        c = Path(tmp) / 'probe.c'
        exe = Path(tmp) / 'probe'
        c.write_text(prelude + body + driver)
        subprocess.run(flags + [str(c), '-o', str(exe)], cwd=entry['directory'], check=True, capture_output=True)
        output = subprocess.check_output([str(exe)], text=True)
    cases = []
    for line in output.splitlines():
        mode, on, flags, actual = line.split()
        on, flags, actual = int(on), int(flags), float(actual)
        expected = 0.75 if on == -1 or (on and not flags & 16) else 0.0
        cases.append(dict(function='SND_Update3DChannelReverb' if int(mode) else 'SND_Update2DChannelReverb',
                          enabled=on, alias_flags=flags, actual=actual, reference=expected, different=actual != expected))
    result = dict(cases=cases, differences=sum(c['different'] for c in cases), total=len(cases),
                  reference_addresses=['0x56878', '0x568fa'], source=str(source_path.relative_to(ROOT)))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if bool(result['differences']) == args.expect_difference else 1


if __name__ == '__main__': raise SystemExit(main())
