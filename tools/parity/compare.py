#!/usr/bin/env python3
"""Fail closed on incomplete traces; report the first differing simulation frame."""
import argparse
from itertools import zip_longest
from pathlib import Path
import re
import sys

LINE = re.compile(r'frame (\d+) simt (-?\d+) ents (\d+) raw ([0-9a-fA-F]{8}) q ([0-9a-fA-F]{8})')
PLAYER_FRAME = re.compile(r'frame (\d+) clients (\d+)')
VECTOR = r'([0-9a-f]{8},[0-9a-f]{8},[0-9a-f]{8})'
PLAYER = re.compile(r'frame (\d+) client (\d+) origin ' + VECTOR + r' velocity ' + VECTOR
                    + r' viewangles ' + VECTOR + r' pm_type (-?\d+) pm_flags ([0-9a-f]{8})'
                    r' pm_time (-?\d+) weapon (-?\d+) weaponstate (-?\d+) health (-?\d+)')
PLAYER_FIELDS = ('origin', 'velocity', 'viewangles', 'pm_type', 'pm_flags', 'pm_time',
                 'weapon', 'weaponstate', 'health')


def read_trace(path):
    rows = []
    for line_no, line in enumerate(Path(path).read_text().splitlines(), 1):
        match = LINE.fullmatch(line)
        if not match:
            raise ValueError(f'{path}:{line_no}: malformed statehash record')
        frame, simt, ents, raw, quantized = match.groups()
        row = (int(frame), int(simt), int(ents), raw.lower(), quantized.lower())
        if row[0] <= 0 or (rows and row[0] != rows[-1][0] + 1):
            raise ValueError(f'{path}:{line_no}: noncontiguous frames or map restart')
        rows.append(row)
    if not rows:
        raise ValueError(f'{path}: empty trace (no local server simulation?)')
    return rows


def read_players(path):
    """Read complete frames, retaining float components as their raw binary32 bits."""
    rows = []
    lines = iter(enumerate(Path(path).read_text().splitlines(), 1))
    for line_no, line in lines:
        match = PLAYER_FRAME.fullmatch(line)
        if not match:
            raise ValueError(f'{path}:{line_no}: malformed player frame header')
        frame, count = map(int, match.groups())
        if frame <= 0 or (rows and frame != rows[-1][0] + 1):
            raise ValueError(f'{path}:{line_no}: noncontiguous player frames or map restart')
        clients = []
        for _ in range(count):
            item = next(lines, None)
            if item is None:
                raise ValueError(f'{path}: truncated player frame {frame}: expected {count} clients')
            line_no, line = item
            match = PLAYER.fullmatch(line)
            if not match:
                raise ValueError(f'{path}:{line_no}: malformed player record in frame {frame}')
            player_frame, client, *values = match.groups()
            client = int(client)
            if int(player_frame) != frame:
                raise ValueError(f'{path}:{line_no}: player frame differs from header {frame}')
            if clients and client <= clients[-1][0]:
                raise ValueError(f'{path}:{line_no}: client IDs must be unique and ascending')
            fields = dict(zip(PLAYER_FIELDS, values))
            for name in PLAYER_FIELDS[:3]:
                fields[name] = tuple(fields[name].split(','))
            for name in PLAYER_FIELDS[3:]:
                if name != 'pm_flags':
                    fields[name] = int(fields[name])
            clients.append((client, fields))
        rows.append((frame, clients))
    if not rows:
        raise ValueError(f'{path}: empty player trace')
    return rows


def validate_players(players, states, path, required_clients=None):
    for state, player in zip_longest(states, players):
        if state is None or player is None or state[0] != player[0]:
            frames = [row[0] for row in (state, player) if row is not None]
            raise ValueError(f'{path}: player/statehash framing differs at frame {min(frames)}')
    if required_clients is not None:
        if not any(len(row[1]) == required_clients for row in players):
            raise ValueError(f'{path}: no frame has required {required_clients} clients')
        if len(players[-1][1]) != required_clients:
            raise ValueError(f'{path}: final frame {players[-1][0]} needs {required_clients} clients')


