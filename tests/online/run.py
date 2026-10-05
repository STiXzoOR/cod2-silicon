#!/usr/bin/env python3
"""Exercise connection text using production functions and the engine ABI."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
database = Path(sys.argv[1])
entry = next(e for e in json.loads(database.read_text())
             if e['file'].endswith('/src/PC/ui_mp/ui_main_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
suites = [
    ('ui_conversion', 'src/PC/ui_mp/ui_main_mp.c',
     ['UI_ReplaceConversions', 'UI_ReplaceConversionString']),
    ('localization', 'src/PC/stringed/stringed_hooks.c',
     ['SEH_IsDigit', 'SEH_LocalizeTextMessage']),
    ('ui_item_color', 'src/PC/ui_mp/ui_shared_mp.c', ['Script_SetItemColor']),
    ('ui_server_insert', 'src/PC/ui_mp/ui_main_mp.c', ['UI_InsertServerAtPosition', 'UI_BinaryInsertServer']),
    ('ui_text_color', 'src/PC/ui_mp/ui_shared_mp.c', ['Item_TextColor']),
    ('setviewpos', 'src/PC/game_mp/g_cmds_mp.c', ['Cmd_SetViewpos_f']),
    ('infostring', 'src/PC/universal/q_shared.c', ['Info_RemoveKey', 'Info_RemoveKey_Big']),
    ('pure_iwds', 'src/PC/qcommon/files.c', ['FS_PureServerSetLoadedIwds']),
    ('timeout', 'src/PC/client_mp/cl_main_mp.c', ['CL_Frame']),
    ('scheduled_fx', 'src/PC/EffectsCore/FxScheduler.c',
     ['FxScheduler_GetDvar', 'FxScheduler_PlayEffect', 'FxScheduler_Clean']),
    ('server_commands', 'src/PC/client_mp/cl_cgame_mp.c', ['CL_GetServerCommand']),
    ('master_response', 'src/PC/client_mp/cl_main_pc_mp.c', ['CL_ServersResponsePacket']),
    ('time_delta', 'src/PC/client_mp/cl_cgame_mp.c', ['CL_AdjustTimeDelta']),
    ('download_names', 'src/PC/qcommon/files.c', ['FS_CompareIwds']),
    ('www_download_begin', 'src/PC/client_mp/cl_main_mp.c', ['CL_BeginDownload']),
    ('www_download', 'src/PC/client_mp/cl_parse_mp.c', ['CL_ParseWWWDownload', 'CL_WWWDownload']),
    ('mantle', 'src/PC/bgame/bg_mantle.c', []),
    ('md4', 'src/PC/qcommon/md4.c', []),
    ('cdkey_hash', 'src/PC/client_mp/cl_main_mp.c', ['CL_BuildMd5StrFromCDKey']),
    ('challenge_resend', 'src/PC/client_mp/cl_main_mp.c',
     ['CL_BuildMd5StrFromCDKey', 'CL_CheckForResend']),
    ('stream_spatialize', 'src/PC/win32/snd_driver.c',
     ['MSS_SpatializeStreamImpl', 'SND_StartAliasStreamOnChannel']),
    ('sprite_entity', 'src/PC/gfx_d3d/rb_tess.c', ['RB_TessEntity']),
]
if len(sys.argv) > 2:
    suites = [suite for suite in suites if suite[0] in sys.argv[2:]]
with tempfile.TemporaryDirectory(prefix='ws14-online-') as tmp:
    out = Path(tmp)
    for name, path, names in suites:
        source = (root / path).read_text()
        if name == 'mantle':
            source = source[:source.index('extern const dvar_t *Dvar_RegisterBool')]
        functions = [] if names else [source]
        if name == 'ui_text_color':
            functions.append(source[source.index('#if defined(COD2_X64)'):
                                    source.index('extern commandDef_t')])
        if name == 'scheduled_fx':
            # Keep the production native link accessor and its layout assertion.
            functions.append(source[source.index('#if defined(COD2_X64)'):
                                    source.index('extern int irand')])
        for function in names:
            match = re.search(r'^(?:static )?(?:const char \*|void |dvar_t \*|qboolean |int )' + function + r'\([^;]*?\)\n\{',
                              source, re.M)
            depth = 0
            for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]',
                                     source[match.start():]):
                if token[0] == '{':
                    depth += 1
                elif token[0] == '}':
                    depth -= 1
                    if not depth:
                        functions.append(source[match.start():match.start() + token.end()])
                        break
        (out / (name + '_source.h')).write_text('\n'.join(functions))
        obj = out / (name + '.o')
        subprocess.run([*flags, '-I' + tmp, '-c', str(root / ('tests/online/' + name + '.c')),
                        '-o', str(obj)], check=True, cwd=root)
        exe = out / name
        subprocess.run(['clang', '-fsanitize=address,undefined', str(obj), '-o', str(exe)],
                       check=True)
        subprocess.run([str(exe)], check=True, timeout=20,
                       env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0'})
