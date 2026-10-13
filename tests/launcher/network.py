#!/usr/bin/env python3
"""Use a local UDP peer to test the actual Network.framework transport."""
import os, pathlib, socket, subprocess, threading
root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(os.environ['COD2_LAUNCHER_TEST_OUTPUT']) / 'network' if 'COD2_LAUNCHER_TEST_OUTPUT' in os.environ else root / 'output/ws25/network'
subprocess.run([str(root/'scripts/build-launcher.sh'), str(out), '--network-test'], check=True)
fixture = (root/'tests/launcher/fixtures/status.bin').read_bytes()
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
    peer.bind(('127.0.0.1', 0)); peer.settimeout(3)
    port = peer.getsockname()[1]
    def answer():
        for index in range(6):
            data, address = peer.recvfrom(65535)
            assert data.startswith(b'\xff'*4)
            assert data[4:].startswith((b'getstatus ', b'getinfo '))
            if index < 2:
                reply = fixture
            elif index < 4:
                header = b'statusResponse' if index == 2 else b'infoResponse'
                competitive = b'1' if index == 2 else b'0'
                reply = b'\xff'*4 + header + b'\n\\protocol\\120\\fs_game\\\\g_competitive\\' + competitive + b'\n'
            elif index == 4:
                reply = b'\xff'*4 + b'statusResponse\n\\protocol\\120\\fs_game\\\\g_competitive\\1\\g_competitive\\0\n'
            else:
                reply = b'\xff'*4 + b'infoResponse\n\\protocol\\120\\fs_game\\\\g_competitive\\0\n'
            peer.sendto(reply, address)
    thread = threading.Thread(target=answer, daemon=True); thread.start()
    subprocess.run([str(out/'LauncherNetworkTests'), str(port)], check=True, timeout=10)
    thread.join(3)
    assert not thread.is_alive()
