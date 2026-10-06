#!/usr/bin/env python3
"""Reference-driven fidelity triage. Emits summaries, never code or binary bytes."""
import argparse
import bisect
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
import hashlib
import inspect
import json
import math
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from macho32 import MachO32
from stabs import macho_symbols

LEX = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
BRANCHES = {'IfStmt', 'SwitchStmt', 'ForStmt', 'WhileStmt', 'DoStmt', 'ConditionalOperator', 'CaseStmt'}


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, errors='replace', **kwargs).stdout


def name_of(symbol):
    symbol = symbol.split(':', 1)[0].split(' ##', 1)[0].strip()
    symbol = re.sub(r'^L_?(.*)\$stub$', r'\1', symbol)
    symbol = symbol.removeprefix('_') if symbol.startswith('__Z') else symbol
    match = re.match(r'_ZL?(\d+)', symbol)
    if match:
        return symbol[match.end():match.end() + int(match[1])]
    if symbol.startswith('_ZN'):
        parts, pos = [], 3
        while pos < len(symbol):
            match = re.match(r'\d+', symbol[pos:])
            if not match:
                break
            pos += len(match[0])
            parts.append(symbol[pos:pos + int(match[0])])
            pos += int(match[0])
        if parts:
            
            if symbol[pos:pos + 2] in ('C1', 'C2', 'C3'):
                parts.append(parts[-1])
            return '_'.join(parts)
    return symbol.removeprefix('_')


def stabs_functions(rows):
    result, source, current = [], '', None
    for text, kind, section, desc, address in rows:
        if kind in (0x64, 0x84) and text and not text.endswith('/'):
            source = text
        elif kind == 0x24 and text:
            current = dict(name=name_of(text), symbol='_' + text.split(':', 1)[0],
                           address=address, size=0, source=source, line=desc)
            result.append(current)
        elif kind == 0x24 and not text and current:
            current['size'] = address
            current = None
    # Some producers omit end entries. Only infer to the next distinct N_FUN.
    addresses = sorted({r['address'] for r in result})
    for row in result:
        if not row['size']:
            pos = bisect.bisect_right(addresses, row['address'])
            if pos < len(addresses):
                row['size'] = addresses[pos] - row['address']
    return result


def literal_at(data, sections, address):
    for s in sections:
        if not s['address'] <= address < s['address'] + s['size']:
            continue
        offset = s['offset'] + address - s['address']
        remaining = min(s['address'] + s['size'] - address, len(data) - offset)
        if s['name'] == '__cstring':
            end = data.find(b'\0', offset, offset + min(remaining, 4096))
            if end >= 0:
                value = data[offset:end].decode('utf-8', 'replace')
                return 'string', value
        size = 8 if s['name'] == '__literal8' else 4
        if s['name'] in ('__literal4', '__literal8', '__const') and remaining >= size:
            value = struct.unpack_from('<d' if size == 8 else '<f', data, offset)[0]
            if math.isfinite(value):
                return 'float', value
    return None


def int32(value):
    return value - 0x100000000 if value & 0x80000000 else value


