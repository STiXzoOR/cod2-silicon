#!/usr/bin/env python3
"""Compare the dormant production recording queue with owned Mac instructions.

Synthetic inputs only. Full observations and original code stay outside Git.
The recorder remains disabled; codec/network services are explicit models.
"""
import argparse
import collections
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys

BACKGROUND = ['/usr/sbin/taskpolicy', '-b', 'nice', '-n', '19']
REFERENCE_SHA256 = '15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12'
SYMBOL = '__Z32Record_QueueAudioDataForEncodingP13audioSample_t'
FIELDS = ['events', 'result', 'offset', 'partial_count', 'volume_bits', 'scale_bits',
          'enc_pos', 'pcm', 'partial', 'encoded_changes', 'guards_preserved']

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def save(path, obj):
    path.write_text(json.dumps(obj, indent=2) + '\n')

def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def cases():
    rows = []
    def add(n, partial=0, scale=4, send=1, talking=1, encoded=3, network=0, position=0):
        rows.append(dict(id=len(rows), count=n, initial=partial, scale_code=scale,
                         send=send, talking=talking, frame=160, encoded=encoded,
                         network=network, position=position, initial_offset=67))
    counts = [1, 159, 160, 161, 319, 320, 321, 479, 480]
    for n in counts: add(n, encoded=1)
    for partial in [1, 37, 156]:
        for n in counts: add(n, partial=partial)
    for n in counts[1:]: add(n, partial=159)
    for scale in [0, 2, 5, -2, 8, 6]:
        for n in [161, 320]:
            for partial in [0, 37]: add(n, partial=partial, scale=scale)
    for send, talking in [(0, 0), (0, 1), (1, 0)]:
        for scale in [2, 4, -2]:
            for partial in [0, 37]: add(321, partial, scale, send, talking)
    for network in [-1, 1]:
        for position in [7, 4093]: add(480, 37, encoded=3, network=network, position=position)
    return rows

def values(case, capacity):
    return [i+1 if case['id'] < 9 else ((i*73+19)%1801)-900 for i in range(capacity)]

def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]

def synthetic(case, exact):
    """Independent stream expectation, including observed pre-existing partial clamp."""
    c = case; n = c['count']; pcm = values(c, n if exact else 1024)
    pcm[:n] = [int(x*c['scale_code']/4) for x in pcm[:n]]
    partial = [-3000+i if i<c['initial'] else 7000+i for i in range(640)]
    row = dict(id=c['id'], events=[dict(service='Voice_SendVoiceData')], result=0,
               offset=c['initial_offset'], partial_count=c['initial'],
               volume_bits=bits(sum(abs(x) for x in pcm[:n])/n),
               scale_bits=bits(c['scale_code']/4), enc_pos=c['position'],
               pcm=pcm, partial=partial, encoded_changes=[], guards_preserved=True)
    if not c['send']: return row
    row['events'].append(dict(service='IsTalking'))
    if not c['talking']:
        row['events'].append(dict(service='CL_VoiceTransmit')); return row
    row['events'].append(dict(service='Encode_GetFrameSize')); row['offset'] = 0
    off = 0; p = c['initial']; total = 0
    while n-off+p >= c['frame']:
        take = min(p, c['frame']-4)
        frame = partial[:take] + pcm[off:off+c['frame']-take]
        assert len(frame) == c['frame']
        off += c['frame']-take; p = 0; total += c['encoded']
        row['events'].append(dict(service='Encode_Sample', avail=4096-c['position'],
                                  out_offset=c['position'], pcm=frame))
        row['events'].append(dict(service='Client_SendVoiceData', length=c['encoded'],
                                  offset=c['position'], bytes=list(range(0x51,0x51+c['encoded']))))
    partial[p:p+n-off] = pcm[off:n]; p += n-off
    row.update(result=total, offset=off, partial_count=p)
    if total:
        row['encoded_changes'] = [[c['position']+i,0x51+i] for i in range(c['encoded'])]
    return row

def compare(actual, expected):
    ids = [r.get('id') for r in actual]
    target = [r['id'] for r in expected]
    if ids != target or len(set(ids)) != len(ids):
        raise ValueError('Missing, duplicate, or reordered observations')
    return [dict(id=a['id'], fields=[f for f in FIELDS if a.get(f) != b[f]])
            for a,b in zip(actual, expected) if any(a.get(f) != b[f] for f in FIELDS)]

