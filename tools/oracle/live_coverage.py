#!/usr/bin/env python3
"""Report observed live actions, rather than merely requested command buttons."""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'parity'))
from compare import read_players


def coverage(players, commands):
    buttons = {}
    for line in commands.read_text().splitlines():
        fields = line.split()
        if fields and fields[0] == 'cmd':
            frame, client, _, value = map(int, fields[1:5])
            buttons[frame, client] = buttons.get((frame, client), 0) | value
    events = {name: [] for name in ('live', 'move', 'jump', 'crouch', 'prone',
                                   'fire', 'grenade', 'damage', 'death', 'respawn')}
    previous = {}
    died = set()
    for frame, clients in read_players(players):
        for client, ps in clients:
            old = previous.get(client)
            live = ps['health'] > 0 and ps['pm_type'] == 0
            cmd = buttons.get((frame, client), 0)
            flags = int(ps['pm_flags'], 16)
            vz = struct.unpack('!f', bytes.fromhex(ps['velocity'][2]))[0]
            observed = {
                'live': live,
                'move': live and old and old['health'] > 0 and ps['origin'] != old['origin'],
                'jump': live and cmd & 0x400 and flags & 0x80000 and vz > 0,
                'crouch': live and flags & 2,
                'prone': live and flags & 1,
                'fire': live and ps['weaponstate'] == 3,
                'grenade': live and cmd & 0x10000 and 13 <= ps['weaponstate'] <= 15,
                'damage': old and old['health'] > 0 and ps['health'] < old['health'],
                'death': old and old['health'] > 0 and ps['health'] <= 0,
                'respawn': live and client in died and old and old['health'] <= 0,
            }
            for name, happened in observed.items():
                if happened:
                    events[name].append((frame, client))
            if observed['death']:
                died.add(client)
            previous[client] = ps
    return {name: {'records': len(rows), 'first': rows[0] if rows else None}
            for name, rows in events.items()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('players', type=Path)
    parser.add_argument('commands', type=Path)
    parser.add_argument('--require-all', action='store_true')
    args = parser.parse_args()
    result = coverage(args.players, args.commands)
    print(json.dumps(result, indent=2))
    missing = [name for name, item in result.items() if not item['records']]
    if args.require_all and missing:
        sys.exit('missing observed actions: ' + ', '.join(missing))
