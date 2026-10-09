#!/usr/bin/env python3
"""Source registration catalog. No game files, private extraction tools or deps.

Clang evaluates build gates and macros; the bounded C reader handles literals,
constructor calls and reviewed indirections. Unknown registration names fail
closed. Facts describe registration sites, not a fabricated startup trace.
"""
import argparse
import ast
import collections
import json
import operator
import re
import subprocess
import struct
from pathlib import Path

MODES = ('stock', 'cod2x')
GROUPS = [
    ('frame', 'Frame & movement'), ('aim', 'Aim & input'), ('net', 'Network'),
    ('vis', 'Visibility'), ('hud', 'HUD & crosshair'), ('audio', 'Audio'),
    ('gfx', 'Graphics'), ('other', 'Other'),
]
FLAG_NAMES = {1: 'archive', 2: 'userinfo', 4: 'serverinfo', 8: 'systeminfo',
              16: 'writeProtected', 32: 'latched', 64: 'readOnly', 128: 'cheatProtected',
              256: 'gameplaySynced', 512: 'savedBit', 1024: 'scriptinfo',
              4096: 'engineOwned', 8192: 'rendererOwned', 16384: 'external', 32768: 'autoexec'}
CALL = re.compile(r'\b(Dvar_Register(?:Bool|Int|Float|String|Enum|Color|Vec[234])(?:_mac|_inl)?|Dvar_RegisterVariantReg|Cmd_AddCommand|CL_AddCgameCommand)\s*\(')
STRING = re.compile(r'"(?:\\.|[^"\\])*"')


class CatalogError(ValueError):
    pass


def dump(value):
    return json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + '\n'


def strip_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]),
                  text, flags=re.S)


def split_args(text):
    parts, start, depth, quoted, escaped = [], 0, 0, False, False
    for i, c in enumerate(text):
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
        elif c == ',' and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    parts.append(text[start:].strip())
    return parts


def call_end(text, start):
    depth, quoted, escaped = 1, False, False
    for i in range(start, len(text)):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
    raise CatalogError('unterminated registration call')


def uncast(expr):
    return re.sub(r'\(\s*(?:const\s+)?(?:unsigned\s+)?(?:char|int|float|double|short|long|void|dvar_t|qboolean|Bool)(?:\s*\*)?\s*\)', '', expr).strip()


def literal(expr):
    expr = uncast(expr)
    if re.fullmatch(r'(?:"(?:\\.|[^"\\])*"\s*)+', expr):
        return ''.join(ast.literal_eval(s) for s in STRING.findall(expr))
    # Hexadecimal f/F digits are digits, not floating-point suffixes.
    expr = re.sub(r'\b(0[xX][0-9a-fA-F]+)[uUlL]*\b|(?<![\w.])((?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?)[fFuUlL]+\b',
                  lambda m: m[1] or m[2], expr)
    ops = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
           ast.Div: operator.truediv, ast.BitOr: operator.or_, ast.BitAnd: operator.and_,
           ast.LShift: operator.lshift, ast.RShift: operator.rshift}

    def evaluate(node):
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd, ast.Invert)):
            return {ast.USub: operator.neg, ast.UAdd: operator.pos, ast.Invert: operator.invert}[type(node.op)](evaluate(node.operand))
        if isinstance(node, ast.BinOp) and type(node.op) in ops:
            return ops[type(node.op)](evaluate(node.left), evaluate(node.right))
        raise ValueError()

    try:
        value = evaluate(ast.parse(expr, mode='eval').body)
        if '(int)' in expr:
            value = (int(value) + 2**31) % 2**32 - 2**31
        return value
    except (ValueError, SyntaxError, TypeError):
        return None


def fact(expr, kind=None):
    value = literal(expr)
    # C's explicit int casts on hexadecimal signed bounds.
    if value is not None and kind == 'int' and '(int)' in expr:
        value = (int(value) + 2**31) % 2**32 - 2**31
    if value is not None and kind == 'bool':
        value = bool(value)
    return {'status': 'literal', 'value': value} if value is not None else {
        'status': 'dynamic', 'value': None, 'expression': expr.strip()}


