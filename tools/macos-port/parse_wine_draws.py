#!/usr/bin/env python3
"""Reduce Wine +d3d9,+d3d_shader,+d3d logs to per-draw D3D9 state.

Keep numeric constants/state and object identifiers, never shader bytecode.
Select the last complete frame at each --eye x,y,z (sky eyePosition is c20).
Constants are captured at Wine's inner draw after its stateblock is applied;
the D3D9 DrawIndexedPrimitive entry log precedes the numeric constant flush.
Output is external JSONL, one file per selected view.
"""
import argparse
import copy
import json
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('log', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--eye', action='append', required=True, help='label:x,y,z')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = a.output.resolve()
if out == root or root in out.parents:
    p.error('Keep game traces outside git')
out.mkdir(parents=True, exist_ok=True)
eyes = {name: list(map(float, eye.split(','))) for name, eye in (x.split(':') for x in a.eye)}
s = {'vs': None, 'ps': None, 'vs_constants': [0.] * 1024, 'ps_constants': [0.] * 1024,
     'states': {}, 'samplers': [[0] * 14 for _ in range(16)],
     'textures': [None] * 16, 'stages': [[0] * 33 for _ in range(8)]}
frame = []
pending = None
api_thread = None
selected = {}
count = 0
number = r'(0x[0-9a-f]+|\d+)'
constant = re.compile(r'wined3d_device_set_([vp])s_consts_f Set vec4 constant (\d+) to \{([^}]+)\}')
draw = re.compile(r'DrawIndexedPrimitive iface \w+, primitive_type ' + number + r', base_vertex_idx (-?\d+), min_vertex_idx (\d+), vertex_count (\d+), start_idx (\d+), primitive_count (\d+)')
state = re.compile(r'SetRenderState iface \w+, state ' + number + r', value ' + number)
sampler = re.compile(r'SetSamplerState iface \w+, sampler (\d+), state ' + number + r', value ' + number)
stage = re.compile(r'SetTextureStageState iface \w+, stage (\d+), state ' + number + r', value ' + number)
texture = re.compile(r'SetTexture iface \w+, stage (\d+), texture (\w+)')
shader = re.compile(r'Set(Vertex|Pixel)Shader iface \w+, shader (\w+)')
inner_draw = re.compile(r'wined3d_device_context_draw_indexed context \w+, base_vertex_index (-?\d+), start_index (\d+), index_count (\d+)')
with a.log.open(errors='replace') as f:
    for line in f:
        thread = line.split(':', 1)[0]
        if 'wined3d_device_set_' in line and 's_consts_f Set vec4' in line:
            m = constant.search(line)
            if m and thread == api_thread:
                values = [float(v) for v in m[3].split(',')]
                index = int(m[2]) * 4
                s['vs_constants' if m[1] == 'v' else 'ps_constants'][index:index + 4] = values
        elif 'wined3d_device_context_draw_indexed ' in line:
            m = inner_draw.search(line)
            if m and pending and thread == pending['thread']:
                d = pending['draw']
                base, first, indices = map(int, m.groups())
                expected = {1: d['primitives'], 2: d['primitives'] * 2,
                            3: d['primitives'] + 1, 4: d['primitives'] * 3,
                            5: d['primitives'] + 2, 6: d['primitives'] + 2}[d['type']]
                if (base, first, indices) != (d['base'], d['first'], expected):
                    p.error('Wine inner draw does not match the pending D3D9 geometry')
                d['vs_constants'] = s['vs_constants'][:]
                d['ps_constants'] = s['ps_constants'][:]
                d['state_capture'] = 'wined3d_device_context_draw_indexed'
                frame.append(d)
                pending = None
        elif ':d3d9:' in line:
            api_thread = thread
            if 'd3d9_swapchain_Present iface' in line:
                if pending:
                    p.error('Missing Wine inner draw before Present; enable +d3d tracing')
                count += 1
                if frame:
                    eye = frame[-1]['vs_constants'][80:83]
                    for name, target in eyes.items():
                        if all(abs(x - y) < .01 for x, y in zip(eye, target)):
                            selected[name] = frame
                frame = []
                continue
            if (m := draw.search(line)):
                if pending:
                    p.error('Missing Wine inner draw before next D3D9 draw; enable +d3d tracing')
                d = copy.deepcopy(s)
                d.update(zip(('type', 'base', 'min', 'vertices', 'first', 'primitives'),
                             (int(x, 0) for x in m.groups())))
                d.update(draw=len(frame), frame=count)
                pending = {'thread': thread, 'draw': d}
            elif (m := state.search(line)):
                s['states'][str(int(m[1], 0))] = int(m[2], 0)
            elif (m := sampler.search(line)):
                unit, key, value = (int(x, 0) for x in m.groups())
                if unit < 16 and key < 14: s['samplers'][unit][key] = value
            elif (m := stage.search(line)):
                unit, key, value = (int(x, 0) for x in m.groups())
                if unit < 8 and key < 33: s['stages'][unit][key] = value
            elif (m := texture.search(line)):
                if int(m[1]) < 16: s['textures'][int(m[1])] = m[2]
            elif (m := shader.search(line)):
                s['vs' if m[1] == 'Vertex' else 'ps'] = m[2]
for name, frame in selected.items():
    with (out / (name + '.jsonl')).open('w') as f:
        for d in frame:
            f.write(json.dumps(d, separators=(',', ':'), allow_nan=False) + '\n')
    print(json.dumps({'view': name, 'frame': frame[0]['frame'], 'draws': len(frame),
                      'eye': frame[0]['vs_constants'][80:83], 'output': str(out / (name + '.jsonl'))}))
missing = eyes.keys() - selected.keys()
if missing: p.error('No complete frames for: ' + ','.join(missing))