def original(binary, reference_module, rows, exact, out):
    import unicorn
    from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX,
                                   UC_X86_REG_FPCW, UC_X86_REG_MXCSR)
    class QueueReference(reference_module.MacReference):
        def _import(self, mu, address, size, user):
            name = self.models.get(address, self.stubs.get(address))
            if name != '_memcpy': raise RuntimeError(f'Unexplained import {name!r}')
            esp = mu.reg_read(UC_X86_REG_ESP)
            dest, src, length = struct.unpack('<3I', mu.mem_read(esp+4,12))
            logical = None
            for tag,base,limit in [('pcm',self.pcm,self.count*2),('partial',self.partial,1280)]:
                if base <= src and src+length <= base+limit: logical=[tag,src-base,length]
            if logical is None: raise ValueError(f'Original copy outside declared input: {src:#x}, {length}')
            self.copies.append(dict(source=logical, destination=('partial' if self.partial<=dest<self.partial+1280 else 'stack'), destination_offset=(dest-self.partial if self.partial<=dest<self.partial+1280 else None), length=length))
            if not (self.partial<=dest and dest+length<=self.partial+1280 or 0x70000000<=dest and dest+length<=0x70010000):
                raise ValueError('Unexpected original imported write')
            super()._import(mu,address,size,user)
    ref = QueueReference(binary); mu = ref.mu
    assert ref.symbols[SYMBOL] == 0x1ea748
    names = dict(Voice_SendVoiceData='__Z19Voice_SendVoiceDatav', IsTalking='__Z9IsTalkingv',
                 CL_VoiceTransmit='__Z16CL_VoiceTransmitv', Encode_GetFrameSize='__Z19Encode_GetFrameSizev',
                 Encode_Sample='__Z13Encode_SamplePsPci', Client_SendVoiceData='__Z20Client_SendVoiceDataiPc')
    # Resolve names from the actual nlist rather than assuming guessed C++ lengths.
    services = {}
    for name in names:
        hits = [(k,v) for k,v in ref.symbols.items() if k == names[name]]
        if len(hits)!=1: raise ValueError((name,hits))
        services[hits[0][1]]=name
    addr = ref.symbols[SYMBOL]
    # Use the raw named STABS length, not the following symbol (which can include padding).
    raw = binary.read_bytes()
    offset = 28
    for _ in range(struct.unpack_from('<I', raw, 16)[0]):
        command, size = struct.unpack_from('<II', raw, offset)
        if command == 2:
            symoff, nsyms, stroff, _ = struct.unpack_from('<4I', raw, offset + 8)
        offset += size
    start_record = None
    end = None
    for index in range(nsyms):
        string, kind, section, desc, value = struct.unpack_from('<IBBHI', raw, symoff + index*12)
        if kind != 0x24:
            continue
        name = raw[stroff+string:raw.index(b'\0', stroff+string)].decode()
        if start_record is not None:
            if name or value == 0:
                raise ValueError('Missing original named function end record')
            end = addr + value
            break
        if name.startswith(SYMBOL[1:] + ':F') and value == addr:
            start_record = index
    if end is None:
        raise ValueError('Original named function extent absent')
    ref.partial=ref.symbols['_partial_audio_buffer']; enc=ref.symbols['_enc_buffer']
    partial_count=ref.symbols['_samples_in_partial_audio_buffer']; enc_pos=ref.symbols['_enc_buffer_pos']
    scaler,volume=ref.alloc(4),ref.alloc(4)
    mu.mem_write(0x1acda05,struct.pack('<I',scaler));mu.mem_write(0x1acda09,struct.pack('<I',volume))
    observations=[]; private=[]; visited=set(); current={}
    def hook(mu,address,size,user):
        visited.add(address)
        if address in services:
            name=services[address]; esp=mu.reg_read(UC_X86_REG_ESP)
            args=struct.unpack('<3I',mu.mem_read(esp+4,12));c=current['case'];events=current['events']
            result=0; event=dict(service=name)
            if name=='Voice_SendVoiceData':result=c['send']
            elif name=='IsTalking':result=c['talking']
            elif name=='CL_VoiceTransmit':pass
            elif name=='Encode_GetFrameSize':result=c['frame']
            elif name=='Encode_Sample':
                event.update(avail=args[2],out_offset=args[1]-enc,pcm=list(struct.unpack('<'+str(c['frame'])+'h',mu.mem_read(args[0],c['frame']*2))))
                assert args[2]==4096-c['position'] and args[1]==enc+c['position'] and args[2]>=c['encoded']
                mu.mem_write(args[1],bytes(range(0x51,0x51+c['encoded'])));result=c['encoded']
            elif name=='Client_SendVoiceData':
                event.update(length=args[0],offset=args[1]-enc,bytes=list(mu.mem_read(args[1],args[0])));result=c['network']
            events.append(event)
            mu.reg_write(UC_X86_REG_EAX,result&0xffffffff)
            mu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',mu.mem_read(esp,4))[0]);mu.reg_write(UC_X86_REG_ESP,esp+4)
        elif not (addr<=address<end or address in ref.stubs):
            raise ValueError(f'Unexplained original instruction execution {address:#x}')
    def write_hook(mu,access,address,size,value,user):
        allowed=[(0x70000000,0x70010000),(volume,volume+4),(ref.pcm,ref.pcm+ref.count*2),
                 (current['sample']+28,current['sample']+32),(partial_count,partial_count+4)]
        if not any(lo<=address and address+size<=hi for lo,hi in allowed):raise ValueError(f'Unexplained original write {address:#x}')
        current['writes'].append([address,size,value])
    mu.hook_add(unicorn.UC_HOOK_CODE,hook)
    mu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write_hook)
    for c in rows:
        cap=c['count'] if exact else 1024
        owned=ref.alloc(cap*2+16);ref.pcm=owned+8;ref.count=c['count']
        guard=bytes.fromhex('1133557799bbddff');mu.mem_write(owned,guard);mu.mem_write(ref.pcm+cap*2,guard)
        pcm=values(c,cap);mu.mem_write(ref.pcm,struct.pack('<'+str(cap)+'h',*pcm))
        sample=ref.alloc(48);descriptor=struct.pack('<8I',ref.pcm,c['count']*2,c['count'],2,8192,0,1,c['initial_offset'])
        mu.mem_write(sample,guard+descriptor+guard);sample+=8
        mu.mem_write(ref.partial,struct.pack('<640h',*[-3000+i if i<c['initial'] else 7000+i for i in range(640)]))
        mu.mem_write(partial_count,struct.pack('<I',c['initial']));mu.mem_write(enc_pos,struct.pack('<I',c['position']))
        mu.mem_write(enc,bytes([0x35])*4096);mu.mem_write(scaler,struct.pack('<f',c['scale_code']/4));mu.mem_write(volume,struct.pack('<f',123.25))
        mu.reg_write(UC_X86_REG_FPCW,0x37f);mu.reg_write(UC_X86_REG_MXCSR,0x1f80)
        current.update(case=c,sample=sample,events=[],writes=[]);ref.copies=[];ref.imports.clear()
        result=ref.call(SYMBOL,sample)
        unchanged=bytes(mu.mem_read(sample,28))==descriptor[:28] and bytes(mu.mem_read(sample-8,8))==guard and bytes(mu.mem_read(sample+32,8))==guard and bytes(mu.mem_read(owned,8))==guard and bytes(mu.mem_read(ref.pcm+cap*2,8))==guard
        encoded=bytes(mu.mem_read(enc,4096))
        row=dict(id=c['id'],events=current['events'],result=result,offset=ref._u32(sample+28),partial_count=ref._u32(partial_count),
                 volume_bits=ref._u32(volume),scale_bits=ref._u32(scaler),enc_pos=ref._u32(enc_pos),
                 pcm=list(struct.unpack('<'+str(cap)+'h',mu.mem_read(ref.pcm,cap*2))),
                 partial=list(struct.unpack('<640h',mu.mem_read(ref.partial,1280))),
                 encoded_changes=[[i,x] for i,x in enumerate(encoded) if x!=0x35],guards_preserved=unchanged,
                 fpcw=mu.reg_read(UC_X86_REG_FPCW),mxcsr=mu.reg_read(UC_X86_REG_MXCSR),layout=[32,0,4,8,12,16,20,24,28])
        # SSE arithmetic may set sticky status bits; controls must remain pinned.
        assert row['fpcw']==0x37f and row['mxcsr']&~0x3f==0x1f80
        assert dict(ref.imports)==({'_memcpy':len(ref.copies)} if ref.copies else {})
        observations.append(row);private.append(dict(id=c['id'],copies=ref.copies,writes=current['writes'],imports=dict(ref.imports)))
    save(out/'original-memory-effects.private.json',private)
    save(out/'original-execution.private.json',dict(symbol=SYMBOL,address=addr,extent_end=end,services=services,visited=sorted(visited)))
    return observations

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--source',type=Path,help='Private exact production copy before integration')
    parser.add_argument('--compile-db',type=Path,required=True)
    parser.add_argument('--mac-binary',type=Path,help='Optional owned Mac reference for paired instruction execution')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--exact',action='store_true',help='Allocate exactly the declared input length under ASan')
    parser.add_argument('--case-id',type=int,help='One preserved sanitizer reproducer')
    args=parser.parse_args();repo=args.repo.resolve();out=args.output.resolve()
    if out.is_relative_to(repo):parser.error('Evidence must stay outside the repository')
    out.mkdir(parents=True,exist_ok=False);scratch=out/'scratch';scratch.mkdir()
    env=dict(os.environ,TMPDIR=str(scratch),CLANG_MODULE_CACHE_PATH=str(scratch/'clang-cache'),PYTHONDONTWRITEBYTECODE='1',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    commands=[]
    def run(command,name,stdin=None):
        full=BACKGROUND+list(map(str,command));commands.append(dict(name=name,argv=full))
        result=subprocess.run(full,cwd=out,env=env,input=stdin,capture_output=True,text=True,timeout=180)
        (out/(name+'.stdout')).write_text(result.stdout);(out/(name+'.stderr')).write_text(result.stderr)
        commands[-1]['exit_code']=result.returncode;save(out/'commands.json',commands)
        return result
    if args.mac_binary is not None:
        assert sha(args.mac_binary)==REFERENCE_SHA256
    source=(args.source or repo/'src/PC/groupvoice/record.c').resolve()
    reference=(load_module('recording_mac_reference',repo/'tests/voice/mac_reference.py')
               if args.mac_binary is not None else None)
    math=load_module('recording_math_flags',repo/'tests/fidelity/math_reference.py');math.ROOT=repo;math.SOURCE=Path('src/platform/macos_voice.c')
    flags,flag_evidence=math.engine_flags(args.compile_db)
    save(out/'flags.json',flag_evidence)
    rows=cases();rows=[c for c in rows if args.case_id is None or c['id']==args.case_id]
    assert rows
    save(out/'cases.json',rows)
    expected=[synthetic(c,args.exact) for c in rows]
    old=original(args.mac_binary,reference,rows,args.exact,out) if reference else expected
    save(out/('original.private.json' if reference else 'synthetic-expected.json'),old)
    if reference:
        assert compare(old,expected)==[], 'Independent synthetic stream disagrees with original'
    extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections']
    obj=out/'record.o';exe=out/'probe';fixture=Path(__file__).with_suffix('.c')
    for command,name in [(flags+extra+['-MD','-MF',str(out/'record.d'),'-c',str(source),'-o',str(obj)],'compile-record'),
                         (flags+extra+['-UNDEBUG','-MD','-MF',str(out/'fixture.d'),str(fixture),str(obj),'-Wl,-dead_strip','-o',str(exe)],'link')]:
        result=run(command,name)
        if result.returncode:raise RuntimeError(name+' failed; see private logs')
    keys=['id','count','initial','scale_code','send','talking','frame','encoded','network','position','initial_offset']
    stdin=''.join(' '.join(str(c[k]) for k in keys)+'\n' for c in rows);(out/'cases.stdin').write_text(stdin)
    result=run([exe,int(args.exact)],'native-run',stdin)
    # A sanitizer abort can leave a partial JSON line; preserve raw logs and fail.
    native=[] if result.returncode else [json.loads(line) for line in result.stdout.splitlines()]
    save(out/'native.private.json',native)
    mismatches=compare(native,old) if result.returncode==0 else None
    if native:
        assert all(x['layout']==[40,0,8,12,16,20,24,28,32] and x['fpcr_before']==x['fpcr']==0 for x in native)
    save(out/'result.json',dict(passed=result.returncode==0 and mismatches==[],native_exit=result.returncode,cases=len(rows),mismatches=mismatches,source_sha256=sha(source),exact=args.exact,original_synthetic_agreement=True if reference else None,original_executed=reference is not None))
    inputs=[source,fixture,Path(__file__),args.compile_db,repo/'tests/fidelity/math_reference.py']
    if reference:
        inputs.extend([args.mac_binary,repo/'tests/voice/mac_reference.py'])
    # Clang's depfiles include every actual consumed header, including the SDK.
    for depfile in [out/'record.d',out/'fixture.d']:
        text=depfile.read_text().replace('\\\n',' ');dependencies=shlex.split(text.split(':',1)[1])
        inputs.extend(Path(p) for p in dependencies if Path(p).is_file())
    inputs.extend([Path(sys.executable).resolve(),Path(flags[0]).resolve()])
    save(out/'inputs.json',[dict(path=str(p.resolve()),sha256=sha(p),bytes=p.stat().st_size) for p in sorted(set(inputs),key=str)])
    print(json.dumps(json.loads((out/'result.json').read_text())))
    return 0 if result.returncode==0 and mismatches==[] else 1

if __name__=='__main__':raise SystemExit(main())