def native_sources(root, mode):
    """Mirror the native client globs/filters, rejecting unhandled source rules."""
    cmake = root / 'cmake/macos-arm64.cmake'
    if not cmake.exists():  # Synthetic fixtures have only src/.
        return sorted((root / 'src').rglob('*.c'))
    text = cmake.read_text()
    paths = set()
    for directory in ('PC', 'Mac', 'stubs'):
        paths.update((root / 'src' / directory).rglob('*.c'))
    paths.update((root / 'src').glob('*.c'))
    for pattern in re.findall(r'list\(FILTER MACOS_C EXCLUDE REGEX "([^"]+)"\)', text):
        pattern = pattern.replace('\\\\', '\\')
        paths = {p for p in paths if not re.search(pattern, '/' + p.relative_to(root).as_posix())}
    for body in re.findall(r'(?:list\(APPEND MACOS_C|target_sources\(cod2_macos PRIVATE)\s+([^)]*)\)', text):
        for name in re.findall(r'\bsrc/[\w/.-]+\.(?:c|m|cpp)\b', body):
            if '/cod2x_native' not in name or mode == 'cod2x':
                paths.add(root / name)
    # CMake HEADER_FILE_ONLY exclusions are independent of preprocessor gates.
    if mode == 'stock':
        paths = {p for p in paths if not re.search(r'/cod2x_(?:identity|protocol|runtime|policy|features|demo|url|pose)\.c$|/cg_cod2x_radar\.c$', str(p))}
    return sorted(paths)


def preprocess(root, path, mode):
    raw = path.read_text()
    source = re.sub(r'^\s*#\s*include[^\n]*', '', raw, flags=re.M)
    prelude = '#include <float.h>\n#include <limits.h>\n'
    for name in ('src/headers/cod2_feature_config.h', 'src/PC/qcommon/cod2x.h'):
        header = root / name
        if header.exists():
            prelude += re.sub(r'^\s*#\s*include[^\n]*', '', header.read_text(), flags=re.M) + '\n'
    rel = path.relative_to(root).as_posix()
    source = prelude + f'\n#line 1 "{rel}"\n' + source
    result = subprocess.run(['clang', '-E', '-x', 'c', '-DCOD2_X64=1', '-DCOD2_APPLE_SDK=1',
                             '-D__APPLE__=1', '-D__aarch64__=1', '-D__arm64__=1',
                             *(['-DCOD2_CODX=1'] if mode == 'cod2x' else []), '-'],
                            input=source, text=True, capture_output=True, check=True)
    lines, line_numbers, active, number = [], [], False, 0
    for line in result.stdout.splitlines(keepends=True):
        marker = re.match(r'# (\d+) "([^"]+)"', line)
        if marker:
            number, active = int(marker[1]), marker[2] == rel
        elif active:
            lines.append(line)
            line_numbers.append(number)
            number += 1
    return ''.join(lines), line_numbers


def tables(text):
    found = {}
    for match in re.finditer(r'\b(\w+)\s*\[[^]]*\]\s*=\s*\{', text):
        start = match.end()
        depth, end = 1, start
        # All reviewed tables contain no braces in string literals.
        for end in range(start, len(text)):
            if text[end] == '{': depth += 1
            if text[end] == '}': depth -= 1
            if depth == 0: break
        found[match[1]] = text[start:end]
    return found


def function_at(text, offset):
    text = re.sub(r'__attribute__\s*\(\([^\n]*?\)\)', '', text[:offset])
    matches = list(re.finditer(r'^\w[\w\s*]*?\b(\w+)\s*\([^;{}]*\)\s*\{', text, re.M))
    return matches[-1][1] if matches else ''


# Generic runtime factories accept arbitrary names. The allowlist is by owner
# AND function, not by argument spelling, so new named registrations cannot hide.
FACTORIES = {
    ('src/PC/universal/dvar.c', 'Dvar_RegisterString_inl'),
    ('src/PC/universal/dvar.c', 'Dvar_SetBoolByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetIntByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetFloatByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetVec2ByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetVec3ByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetVec4ByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetStringByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetColorByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetFromStringByName'),
    ('src/PC/universal/dvar.c', 'Dvar_SetFromStringByNameFromSource'),
    ('src/PC/qcommon/dvar_cmds.c', 'Dvar_RegisterBool_f'),
    ('src/PC/qcommon/dvar_cmds.c', 'Dvar_RegisterInt_f'),
    ('src/PC/qcommon/dvar_cmds.c', 'Dvar_RegisterFloat_f'),
    ('src/PC/game_mp/g_scr_main_mp.c', 'GScr_MakeDvarServerInfo'),
    ('src/PC/client_mp/cl_cgame_mp.c', 'CL_AddCgameCommand'),
    ('src/PC/universal/dvar.c', 'Dvar_RegisterVariantReg'),
}


