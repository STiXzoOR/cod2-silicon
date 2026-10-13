#!/usr/bin/env python3
"""Exercise production browser predicates with controlled server replies."""
import argparse, json, re, shlex, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
a = p.parse_args()
out = a.build.resolve() / 'ui-browser-tests'; out.mkdir(exist_ok=True)
e = next(e for e in json.loads((a.build / 'compile_commands.json').read_text()) if e['file'].endswith('ui_main_mp.c'))
flags = shlex.split(e['command']); flags = flags[:flags.index('-o')]
flags = [f for f in flags if f != '-DNDEBUG'] + ['-UNDEBUG','-w','-fsanitize=address,undefined']
path = 'src/PC/ui_mp/ui_main_mp.c'
raw = subprocess.check_output(['git','show',a.baseline + ':' + path],cwd=ROOT) if a.baseline else (ROOT/path).read_bytes()
s = subprocess.run([*flags,'-E','-P','-x','c','-'],input=raw,cwd=ROOT,capture_output=True,check=True).stdout.decode()
start = s.index('if (I_strnicmp(Info_ValueForKey(info_buf, "addr"), "000.000.000.000", 15) == 0)')
end = s.index('int joinGTIdx',start); end = s.rfind('{',start,end)
fragment = s[start:end]
names = sorted(set(re.findall(r'ui_browser\w+', fragment)) | {'ui_browserShowPunkBuster'})
code = '#include "common_types.h"\n#include <assert.h>\n#include <stdlib.h>\n#include <stdio.h>\n#include <string.h>\n#include <strings.h>\n'
for name in names: code += f'static dvar_t state_{name}; static const dvar_t *{name} = &state_{name};\n'
code += '''static int pb;
static const char *Info_ValueForKey(const char *s, const char *key) {
    (void)s;
    if (!strcmp(key,"addr")) return "127.0.0.1";
    if (!strcmp(key,"sv_maxclients")) return "16";
    if (!strcmp(key,"pure") || !strcmp(key,"hw")) return "1";
    if (!strcmp(key,"punkbuster")) return pb ? "1" : "0";
    return "0";
}
static int I_strnicmp(const char *a, const char *b, int n) { return strncasecmp(a,b,n); }
static int accepts(void) { char info_buf[1] = {0}; int clients = 2;
'''+fragment+'''return 1; reject: return 0; }
int main(void) {
'''
for name in names: code += f'state_{name}.current.integer = '+('-1' if name in ['ui_browserMod','ui_browserFriendlyfire','ui_browserKillcam','ui_browserShowPunkBuster'] else '0' if name=='ui_browserShowDedicated' else '1')+';\n'
code += '''pb = 0; assert(accepts()); pb = 1; assert(accepts());
state_ui_browserShowPunkBuster.current.integer = 0;
pb = 0; assert(accepts()); pb = 1; assert(!accepts());
state_ui_browserShowPunkBuster.current.integer = 1;
pb = 1; assert(accepts()); pb = 0; assert(!accepts());
puts("PASS browser default and both PunkBuster filter choices"); }
'''
source = out / 'browser.c'; source.write_text(code)
exe = out / 'browser'
subprocess.run([*flags,str(source),'-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(exe)],check=True)
