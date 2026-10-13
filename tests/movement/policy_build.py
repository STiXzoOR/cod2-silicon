"""Link the actual native runtime/dvars into production gameplay fixtures."""
from pathlib import Path
import re
import subprocess

SERVICE_NAMES = ['Com_Error', 'Com_Printf', 'Com_PrintMessage', 'I_stricmp',
                 'stricmp', 'strnicmp', 'I_strncpyz', 'I_strncat', 'Com_sprintf', 'va']


def function(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


def compile_policy(root, output, flags):
    objects = []
    for path, name in [('tests/movement/policy_fixture.c', 'policy_fixture'),
                       ('src/PC/qcommon/cod2x_runtime.c', 'policy_runtime'),
                       ('src/PC/universal/dvar.c', 'policy_dvar')]:
        obj = output / (name + '.o')
        extra = ['-D' + service + '=Matrix_' + service for service in SERVICE_NAMES] if name == 'policy_dvar' else []
        with (output / (name + '.compile.log')).open('w') as log:
            subprocess.run([*flags, *extra, '-c', str(root / path), '-o', str(obj)],
                           cwd=root, stdout=log, stderr=log, check=True)
        objects.append(obj)
    return objects
