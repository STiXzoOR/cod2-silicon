#!/usr/bin/env python3
"""Verify inactive-port source bodies and script macro expansions against WS6 base.

Includes are omitted by the shared checker; this proves guard discipline, not
binary parity. The i386 build/run remains the cross-architecture harness's job.
"""
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASE = 'a2f4477'
spec = importlib.util.spec_from_file_location(
    'legacy', ROOT / 'tools/macos-port/check_legacy_guards.py')
legacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(legacy)
configs = {k: v for k, v in legacy.CONFIGURATIONS.items()
           if k != 'linux-x86_64-port-on'}
configs['linux-x86_64-port-off'] = ['__GNUC__=4', '__linux__=1', '__x86_64__=1']
configs['apple-arm64-port-off'] = ['__GNUC__=4', '__clang__=1', '__APPLE__=1', '__aarch64__=1']
configs['linux-i386-debugger-on'] = [*configs['linux-i386'], 'COD2_FEATURE_SCRIPT_DEBUGGER=1']
paths = subprocess.check_output(['git', 'diff', '--name-only', BASE], cwd=ROOT,
                                text=True).splitlines()
paths = [p for p in paths if p.startswith('src/') and p.endswith(('.c', '.h'))]
macro_probe = '''
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
void *probe_arena(unsigned int off) { return SCR_ARENA_PTR(off); }
unsigned int probe_encode(void *ptr) { return SCR_ARENA_ENC(ptr); }
unsigned int probe_codepos(const char *ptr) { return SCR_CODEPOS_ENC(ptr); }
const char *probe_decode(unsigned int off) { return SCR_CODEPOS_PTR(off); }
unsigned int probe_vector(const float *ptr) { return SCR_VEC_ENC(ptr); }
#endif
'''
failures = []
for path in paths:
    original = subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT,
                                       text=True)
    current = (ROOT / path).read_text()
    if path == 'src/headers/cod2_defs.h':
        original += macro_probe
        current += macro_probe
    for name, defines in configs.items():
        if legacy.body(original, defines) != legacy.body(current, defines):
            failures.append((path, name))
print(f'{len(paths)} production files x {len(configs)} inactive configurations: '
      f'{len(failures)} mismatches')
for path, name in failures:
    print(path, name)
raise SystemExit(bool(failures))
