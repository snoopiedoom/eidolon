"""One-shot local SSE endpoint for the optional WinHTTP provider probe."""

import argparse
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self):  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path != "/event":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        event = {
            "type": "session.next.text.delta",
            "properties": {
                "sessionID": "mock-session",
                "assistantMessageID": "mock-message",
                "delta": "hello from sse",
            },
        }
        self.wfile.write(f"data: {json.dumps(event)}\n\n".encode())
        self.wfile.flush()
        time.sleep(30)

    def log_message(self, format, *args):
        del format, args


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=4099)
    args = parser.parse_args()
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
