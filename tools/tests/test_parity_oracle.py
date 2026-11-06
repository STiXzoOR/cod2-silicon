"""Synthetic replay-oracle traces; no game binary, data or VM required."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
COMPARE = ROOT / 'tools/parity/compare.py'
RECORD = ROOT / 'tools/parity/record.py'


def player(frame, client, origin='3f800000,00000000,00000000', health=100):
    return (f'frame {frame} client {client} origin {origin} '
            'velocity 00000000,00000000,00000000 '
            'viewangles 00000000,00000000,00000000 '
            f'pm_type 0 pm_flags 00000000 pm_time 0 weapon 1 weaponstate 0 health {health}\n')


def players(frame, clients=(0, 1)):
    return f'frame {frame} clients {len(clients)}\n' + ''.join(player(frame, c) for c in clients)


def state(frame, q='22222222'):
    return f'frame {frame} simt {frame * 50} ents 2 raw 11111111 q {q}\n'


class OracleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cod2-parity-oracle-')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def compare(self, left, right, left_state=None, right_state=None, extra=()):
        paths = [self.path / name for name in ('state-a', 'state-b', 'players-a', 'players-b')]
        for path, content in zip(paths, (left_state or state(1), right_state or state(1), left, right)):
            path.write_text(content)
        return subprocess.run([sys.executable, str(COMPARE), str(paths[0]), str(paths[1]),
                               '--players', str(paths[2]), str(paths[3]), *extra],
                              capture_output=True, text=True)

    def test_matching_players_with_warmup_and_nonconsecutive_client_slots(self):
        trace = players(1, ()) + players(2, (1, 3))
        result = self.compare(trace, trace, state(1) + state(2), state(1) + state(2),
                              ('--require-clients', '2'))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('2 frames match', result.stdout)

    def test_live_coverage_rejects_requested_actions_without_observed_effects(self):
        trace = self.path / 'players'
        commands = self.path / 'commands'
        trace.write_text(players(1) + players(2))
        commands.write_text('cod2-replay 1\ncmd 1 0 50 66560 1 0 0 0 0 127 0\nend 2\n')
        result = subprocess.run([sys.executable, str(ROOT / 'tools/oracle/live_coverage.py'),
                                 str(trace), str(commands), '--require-all'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('missing observed actions', result.stderr)
        self.assertIn('grenade', result.stderr)

    def test_live_coverage_counts_death_and_respawn_after_being_alive(self):
        trace = self.path / 'players'
        commands = self.path / 'commands'
        trace.write_text('frame 1 clients 1\n' + player(1, 0, health=0) +
                         'frame 2 clients 1\n' + player(2, 0) +
                         'frame 3 clients 1\n' + player(3, 0, health=0) +
                         'frame 4 clients 1\n' + player(4, 0))
        commands.write_text('cod2-replay 1\nend 4\n')
        result = subprocess.run([sys.executable, str(ROOT / 'tools/oracle/live_coverage.py'),
                                 str(trace), str(commands)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['death'], {'records': 1, 'first': [3, 0]})
        self.assertEqual(report['respawn'], {'records': 1, 'first': [4, 0]})

    def test_malformed_reordered_and_truncated_players_cannot_pass(self):
        good = players(1)
        bad = ('', 'garbage\n', good.replace('pm_flags 00000000', 'pm_flags 0'),
               good.replace('3f800000', '3F800000'),
               'frame 1 clients 2\n' + player(1, 1) + player(1, 0),
               'frame 1 clients 2\n' + player(1, 0) + player(1, 0),
               'frame 1 clients 2\n' + player(1, 0),
               'frame 1 clients 1\n' + player(2, 0),
               good + players(3), good + good)
        for trace in bad:
            with self.subTest(trace=trace):
                result = self.compare(trace, trace)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('harness', result.stdout + result.stderr)

    def test_missing_player_frame_relative_to_statehash_cannot_pass(self):
        result = self.compare(players(1), players(1), state(1) + state(2), state(1) + state(2))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('harness', result.stdout + result.stderr)
        self.assertIn('frame 2', result.stdout + result.stderr)

    def test_first_chronological_difference_names_client_and_field(self):
        left = players(1) + players(2)
        right = players(1).replace('health 100', 'health 99', 1) + players(2)
        result = self.compare(left, right, state(1) + state(2), state(1) + state(2, '33333333'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('frame 1', result.stdout)
        self.assertIn('client 0', result.stdout)
        self.assertIn('health', result.stdout)
        self.assertIn('logic', result.stdout)

    def test_player_field_explains_same_frame_hash_difference(self):
        result = self.compare(players(1), players(1).replace('health 100', 'health 99', 1),
                              state(1), state(1, '33333333'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('health', result.stdout)

    def test_earlier_statehash_difference_precedes_player_difference(self):
        result = self.compare(players(1) + players(2), players(1) + players(2, (0,)),
                              state(1) + state(2), state(1, '33333333') + state(2))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('frame 1', result.stdout)
        self.assertIn('field q', result.stdout)

    def test_float_last_bit_requires_finite_same_sign_adjacent_bits(self):
        for a, b, classification in (('3f800000', '3f800001', 'float-last-bit'),
                                     ('bf800000', 'bf800001', 'float-last-bit'),
                                     ('3f800000', '3f800002', 'logic'),
                                     ('00000000', '80000000', 'logic'),
                                     ('7f7fffff', '7f800000', 'logic'),
                                     ('7fc00000', '7fc00001', 'logic')):
            with self.subTest(a=a, b=b):
                left = players(1).replace('3f800000', a, 1)
                right = players(1).replace('3f800000', b, 1)
                result = self.compare(left, right)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('origin[0]', result.stdout)
                self.assertIn(classification, result.stdout)

    def test_required_clients_rejects_zero_input_and_lost_final_client(self):
        for trace in (players(1, ()), players(1) + players(2, (0,))):
            with self.subTest(trace=trace):
                states = state(1) + (state(2) if 'frame 2' in trace else '')
                result = self.compare(trace, trace, states, states, ('--require-clients', '2'))
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('harness', result.stdout + result.stderr)

    def record(self, extra=(), body='', helper=True, duplicate_helper=False):
        binary = self.path / 'engine'
        binary.write_text('#!' + sys.executable + '\n' + '''import os
from pathlib import Path
Path(os.environ['SYSDIFF_STATEHASH']).write_text(%r)
Path(os.environ['SYSDIFF_DUMP']).write_text('e0 eq=22222222\\n')
if 'SYSDIFF_PLAYERS' in os.environ:
    Path(os.environ['SYSDIFF_PLAYERS']).write_text(%r)
''' % (state(1) + state(2), players(1, ()) + players(2)) + body)
        binary.chmod(0o755)
        data = self.path / 'data'
        (data / 'main').mkdir(parents=True)
        with zipfile.ZipFile(data / 'main/synthetic.iwd', 'w') as archive:
            archive.writestr('synthetic.txt', 'synthetic fixture')
            if helper:
                # An independent minimal input for the helper-call syntax, not an extracted script.
                line = '// thread maps\\mp\\gametypes\\_teams::addTestClients();\n'
                archive.writestr('maps/mp/gametypes/dm.gsc', 'synthetic() {\n' +
                                 line * (2 if duplicate_helper else 1) + '}\n')
                archive.writestr('maps/mp/gametypes/_teams.gsc',
                                 'setup() { ent[0] thread TestClient(team); }\nTestClient(team) {\n'
                                 'self notify("menuresponse", game["menu_team"], team);\n}\n')
        return subprocess.run([sys.executable, str(RECORD), '--binary', str(binary), '--data', str(data),
                               '--output', str(self.path / 'run'), '--frames', '2', '--timeout', '3',
                               *extra], capture_output=True, text=True)

    def test_record_players_commands_and_metadata(self):
        result = self.record(('--players', '--clients', '2', '--commands-record', '--dedicated'), '''
assert os.environ['SYSDIFF_CLIENTS'] == '2'
import sys
assert sys.argv[sys.argv.index('scr_testclients') + 1] == '2'
assert sys.argv[sys.argv.index('sv_pure') + 1] == '0'
home = Path(sys.argv[sys.argv.index('fs_homepath') + 1].strip('"'))
script = (home / 'main/maps/mp/gametypes/dm.gsc').read_text()
assert '// thread' not in script and 'thread maps' in script
teams = (home / 'main/maps/mp/gametypes/_teams.gsc').read_text()
assert 'thread oracleTestClient(team);' in teams
assert 'self [[level.weapon]]' in teams
Path(os.environ['SYSDIFF_USERCMDS']).write_text('cod2-replay 1\\nprivate-command-data\\n')
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        metadata = json.loads((self.path / 'run/run.json').read_text())
        self.assertEqual(metadata['players'], 'players.txt')
        self.assertEqual(metadata['clients'], 2)
        self.assertEqual(metadata['commands_record'], 'usercmds.txt')
        self.assertIn('testclients_bootstrap', metadata)
        self.assertNotIn('private-command-data', json.dumps(metadata))

    def test_replay_input_is_absolute_and_not_copied_into_metadata(self):
        replay = self.path / 'input.txt'
        replay.write_text('cod2-replay 1\nprivate-replay-data\n')
        result = self.record(('--players', '--replay', str(replay)), '''
assert Path(os.environ['SYSDIFF_REPLAY']).is_absolute()
assert 'SYSDIFF_USERCMDS' not in os.environ
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        metadata = json.loads((self.path / 'run/run.json').read_text())
        self.assertEqual(metadata['replay'], str(replay.resolve()))
        self.assertNotIn('private-replay-data', json.dumps(metadata))

    def test_empty_commands_record_cannot_pass(self):
        result = self.record(('--commands-record',), "Path(os.environ['SYSDIFF_USERCMDS']).touch()\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing command recording', result.stderr)

    def test_clients_requires_players(self):
        result = self.record(('--clients', '2'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--players', result.stderr)

    def test_testclients_missing_helper_cannot_pass(self):
        result = self.record(('--players', '--clients', '2'), helper=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('test-client helper', result.stderr)

    def test_testclients_ambiguous_helper_cannot_pass(self):
        result = self.record(('--players', '--clients', '2'), duplicate_helper=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('test-client helper', result.stderr)

    def test_record_replay_are_mutually_exclusive(self):
        result = self.record(('--commands-record', '--replay', str(self.path / 'input.txt')))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('not allowed with', result.stderr)


if __name__ == '__main__':
    unittest.main()
