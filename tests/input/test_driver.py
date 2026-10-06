"""Driver geometry and real loopback fixture transport checks; no game window."""
import socket
import time
import unittest
from unittest.mock import Mock

from run import Driver, center, find_item
from scenarios import list_point, list_capacity, ui_scripts
from servers import FixtureServers, MARKER


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
            pool.announce(.1)
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