def reference_dvars(instructions, read_literal, symbols=None):
    """Conservative straight-line i386 cdecl argument recovery; unknowns stay null."""
    stack, regs, calls = {}, {}, []
    symbols = symbols or {}

    def value(expr):
        expr = expr.strip()
        if expr in regs:
            return regs[expr]
        if expr.startswith('$'):
            token = expr[1:]
            try:
                return int(token, 0)
            except ValueError:
                return symbols.get(token)
        try:
            address = int(expr, 0)
        except ValueError:
            address = symbols.get(expr)
        if address is not None:
            literal = read_literal(address)
            if literal:
                return literal[1]
        return None

    for mnemonic, operands in instructions:
        operands = operands.split(' ##', 1)[0]
        if mnemonic.startswith('j'):
            stack.clear(); regs.clear()
        if mnemonic in ('movl', 'movss', 'movsd', 'movw', 'movb', 'leal'):
            parts = operands.rsplit(',', 1)
            if len(parts) == 2:
                src, dst = map(str.strip, parts)
                val = value(src) if mnemonic != 'leal' else None
                target = re.fullmatch(r'(0x[0-9a-f]+)?\(%esp\)', dst)
                if target:
                    stack[int(target[1] or '0', 0)] = val
                elif re.fullmatch(r'%\w+', dst):
                    regs[dst] = val
        elif mnemonic in ('xorl', 'xorps'):
            parts = list(map(str.strip, operands.split(',')))
            if len(parts) == 2:
                regs[parts[1]] = 0 if parts[0] == parts[1] else None
        elif mnemonic.startswith('call'):
            target = name_of(operands)
            if operands.startswith('0x'):
                target = symbols.get(int(operands, 16), '')
            if target.startswith('Dvar_Register') and target != 'Dvar_RegisterNew':
                count = {'Bool': 3, 'Int': 5, 'Float': 5, 'String': 3, 'Enum': 4,
                         'Vec2': 6, 'Vec3': 7, 'Vec4': 8, 'Color': 6}.get(target.removeprefix('Dvar_Register'))
                if count:
                    args = [stack.get(i * 4) for i in range(count)]
                    if isinstance(args[0], int):
                        literal = read_literal(args[0])
                        args[0] = literal[1] if literal and literal[0] == 'string' else None
                    if args[0]:
                        for i in range(1, count - 1):
                            if isinstance(args[i], int) and target == 'Dvar_RegisterInt':
                                args[i] = int32(args[i])
                            elif isinstance(args[i], int) and target in ('Dvar_RegisterFloat', 'Dvar_RegisterVec2', 'Dvar_RegisterVec3', 'Dvar_RegisterVec4', 'Dvar_RegisterColor'):
                                args[i] = struct.unpack('<f', struct.pack('<I', args[i] & 0xffffffff))[0]
                        if target == 'Dvar_RegisterString' and isinstance(args[1], int):
                            literal = read_literal(args[1])
                            args[1] = literal[1] if literal and literal[0] == 'string' else None
                        calls.append(dict(type=target.removeprefix('Dvar_Register'), arguments=args))
            for reg in ('%eax', '%ecx', '%edx'):
                regs.pop(reg, None)
    return calls


def disassembly_features(text, refs, addresses, data=b'', sections=(), symbols=None):
    starts = sorted({r['address'] for r in refs})
    extents = {r['address']: r['size'] for r in refs}
    features = {a: dict(callees=set(), branches=0, instructions=0, floats=set(), strings=set(),
                        literal_sites=[], indirect_calls=0, dvars=[]) for a in starts}
    instructions = defaultdict(list)
    for line in text.splitlines():
        match = re.match(r'^([0-9a-fA-F]{8,16})\s+(\w+)\s*(.*)$', line)
        if not match:
            continue
        address, mnemonic, operands = int(match[1], 16), match[2], match[3]
        index = bisect.bisect_right(starts, address) - 1
        if index < 0:
            continue
        start = starts[index]
        if not start <= address < start + extents[start]:
            continue
        f = features[start]
        f['instructions'] += 1
        instructions[start].append((mnemonic, operands))
        if mnemonic.startswith('j') and mnemonic not in ('jmp', 'jmpl'):
            f['branches'] += 1
        if mnemonic.startswith('call') or mnemonic in ('jmp', 'jmpl'):
            target = operands.split(' ##', 1)[0].strip()
            if target.startswith('*'):
                if mnemonic.startswith('call'):
                    f['indirect_calls'] += 1
            elif target.startswith('0x'):
                destination = int(target, 16)
                if destination in addresses and not start <= destination < start + extents[start]:
                    f['callees'].add(addresses[destination])
            else:
                f['callees'].add(name_of(target))
        # Absolute references only: offsets from base/index registers aren't addresses.
        for token in re.findall(r'(?<![\w])0x[0-9a-f]+(?![\w])', operands.split(' ##', 1)[0]):
            pos = operands.find(token) + len(token)
            if operands[pos:pos + 1] == '(':
                continue
            addr = int(token, 16)
            literal = literal_at(data, sections, addr)
            if literal:
                kind, value = literal
                # __const is untyped; only SSE scalar loads support a float claim.
                section = next(s['name'] for s in sections if s['address'] <= addr < s['address'] + s['size'])
                if kind == 'float' and section == '__const' and not re.search(r'(ss|sd)$', mnemonic):
                    continue
                f['floats' if kind == 'float' else 'strings'].add(value)
                f['literal_sites'].append(dict(address=addr, instruction=address, kind=kind, value=value))
    for start, f in features.items():
        for key in ('callees', 'floats', 'strings'):
            f[key] = sorted(f[key])
        f['dvars'] = reference_dvars(instructions[start], lambda a: literal_at(data, sections, a), symbols)
    return features


