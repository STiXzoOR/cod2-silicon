#!/usr/bin/env python3
"""A stand-in for the website's share service, for the launcher's tests. Never the network.

Serves POST /api/configs/share and POST /api/configs/withdraw on 127.0.0.1 with scripted
replies, in the shapes of the share contract (_agents/r041-share-contract.md), so every state
of the Share sheets can be shown without GitHub or cod2unbound.com.

Start it, then point the launcher at it:

    python3 tools/config-directory/share_standin.py --port 0 --share sent --withdraw withdrawn
    # prints: share stand-in listening on http://127.0.0.1:53124
    COD2_SHARE_URL=http://127.0.0.1:53124 <launcher or test>

Share replies (--share, or one per request with --share sent,already,limit):
    sent         201 {"request", "url", "id", "token"}; request numbers count up from 128
    already      409 {"error": "already"}
    limit        429 {"error": "limit", "scope": "network"}
    limit-day    429 {"error": "limit", "scope": "day"}
    unavailable  503 {"error": "unavailable"}
    invalid      400 {"error": "invalid", "field": "config"}
    too-large    413 {"error": "invalid", "field": "config"}
    slow         waits --delay seconds (default 30), then answers as `sent`
    drop         closes the connection without an answer

Withdraw replies (--withdraw, same list form):
    withdrawn    200 {"state": "withdrawn"}
    removal      200 {"state": "removal", "url": ".../pull/<n+1>"}
    forbidden    403 {"error": "forbidden"}
    unavailable  503 {"error": "unavailable"}
    slow         waits --delay seconds, then answers as `withdrawn`

With a list, each request takes the next reply and the last one repeats. A request that is
not POST gets 405; a body that is not JSON with "v": 1 gets 400 {"error":"invalid","field":"v"}
whatever the script says, as the real service does. A test can change the script while the
stand-in runs: POST /standin {"share": "already", "withdraw": "removal"} (either key).

--record <file> appends every request as one JSON line ({"path", "body"}) so a test can check
what the launcher sent. Stop the stand-in with Ctrl-C or SIGTERM.
"""
import argparse
import hashlib
import hmac
import json
import signal
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SHARE = {"sent", "already", "limit", "limit-day", "unavailable", "invalid", "too-large", "slow", "drop"}
WITHDRAW = {"withdrawn", "removal", "forbidden", "unavailable", "slow", "drop"}
SECRET = b"share-standin"
PULLS = "https://github.com/cod2unbound/cod2-configs/pull/"


def script(value, allowed):
    steps = [step.strip() for step in value.split(",") if step.strip()]
    if not steps or any(step not in allowed for step in steps):
        raise argparse.ArgumentTypeError(f"choose from {', '.join(sorted(allowed))}")
    return steps


def slug(name):
    out, dash = "", False
    for c in name.lower():
        if "a" <= c <= "z" or "0" <= c <= "9":
            out, dash = out + c, False
        elif not dash:
            out, dash = out + "-", True
    return out.strip("-")[:40].rstrip("-") or "player"


class State:
    def __init__(self, share, withdraw, delay, record):
        self.lock = threading.Lock()
        self.scripts = {"share": share, "withdraw": withdraw}
        self.delay = delay
        self.record = record
        self.next_request = 128

    def step(self, kind):
        with self.lock:
            steps = self.scripts[kind]
            return steps.pop(0) if len(steps) > 1 else steps[0]


class Handler(BaseHTTPRequestHandler):
    server_version = "share-standin"
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        sys.stderr.write("share stand-in: " + (format % args) + "\n")

    def reply(self, status, body):
        data = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        if status == 405:
            self.send_header("Allow", "POST")
        self.end_headers()
        self.wfile.write(data)

    def not_post(self):
        self.reply(405, {"error": "method"})

    do_GET = do_PUT = do_DELETE = do_PATCH = do_HEAD = do_OPTIONS = not_post

    def do_POST(self):
        state = self.server.state
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b""
        try:
            body = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            body = None
        path = self.path.split("?")[0]
        if state.record:
            with state.lock, open(state.record, "a", encoding="utf-8") as log:
                log.write(json.dumps({"path": path, "body": body}) + "\n")
        if path == "/standin":
            try:
                with state.lock:
                    for kind, allowed in (("share", SHARE), ("withdraw", WITHDRAW)):
                        if isinstance(body, dict) and kind in body:
                            state.scripts[kind] = script(str(body[kind]), allowed)
            except argparse.ArgumentTypeError as error:
                return self.reply(400, {"error": str(error)})
            return self.reply(200, {"share": state.scripts["share"], "withdraw": state.scripts["withdraw"]})
        if path not in ("/api/configs/share", "/api/configs/withdraw"):
            return self.reply(404, {"error": "not found"})
        if not isinstance(body, dict) or body.get("v") != 1:
            return self.reply(400, {"error": "invalid", "field": "v"})
        if path == "/api/configs/share":
            self.share(state, body)
        else:
            self.withdraw(state, body)

    def drop(self):
        self.close_connection = True
        self.connection.close()

    def share(self, state, body):
        step = state.step("share")
        if step == "slow":
            time.sleep(state.delay)
            step = "sent"
        if step == "drop":
            return self.drop()
        if step == "sent":
            with state.lock:
                number = state.next_request
                state.next_request += 1
            config = body.get("config") if isinstance(body.get("config"), str) else ""
            digest = hashlib.sha256(config.encode("utf-8")).hexdigest()
            token = hmac.new(SECRET, f"{number}:{digest}".encode(), hashlib.sha256).hexdigest()
            name = body.get("name") if isinstance(body.get("name"), str) else ""
            return self.reply(201, {"request": number, "url": PULLS + str(number), "id": slug(name), "token": token})
        replies = {
            "already": (409, {"error": "already"}),
            "limit": (429, {"error": "limit", "scope": "network"}),
            "limit-day": (429, {"error": "limit", "scope": "day"}),
            "unavailable": (503, {"error": "unavailable"}),
            "invalid": (400, {"error": "invalid", "field": "config"}),
            "too-large": (413, {"error": "invalid", "field": "config"}),
        }
        self.reply(*replies[step])

    def withdraw(self, state, body):
        step = state.step("withdraw")
        if step == "slow":
            time.sleep(state.delay)
            step = "withdrawn"
        if step == "drop":
            return self.drop()
        number = body.get("request") if isinstance(body.get("request"), int) else 0
        replies = {
            "withdrawn": (200, {"state": "withdrawn"}),
            "removal": (200, {"state": "removal", "url": PULLS + str(number + 1)}),
            "forbidden": (403, {"error": "forbidden"}),
            "unavailable": (503, {"error": "unavailable"}),
        }
        self.reply(*replies[step])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", type=int, default=0, help="0 picks a free port (printed)")
    parser.add_argument("--share", type=lambda v: script(v, SHARE), default=["sent"])
    parser.add_argument("--withdraw", type=lambda v: script(v, WITHDRAW), default=["withdrawn"])
    parser.add_argument("--delay", type=float, default=30.0, help="seconds a `slow` reply waits")
    parser.add_argument("--record", help="append each request to this file as a JSON line")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.daemon_threads = True
    server.state = State(args.share, args.withdraw, args.delay, args.record)
    signal.signal(signal.SIGTERM, lambda *_: threading.Thread(target=server.shutdown).start())
    print(f"share stand-in listening on http://127.0.0.1:{server.server_address[1]}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
