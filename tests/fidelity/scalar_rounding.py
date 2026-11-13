#!/usr/bin/env python3
"""Check the production frame-time and dvar-color conversions at Windows ties."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def body(path, marker):
    text = (ROOT / path).read_text()
    begin = text.index('{', text.index(marker))
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]

color = body('src/PC/universal/dvar.c', 'byte Dvar_FloatToColorComponent(float value)')
common = (ROOT / 'src/PC/qcommon/common.c').read_text()
start = common.index('        float ts = com_timescale->current.value;')
end = common.index('    if (msec <= 0)', start)
frame = common[start:end].replace('float ts = com_timescale->current.value;', '')
# Include the surrounding else block; the production slice contains its closing brace.
source = '''#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef unsigned char byte;
static byte color(float value) ''' + color + '''
static int frame(int rawMsec, float ts, float com_codeTimeScale) {
    int msec, useTimescale;
    {
''' + frame + '''
    if (msec <= 0) msec = 1;
    return msec;
}
int main(void) {
    /* Owned Windows Com_ModifyMsec 0x434cb0; Dvar_RegisterColor 0x438419. */
    assert(frame(17, .5f, 1.f) == 8);
    assert(frame(16, .5f, 1.f) + frame(17, .5f, 1.f) + frame(17, .5f, 1.f) == 24);
    assert(frame(7, .5f, 1.f) == 4);
    assert(frame(1, 0.f, 1.f) == 1);
    assert(frame(17, 1.f, 1.f) == 17);
    assert(color(.3f) == 76 && color(.7f) == 178);
    assert(color(.5f) == 128 && color(.0f) == 0 && color(1.f) == 255);
    assert(color(-1.f) == 0 && color(2.f) == 255);
    puts("Windows frame-time/color tie conversions pass");
}
'''
fixture = a.output / 'scalar_rounding.c'
fixture.write_text(source)
exe = a.output / 'scalar_rounding'
command = ['clang', '-std=c11', '-O3', '-ffp-contract=off', '-fno-strict-aliasing', '-DCOD2_APPLE_SDK=1', str(fixture), '-o', str(exe)]
build = subprocess.run(command, capture_output=True, text=True)
(a.output / 'compile.log').write_text(build.stdout + build.stderr)
if build.returncode:
    print(build.stderr)
    raise SystemExit(build.returncode)
result = subprocess.run([str(exe.resolve())], capture_output=True, text=True)
(a.output / 'result.log').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end='')
raise SystemExit(result.returncode)
