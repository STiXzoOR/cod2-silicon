#!/usr/bin/env python3
"""Summarize repeated runs; reject locked/missing evidence as acceptance."""
import argparse
import json
from pathlib import Path
import statistics
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--allow-locked', action='store_true', help='write observational tables only')
args = parser.parse_args()
groups = {}
for path in sorted(args.output.glob('*/results.json')):
    row = json.loads(path.read_text())
    if not path.parent.name.startswith(('live-', 'demo-')):
        continue
    key = (path.parent.name.split('-')[0], row['resolution'], row['maxfps'],
           row.get('dvars', {}).get('r_presentMode', 'default'),
           row.get('fullscreen_spaces', 'unknown'))
    groups.setdefault(key, []).append(row)
summary = []
for key, rows in sorted(groups.items(), key=lambda pair: str(pair[0])):
    result = dict(kind=key[0], resolution=key[1], cap=key[2], present_mode=key[3],
                  fullscreen_spaces=key[4], repeats=len(rows))
    for metric in ('fps', 'one_percent_low', 'frame_ms_p99', 'main_thread_cpu_ms'):
        values = [row[metric] for row in rows if metric in row]
        if values:
            result[metric] = dict(median=statistics.median(values), min=min(values), max=max(values))
    result['screen_locked'] = any(row.get('machine_state', {}).get(phase, {}).get('screen_locked') is not False
                                  for row in rows for phase in ('before', 'after'))
    result['actual_presentation'] = sorted({row.get('actual_presentation') for row in rows
                                          if row.get('actual_presentation')})
    presented = [row['presented'] for row in rows if 'presented' in row]
    if presented:
        result['presented'] = {metric: dict(median=statistics.median(row[metric] for row in presented),
            min=min(row[metric] for row in presented), max=max(row[metric] for row in presented))
            for metric in ('fps', 'frame_ms_p99', 'frame_ms_max')}
    def valid_live(row):
        actual = row.get('actual_presentation') or ''
        flags = re.search(r'windowFlags=0x([0-9a-f]+)', actual)
        size = row['resolution']
        return (row.get('clock_boundary') == 'client' and not row.get('cocoa_probe')
                and flags and int(flags[1], 16) & 1
                and int(flags[1], 16) & 0x200  # SDL_WINDOW_INPUT_FOCUS
                and f'viewport={size} ' in actual and f'drawable={size} ' in actual
                and not row.get('presentation_changes') and row.get('measured_seconds', 0) >= 119.9
                and row.get('shutdown') == 'quit')
    result['target_met'] = (key[0] == 'live' and key[2] == 333 and len(rows) >= 3
        and not result['screen_locked'] and all(valid_live(row)
        and row['one_percent_low'] >= 320 and row['frame_ms_p99'] <= 3.3 for row in rows))
    summary.append(result)
(args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
for row in summary:
    low, p99 = row['one_percent_low'], row['frame_ms_p99']
    print(f"{row['kind']} {row['resolution']} cap={row['cap']} p={row['present_mode']} "
          f"spaces={row['fullscreen_spaces']} n={row['repeats']} locked={row['screen_locked']}: "
          f"low {low['median']:.1f} [{low['min']:.1f},{low['max']:.1f}], "
          f"P99 {p99['median']:.3f} [{p99['min']:.3f},{p99['max']:.3f}] ms")
required = [row for row in summary if row['kind'] == 'live' and row['cap'] == 333
            and row['present_mode'] == '0' and row['fullscreen_spaces'] == '0'
            and row['resolution'] in ('1920x1080', '2560x1440')]
if not args.allow_locked and (len(required) < 2 or not all(row['target_met'] for row in required)):
    raise SystemExit('333 acceptance failed or is unverified; see summary.json')
