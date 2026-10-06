#!/usr/bin/env python3
"""Exercise the production console prompt and footer in stock/CoD2x builds."""
import argparse
import ast
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
a = p.parse_args()
out = a.build.resolve() / 'ui-version-tests'
out.mkdir(exist_ok=True)
entry = next(e for e in json.loads((a.build / 'compile_commands.json').read_text())
             if e['file'].endswith('cl_console_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [x for x in flags if x != '-DNDEBUG' and not x.startswith(('-DCOD2_X64=', '-DCOD2_CODX=', '-DCOD2_RELEASE_VERSION='))]
flags += ['-UNDEBUG', '-w', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
path = 'src/PC/client_mp/cl_console_mp.c'
source = subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()


def function(source, name):
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError(name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', source[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError(name)


harness = r'''
#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static clientActive_t active;
static clientActive_t *activePointer = &active;
static void *imp_cl = &activePointer;
static field_t field;
static void *imp_g_consoleField = &field;
static clientStatic_t cls;
static refexport_t re;
static struct Console con;
static struct ConDrawInputGlob conDrawInputGlob;
static dvar_t color;
static const dvar_t *con_inputBoxColor = &color;
static const dvar_t *con_outputWindowColor = &color;
static const dvar_t *con_outputBarColor = &color;
static const dvar_t *con_outputSliderColor = &color;
static const vec4_t con_versionColor, con_inputDvarMatchColor;
static char promptText[256], footerText[256];
static int promptCalls, footerCalls;
static const char *va(const char *format, ...)
{
    static char text[512];
    va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args);
    return text;
}
static const char *getBuildNumber(void) { return "reference-build"; }
static int Sys_IsMainThread(void) { return 1; }
static int textHeight(FontHandle font) { return 16; }
static int textWidth(const char *text, int max, FontHandle font) { return (int)strlen(text) * 8; }
static void ConDraw_Box(float x, float y, float w, float h, const float *color) {}
static void ConDrawInput_DrawText(const char *text, int max, const float *color)
{ promptCalls++; snprintf(promptText, sizeof(promptText), "%s", text); }
static void SCR_DrawSmallStringExt(int x, int y, const char *text, const float *color)
{ footerCalls++; snprintf(footerText, sizeof(footerText), "%s", text); }
static void Field_Draw(field_t *f, int x, int y, int width, int ha, int va) {}
static void ConDrawInput_NextLine(void) {}
static const char *Con_TokenizeInput(void) { return ""; }
static void Dvar_ForEach(void (*callback)(const char *)) {}
static void Cmd_ForEach(void (*callback)(const char *)) {}
static void ConDrawInput_IncrMatchCounter(const char *text) {}
static void ConDrawInput_DetailedDvarMatch(const char *text) {}
static void ConDrawInput_DetailedCmdMatch(const char *text) {}
static void ConDrawInput_DvarMatch(const char *text) {}
static void ConDrawInput_CmdMatch(const char *text) {}
static void ConDrawInput_DrawHintBox(int count, float offset) {}
static void CL_LookupColor(int index, float *color) { memset(color, 0, sizeof(vec4_t)); }
static void SCR_DrawConsoleString(int x, int y, const short *text, int len, const float *color) {}
#include "console_production.h"
int main(int argc, char **argv)
{
    assert(argc == 3);
    re.TextHeight = textHeight; re.TextWidth = textWidth;
    active.keyCatchers = 1;
    con.screenMax[0] = 640; con.screenMax[1] = 480;
    con.currentLine = con.display = -1; con.totallines = 1;
    Con_DrawInput(); Con_DrawOuputWindow();
    assert(promptCalls == 1 && footerCalls == 1);
    if (strcmp(promptText, argv[1]) || strcmp(footerText, argv[2])) {
        fprintf(stderr, "prompt: <%s>\nfooter: <%s>\n", promptText, footerText);
        return 1;
    }
    /* The rendered prompt width reserves the correct space for command input. */
    assert(field.widthInPixels == 628 - (int)strlen(promptText) * 8);
    return 0;
}
'''
(out / 'version.c').write_text(harness)

for x64, codx, release in [(1, 0, '9.8.7'), (1, 1, '9.8.7'), (1, 1, '10.20.30'), (0, 1, '9.8.7')]:
    config = [*flags, '-DCOD2_X64=' + str(x64), '-DCOD2_CODX=' + str(codx),
              '-DCOD2_RELEASE_VERSION="' + release + '"']
    active_source = subprocess.run([*config, '-E', '-P', '-x', 'c', '-'], input=source,
                                  cwd=ROOT, capture_output=True, check=True).stdout.decode()
    (out / 'console_production.h').write_text(function(active_source, 'Con_DrawInput') +
                                            function(active_source, 'Con_DrawOuputWindow'))
    exe = out / 'version'
    subprocess.run([*config, '-I' + str(out), str(out / 'version.c'), '-o', str(exe)], cwd=ROOT, check=True)
    prompt = 'CoD2x MP: 1.4.6.8> ' if x64 and codx else 'CoD2 MP: 1.3> '
    if not x64:
        footer = 'Build reference-build MacOSXS-i386'
    else:
        footer = 'CoD2 Silicon ' + release + ' · CoD2x 1.4.6.8 · macOS arm64' if codx else 'Build reference-build macOS arm64'
    subprocess.run([str(exe), prompt, footer], cwd=out, check=True)
print('PASS production stock/CoD2x prompt, footer, dynamic release version, feature gates and input width')

# Evaluate the production defaults without building or packaging an application.
release = (ROOT / 'RELEASE_VERSION').read_text().strip()
cmake = (ROOT / 'cmake/macos-arm64.cmake').read_text()
version_config = cmake[cmake.index('set(COD2_RELEASE_VERSION'):cmake.index('enable_language')]
cmake_script = out / 'version.cmake'
cmake_script.write_text('set(CMAKE_SOURCE_DIR "' + str(ROOT) + '")\n' + version_config +
                        'message(STATUS "release=${COD2_RELEASE_VERSION}")\n')
for override, expected in [([], release), (['-DCOD2_RELEASE_VERSION=9.8.7'], '9.8.7')]:
    result = subprocess.run(['cmake', *override, '-P', str(cmake_script)], capture_output=True, text=True, check=True)
    assert 'release=' + expected in result.stdout
assert subprocess.run(['cmake', '-DCOD2_RELEASE_VERSION=bad', '-P', str(cmake_script)],
                      capture_output=True).returncode != 0

bundle_path = ROOT / 'tools/cod2x/make_macos_app.py'
bundle = ast.parse(bundle_path.read_text())
main = next(node for node in bundle.body if isinstance(node, ast.FunctionDef) and node.name == 'main')
parse_at = next(i for i, node in enumerate(main.body) if isinstance(node, ast.Assign) and
                any(isinstance(target, ast.Name) and target.id == 'args' for target in node.targets))
namespace = {'__file__': str(bundle_path), '__doc__': ast.get_docstring(bundle),
             'argparse': argparse, 'pathlib': __import__('pathlib')}
exec(compile(ast.Module(body=main.body[:parse_at], type_ignores=[]), str(bundle_path), 'exec'), namespace)
parser = namespace['parser']
assert parser.parse_args(['unused', 'unused.app']).version == release
assert parser.parse_args(['unused', 'unused.app', '--version', '9.8.7']).version == '9.8.7'
print('PASS RELEASE_VERSION default and explicit release override in production CMake/bundle parser')
