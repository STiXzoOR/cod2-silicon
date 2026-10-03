#!/usr/bin/env python3
"""Audit renderer table bindings, named function casts, and indirect FP calls.

Clang supplies both record-field types and definition signatures. Source text
is used only to identify RE(slot, symbol) and ri_local field bindings. This
complements audit.py: casts to void * otherwise hide the table contracts.
Reviewed residual findings require exact keys in the supplied baseline; new
ABI mismatches and unprototyped indirect calls with floating arguments fail.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from audit import abi_type, compiler_args


def split_params(text):
    result, start, depth = [], 0, 0
    for i, ch in enumerate(text):
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        elif ch == ',' and not depth:
            result.append(text[start:i].strip())
            start = i + 1
    result.append(text[start:].strip())
    return [] if result == ['void'] or result == [''] else result


def signature_abi(signature, aliases):
    signature = aliases.get(signature, signature)
    signature = re.sub(r'\s*__attribute__\(\(\w+\)\)', '', signature)
    match = re.match(r'^(.*?)\s*\(\*\)\((.*)\)$', signature)
    if match is None:
        return None
    ret, params = match.groups()
    values = split_params(params)
    return {'abi': [abi_type(ret, aliases)] +
                   [abi_type(p, aliases) for p in values if p != '...'],
            'variadic': '...' in values, 'unprototyped': params == ''}


def direct_signature_abi(signature, aliases):
    signature = re.sub(r'\s*__attribute__\(\(\w+\)\)', '', signature)
    match = re.match(r'^(.*?)\s*\((.*)\)$', signature)
    if match is None:
        return None
    ret, params = match.groups()
    return signature_abi(ret + ' (*)(' + params + ')', aliases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compile_commands', type=Path)
    parser.add_argument('--audit', type=Path, help='Completed audit.py JSON')
    parser.add_argument('--cache', type=Path, help='Alternative audit.py cache directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, help='Reviewed finding keys, as a JSON list')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    commands = json.loads(args.compile_commands.read_text())
    entry = next(e for e in commands if e['file'].endswith('/r_init.c') and
                 '-DDEDICATED' not in e.get('command', ''))
    proc = subprocess.run(compiler_args(entry), cwd=entry['directory'], capture_output=True)
    if proc.returncode:
        sys.stderr.write(proc.stderr.decode(errors='replace'))
        return 2
    tree = json.loads(proc.stdout)
    aliases, tables, local_definitions = {}, {}, {}
    for node in tree.get('inner', []):
        kind = node['kind']
        if kind == 'TypedefDecl':
            typ = node.get('type', {})
            aliases[node['name']] = typ.get('desugaredQualType', typ.get('qualType', node['name']))
        elif kind == 'RecordDecl' and node.get('name') in ('refexport_t', 'refimport_t'):
            fields = [f for f in node.get('inner', []) if f['kind'] == 'FieldDecl']
            if fields:
                tables[node['name']] = fields
        elif kind == 'FunctionDecl' and any(c['kind'] == 'CompoundStmt' for c in node.get('inner', [])):
            typ = node['type']['qualType']
            ret = re.split(r'\s*\(', typ, maxsplit=1)[0]
            params = [p['type'].get('desugaredQualType', p['type']['qualType'])
                      for p in node['inner'] if p['kind'] == 'ParmVarDecl']
            local_definitions[node['name']] = {'name': node['name'], 'signature': typ,
                'file': 'src/PC/gfx_d3d/r_init.c', 'line': node.get('loc', {}).get('line'),
                'abi': [abi_type(ret, aliases)] + [abi_type(p, aliases) for p in params],
                'variadic': node.get('variadic', False), 'definition': True}
    if args.audit:
        audit = json.loads(args.audit.read_text())
        declarations = audit['declarations']
        casts = audit.get('function_casts', [])
        indirect = audit.get('indirect_calls', [])
    elif args.cache:
        latest = {}
        for path in sorted(args.cache.glob('*.json'), key=lambda p: p.stat().st_mtime):
            data = json.loads(path.read_text())
            latest[data['file']] = data
        declarations = [d for result in latest.values() for d in result.get('declarations', [])]
        casts = [c for result in latest.values() for c in result.get('function_casts', [])]
        indirect = [c for result in latest.values() for c in result.get('indirect_calls', [])]
    else:
        parser.error('--audit or --cache is required')
    definitions = {d['name']: d for d in declarations if d['definition']}
    definitions.update(local_definitions)
    bindings = []
    re_path = root / 'src/PC/gfx_d3d/r_init.c'
    re_text = re_path.read_text()
    for match in re.finditer(r'\bRE\((\d+),\s*(\w+)\)', re_text):
        slot, symbol = match.groups()
        bindings.append(('refexport_t', int(slot) // 4, symbol.removeprefix('imp_'),
                         'src/PC/gfx_d3d/r_init.c', re_text.count('\n', 0, match.start()) + 1))
    ri_path = root / 'src/PC/client_mp/cl_main_mp.c'
    ri_text = ri_path.read_text()
    ri_fields = {f['name']: i for i, f in enumerate(tables['refimport_t'])}
    for match in re.finditer(r'\bri_local\.(\w+)\s*=\s*(?:\(void\s*\*\))?\s*(\w+)\s*;', ri_text):
        field, symbol = match.groups()
        if field in ri_fields:
            bindings.append(('refimport_t', ri_fields[field], symbol.removeprefix('imp_'),
                             'src/PC/client_mp/cl_main_mp.c', ri_text.count('\n', 0, match.start()) + 1))
    findings, inventory = [], []
    for table, slot, symbol, file, line in bindings:
        field = tables[table][slot]
        signature = field['type']['qualType']
        expected = signature_abi(signature, aliases)
        definition = definitions.get(symbol)
        record = {'table': table, 'slot': slot, 'field': field['name'], 'symbol': symbol,
                  'file': file, 'line': line, 'field_signature': signature, 'definition': definition}
        if expected is None:
            record['status'] = 'non-callable'
        elif definition is None:
            record['status'] = 'definition-missing'
        elif expected['unprototyped']:
            record['status'] = 'unprototyped'
        else:
            reasons = []
            if expected['variadic'] != definition['variadic']:
                reasons.append('variadic')
            if expected['abi'] != definition['abi']:
                reasons.append('abi')
            record['status'] = 'mismatch' if reasons else 'compatible'
            if reasons:
                record['reasons'] = reasons
                record['field_abi'] = expected['abi']
                record['key'] = '|'.join((table, field['name'], symbol, signature, definition['signature']))
                findings.append(record)
        inventory.append(record)
    # Inventory named casts even when the target is unprototyped. ABI-compatible
    # pointer types can differ in spelling; void-return casts deliberately discard
    # results but are still listed as contract mismatches.
    cast_inventory = {}
    for cast in casts:
        source = (signature_abi if cast.get('field') else direct_signature_abi)(cast['source'], aliases)
        target = signature_abi(cast['target'], aliases)
        if source is None or target is None:
            continue  # audit.py also captures pointer-to-array casts.
        record = dict(cast)
        if cast.get('storage'):
            record['status'] = 'generic-storage'
        elif source['unprototyped'] or target['unprototyped']:
            record['status'] = 'unprototyped'
        else:
            record['status'] = 'compatible' if source == target else 'mismatch'
        record['key'] = '|'.join(('cast', cast['file'], cast['name'], cast['source'], cast['target']))
        key = '|'.join(str(cast[k]) for k in ('file', 'line', 'name', 'source', 'target'))
        cast_inventory[key] = record
    named_mismatches = {c['key']: c for c in cast_inventory.values() if c['status'] == 'mismatch'}
    float_calls = {}
    for call in indirect:
        signature = signature_abi(call['signature'], aliases)
        if signature and signature['unprototyped'] and any(
                abi_type(t, aliases) in ('float', 'double') for t in call['args']):
            record = dict(call)
            record['key'] = '|'.join(('unprototyped-float', call['file'], str(call['line']),
                                      call['signature'], ','.join(call['args'])))
            float_calls[record['key']] = record
    fields = []
    for table, members in tables.items():
        bound = {r['slot'] for r in inventory if r['table'] == table}
        for slot, field in enumerate(members):
            fields.append({'table': table, 'slot': slot, 'name': field['name'],
                           'signature': field['type']['qualType'], 'bound': slot in bound})
    report = {'field_counts': {k: len(v) for k, v in tables.items()}, 'fields': fields,
              'bindings': inventory, 'mismatches': findings,
              'named_function_casts': list(cast_inventory.values()),
              'named_cast_mismatches': list(named_mismatches.values()),
              'indirect_call_count': len({(c['file'], c['line'], c['signature']) for c in indirect}),
              'unprototyped_float_calls': list(float_calls.values())}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    baseline = set(json.loads(args.baseline.read_text())) if args.baseline else set()
    failures = findings + list(named_mismatches.values()) + list(float_calls.values())
    new = [f for f in failures if f['key'] not in baseline]
    print(f'{len(inventory)} renderer bindings; {len(findings)} table mismatches; '
          f'{len(named_mismatches)} named cast mismatches; {len(float_calls)} '
          f'unprototyped floating calls; {len(new)} new')
    for finding in new[:20]:
        print(f"{finding['file']}:{finding['line']}: {finding['key']}")
    if len(new) > 20:
        print(f'{len(new) - 20} more findings are recorded in {args.output}')
    return bool(new)


if __name__ == '__main__':
    sys.exit(main())