def extract_source(root, mode):
    root = Path(root)
    settings, commands, dispatch = (collections.defaultdict(list) for _ in range(3))
    dynamic, unresolved = [], []
    for path in native_sources(root, mode):
        if not path.exists() or not (CALL.search(path.read_text()) or path.name in ('g_cmds_mp.c', 'g_svcmds_mp.c')):
            continue
        text, numbers = preprocess(root, path, mode)
        text = strip_comments(text)
        arrays = tables(text)
        rel = path.relative_to(root).as_posix()
        if rel in ('src/PC/game_mp/g_cmds_mp.c', 'src/PC/game_mp/g_svcmds_mp.c'):
            for m in re.finditer(r'I_stricmp\(\s*cmd\s*,\s*("(?:\\.|[^"\\])*")\s*\)', text):
                owner = function_at(text, m.start())
                if owner in ('ClientCommand', 'ConsoleCommand'):
                    name = ast.literal_eval(m[1])
                    dispatch[name.lower()].append({'name': name, 'path': rel,
                        'line': numbers[text.count('\n', 0, m.start())], 'mechanism': owner + ':dispatch'})
        # Source-local bit-pattern float constants, not values from the binary.
        for m in re.finditer(r'\b(flt_\w+)\s*=\s*\{\s*(0x[\da-fA-F]+)\s*\}', text):
            value = struct.unpack('!f', struct.pack('!I', int(m[2], 16)))[0]
            text = re.sub(r'\b' + m[1] + r'\.f\b', repr(value), text)
        for match in CALL.finditer(text):
            end = call_end(text, match.end())
            args = split_args(text[match.end():end])
            func = match[1]
            # Function prototypes/definitions are not calls.
            if re.match(r'(?:const|char|unsigned|int|float|void)\b', args[0]):
                continue
            line_index = text.count('\n', 0, match.start())
            site = {'path': rel, 'line': numbers[line_index], 'mechanism': func}
            names = [literal(args[0])]
            owner = function_at(text, match.start())
            if (rel == 'src/PC/universal/dvar.c' and owner in {
                    'Dvar_RegisterBool', 'Dvar_RegisterInt', 'Dvar_RegisterFloat',
                    'Dvar_RegisterString', 'Dvar_RegisterEnum', 'Dvar_RegisterColor',
                    'Dvar_RegisterVec2', 'Dvar_RegisterVec3', 'Dvar_RegisterVec4'}
                    or rel == 'src/stubs/link_stubs.c' and owner in {
                        'Dvar_RegisterString_mac', 'Dvar_RegisterBool_mac'}):
                continue  # Constructor forwarding, not another registration.
            if not isinstance(names[0], str):
                table = re.fullmatch(r'(\w+)\[[^]]+\](?:\.cmd)?', args[0])
                if table and table[1] in arrays:
                    body = arrays[table[1]]
                    if args[0].endswith('.cmd'):
                        names = [ast.literal_eval(s) for s in re.findall(r'\{\s*("(?:\\.|[^"\\])*")', body)]
                    else:
                        names = [ast.literal_eval(s) for s in STRING.findall(body)]
                    site['mechanism'] += ':table:' + table[1]
                elif rel == 'src/PC/cgame_mp/cg_consolecmds_mp.c' and owner == 'CG_InitConsoleCommands':
                    names = [ast.literal_eval(s) for s in re.findall(r'\{\s*("(?:\\.|[^"\\])*")', arrays['commandsList'])]
                    site['mechanism'] += ':table:commandsList'
                elif (rel == 'src/PC/stringed/stringed_hooks.c' and owner == 'SEH_UpdateLanguageInfo'
                      and args[0] in ('loc_language->name', 'loc_forceEnglish->name')):
                    names = [args[0].split('->')[0]]
                    if not re.search(r'Dvar_Register\w+\("' + names[0] + '"', text):
                        raise CatalogError('localization alias has no literal registration')
                    site['mechanism'] += ':existingName'
                elif (rel, owner) in FACTORIES:
                    dynamic.append(dict(site, function=owner, nameExpression=args[0]))
                    continue
                else:
                    unresolved.append(f'{rel}:{site["line"]} ({owner}): unresolved registration name {args[0]}')
                    continue
            if func in ('Cmd_AddCommand', 'CL_AddCgameCommand'):
                for name in names:
                    commands[name.lower()].append(dict(site, name=name))
                continue
            if func == 'Dvar_RegisterVariantReg':
                # Reviewed Dvar_Init constructor initializes integer/domain to 0.
                if rel != 'src/PC/universal/dvar.c' or owner != 'Dvar_Init' or args[:3] != ['"sv_cheats"', 'DVAR_TYPE_BOOL', '0x1018']:
                    raise CatalogError(f'{rel}:{site["line"]}: unreviewed variant constructor')
                args, typ = ['"sv_cheats"', '0', '0x1018'], 'bool'
            else:
                typ = re.search(r'Register(\w+?)(?:_mac|_inl)?$', func)[1].lower()
            site['type'] = typ
            flags = literal(args[-1])
            if not isinstance(flags, int):
                choice = re.fullmatch(r'\w+\s*\?\s*(\w+)\s*:\s*(\w+)', args[-1])
                candidates = [literal(choice[1]), literal(choice[2])] if choice else []
                if not candidates or not all(isinstance(x, int) for x in candidates):
                    raise CatalogError(f'{rel}:{site["line"]}: unresolved flags {args[-1]}')
                flags = None
                site.update(flagsExpression=args[-1], flagCandidates=candidates)
            site['flags'] = flags
            if typ in ('vec2', 'vec3', 'vec4', 'color'):
                count = 4 if typ == 'color' else int(typ[-1])
                components = [fact(x) for x in args[1:count + 1]]
                site['default'] = {'status': 'literal', 'value': [x['value'] for x in components]} if all(x['status'] == 'literal' for x in components) else {'status': 'dynamic', 'value': None, 'expression': ', '.join(args[1:count + 1])}
                if typ != 'color':
                    site['minimum'], site['maximum'] = fact(args[count + 1]), fact(args[count + 2])
            elif typ == 'enum':
                site['default'] = fact(args[2], 'int')  # Constructor index, not a guessed display-mode string.
                values = arrays.get(args[1])
                site['enumValues'] = {'status': 'literal', 'value': [ast.literal_eval(s) for s in STRING.findall(values)]} if values and STRING.search(values) else {'status': 'dynamic', 'value': None, 'expression': args[1]}
            else:
                site['default'] = fact(args[1], typ)
                if typ in ('int', 'float'):
                    site['minimum'], site['maximum'] = fact(args[2], typ), fact(args[3], typ)
            for name in names:
                settings[name.lower()].append(dict(site, name=name))
    if unresolved:
        raise CatalogError('\n'.join(unresolved))
    return {'settings': dict(settings), 'commands': dict(commands), 'dispatchCommands': dict(dispatch), 'dynamicRegistrations': dynamic}