def state_difference(left, right, raw=False):
    for index, (a, b) in enumerate(zip_longest(left, right)):
        frame = min(row[0] for row in (a, b) if row is not None)
        if a is None or b is None or a[0] != b[0]:
            return frame, (f'FAIL: first difference at record {index + 1}, frame {frame}, '
                           f'field frame (harness)\nleft: {a}\nright: {b}')
        for field, offset in (('simt', 1), ('ents', 2), ('raw' if raw else 'q', 3 if raw else 4)):
            if a[offset] != b[offset]:
                return frame, (f'FAIL: first difference at record {index + 1}, frame {frame}, '
                               f'field {field} (logic)\nleft: {a}\nright: {b}')
    return None


def compare(left, right, raw=False):
    difference = state_difference(left, right, raw)
    return difference[1] if difference else None


def float_classification(a, b):
    a, b = int(a, 16), int(b, 16)
    if ((a ^ b) & 0x80000000 == 0 and abs(a - b) <= 1
            and a & 0x7f800000 != 0x7f800000 and b & 0x7f800000 != 0x7f800000):
        return 'float-last-bit'
    return 'logic'


def player_difference(left, right):
    for a, b in zip_longest(left, right):
        frame = min(row[0] for row in (a, b) if row is not None)
        if a is None or b is None or a[0] != b[0]:
            return frame, f'FAIL: first difference at frame {frame}, field player frame (harness)'
        if len(a[1]) != len(b[1]):
            return frame, (f'FAIL: first difference at frame {frame}, field clients (logic)\n'
                           f'left: {len(a[1])}\nright: {len(b[1])}')
        for (client_a, fields_a), (client_b, fields_b) in zip(a[1], b[1]):
            if client_a != client_b:
                return frame, (f'FAIL: first difference at frame {frame}, field client ID (logic)\n'
                               f'left: {client_a}\nright: {client_b}')
            for field in PLAYER_FIELDS:
                value_a, value_b = fields_a[field], fields_b[field]
                if field in PLAYER_FIELDS[:3]:
                    for component, (bits_a, bits_b) in enumerate(zip(value_a, value_b)):
                        if bits_a != bits_b:
                            classification = float_classification(bits_a, bits_b)
                            return frame, (f'FAIL: first difference at frame {frame}, client {client_a}, '
                                           f'field {field}[{component}] ({classification})\n'
                                           f'left: {bits_a}\nright: {bits_b}')
                elif value_a != value_b:
                    return frame, (f'FAIL: first difference at frame {frame}, client {client_a}, '
                                   f'field {field} (logic)\nleft: {value_a}\nright: {value_b}')
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('candidate')
    parser.add_argument('--raw', action='store_true', help='strict same-layout diagnostic; not cross-run parity')
    parser.add_argument('--players', nargs=2, metavar=('REFERENCE', 'CANDIDATE'),
                        help='compare raw player fields alongside statehash traces')
    parser.add_argument('--require-clients', type=int, metavar='N',
                        help='require N clients in at least one frame and the final player frame')
    args = parser.parse_args()
    if args.require_clients is not None and (not args.players or args.require_clients < 1):
        parser.error('--require-clients needs --players and a positive client count')
    left, right = read_trace(args.reference), read_trace(args.candidate)
    difference = state_difference(left, right, args.raw)
    if args.players:
        left_players, right_players = map(read_players, args.players)
        validate_players(left_players, left, args.players[0], args.require_clients)
        validate_players(right_players, right, args.players[1], args.require_clients)
        player_error = player_difference(left_players, right_players)
        if player_error and (difference is None or player_error[0] <= difference[0]):
            difference = player_error
    error = difference[1] if difference else None
    fields = f'frame, simt, active entities, {"raw" if args.raw else "q"}'
    if args.players:
        fields += ', player fields'
    print(error or f'PASS: {len(left)} frames match ({fields})')
    return bool(error)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(f'FAIL: harness: {error}')
