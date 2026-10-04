#!/usr/bin/env python3
"""Compare clang's cross-TU function signatures and inventory indirect calls.

Uses the compile database, never a source regex as a substitute for C types.
JSON output includes all project declarations, unprototyped calls and casts.
An optional reviewed baseline makes the command fail on new ABI mismatches.
"""
import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys


def canonical(typ, aliases):
    for _ in range(12):
        new = re.sub(r'\b(?:struct|union|enum)\s+[A-Za-z_]\w*|\b[A-Za-z_]\w*\b',
                     lambda m: aliases.get(m[0], m[0]), typ)
        if new == typ:
            break
        typ = new
    return re.sub(r'\b(const|volatile|restrict|__restrict)\b', '', typ).strip()


def abi_type(typ, aliases):
    typ = canonical(typ, aliases)
    if '*' in typ or '[' in typ or typ.endswith('&'):
        return 'pointer'
    if typ == 'void':
        return 'void'
    if 'long double' in typ:
        return 'long-double'
    if re.search(r'\bdouble\b', typ):
        return 'double'
    if re.search(r'\bfloat\b', typ):
        return 'float'
    if re.search(r'\blong\b', typ):
        return 'integer64'
    if re.search(r'\b(char|short|int|_Bool|bool)\b', typ) or typ.startswith('enum '):
        # Narrow integers use the same GPR, but Darwin extends at the caller.
        return 'integer32'
    return 'aggregate:' + re.sub(r'\s+', ' ', typ)


def compiler_args(entry):
    args = entry.get('arguments') or shlex.split(entry['command'])
    result, skip = [], False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', entry['file']):
            result.append(arg)
    return result + ['-w', '-fsyntax-only', '-Xclang', '-ast-dump=json', entry['file']]


def summarize(entry, root, cache, header_digest):
    path = Path(entry['file'])
    args = compiler_args(entry)
    digest = hashlib.sha256((str(args) + header_digest).encode() + path.read_bytes()).hexdigest()
    cached = cache / (digest + '.json')
    if cached.exists():
        return json.loads(cached.read_text())
    proc = subprocess.run(args, cwd=entry['directory'], capture_output=True)
    if proc.returncode:
        return {'file': str(path), 'error': proc.stderr.decode(errors='replace')}
    tree = json.loads(proc.stdout)
    aliases = {}
    for n in tree.get('inner', []):
        if n['kind'] == 'TypedefDecl':
            t = n.get('type', {})
            aliases[n['name']] = t.get('desugaredQualType', t.get('qualType', n['name']))
    records, unproto, indirect, casts = [], [], [], []
    files = {}
    last_file = str(path)

    def location(node, inherited, include_system=False):
        nonlocal last_file
        loc = node.get('loc', node.get('range', {}).get('begin', {}))
        loc = loc.get('expansionLoc', loc)
        if 'file' in loc:
            last_file = loc['file']
        file = loc.get('file', inherited or last_file)
        system = not str(file).startswith(str(root))
        if system and not include_system:
            return None
        if file not in files:
            try:
                files[file] = Path(file).read_text(errors='replace')
            except OSError:
                files[file] = ''
        offset = loc.get('offset', 0)
        line = loc.get('line', files[file].count('\n', 0, offset) + 1)
        return {'file': str(file) if system else str(Path(file).relative_to(root)),
                'line': line, 'system': system}

    def refs(node):
        if node.get('referencedDecl', {}).get('kind') == 'FunctionDecl':
            yield node['referencedDecl']
        for child in node.get('inner', []):
            yield from refs(child)

    def fields(node):
        typ = node.get('type', {})
        if node.get('kind') == 'MemberExpr' and '(*' in typ.get('desugaredQualType', typ.get('qualType', '')):
            yield node
        for child in node.get('inner', []):
            yield from fields(child)

    def visit(node, inherited=None, in_initializer=False):
        kind = node.get('kind', '')
        loc = location(node, inherited, kind == 'FunctionDecl')
        file = str(root / loc['file']) if loc else inherited
        typ = node.get('type', {}).get('qualType', '')
        children = node.get('inner', [])
        if kind == 'FunctionDecl' and loc and node.get('storageClass') != 'static':
            params = [p.get('type', {}).get('desugaredQualType', p.get('type', {}).get('qualType', ''))
                      for p in children if p['kind'] == 'ParmVarDecl']
            ret = typ.split(' (', 1)[0]
            records.append(dict(loc, name=node['name'], signature=typ,
                                symbol=node.get('mangledName', node['name']),
                                definition=any(p['kind'] == 'CompoundStmt' for p in children),
                                variadic=node.get('variadic', False),
                                unprototyped=typ.endswith('()'),
                                abi=[abi_type(ret, aliases)] + [abi_type(p, aliases) for p in params]))
        if kind == 'CallExpr' and loc and children:
            targets = list(refs(children[0]))
            if targets:
                ref = targets[0]
                if ref.get('type', {}).get('qualType', '').endswith('()'):
                    unproto.append(dict(loc, name=ref['name'], signature=ref['type']['qualType'],
                                        args=[p.get('type', {}).get('qualType', '') for p in children[1:]]))
            else:
                callee_type = children[0].get('type', {})
                indirect.append(dict(loc, signature=callee_type.get('desugaredQualType', callee_type.get('qualType', '')),
                                     args=[p.get('type', {}).get('qualType', '') for p in children[1:]]))
        cast_type = node.get('type', {}).get('desugaredQualType', typ)
        if kind == 'CStyleCastExpr' and loc and '(*' in cast_type:
            for ref in refs(node):
                ref_type = ref.get('type', {})
                casts.append(dict(loc, name=ref['name'], source=ref_type.get('desugaredQualType', ref_type.get('qualType')),
                                  target=cast_type, target_spelling=typ,
                                  storage=in_initializer and typ == 'fnptr_t'))
            for field in fields(node):
                field_type = field['type']
                casts.append(dict(loc, name=field.get('name', '<field>'),
                                  source=field_type.get('desugaredQualType', field_type['qualType']), target=cast_type, field=True,
                                  target_spelling=typ, storage=in_initializer and typ == 'fnptr_t'))
        for child in children:
            visit(child, file if kind == 'FunctionDecl' else inherited,
                  in_initializer or kind == 'InitListExpr')

    # Header records account for most of this decompiler-derived AST. Their
    # fields cannot contain calls; do not walk tens of thousands of them per TU.
    def top(node):
        nonlocal last_file
        if 'file' in node.get('loc', {}):
            last_file = node['loc']['file']
        if node['kind'] in ('FunctionDecl', 'VarDecl'):
            visit(node)
        elif node['kind'] in ('TranslationUnitDecl', 'LinkageSpecDecl', 'NamespaceDecl'):
            for child in node.get('inner', []):
                top(child)

    top(tree)
    result = {'file': str(path), 'declarations': records, 'unprototyped_calls': unproto,
              'indirect_calls': indirect, 'function_casts': casts}
    cached.write_text(json.dumps(result))
    return result