def coverage_errors(document, facts, mode):
    errors = []
    for plural, singular in [('settings', 'setting'), ('commands', 'command')]:
        claimed = {}
        for entry in document[plural]:
            name = entry['name'].lower()
            if entry['availability'] in ('registered', 'dispatchOnly') and mode in entry['variants']:
                if name in claimed:
                    errors.append(f'duplicate {singular} {name}')
                claimed[name] = entry['variants'][mode]
        actual = dict(facts[plural])
        if plural == 'commands':
            for name, sites in facts['dispatchCommands'].items():
                actual[name] = actual.get(name, []) + sites
        errors.extend(f'missing {singular} {name}' for name in sorted(actual.keys() - claimed.keys()))
        errors.extend(f'extra {singular} {name}' for name in sorted(claimed.keys() - actual.keys()))
        if plural == 'settings':
            for name in sorted(actual.keys() & claimed.keys()):
                sites = claimed[name].get('sites', [])
                for field in ('type', 'default', 'flags', 'minimum', 'maximum', 'enumValues', 'flagCandidates', 'flagsExpression'):
                    a = sorted(json.dumps(s.get(field), sort_keys=True) for s in actual[name])
                    b = sorted(json.dumps(s.get(field), sort_keys=True) for s in sites)
                    if a != b:
                        errors.append(f'{field} mismatch {name}')
        elif any('sites' in variant for variant in claimed.values()):
            for name in sorted(actual.keys() & claimed.keys()):
                signature = lambda sites: sorted(json.dumps({k: v for k, v in s.items() if k != 'line'}, sort_keys=True) for s in sites)
                if signature(claimed[name].get('sites', [])) != signature(actual[name]):
                    errors.append(f'sites mismatch {name}')
    dynamic_signature = lambda sites: sorted(json.dumps({k: v for k, v in s.items() if k != 'line'}, sort_keys=True) for s in sites)
    if 'dynamicRegistrations' in document and dynamic_signature(document['dynamicRegistrations'].get(mode, [])) != dynamic_signature(facts['dynamicRegistrations']):
        errors.append('dynamic registration evidence mismatch')
    return errors


