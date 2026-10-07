#!/usr/bin/env python3
"""Probe production temporary-model point traces against reference capsule geometry."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
from spotcheck_reverb import ROOT, extract_function


def reference_blocked(x, y):
    # Mac 1.3 CM_Trace's 0x3ff branch (0x6498c -> 0x64e2a) uses
    # rounded model geometry, with sphere calls at 0x6514c and 0x651ee.
    # Our z=0..1 rays are in the cylindrical middle of [-10,-10,-20]..
    # [10,10,20]. Cases deliberately avoid the 0.125 collision epsilon.
    return x * x + y * y < 100


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-difference', action='store_true')
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error('Keep observations outside git')
    source = ROOT / 'src/PC/qcommon/cm_trace.c'
    entries = json.loads((args.build / 'compile_commands.engine.json').read_text())
    entry = next(e for e in entries if Path(e['file']) == source and 'cod2_macos.dir' in e['command'])
    command = shlex.split(entry['command'])
    flags = command[:command.index('-o')]
    extents = extract_function((ROOT / 'src/PC/qcommon/cm_tracebox.c').read_text(), 'CM_CalcTraceEntents')
    driver = '''#include "src/PC/qcommon/cm_trace.c"
#include <stdio.h>
clipMap_t cm;
void *imp_cm = &cm;
static cbrush_t probeBrush;
static cmodel_t probeModel;
static TraceThreadInfo info;
void *Sys_GetValue(int key) { return &info; }
void CM_TraceThroughAabbTree(const traceWork_t *t, CollisionAabbTree *a, trace_t *r) { }
short int CM_MeshTestInLeaf(const traceWork_t *t, cLeaf_t *l, trace_t *r) { return 0; }
int CM_BoxLeafnums(const vec_t *a, const vec_t *b, int *l, int n, int *last) { return 0; }
const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out) {
    float length = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    float inverse = length ? 1.0f / length : 0.0f;
    for (int i=0;i<3;i++) out[i] = v[i] * inverse;
    return length;
}
'''
    driver += extents
    driver += '''
int main(void) {
    const float xy[][2] = {{9,9}, {-9,9}, {0,0}, {9,0}, {16,0}};
    const vec3_t mins = {-10,-10,-20}, maxs = {10,10,20}, zero = {0,0,0};
    info.box_brush = &probeBrush;
    info.box_model = &probeModel;
    clipHandle_t handle = CM_TempBoxModel(mins, maxs, 1);
    for (int i=0;i<5;i++) {
        vec3_t start = {xy[i][0],xy[i][1],0}, end = {xy[i][0],xy[i][1],1};
        trace_t result;
        CM_BoxTrace(&result, start, end, zero, zero, handle, 1);
        printf("%g %g %g %d %d\\n", xy[i][0], xy[i][1], result.fraction, result.startsolid, result.allsolid);
    }
    return 0;
}
'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as tmp:
        c, exe = Path(tmp) / 'probe.c', Path(tmp) / 'probe'
        c.write_text(driver)
        result = subprocess.run(flags + ['-Wl,-dead_strip', str(c), '-o', str(exe)], cwd=entry['directory'], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
        output = subprocess.check_output([str(exe)], text=True)
    cases = []
    for line in output.splitlines():
        x, y, fraction, startsolid, allsolid = map(float, line.split())
        blocked = fraction < 1 or bool(startsolid) or bool(allsolid)
        expected = reference_blocked(x, y)
        cases.append(dict(x=x, y=y, fraction=fraction, startsolid=bool(startsolid), allsolid=bool(allsolid),
                          actual_blocked=blocked, reference_blocked=expected, different=blocked != expected))
    result = dict(source=str(source.relative_to(ROOT)), reference_address='0x64426',
                  oracle='reference rounded temporary-model geometry, analytic middle-cylinder cases; no i386 execution',
                  cases=cases, total=len(cases), differences=sum(c['different'] for c in cases))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if bool(result['differences']) == args.expect_difference else 1


if __name__ == '__main__':
    raise SystemExit(main())
