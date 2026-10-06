#!/usr/bin/env python3
"""Exercise production key routing and cross-panel menu capture cleanup."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
parser.add_argument('--baseline')
parser.add_argument('--case', choices=['key', 'oob', 'all'], default='all')
args = parser.parse_args()
out = args.build.resolve() / 'ui-input-route-tests'
out.mkdir(exist_ok=True)
entry = next(e for e in json.loads((args.build / 'compile_commands.json').read_text())
             if e['file'].endswith('ui_shared_mp.c'))
flags = shlex.split(entry['command'])
flags = [f for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-w', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']


def active(path):
    raw = (subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
           if args.baseline else (ROOT / path).read_bytes())
    return subprocess.run([*flags, '-E', '-P', '-x', 'c', '-'], input=raw,
                          cwd=ROOT, capture_output=True, check=True).stdout.decode()


def function(source, name):
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', source[match.start():]):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return source[match.start():match.start() + token.end()]
    raise ValueError(name)


keys = function(active('src/PC/client_mp/cl_keys_mp.c'), 'CL_KeyEvent')
start = keys.index('bypassUi = 0;')
route = keys[start:keys.index('if (keyCatchers & 0x10)', start)]
oob = function(active('src/PC/ui_mp/ui_shared_mp.c'), 'Menus_HandleOOBClick')
cleanup = oob[oob.index('int menuCount ='):oob.rfind('}')]
source = out / 'routes.c'
source.write_text(f'#define CASE_KEY {int(args.case != "oob")}\n#define CASE_OOB {int(args.case != "key")}\n' + '''#include "common_types.h"
#include <assert.h>
#include <stdio.h>
static int bypass, wants, queries, uiKeys, gameplay, pauses, stops;
static void *imp_cl_bypassMouseInput;
static clientStatic_t cls;
static qboolean CL_DvarEnabledFromImport(void *p) { (void)p; return bypass; }
static qboolean UI_checkKeyExec(int key) { (void)key; queries++; return wants; }
static void UI_KeyEvent(int key, qboolean down) { assert(down); (void)key; uiKeys++; }
static void UI_Pause(int value) { assert(value == 0); pauses++; }
static void CIN_StopCinematic(int handle) { assert(handle == 7); stops++; }
static void route(int key) { int down = 1, keyCatchers = 8, bypassUi;
''' + route + '''gameplay++; }
static void cleanup(displayContextDef_t *dc) { byte *d = (byte *)dc; int i,j,k,openCount;
''' + cleanup + '''}
int main(void) {
  if (CASE_KEY) {
  cls.uiStarted = 1;
  bypass = 0; wants = 0; route(0xae); /* F8 while binding. */
  assert(uiKeys == 1 && gameplay == 0 && queries == 0);
  route(0xbf); assert(uiKeys == 2); /* Backspace clears a binding. */
  bypass = 1; route(0xae); assert(gameplay == 1 && uiKeys == 2);
  wants = 1; route(0xae); assert(uiKeys == 3 && gameplay == 1);
  route(0xc8); assert(gameplay == 2); /* Explicit bypass keeps mouse in game. */
  bypass = 0; route(0xc8); assert(uiKeys == 4);
  }
  if (CASE_OOB) {
  displayContextDef_t dc = {0}; menuDef_t menu = {0}; itemDef_t item = {0};
  dc.menuCount = 1; dc.Menus[0] = &menu; dc.openMenuCount = 1; dc.menuStack[0] = &menu;
  menu.window.dynamicFlags[0] = 4;
  menu.window.style = 5; menu.window.cinematic = 7;
  menu.itemCount = 1; menu.items = (itemDef_t*[]){&item};
  item.window.style = 5; item.window.cinematic = 7;
  cleanup(&dc); assert(pauses == 0 && stops == 2 && menu.window.cinematic == -1);
  menu.window.dynamicFlags[0] = 0x4000; cleanup(&dc);
  assert(pauses == 0 && stops == 2); /* Forced-visible menus also retain capture. */
  menu.window.cinematic = 7; item.window.cinematic = 7;
  menu.window.dynamicFlags[0] = 0; cleanup(&dc);
  assert(pauses == 1 && stops == 4 && menu.window.cinematic == -1 && item.window.cinematic == -1);
  }
  puts("PASS UI key routing with bypass off/on and cross-panel capture until last menu closes");
}
''')
exe = out / 'routes'
subprocess.run([*flags, str(source), '-o', str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], check=True)
