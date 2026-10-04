#!/usr/bin/env python3
"""Build isolated tests from production C fragments; no game data or engine link needed."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build/ws9')
parser.add_argument('--test', action='append')
parser.add_argument('--baseline', help='extract production code from this local git commit')
args = parser.parse_args()
suffix = ''
if args.baseline:
    suffix = '-baseline-' + subprocess.check_output(
        ['git', 'rev-parse', '--short', args.baseline], cwd=ROOT, text=True).strip()
out = args.build.resolve() / ('fixes13-tests' + suffix)
out.mkdir(parents=True, exist_ok=True)
entries = json.loads((args.build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/src/PC/server_mp/sv_init_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [f for f in flags if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-ffunction-sections', '-fdata-sections', '-I' + str(out),
          '-fsanitize=address', '-fno-omit-frame-pointer']
preprocessed = {}
ABI_SOURCES = ['src/PC/script/scr_compiler.c', 'src/PC/client_mp/cl_main_pc_mp.c',
               'src/PC/gfx_d3d/r_scene.c', 'src/PC/gfx_d3d/r_staticmodel_load_obj.c',
               'src/PC/cgame_mp/cg_main_mp.c', 'src/PC/win32/gfx_v60_threads.c',
               'src/PC/client_mp/cl_console_mp.c', 'src/PC/qcommon/net_chan_mp.c',
               'src/PC/EffectsCore/FxUtil.c', 'src/PC/script/scr_debugger_ui_lists.c',
               'src/PC/script/scr_debugger_ui_watch.c']


def active(path):
    if path not in preprocessed:
        source = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], text=True, cwd=ROOT) if args.baseline else (ROOT / path).read_text()
        preprocessed[path] = subprocess.run(
            [*flags, '-iquote', str((ROOT / path).parent), '-E', '-P', '-x', 'c', '-'],
            input=source, capture_output=True, text=True, cwd=ROOT, check=True).stdout
    return preprocessed[path]


def declaration(path, name):
    return re.search(r'^(?!extern\b)[\w *]+\b' + name + r'\s*(?:\[[^]]*\])?\s*(?:=[\s\S]*?)?;', active(path), re.M)[0] + '\n'


def function(path, name):
    source = active(path)
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError(f'function {name} missing in {path}')
    # Ignore braces inside comments and string literals.
    tokens = re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():])
    depth = 0
    for token in tokens:
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError('unterminated function')


def run(name, generated, sources=(), cases=((),)):
    if args.test and name not in args.test:
        return
    (out / f'{name}_source.h').write_text(generated)
    objects = []
    for index, source in enumerate([*sources, f'tests/fixes13/{name}.c']):
        obj = out / f'{name}_{index}.o'
        with (out / f'{name}_{index}.log').open('w') as log:
            result = subprocess.run([*flags, '-c', str(ROOT / source), '-o', str(obj)],
                                    cwd=ROOT, stdout=log, stderr=log)
        if result.returncode:
            print((out / f'{name}_{index}.log').read_text())
            result.check_returncode()
        objects.append(str(obj))
    exe = out / name
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address', '-Wl,-dead_strip',
                    *objects, '-o', str(exe)], check=True)
    for case in cases:
        subprocess.run([str(exe), *case], check=True)
    print(f'PASS {name}', flush=True)


if not args.test or 'receive' in args.test:
    run('receive', function('src/Mac/Main/mac_main.c', 'Sys_GetEvent'), ['src/blobs/bss.c'])
if not args.test or 'snapshot' in args.test:
    path = 'src/PC/server_mp/sv_snapshot_mp.c'
    run('snapshot', ''.join(function(path, n) for n in
        ['SV_WriteSnapshotToClientLocal', 'SV_WriteOverflowRecoveryCommandsLocal', 'SV_SendClientSnapshot']))
if not args.test or 'abi' in args.test:
    for path in ABI_SOURCES:
        subprocess.run([*flags, '-include', 'stdlib.h', '-include', 'string.h',
                        '-fsyntax-only', str(ROOT / path)], check=True,
                       stdout=subprocess.DEVNULL, stderr=(out / (Path(path).stem + '_abi.log')).open('w'))
    run('abi', '')
if not args.test or 'switch' in args.test:
    path = 'src/PC/script/scr_compiler.c'
    source = function(path, 'EmitSwitchStatement')
    start = source.index('if (numCases > 1)')
    end = source.index('nextCodePos = TempMalloc(0);', start)
    generated = function(path, 'CompareCaseInfo') + '''
static void check_cases(unsigned int (*caseTable)[2], int numCases)
{
    int i;
    CaseStatementInfo *caseStatement;
    qsort(caseTable, numCases, 8, (int (*)(const void *, const void *))CompareCaseInfo);
''' + source[start:end].replace('return 0;', 'return;') + '}\n'
    run('switch', generated)
if not args.test or 'storage' in args.test:
    path = 'src/PC/universal/com_memory.c'
    run('storage', declaration(path, 'g_largeLocalBuf') + ''.join(function(path, n) for n in
        ['LargeLocal_LargeLocal', 'LargeLocal_GetBuf', 'ZN10LargeLocalD1Ev']))
if not args.test or 'tables' in args.test:
    generated = ''
    for path, names in [('src/PC/server_mp/sv_client_mp.c', ['ucmds']),
                        ('src/PC/ui_mp/ui_main_mp.c', ['serverStatusDvars']),
                        ('src/PC/client_mp/cl_keys_mp.c', ['frenchNumberKeysMap', 'keynames_localized']),
                        ('src/PC/cgame_mp/cg_shellshock.c', ['cg_shock_dvar_names'])]:
        tables = ''.join(declaration(path, n) for n in names)
        for label in sorted(set(re.findall(r'\bstr_[\w]+', tables))):
            generated += declaration(path, label)
        for name in sorted(set(re.findall(r'&?(SV_\w+)', tables))):
            generated += f'void {name}(void) {{}}\n'
        generated += tables
    run('tables', generated)
if not args.test or 'hotpaths' in args.test:
    for path in ['src/PC/game_mp/g_main_mp.c', 'src/PC/script/scr_vm.c',
                 'src/PC/gfx_d3d/rb_backend.c', 'src/PC/client_mp/cl_scrn_mp.c',
                 'src/PC/gfx_d3d/rb_shade.c', 'src/PC/gfx_d3d/r_rendercmds.c',
                 'src/PC/gfx_d3d/r_getrefapi_v60.c']:
        obj = out / (Path(path).stem + '_hot.o')
        with (out / (Path(path).stem + '_hot.log')).open('w') as log:
            subprocess.run([*flags, '-O0', '-c', str(ROOT / path), '-o', str(obj)],
                           check=True, stdout=log, stderr=log)
        symbols = subprocess.check_output(['nm', str(obj)], text=True)
        assert not re.search(r'\b(?:_getenv|_dbg_check439\w*|_dbg_protect_439|_dbg_end_probe|_g_lastop_dbg|_VM_DebugRecordOpcode|_RB_X64Trace\w+|_R_X64Trace\w+)$', symbols, re.M), path
        relocations = subprocess.check_output(['otool', '-rv', str(obj)], text=True)
        text_relocs = relocations.split('Relocation information (__TEXT,__text)', 1)[-1].split('Relocation information', 1)[0]
        assert not re.search(r'\b_(?:g_rb_draw_dbg|g_rb_endsurface_\w+|g_rb_tess_type_\w+|g_rb_last_tess_type|g_tess_since_begin|g_rdsl_\w+|g_q_stretchpic|g_disp_stretchpic|g_rb_stretchpic_calls|diag_endsurface_entry|diag_idxzero)\b', text_relocs), path
    print('PASS hotpaths: no getenv/opcode probes, renderer trace calls, or counter references at O0')
if not args.test or 'trajectory' in args.test:
    path = 'src/PC/bgame/bg_misc.c'
    run('trajectory', ''.join(function(path, n) for n in
        ['BG_Vec3Copy', 'BG_Vec3Mad', 'BG_EvaluateTrajectory']))
if not args.test or 'timing' in args.test:
    # This fixture includes CoD2x's optional policy cap, even with a stock database.
    saved_flags = flags
    flags = [*flags, '-DCOD2_CODX=1']
    for path in ['src/PC/qcommon/cod2x_protocol.c', 'src/PC/qcommon/cod2x_runtime.c',
                 'src/PC/qcommon/common.c', 'src/PC/client_mp/cl_input.c']:
        preprocessed.pop(path, None)
    run('timing', function('src/PC/qcommon/cod2x_protocol.c', 'Cod2x_LimitedFPS') +
        function('src/PC/qcommon/cod2x_runtime.c', 'Cod2x_FrameFPS') +
        function('src/PC/qcommon/common.c', 'Com_Frame_Try_Block_Function') +
        function('src/PC/client_mp/cl_input.c', 'CL_SendCmdInternal'))
    flags = saved_flags
if not args.test or 'renderer_options' in args.test:
    path = 'src/Mac/DirectX_9/CDirect3DDevice.c'
    assert 'getenv(' not in function(path, 'CDirect3DDevice_DrawIndexedPrimitive')
    run('renderer_options', function(path, 'CDirect3DDevice_UsePrograms'), cases=((), ('on',), ('off',)))
if args.test and 'debug_enabled' in args.test:
    checked = set()
    for entry in entries:
        path = Path(entry['file'])
        if (path.suffix != '.c' or path in checked or
                'cod2_macos.dir/' not in entry['command'] or
                not any(name in path.read_text() for name in
                        ('port_debug.h', 'gfx_dll_v60_map.h'))):
            continue
        options = shlex.split(entry['command'])
        options = options[:options.index('-o')]
        with (out / (path.stem + '_debug.log')).open('w') as log:
            subprocess.run([*options, '-DCOD2_PORT_DEBUG=1', '-fsyntax-only', str(path)],
                           check=True, stdout=log, stderr=log)
        checked.add(path)
    assert checked
    print(f'PASS debug opt-in: {len(checked)} native translation units')