def scrub(text):
    return LEX.sub(lambda m: '\n' * m[0].count('\n') if m[0].startswith('/') else '""', text)


def stub_patterns(body):
    comments = [m[0] for m in LEX.finditer(body) if m[0].startswith('/')]
    patterns = ['comment:' + word for word in ('stub', 'simplified', 'todo', 'approximate', 'placeholder')
                if any(re.search(r'\b' + word + r'\b', c, re.I) for c in comments)]
    clean = scrub(body).strip().strip('{}').strip()
    clean = re.sub(r'\(void\)\s*\w+\s*;', '', clean).strip()
    if not clean:
        patterns.append('empty-body')
    elif re.fullmatch(r'return\s+(?:0(?:\.0)?[fF]?|NULL|false|qfalse)\s*;', clean):
        patterns.append('constant-return')
    elif re.fullmatch(r'(?:\s*\*?\w+(?:(?:->|\.)\w+|\[\d+\])*\s*=\s*0(?:\.0)?[fF]?\s*;)+\s*(?:return\s*;)?', clean):
        patterns.append('zero-output')
    elif re.fullmatch(r'(?:Com_)?Memset\s*\([^,]+,\s*0\s*,[^;]+\)\s*;', clean, re.I):
        patterns.append('zero-output')
    return patterns


def walk(node):
    yield node
    for child in node.get('inner', []):
        yield from walk(child)


def expression_value(node):
    kind = node.get('kind')
    if kind == 'StringLiteral':
        try:
            return json.loads(node['value'])
        except (ValueError, KeyError):
            return None
    if kind in ('FloatingLiteral', 'IntegerLiteral', 'CharacterLiteral'):
        return float(node['value']) if kind == 'FloatingLiteral' else int(node['value'])
    inner = node.get('inner', [])
    if len(inner) == 1:
        value = expression_value(inner[0])
        if kind == 'UnaryOperator' and node.get('opcode') == '-' and isinstance(value, (int, float)):
            return -value
        return value
    if kind == 'BinaryOperator' and len(inner) == 2:
        left, right = map(expression_value, inner)
        if isinstance(left, (int, float)) and isinstance(right, (int, float)):
            op = node.get('opcode')
            if op == '|': return int(left) | int(right)
            if op == '+': return left + right
            if op == '-': return left - right
            if op == '*': return left * right
            if op == '/' and right: return left / right
    return None