def group_for(name, sites):
    if name.startswith(('snd_', 'mss_', 'winvoice_', 'cl_voice')): return 'audio'
    if name.startswith(('m_', 'in_', 'cl_mouse', 'sensitivity', 'input_', 'gpad_')): return 'aim'
    if name.startswith(('cl_', 'net_', 'sv_', 'rate', 'snaps', 'protocol', 'g_competitive', 'g_cod2x', 'match_')): return 'net'
    if name.startswith(('cg_hud', 'cg_draw', 'cg_chat', 'cg_cross', 'con_', 'hud_', 'ui_hud')): return 'hud'
    if name.startswith(('r_gamma', 'r_picmip', 'r_lod', 'r_fog')): return 'vis'
    if name.startswith(('r_', 'sc_', 'fx_', 'cg_brass', 'cg_marks', 'cg_blood', 'vid_')): return 'gfx'
    if name.startswith(('com_maxfps', 'cg_bob', 'bg_', 'player_', 'jump_', 'mantle_', 'friction', 'stopspeed', 'inertia', 'com_timescale')): return 'frame'
    return 'other'


def generic_title(name):
    # Technical references still have readable titles; the raw spelling remains
    # solely in the separate Expert name field.
    words = re.sub(r'([a-z])([A-Z])', r'\1 \2', name).replace('_', ' ').split()
    prefixes = {'r', 'cg', 'cl', 'sv', 'g', 'bg', 'ui', 'com', 'net', 'fx', 'mss', 'snd', 'con'}
    if words and words[0] in prefixes:
        words = words[1:]
    return ' '.join(words).capitalize() or 'Engine setting'


