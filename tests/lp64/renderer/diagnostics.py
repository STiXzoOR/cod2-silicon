#!/usr/bin/env python3
"""Compare owned CMake syntax diagnostics to a baseline in an ignored local export.

Run from the checkout root. The export contains only tracked source/headers;
reference binaries and assets are never copied. Counts deduplicate both targets.
"""
import argparse, concurrent.futures, csv, importlib.util, json, pathlib, shlex, subprocess, tarfile, io
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--base", default="a2f4477")
parser.add_argument("--build", default="build-macos")
args = parser.parse_args()
root=pathlib.Path.cwd(); build=root/args.build; baseline=build/'ws6-baseline-source'
prefixes=('src/PC/gfx_d3d/','src/Mac/DirectX_9/','src/Mac/DirectX 9/')
paths=subprocess.check_output(['git','ls-tree','-r','--name-only',args.base,'src'],text=True).splitlines()
paths=[p for p in paths if p.endswith('.h') or (p.startswith(prefixes) and p.endswith('.c'))]
baseline.mkdir(exist_ok=True)
archive=subprocess.check_output(['git','archive',args.base,'--',*paths])
with tarfile.open(fileobj=io.BytesIO(archive)) as t: t.extractall(baseline, filter='data')
entries=json.loads((build/'compile_commands.json').read_text()); entries=[e for e in entries if str(pathlib.Path(e['file']).relative_to(root)).startswith(prefixes)]
spec=importlib.util.spec_from_file_location('inv',root/'tools/macos-port/lp64_inventory.py');inv=importlib.util.module_from_spec(spec);spec.loader.exec_module(inv)
def probe(job):
 label,e=job;argv=shlex.split(e['command']);argv=argv[:argv.index('-o')]+['-fsyntax-only','-fno-color-diagnostics',e['file']]
 if label=='before': argv=[a.replace(str(root/'src'),str(baseline/'src')) for a in argv]
 r=subprocess.run(argv,cwd=e['directory'],capture_output=True,text=True)
 source=str(pathlib.Path(e['file']).relative_to(root));target='ded' if 'cod2_macos_ded.dir' in e['command'] else 'client'
 dest=build/('ws6-probes-'+label)/target/(source.replace('/','__')+'.log');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(r.stderr.replace(str(baseline)+'/',''))
 return label,source,r.returncode,dest
with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool: results=list(pool.map(probe,[(l,e) for l in ['before','after'] for e in entries]))
for label in ['before','after']:
 records=set()
 for l,s,rc,p in results:
  if l!=label:continue
  if rc: print('FAILED',label,s,rc)
  for line in p.read_text().splitlines():
   m=inv.DIAGNOSTIC.match(line)
   if m:
    n,row,col,sev,msg,flag=m.groups();n=n.removeprefix(str(root)+'/');records.add((n,int(row),int(col),sev,flag or '',msg))
 with (build/('ws6-diagnostics-'+label+'.tsv')).open('w') as f:
  w=csv.writer(f,delimiter='\t');w.writerow(['file','line','column','severity','flag','message']);w.writerows(sorted(records))
 for prefix in prefixes:
  owned=[r for r in records if r[0].startswith(prefix)];warn=[r for r in owned if r[3]=='warning'];focus=[r for r in warn if r[4] in inv.FOCUS];errors=[r for r in owned if r[3]!='warning']
  print(label,prefix,'warnings',len(warn),'LP64',len(focus),'errors',len(errors))
print(len(entries),'owned translation-unit entries per version')
