#!/usr/bin/env python3
"""Exercise the production DirectX fatal-error threshold with tracing off."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def production_function(source):
    start = re.search(r'\bvoid Sys_DirectXFatalError\(void\)\s*\{', source)
    if not start:
        raise ValueError('Sys_DirectXFatalError definition missing')
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', source[start.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[start.start():start.start() + token.end()] + '\n'
    raise ValueError('Sys_DirectXFatalError definition unterminated')


HARNESS = r'''
#include "PC/qcommon/port_debug.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

int g_gfxV60DllActive = 1;
static jmp_buf fatalJump;
static int messageBoxes, diagnosticPrints, exitStatus;

void Com_Printf(const char *format, ...)
{
    (void)format;
    ++diagnosticPrints;
}

static const char *Win_LocalizeRef(const char *key)
{
    return key;
}

static int MessageBoxA(void *window, const char *body, const char *title, unsigned flags)
{
    assert(!window && body && title && flags == 0x10);
    ++messageBoxes;
    return 0;
}

static _Noreturn void fixture_exit(int status)
{
    exitStatus = status;
    longjmp(fatalJump, 1);
}

#define exit fixture_exit
#include "sys_directx_fatal.h"
#undef exit

int main(void)
{
    unsetenv("COD2_NATIVE_TRACE");
    COD2_InitNativeTracing();
    assert(!COD2_NATIVE_TRACE_ENABLED());

    if (!setjmp(fatalJump)) {
        for (int i = 0; i < 999; ++i)
            Sys_DirectXFatalError();
        assert(messageBoxes == 0 && diagnosticPrints == 0);
        Sys_DirectXFatalError();
        fputs("FAIL: the 1000th DirectX failure returned with tracing disabled\n", stderr);
        return 1;
    }

    assert(exitStatus == -1 && messageBoxes == 1 && diagnosticPrints == 0);
    puts("PASS: tracing off preserves the 1000-error DirectX escalation threshold");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--output', type=Path, default=Path.home() /
                        'Library/Application Support/CoD2-native-ws42/directx-escalation')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/Mac/Main/mac_main.c').read_text()
    (args.output / 'sys_directx_fatal.h').write_text(production_function(source))
    harness = args.output / 'fixture.c'
    harness.write_text(HARNESS)
    executable = args.output / 'directx-escalation'
    subprocess.run([args.cc, '-DCOD2_X64=1', '-DCOD2_APPLE_SDK=1', '-UNDEBUG',
                    '-g', '-ffp-contract=off', '-fsanitize=address,undefined',
                    '-I' + str(ROOT / 'src'), str(harness),
                    str(ROOT / 'src/PC/qcommon/port_debug.c'), '-o', str(executable)],
                   check=True)
    return subprocess.run([str(executable)], cwd=args.output).returncode


if __name__ == '__main__':
    raise SystemExit(main())