def ast_features(tree, root, path, source):
    funcs, last_file = [], str(path)
    def top(node):
        nonlocal last_file
        loc = node.get('loc', {})
        if 'file' in loc:
            last_file = loc['file']
        kind = node.get('kind')
        if kind in ('FunctionDecl', 'CXXMethodDecl', 'CXXConstructorDecl', 'CXXDestructorDecl'):
            body = next((c for c in node.get('inner', []) if c['kind'] == 'CompoundStmt'), None)
            if body is None or Path(last_file) != path:
                return
            begin = body.get('range', {}).get('begin', {})
            end = body.get('range', {}).get('end', {})
            begin, end = begin.get('expansionLoc', begin), end.get('expansionLoc', end)
            fragment = source[begin.get('offset', 0):end.get('offset', 0) + end.get('tokLen', 1)]
            f = dict(name=node.get('name', ''), symbol=node.get('mangledName', node.get('name', '')),
                     file=str(path.relative_to(root)), line=loc.get('line', source.count('\n', 0, loc.get('offset', 0)) + 1),
                     callees=set(), floats=set(), strings=set(), variables=set(), branches=0, nodes=0,
                     indirect_calls=0, stubs=stub_patterns(fragment), dvars=[])
            for child in walk(body):
                f['nodes'] += 1
                if child.get('kind') in BRANCHES:
                    f['branches'] += 1
                if child.get('kind') == 'DeclRefExpr' and child.get('referencedDecl', {}).get('kind') == 'VarDecl':
                    f['variables'].add(child['referencedDecl'].get('name', ''))
                if child.get('kind') == 'FloatingLiteral':
                    f['floats'].add(float(child['value']))
                if child.get('kind') == 'StringLiteral':
                    value = expression_value(child)
                    if value is not None:
                        f['strings'].add(value)
                if child.get('kind') in ('CallExpr', 'CXXMemberCallExpr') and child.get('inner'):
                    targets = [n['referencedDecl']['name'] for n in walk(child['inner'][0])
                               if n.get('referencedDecl', {}).get('kind') in ('FunctionDecl', 'CXXMethodDecl')]
                    if targets:
                        target = targets[0]
                        f['callees'].add(target)
                        if target.startswith('Dvar_Register'):
                            f['dvars'].append(dict(type=target.removeprefix('Dvar_Register'),
                                                   arguments=[expression_value(n) for n in child['inner'][1:]]))
                    else:
                        f['indirect_calls'] += 1
            for key in ('callees', 'floats', 'strings', 'variables'):
                f[key] = sorted(f[key])
            funcs.append(f)
        elif kind in ('TranslationUnitDecl', 'NamespaceDecl', 'LinkageSpecDecl'):
            for child in node.get('inner', []):
                top(child)
    top(tree)
    return funcs


def compiler_args(entry):
    args = entry.get('arguments') or shlex.split(entry['command'])
    result, skip = [], False
    for arg in args:
        if skip:
            skip = False
        elif arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', '-MP'):
            result.append(arg)
    return result + ['-w', '-fsyntax-only', '-Xclang', '-ast-dump=json']


def native_inventory(database, cache, jobs):
    entries = json.loads(Path(database).read_text())
    entries = {e['file']: e for e in entries if 'cod2_macos.dir' in (e.get('command') or ' '.join(e['arguments']))
               and Path(e['file']).suffix in ('.c', '.cpp', '.m', '.mm')}
    if not entries:
        raise ValueError('No cod2_macos entries in compile database')
    cache.mkdir(parents=True, exist_ok=True)
    extraction = ''.join(inspect.getsource(f) for f in (compiler_args, ast_features, walk, expression_value, stub_patterns, scrub))
    headers = hashlib.sha256(extraction.encode())
    for path in sorted(p for p in (ROOT / 'src').rglob('*') if p.suffix in ('.h', '.inc')):
        headers.update(path.read_bytes())
    def inspect_entry(entry):
        path = Path(entry['file']).resolve()
        if not path.is_relative_to(ROOT):
            raise ValueError('Compile source outside this worktree: ' + str(path))
        args = compiler_args(entry)
        digest = hashlib.sha256((str(args) + headers.hexdigest()).encode() + path.read_bytes()).hexdigest()
        out = cache / (digest + '.json')
        if out.exists():
            return json.loads(out.read_text())
        funcs = ast_features(json.loads(run(args, cwd=entry['directory'])), ROOT, path, path.read_text(errors='replace'))
        out.write_text(json.dumps(funcs))
        return funcs
    funcs = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for index, rows in enumerate(pool.map(inspect_entry, entries.values()), 1):
            funcs.extend(rows)
            if index % 100 == 0:
                print(f'AST: {index}/{len(entries)} translation units', file=sys.stderr)
    return funcs


