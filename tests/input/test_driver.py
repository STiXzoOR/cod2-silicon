"""Driver geometry and real loopback fixture transport checks; no game window."""
import json
from pathlib import Path
import socket
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

from run import Driver, center, find_item
from scenarios import list_point, list_capacity, ui_scripts
from servers import FixtureServers, MARKER


class AcknowledgementTests(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.output = Path(scratch.name)
        self.path = self.output / 'input.jsonl'
        self.path.write_text('{"id":0,"command":"ready","status":1}\n')
        # These checks read files only; no game or FIFO injector is involved.
        (self.output / 'input.fifo').touch()
        self.driver = Driver(None, self.output)
        self.addCleanup(self.driver.close)

    def append(self, data):
        with self.path.open('ab') as stream:
            stream.write(data)

    def test_consumed_acknowledgements_are_not_parsed_again(self):
        decoded = []
        loads = json.loads

        def record_loads(data):
            decoded.append(data)
            return loads(data)

        self.append(b'{"id":1,"command":"barrier","status":1}\n')
        with patch('run.json.loads', side_effect=record_loads):
            self.assertEqual(self.driver.acks()[1]['status'], 1)
            self.assertEqual(len(decoded), 1)
            for _ in range(3):
                self.assertEqual(set(self.driver.acks()), {0, 1})
            self.assertEqual(len(decoded), 1)
            self.append(b'{"id":2,"command":"snapshot","status":-2}\n')
            self.assertEqual(self.driver.acks()[2]['status'], -2)
            self.assertEqual(len(decoded), 2)

    def test_partial_utf8_json_line_survives_multiple_reads(self):
        self.append(b'{"id":1,"command":"text-\xe2')
        self.assertNotIn(1, self.driver.acks())
        self.append(b'\x82')
        self.assertNotIn(1, self.driver.acks())
        self.append(b'\xac","status":1}\n')
        self.assertEqual(self.driver.acks()[1]['command'], 'text-\u20ac')

    def test_malformed_lines_and_duplicate_ids_keep_latest_status(self):
        self.append(b'not-json\n{"id":1,"status":1}\n{"id":1,"status":-1}\n')
        self.assertEqual(self.driver.acks()[1]['status'], -1)

    def test_observed_events_remain_in_order_without_poll_duplicates(self):
        self.append(b'{"observed":768,"tick":10}\n{"id":1,"status":1}\n')
        self.driver.acks()
        self.append(b'{"observed":769,"tick":12}\n{"observed":1024,"tick":15}\n')
        self.driver.acks()
        self.driver.acks()
        self.assertEqual(self.driver.events, [
            {'observed': 768, 'tick': 10}, {'observed': 769, 'tick': 12},
            {'observed': 1024, 'tick': 15}])

    def test_truncation_discards_old_acknowledgements_and_partial_line(self):
        self.append(b'{"observed":768,"tick":10}\n{"id":1,"status":1}\n{"id":2')
        self.driver.acks()
        self.path.write_text('{"id":3,"status":-2}\n')
        self.assertEqual(self.driver.acks(), {3: {'id': 3, 'status': -2}})
        self.assertEqual(self.driver.events, [])

    def test_replaced_file_starts_a_new_acknowledgement_stream(self):
        self.append(b'{"id":1,"status":1}\n')
        self.driver.acks()
        replacement = self.output / 'replacement.jsonl'
        replacement.write_text('{"id":2,"status":-1}\n')
        replacement.replace(self.path)
        self.assertEqual(self.driver.acks(), {2: {'id': 2, 'status': -1}})

    def test_missing_file_discards_the_previous_stream(self):
        self.append(b'{"observed":768,"tick":10}\n')
        self.driver.acks()
        self.path.unlink()
        self.assertEqual(self.driver.acks(), {})
        self.assertEqual(self.driver.events, [])


class GeometryTests(unittest.TestCase):
    def test_unknown_catcher_cannot_inject_ui_motion(self):
        driver = Driver.__new__(Driver)
        driver.command, driver.settle = Mock(), Mock()
        for context in ({}, {'keyCatchers': None}, {'keyCatchers': -1}, {'keyCatchers': 0}):
            with self.subTest(context=context):
                state = {'uiContext': context, 'render': [640, 480], 'window': [640, 480], 'cursor': [0, 0]}
                driver.snapshot = Mock(return_value=state)
                with self.assertRaises(AssertionError): driver.move((0, 0), state)
                driver.command.assert_not_called()
        state['uiContext'] = {'keyCatchers': 8}
        driver.move((0, 0), state)
        driver.command.assert_called_once_with('motion 0 0 0 0')

    def test_scrolled_row_uses_start_position_and_runtime_height(self):
        item = {'rect': [20, 40, 300, 160], 'list': {'count': 30, 'start': 8, 'rowHeight': 16}}
        self.assertEqual(list_point(item, 11), (50, 98))
        with self.assertRaises(AssertionError): list_point(item, 7)
        with self.assertRaises(AssertionError): list_point(item, 19)

    def test_menu_item_index_disambiguates_unnamed_controls(self):
        state = {'menus': [{'name': 'runtime', 'items': [
            {'index': 0, 'name': '', 'dvar': 'first'}, {'index': 1, 'name': '', 'dvar': 'second'}]}]}
        self.assertEqual(find_item(state, 'runtime', 1)['dvar'], 'second')
        with self.assertRaises(AssertionError): find_item(state, 'runtime', '')

    def test_painted_last_row_padding_limits_clickable_body(self):
        item = {'rect': [173.5, 120, 930, 498], 'list': {'count': 30, 'start': 0,
                'rowHeight': 22.5, 'paddingHeight': 22.5, 'rowInset': 3}}
        self.assertEqual(list_capacity(item), 21)
        self.assertEqual(list_point(item, 20), (203.5, 584.25))
        with self.assertRaises(AssertionError): list_point(item, 21)

    def test_click_rect_requires_real_area(self):
        self.assertEqual(center([10, 20, 30, 40]), (25, 40))
        with self.assertRaises(AssertionError): center([10, 20, 0, 40])

    def test_ui_script_action_does_not_match_closed_menu_name(self):
        self.assertEqual(ui_scripts('"close" "joinserver" ; "open" "main_text"'), [])
        self.assertEqual(ui_scripts('"play" "mouse_click" ; "uiScript" "JoinServer" ;'), ['JoinServer'])


class NetworkTests(unittest.TestCase):
    def test_real_udp_announcements_and_challenge_response(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client.bind(('127.0.0.1', 0)); client.settimeout(2)
        pool = FixtureServers(client.getsockname()[1], count=3).start()
        try:
            # Keep announcements available for the existing receive deadline;
            # background scheduling can delay the worker beyond 100ms.
            pool.announce(2)
            addresses = set()
            for _ in range(3):
                data, address = client.recvfrom(4096)
                self.assertTrue(data.startswith(MARKER + b'infoResponse\n'))
                self.assertIn(b'\\protocol\\118', data)
                addresses.add(address)
            self.assertEqual(len(addresses), 3)
            pool.announce_until = 0
            target = next(iter(addresses))
            client.sendto(MARKER + b'getinfo driver-challenge', target)
            deadline = time.monotonic() + 2
            while True:
                data, address = client.recvfrom(4096)
                if b'driver-challenge' in data:
                    self.assertEqual(address, target); break
                self.assertLess(time.monotonic(), deadline)
        finally: pool.close(); client.close()

    def test_join_proxy_forwards_real_server_reply_from_advertised_port(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server.bind(('127.0.0.1', 0)); server.settimeout(2)
        client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client.bind(('127.0.0.1', 0)); client.settimeout(2)
        pool = FixtureServers(client.getsockname()[1], count=1,
                              target=f'127.0.0.1:{server.getsockname()[1]}').start()
        try:
            advertised = pool.sockets[0].getsockname()
            packet = MARKER + b'getchallenge test'
            client.sendto(packet, advertised)
            forwarded, upstream_peer = server.recvfrom(4096)
            self.assertEqual(forwarded, packet)
            self.assertEqual(upstream_peer, advertised)
            reply = MARKER + b'challengeResponse 123'
            server.sendto(reply, upstream_peer)
            data, peer = client.recvfrom(4096)
            self.assertEqual((data, peer), (reply, advertised))
            query = MARKER + b'getinfo real-challenge'
            client.sendto(query, advertised)
            forwarded, upstream_peer = server.recvfrom(4096)
            self.assertEqual(forwarded, query)
            reply = MARKER + b'infoResponse\n\\hostname\\Actual server\\mapname\\qa_map\\pure\\1'
            server.sendto(reply, upstream_peer)
            data, peer = client.recvfrom(4096)
            self.assertEqual(peer, advertised)
            self.assertIn(b'\\hostname\\WS42 row 00 (proxy)', data)
            self.assertIn(b'\\mapname\\qa_map\\pure\\1', data)
        finally: pool.close(); client.close(); server.close()


if __name__ == '__main__': unittest.main()
