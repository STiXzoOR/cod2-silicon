#!/usr/bin/env python3
"""Real local TLS upload fixture; it never changes the system trust store."""
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]


class Upload(http.server.BaseHTTPRequestHandler):
    received = []

    def do_POST(self):
        assert self.path == "/upload/round"
        assert self.headers["Content-Type"] == "application/octet-stream"
        assert self.headers["Transfer-Encoding"] == "chunked"
        body = bytearray()
        while True:
            length = int(self.rfile.readline().strip(), 16)
            if not length:
                self.rfile.readline()
                break
            body.extend(self.rfile.read(length))
            assert self.rfile.read(2) == b"\r\n"
        self.received.append(bytes(body))
        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, *_args):
        pass


with tempfile.TemporaryDirectory(prefix="cod2x-demo-tls.") as work:
    work = Path(work)
    cert, key, binary = work / "cert.pem", work / "key.pem", work / "test-demo-https"
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-keyout", str(key), "-out", str(cert), "-days", "1",
        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([
        "clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-DCOD2_X64=1", "-DCOD2_CODX=1",
        str(ROOT / "tests/cod2x/test_demo_https.c"), "-lcurl", "-o", str(binary),
    ], check=True)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Upload)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        body = b"Owned native demo fixture\x00\x01\x02" * 8192
        env = dict(os.environ, COD2X_TEST_CA=str(cert))
        for hostname, outcome in (("localhost", "success"), ("127.0.0.1", "failure")):
            directory = work / hostname
            directory.mkdir()
            demo = directory / "round.dm_1"
            marker = directory / "round.dm_1.upload"
            demo.write_bytes(body)
            marker.write_text(f"https://{hostname}:{server.server_port}/upload/round")
            subprocess.run([str(binary), str(directory), outcome], env=env, check=True)
            assert demo.read_bytes() == body
            assert marker.exists() == (outcome == "failure")
        assert Upload.received == [body], "bad hostname must never deliver demo bytes"
    finally:
        server.shutdown()
        server.server_close()
        worker.join()
print("CoD2x real HTTPS chunked POST and TLS hostname verification: pass")
