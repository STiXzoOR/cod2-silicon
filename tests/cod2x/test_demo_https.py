#!/usr/bin/env python3
"""Real local TLS upload fixture; it never changes the system trust store."""
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]


class Upload(http.server.BaseHTTPRequestHandler):
    received = []
    elapsed = []

    def do_POST(self):
        assert self.path in ("/upload/round", "/upload/round#fragment", "/status200", "/status409", "/redirect", "/timeout", "/rate")
        assert self.headers["Content-Type"] == "application/octet-stream"
        assert self.headers["Transfer-Encoding"] == "chunked"
        assert self.headers["Host"] == "localhost"
        assert self.headers.get("Authorization") is None
        assert self.headers.get("Expect") is None
        assert self.headers.get("Accept") is None
        started = time.monotonic()
        body = bytearray()
        while True:
            length = int(self.rfile.readline().strip(), 16)
            if not length:
                self.rfile.readline()
                break
            body.extend(self.rfile.read(length))
            assert self.rfile.read(2) == b"\r\n"
        self.received.append((self.path, bytes(body)))
        self.elapsed.append((self.path, time.monotonic() - started))
        if self.path == "/timeout":
            self.connection.settimeout(4)
            assert self.connection.recv(1) == b""
            return
        self.send_response({"/status200": 200, "/status409": 409, "/redirect": 302}.get(self.path, 201))
        if self.path == "/redirect":
            self.send_header("Location", "/must-not-follow")
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
        cases = (("localhost", "success", "/upload/round", 201),
                 ("127.0.0.1", "failure", "/upload/round", 0),
                 ("user:password@localhost", "success", "/upload/round#fragment", 201),
                 ("localhost", "success", "/status200", 200),
                 ("localhost", "success", "/status409", 409),
                 ("localhost", "failure", "/redirect", 302),
                 ("localhost", "timeout", "/timeout", 0),
                 ("localhost", "success", "/rate", 201))
        for number, (hostname, outcome, path, status) in enumerate(cases):
            directory = work / str(number)
            directory.mkdir()
            demo = directory / "round.dm_1"
            marker = directory / "round.dm_1.upload"
            contents = b"X" * (10 * 1024 * 1024) if path == "/rate" else body
            demo.write_bytes(contents)
            marker.write_text(f"https://{hostname}:{server.server_port}{path}")
            started = time.monotonic()
            subprocess.run([str(binary), str(directory), outcome, str(status)], env=env, check=True)
            assert demo.read_bytes() == contents
            assert marker.exists() == (outcome != "success")
            if outcome == "timeout":
                assert time.monotonic() - started >= 3
        assert sum(path == "/upload/round" for path, _ in Upload.received) == 1, "bad hostname must never deliver demo bytes"
        assert sum(path == "/redirect" for path, _ in Upload.received) == 3
        assert all(data == (b"X" * (10 * 1024 * 1024) if path == "/rate" else body) for path, data in Upload.received)
        rate_time = next(elapsed for path, elapsed in Upload.elapsed if path == "/rate")
        assert rate_time >= 1.8, f"10 MiB exceeded 5 MiB/s limit: {rate_time:.3f}s"
        print(f"10 MiB upload in {rate_time:.3f}s; 200/201/409 accepted; 302 not followed; timeout retries: pass")
    finally:
        server.shutdown()
        server.server_close()
        worker.join()
print("CoD2x real HTTPS chunked POST and TLS hostname verification: pass")
