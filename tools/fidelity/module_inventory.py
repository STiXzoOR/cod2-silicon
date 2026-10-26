#!/usr/bin/env python3
"""Join private function indexes into a code-free module reconciliation ledger."""
import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path
import posixpath


def module_name(source):
    source = posixpath.normpath(source)
    if source.startswith(('/Developer/', '/SourceCache/')):
        return 'system/' + source.lstrip('/')
    if source.startswith('PC/'):
        source = source[3:]
    parts = source.split('/')
    if parts[0] in ('game_mp', 'server_mp', 'client_mp', 'cgame_mp', 'ui_mp'):
        parts[0] = parts[0][:-3]
    return str(Path('/'.join(parts)).with_suffix(''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mac', type=Path, required=True)
    parser.add_argument('--linux', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--decisions', type=Path, help='Reviewed per-function status and evidence overrides')
    args = parser.parse_args()
    mac = json.loads(args.mac.read_text())
    linux = json.loads(args.linux.read_text())
    rows = []
    for function in mac['functions']:
        sources = function['sources']
        module = module_name(sources[0]) if len(sources) == 1 else '__unmatched__'
        categories = function['categories']
        scope = ('upstream-library' if categories == ['bundled_third_party'] else
                 'unassigned' if categories == ['system_runtime'] else
                 'unassigned' if module == '__unmatched__' else 'engine')
        rows.append(dict(id='mac:' + function['address_hex'], module=module,
                         names=function['friendly_names'], scope=scope,
                         mac=[dict(address=function['address_hex'], symbols=function['raw_symbols'],
                                   export_ids=function['export_ids'], source_module=module)], linux=[]))
    for function in mac['stabs_without_export']:
        rows.append(dict(id='mac:' + function['address_hex'],
                         module=module_name(function['normalized_source']), names=[function['name']],
                         scope='engine', mac=[dict(address=function['address_hex'],
                         symbols=[function['symbol']], export_ids=[], source_module=module_name(function['normalized_source']))], linux=[]))

    mac_keys = defaultdict(list)
    mac_names = defaultdict(list)
    for row in rows:
        for name in set(row['names']):
            mac_keys[(row['module'], name)].append(row)
            mac_names[name].append(row)
    def linux_name(function):
        return function.get('qualified_name', function['name']).replace('::', '_')
    linux_keys = Counter((module_name(f['source']), linux_name(f))
                         for f in linux['functions'])
    linux_names = Counter(linux_name(f) for f in linux['functions'])
    for function in linux['functions']:
        module = module_name(function['source'])
        name = linux_name(function)
        key = (module, name)
        definition = {k: function[k] for k in ('source', 'line', 'name', 'signature', 'static')}
        matches = mac_keys[key]
        if len(matches) == 1 and linux_keys[key] == 1:
            matches[0]['linux'].append(definition)
            matches[0]['pairing'] = 'unique module and name; semantics unchecked'
            if function['classification'] == 'game_owned':
                matches[0]['scope'] = 'engine'
        elif not function['static'] and len(mac_names[name]) == 1 and linux_names[name] == 1 and not mac_names[name][0]['linux']:
            mac_names[name][0]['linux'].append(definition)
            mac_names[name][0]['pairing'] = 'unique name across source modules; identity and semantics require review'
            mac_names[name][0]['module'] = module
            if mac_names[name][0]['scope'] == 'unassigned' or function['classification'] == 'game_owned':
                mac_names[name][0]['scope'] = 'engine'
        else:
            rows.append(dict(id=f"linux:{function['source']}:{function['line']}:{function['name']}",
                             module=module, names=[function['name']],
                             scope='upstream-library' if function['classification'] == 'bundled_library' else 'engine',
                             mac=[], linux=[definition]))
    for candidate in linux['validation']['unmatched_candidates']:
        rows.append(dict(id=f"candidate:{candidate['source']}:{candidate['line']}:{candidate['name']}",
                         module=module_name(candidate['source']), names=[candidate['name']],
                         scope='upstream-library' if candidate['source'].startswith('zlib/') else 'unassigned',
                         mac=[], linux=[], candidate={k: candidate[k] for k in ('source', 'line', 'name')}))
    decisions = json.loads(args.decisions.read_text()) if args.decisions else {}
    assert set(decisions) <= {r['id'] for r in rows}, 'Decisions name missing function identities'
    modules = defaultdict(list)
    for row in rows:
        if row['module'].startswith('botlib/'):
            row['scope'] = 'engine'
        row['status'] = 'unchecked'
        row['evidence'] = ''
        if row['scope'] == 'upstream-library':
            row['status'] = 'intentional'
            row['evidence'] = 'Faithful-decompilation plan: bundled upstream libraries are not re-derived; no semantic-equivalence claim.'
        if row['id'] in decisions:
            decision = decisions[row['id']]
            assert decision['status'] in ('equal', 'fixed', 'intentional', 'unchecked')
            assert decision['evidence'], 'Every explicit decision needs evidence'
            row.update(status=decision['status'], evidence=decision['evidence'])
        modules[row.pop('module')].append(row)
    for functions in modules.values():
        functions.sort(key=lambda f: (f['names'][0], f['id']))
    assert sum(len(f['mac']) for f in rows) == mac['summary']['unique_export_addresses'] + len(mac['stabs_without_export'])
    assert sum(len(f['linux']) for f in rows) == len(linux['functions'])
    assert len({f['id'] for f in rows}) == len(rows)
    summary = dict(mac_exports=mac['summary']['export_records'], mac_stabs_only=len(mac['stabs_without_export']),
                   linux_definitions=len(linux['functions']), linux_conditional_candidates=len(linux['validation']['unmatched_candidates']),
                   ledger_rows=len(rows), matched_pairs=sum(bool(f['mac'] and f['linux']) for f in rows),
                   statuses=dict(Counter(f['status'] for f in rows)), scopes=dict(Counter(f['scope'] for f in rows)))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'functions.json').write_text(json.dumps(dict(schema=1, summary=summary,
        modules=dict(sorted(modules.items()))), indent=2) + '\n')
    with (args.output / 'modules.csv').open('w', newline='') as stream:
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(['module', 'total', 'equal', 'fixed', 'intentional', 'unchecked'])
        for name, functions in sorted(modules.items()):
            counts = Counter(f['status'] for f in functions)
            writer.writerow([name, len(functions), *[counts[s] for s in ('equal', 'fixed', 'intentional', 'unchecked')]])
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
