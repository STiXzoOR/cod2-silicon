#!/usr/bin/env python3
"""Use a local UDP peer to test the actual Network.framework transport."""
import pathlib, socket, subprocess, threading
root = pathlib.Path(__file__).resolve().parents[2]
out = root / 'output/ws25/network'
subprocess.run([str(root/'scripts/build-launcher.sh'), str(out), '--network-test'], check=True)
fixture = (root/'tests/launcher/fixtures/status.bin').read_bytes()
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
    peer.bind(('127.0.0.1', 0)); peer.settimeout(3)
    port = peer.getsockname()[1]
    def answer():
        for _ in range(2):
            data, address = peer.recvfrom(65535)
            assert data.startswith(b'\xff'*4)
            assert data[4:].startswith((b'getstatus ', b'getinfo '))
            peer.sendto(fixture, address)
    thread = threading.Thread(target=answer, daemon=True); thread.start()
    subprocess.run([str(out/'LauncherNetworkTests'), str(port)], check=True, timeout=10)
    thread.join(3)
    assert not thread.is_alive()