def expanded_calls(function, definitions, depth=3):
    calls = set(function['callees'])
    if depth:
        for name in list(calls):
            helper = definitions.get(name)
            # Reconstruction extraction changes function boundaries; compare bounded reachability.
            if helper:
                calls |= expanded_calls(helper, definitions, depth - 1)
    return calls


def compare(reference, native, definitions):
    original = set(reference['callees'])
    ours = expanded_calls(native, definitions)
    expanded_original = set(original)
    for name in original:
        helper = definitions.get(name)
        if helper:
            expanded_original |= expanded_calls(helper, definitions)
    missing = sorted(original - ours)
    added = sorted(set(native['callees']) - expanded_original)
    # SDK/compiler lowering and deliberate platform adapters need human review.
    engine_missing = [n for n in missing if re.match(r'^(?:[A-Z][A-Za-z0-9]*_)', n)]
    absent_floats = [x for x in reference['floats'] if not any(math.isclose(x, y, rel_tol=1e-6, abs_tol=1e-8) for y in native['floats'])]
    added_floats = [x for x in native['floats'] if not any(math.isclose(x, y, rel_tol=1e-6, abs_tol=1e-8) for y in reference['floats'])]
    missing_strings = sorted(set(reference['strings']) - set(native['strings']))
    added_strings = sorted(set(native['strings']) - set(reference['strings']))
    shape = reference['branches'] >= 8 and native['branches'] * 3 < reference['branches']
    score = min(40, len(engine_missing) * 8) + min(12, (len(missing) - len(engine_missing)) * 2)
    stub_weight = len(native['stubs']) * 10 if reference['instructions'] > 8 else 0
    score += min(15, stub_weight) + (4 if shape else 0)
    score += min(8, len(absent_floats) * 2) + min(8, len(missing_strings) * 2)
    return dict(score=score, missing_callees=missing, added_callees=added,
                missing_floats=absent_floats, added_floats=added_floats,
                missing_strings=missing_strings, added_strings=added_strings,
                shape_warning=shape, stubs=native['stubs'])


def stub_inventory(functions):
    return [dict(file=f['file'], name=f['name'], patterns=f['stubs']) for f in functions
            if f['file'].startswith(('src/PC/', 'src/Mac/')) and f['stubs']]


def new_stubs(current, baseline):
    def key(row):
        return row['file'], row['name'], tuple(sorted(row['patterns']))
    old = Counter(map(key, baseline))
    result = []
    for row in current:
        identity = key(row)
        if old[identity]:
            old[identity] -= 1
        else:
            result.append(row)
    return result


def build_report(reference, functions):
    byname = defaultdict(list)
    for f in functions:
        byname[f['name']].append(f)
    definitions = {n: fs[0] for n, fs in byname.items() if len(fs) == 1}
    rows = []
    for ref in reference:
        candidates = byname.get(ref['name'], [])
        if len(candidates) > 1:
            tail = Path(ref['source']).stem
            narrowed = [f for f in candidates if Path(f['file']).stem == tail]
            if len(narrowed) == 1:
                candidates = narrowed
        row = dict(reference=ref)
        if len(candidates) == 1:
            row['native'] = candidates[0]
            row['status'] = 'stub-candidate' if candidates[0]['stubs'] else 'present'
            row['comparison'] = compare(ref, candidates[0], definitions)
        else:
            row['status'] = 'ambiguous' if candidates else 'missing'
            row['candidates'] = [f['file'] for f in candidates]
        rows.append(row)
    return rows