def build_catalog(root, revision, copy_path):
    facts = {mode: extract_source(root, mode) for mode in MODES}
    authored = json.loads(Path(copy_path).read_text())
    allowed_copy = {'title', 'summary', 'details', 'group', 'guided', 'control', 'bindable', 'menuLabel', 'menuExposed', 'effectSources', 'runtimeNotes'}
    for plural in ('settings', 'commands'):
        actual_names = set().union(*(set(facts[m][plural]) | (set(facts[m]['dispatchCommands']) if plural == 'commands' else set()) for m in MODES))
        for entry in authored.get('supplemental', {}).get(plural, []):
            if entry['name'].lower() in actual_names:
                raise CatalogError('supplemental shadows registration: ' + entry['name'])
        for name, entry in authored.get(plural, {}).items():
            if entry.keys() - allowed_copy:
                raise CatalogError('authored copy cannot replace source facts: ' + name)
            if not any(name in facts[m][plural] or plural == 'commands' and name in facts[m]['dispatchCommands'] for m in MODES):
                raise CatalogError('copy for unregistered name: ' + name)
    document = {'schemaVersion': 1, 'sourceRevision': revision,
                'groups': [{'id': key, 'title': label} for key, label in GROUPS],
                'settings': [], 'commands': [],
                'dynamicRegistrations': {mode: facts[mode]['dynamicRegistrations'] for mode in MODES}}
    for plural in ('settings', 'commands'):
        mode_sites = {mode: dict(facts[mode][plural]) for mode in MODES}
        if plural == 'commands':
            for mode in MODES:
                for name, sites in facts[mode]['dispatchCommands'].items():
                    mode_sites[mode][name] = mode_sites[mode].get(name, []) + sites
        for name in sorted(set(mode_sites['stock']) | set(mode_sites['cod2x'])):
            variants = {mode: {'sites': mode_sites[mode][name]} for mode in MODES if name in mode_sites[mode]}
            sites = [s for v in variants.values() for s in v['sites']]
            display_name = sites[0]['name']
            signatures = {m: [{k: v for k, v in s.items() if k not in ('path', 'line', 'name')} for s in v['sites']] for m, v in variants.items()}
            entry = {'name': display_name, 'availability': 'dispatchOnly' if plural == 'commands' and all(s['mechanism'].endswith(':dispatch') for s in sites) else 'registered', 'variants': variants,
                     'cod2x': 'added' if 'stock' not in variants else ('changed' if 'cod2x' in variants and signatures['stock'] != signatures['cod2x'] else 'unchanged'),
                     'origins': ['cod2x'] if 'stock' not in variants else ['stock'],
                     'group': group_for(name, sites), 'title': generic_title(display_name),
                     'summary': 'An engine reference setting. See Expert for its registered values and restrictions.',
                     'details': 'The source establishes registration metadata. Its effect has not been independently documented here.',
                     'sources': sorted({s['path'] + ':' + str(s['line']) for s in sites})}
            if any('/platform/' in s['path'] or '/Mac/' in s['path'] and s['name'].lower() in ('in_rawmouse', 'r_presentmode', 'r_borderless') for s in sites):
                entry['origins'] = ['port']
            if name in ('com_maxfps_limit', 'cl_masterserver', 'cl_masterport', 'm_rinput_max'):
                entry['origins'] = ['port']  # Compatibility names absent from upstream 1.4.6.8 registration sites.
            elif entry['cod2x'] == 'changed':
                entry['origins'] = ['stock', 'cod2x']
            if plural == 'settings':
                entry['guided'] = False
                types = {s['type'] for s in sites}
                kind = {'bool': 'switch', 'enum': 'menu', 'color': 'colour', 'int': 'number', 'float': 'number', 'vec2': 'vector', 'vec3': 'vector', 'vec4': 'vector', 'string': 'text'}
                entry['control'] = {'kind': kind[next(iter(types))] if len(types) == 1 else 'text'}
                for variant in variants.values():
                    flags = 0
                    for s in variant['sites']:
                        for candidate in s.get('flagCandidates', [s['flags']]): flags |= candidate
                    variant.update(flags=flags, flagNames=[v for k, v in FLAG_NAMES.items() if flags & k],
                                   flagsDynamic=any(s['flags'] is None for s in variant['sites']),
                                   serverControlled=bool(flags & (8 | 256)), editable=not bool(flags & (16 | 64 | 128)))
            else:
                entry.update(bindable=display_name.startswith(('+', '-')), menuLabel=None,
                             summary='An engine command. See Expert for where it is registered.',
                             details='Command availability may depend on the current game state. Registration alone does not establish that it is safe or useful in a bind.')
            overrides = authored.get(plural, {}).get(name, {})
            entry.update(overrides)
            if plural == 'settings' and any(not v['editable'] for v in variants.values()):
                # A Guided flag cannot create an editable cheat/read-only control.
                entry['guided'] = False
            document[plural].append(entry)
        document[plural].extend(authored.get('supplemental', {}).get(plural, []))
        document[plural].sort(key=lambda e: e['name'].lower())
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    gen = sub.add_parser('generate')
    gen.add_argument('--source-root', type=Path, required=True)
    gen.add_argument('--source-revision', required=True)
    gen.add_argument('--copy', type=Path, default=Path(__file__).with_name('copy.json'))
    gen.add_argument('--output', type=Path, required=True)
    check = sub.add_parser('check')
    check.add_argument('--source-root', type=Path, required=True)
    check.add_argument('--catalog', type=Path, required=True)
    check.add_argument('--mode', choices=MODES, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'generate':
            doc = build_catalog(args.source_root, args.source_revision, args.copy)
            args.output.write_text(dump(doc))
            print(f'generated {len(doc["settings"])} settings, {len(doc["commands"])} commands')
        else:
            doc = json.loads(args.catalog.read_text())
            facts = extract_source(args.source_root, args.mode)
            errors = coverage_errors(doc, facts, args.mode)
            dispatch_only = facts['dispatchCommands'].keys() - facts['commands'].keys()
            print(f'{args.mode}: {len(facts["settings"])} settings, {len(facts["commands"])} registered commands + {len(dispatch_only)} dispatch-only, {len(errors)} differences')
            for error in errors: print(error)
            return 1 if errors else 0
    except (CatalogError, subprocess.CalledProcessError) as error:
        print(error)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
