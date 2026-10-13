"""Localhost-only advertised browse rows; optionally proxy joins to a real server."""
import select
import socket
import threading
import time

MARKER = b'\xff' * 4


class FixtureServers:
    def __init__(self, client_port, count=30, target=''):
        self.client = ('127.0.0.1', client_port)
        self.target = None
        if target:
            host, port = target.rsplit(':', 1)
            if host not in ('127.0.0.1', 'localhost'):
                raise ValueError('The QA server must be on localhost')
            self.target = ('127.0.0.1', int(port))
        self.sockets = []
        for n in range(count):
            udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # The stock address formatter treats the stored port as signed.
            # Stay below 32768 so Favorites and Join receive valid text ports.
            for port in range(31000, 32768):
                try: udp.bind(('127.0.0.1', port)); break
                except OSError: continue
            else:
                udp.close()
                for existing in self.sockets: existing.close()
                raise RuntimeError('No free private browse fixture port')
            udp.setblocking(False)
            self.sockets.append(udp)
        self.peers = {}
        self.stopping = threading.Event()
        self.announce_until = 0
        self.thread = threading.Thread(target=self.run, daemon=True)

    def info(self, index, challenge='xxx'):
        pairs = {'challenge': challenge, 'protocol': '118', 'hostname': f'WS42 row {index:02d}',
                 'mapname': 'mp_toujane', 'clients': '0', 'sv_maxclients': '8',
                 'gametype': 'dm', 'pure': '0', 'hw': '2', 'mod': '0', 'voice': '1'}
        text = ''.join('\\' + key + '\\' + value for key, value in pairs.items())
        return MARKER + b'infoResponse\n' + text.encode() + b'\nWS42 localhost fixture\n'

    def start(self):
        self.thread.start()
        return self

    def announce(self, seconds=5):
        # Exercises the real network parser with loopback responses. Stock LAN
        # discovery probes only four fixed ports, which cannot fill a long list.
        self.announce_until = time.monotonic() + seconds

    def run(self):
        next_announce = 0
        while not self.stopping.is_set():
            now = time.monotonic()
            if now < self.announce_until and now >= next_announce:
                for n, udp in enumerate(self.sockets): udp.sendto(self.info(n), self.client)
                next_announce = now + .25
            ready, _, _ = select.select(self.sockets, [], [], .03)
            for udp in ready:
                packet, peer = udp.recvfrom(65535)
                if self.target and peer == self.target:
                    if packet.startswith(MARKER + b'infoResponse\n'):
                        # Keep actual server flags/map/player counts, while making
                        # the synthetic row identity explicit in the browser.
                        prefix, details = packet.split(b'\n', 1)
                        fields = details.split(b'\\')
                        for n in range(1, len(fields) - 1, 2):
                            if fields[n] == b'hostname': fields[n + 1] = f'WS42 row {self.sockets.index(udp):02d} (proxy)'.encode()
                        packet = prefix + b'\n' + b'\\'.join(fields)
                    if udp in self.peers: udp.sendto(packet, self.peers[udp])
                    continue
                self.peers[udp] = peer
                if packet.startswith(MARKER + b'getinfo'):
                    if self.target:
                        udp.sendto(packet, self.target)
                        continue
                    args = packet[4:].decode(errors='replace').split()
                    challenge = args[1] if len(args) > 1 else 'xxx'
                    udp.sendto(self.info(self.sockets.index(udp), challenge), peer)
                elif self.target:
                    udp.sendto(packet, self.target)

    def close(self):
        self.stopping.set()
        if self.thread.ident is not None: self.thread.join(timeout=1)
        for udp in self.sockets: udp.close()