def native_only_context(rows, functions):
    paired = {r['reference']['name']: r for r in rows if 'native' in r}
    references = {r['reference']['name'] for r in rows}
    callers = defaultdict(set)
    for f in functions:
        for callee in f['callees']:
            callers[callee].add(f['name'])
    result = []
    for f in functions:
        if f['name'] in references:
            continue
        ancestors, frontier, seen = [], {f['name']}, set()
        for _ in range(4):
            frontier = {caller for name in frontier for caller in callers[name]} - seen
            seen |= frontier
            for name in sorted(frontier):
                row = paired.get(name)
                if row and row['comparison']['score'] >= 8:
                    ancestors.append(dict(name=name, address=row['reference']['address'],
                                          score=row['comparison']['score']))
        result.append(dict(name=f['name'], file=f['file'], line=f.get('line'),
                           stubs=f.get('stubs', []), ancestor_signals=ancestors,
                           status='no-standalone-reference'))
    return result


def dvar_report(rows):
    reference, native = {}, {}
    for row in rows:
        for d in row['reference'].get('dvars', []):
            reference.setdefault(d['arguments'][0], dict(d, function=row['reference']['name'], address=row['reference']['address']))
        if 'native' in row:
            for d in row['native']['dvars']:
                if d['arguments'] and isinstance(d['arguments'][0], str):
                    native.setdefault(d['arguments'][0], dict(d, function=row['native']['name'], file=row['native']['file']))
    mismatches = []
    for name in sorted(reference.keys() & native.keys()):
        a, b = reference[name], native[name]
        differences = []
        if a['type'] != b['type']:
            differences.append('type')
        for i, (left, right) in enumerate(zip(a['arguments'][1:], b['arguments'][1:]), 1):
            if left is None or right is None:
                continue
            same = math.isclose(left, right, rel_tol=1e-6, abs_tol=1e-8) if isinstance(left, (int, float)) and isinstance(right, (int, float)) else left == right
            if not same:
                differences.append('argument:' + str(i))
        if differences:
            mismatches.append(dict(name=name, reference=a, native=b, differences=differences))
    return dict(reference_count=len(reference), native_count=len(native),
                compared_count=len(reference.keys() & native.keys()), mismatches=mismatches,
                reference_only=sorted(reference.keys() - native.keys()), native_only=sorted(native.keys() - reference.keys()),
                reference_registrations=reference, native_registrations=native)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=Path.home() / 'Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386')
    parser.add_argument('--compile-commands', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--stub-gate', type=Path, help='fail on stub candidates absent from reviewed baseline')
    parser.add_argument('--write-stub-baseline', type=Path, help='initial inventory only; never use to approve unchecked new stubs')
    parser.add_argument('--sample-seed', type=int, default=44)
    options = parser.parse_args()
    options.output = options.output.expanduser().resolve()
    if options.output.is_relative_to(ROOT):
        parser.error('Write evidence outside the repository')
    cache = options.cache or options.output.parent / 'ast-cache'
    functions = native_inventory(options.compile_commands, cache, options.jobs)
    stubs = stub_inventory(functions)
    if options.write_stub_baseline:
        options.write_stub_baseline.write_text(json.dumps(stubs, indent=2) + '\n')
    if options.stub_gate:
        new = new_stubs(stubs, json.loads(options.stub_gate.read_text()))
        print(f'Stub gate: {len(stubs)} existing candidates, {len(new)} new')
        if new:
            print(json.dumps(new, indent=2))
            return 1
        # A gate needs no proprietary input or reference disassembly.
        return 0
    image = MachO32(options.binary)
    symbols = list(macho_symbols(options.binary))
    refs = stabs_functions(symbols)
    nm_text = run(['nm', '-ap', str(options.binary)])
    nm_count = sum(bool(re.search(r'\bFUN\s+\S', line)) for line in nm_text.splitlines())
    if nm_count != len(refs):
        raise ValueError(f'nm/STABS inventory mismatch: {nm_count}/{len(refs)}')
    addresses = {r['address']: r['name'] for r in refs}
    symbol_addresses = {s[0]: s[4] for s in symbols if not s[1] & 0xe0}
    for s in symbols:
        if not s[1] & 0xe0 and s[4] and s[2]:
            addresses.setdefault(s[4], name_of(s[0]))
    symbol_addresses.update(addresses)
    features = disassembly_features(run(['otool', '-arch', 'i386', '-tV', str(options.binary)]), refs,
                                    addresses, image.data, [vars(s) for s in image.sections], symbol_addresses)
    coal_verbose = disassembly_features(run(['otool', '-arch', 'i386', '-V', '-s', '__TEXT', '__textcoal_nt', str(options.binary)]), refs,
                                          addresses, image.data, [vars(s) for s in image.sections], symbol_addresses)
    raw = disassembly_features(run(['otool', '-arch', 'i386', '-tv', str(options.binary)]) + '\n' +
                               run(['otool', '-arch', 'i386', '-v', '-s', '__TEXT', '__textcoal_nt', str(options.binary)]), refs,
                               addresses, image.data, [vars(s) for s in image.sections], symbol_addresses)
    for ref in refs:
        f = raw[ref['address']]
        f['callees'] = sorted(set(f['callees']) | set(features[ref['address']]['callees']) | set(coal_verbose[ref['address']]['callees']))
        ref.update(f)
    rows = build_report(refs, functions)
    flagged = [r for r in rows if r.get('comparison', {}).get('score', 0) >= 8]
    unflagged = [r for r in rows if 'comparison' in r and r['comparison']['score'] < 8]
    # Sample owned engine functions, excluding bundled codecs and platform adapters.
    def owned(row):
        file = row['native']['file']
        return file.startswith('src/PC/') and not any('/' + p + '/' in file for p in ('speex', 'zlib', 'jpeg-6'))
    rng = random.Random(options.sample_seed)
    sample = {group: [dict(name=r['reference']['name'], address=r['reference']['address'], file=r['native']['file'])
                      for r in rng.sample([r for r in pool if owned(r)], min(30, sum(owned(r) for r in pool)))]
              for group, pool in [('flagged', flagged), ('unflagged', unflagged)]}
    result = dict(schema=1, binary=str(options.binary), binary_sha256=hashlib.sha256(image.data).hexdigest(),
                  compile_commands=str(options.compile_commands.resolve()), source_commit=run(['git', 'rev-parse', 'HEAD'], cwd=ROOT).strip(),
                  counts=dict(reference=len(refs), native=len(functions), status=dict(Counter(r['status'] for r in rows)), flagged=len(flagged), unflagged=len(unflagged)),
                  functions=rows, native_only=native_only_context(rows, functions), dvars=dvar_report(rows), sample_seed=options.sample_seed, sample=sample,
                  stub_candidates=stubs)
    # Annotate native global uses through blob/import storage without assuming these are bugs.
    blob_source = '\n'.join(p.read_text(errors='replace') for p in (ROOT / 'src/blobs').glob('*') if p.is_file())
    blob_names = {name.removeprefix('_') for names in re.findall(r'^([A-Za-z_]\w*):|\b(?:char|int|float|byte)\s+(\w+)\s*[\[;=]', blob_source, re.M) for name in names if name}
    for row in rows:
        if 'native' in row:
            row['storage_references'] = [n for n in row['native']['variables'] if n.startswith('imp_') or n in blob_names]
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps(result['counts'], indent=2))
    print(f'Dvars: {result["dvars"]["compared_count"]} compared, {len(result["dvars"]["mismatches"])} candidates')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