def mismatches(declarations):
    groups = {}
    for d in declarations:
        groups.setdefault(d.get('symbol', d['name']), []).append(d)
    findings = []
    for symbol, ds in groups.items():
        name = ds[0]['name']
        definitions = [d for d in ds if d['definition']]
        if not definitions:
            # SDK headers are authoritative for imported APIs (qsort, malloc,
            # memcpy, etc.) even though their definitions are in system dylibs.
            definitions = [d for d in ds if d.get('system')]
        for definition in definitions:
            for declaration in ds:
                if declaration.get('system'):
                    continue
                reasons = []
                if declaration['unprototyped'] and not declaration['definition']:
                    continue  # Calls through these are reported separately.
                if definition['variadic'] != declaration['variadic']:
                    reasons.append('variadic')
                if len(definition['abi']) != len(declaration['abi']):
                    reasons.append('parameter-count')
                if definition['abi'][0] != declaration['abi'][0]:
                    reasons.append('return')
                if definition['abi'][1:] != declaration['abi'][1:] and len(definition['abi']) == len(declaration['abi']):
                    reasons.append('parameters')
                if reasons:
                    findings.append({'name': name, 'definition': definition, 'declaration': declaration, 'reasons': reasons})
    unique = {finding_key(f): f for f in findings}
    return [unique[k] for k in sorted(unique)]


def finding_key(f):
    d, c = f['definition'], f['declaration']
    return '|'.join([f['name'], d['file'], d['signature'], c['file'], c['signature']])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compile_commands', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--target', default='', help='Optional compile command substring')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    commands = json.loads(options.compile_commands.read_text())
    commands = [c for c in commands if c['file'].endswith(('.c', '.cpp', '.m'))
                and options.target in c.get('command', str(c.get('arguments', [])))
                and Path(c['file']).is_relative_to(root / 'src')]
    cache = options.cache or options.output.parent / 'audit-cache'
    cache.mkdir(parents=True, exist_ok=True)
    headers = hashlib.sha256()
    headers.update(Path(__file__).read_bytes())
    for p in sorted((root / 'src').rglob('*.h')):
        headers.update(p.read_bytes())
    results = []
    with ProcessPoolExecutor(max_workers=options.jobs) as executor:
        futures = [executor.submit(summarize, c, root, cache, headers.hexdigest()) for c in commands]
        for i, future in enumerate(as_completed(futures), 1):
            results.append(future.result())
            if i % 50 == 0:
                print(f'{i}/{len(commands)} translation units', flush=True)
    declarations = [d for r in results for d in r.get('declarations', [])]
    findings = mismatches(declarations)
    floating_calls = [c for r in results for c in r.get('unprototyped_calls', [])
                      if any(t in ('float', 'double', 'long double') for t in c['args'])]
    report = {'translation_units': len(commands), 'errors': [r for r in results if 'error' in r],
              'unprototyped_floating_calls': floating_calls,
              'declarations': declarations, 'mismatches': findings,
              **{k: [d for r in results for d in r.get(k, [])]
                 for k in ('unprototyped_calls', 'indirect_calls', 'function_casts')}}
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(report, indent=2) + '\n')
    baseline = set(json.loads(options.baseline.read_text())) if options.baseline else set()
    new = [f for f in findings if finding_key(f) not in baseline]
    print(f"{len(commands)} TUs; {len(report['errors'])} errors; {len(findings)} mismatches; {len(new)} new")
    return bool(report['errors'] or new or floating_calls)


if __name__ == '__main__':
    sys.exit(main())
