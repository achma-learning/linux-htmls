#!/usr/bin/env python3
"""Static file server for the AI Hub, plus one extra endpoint: /open?id=<id>.

Why the extra endpoint exists
-----------------------------
A link in the hub cannot open an assistant in its own window. A browser opens
target="_blank" from an app window as a TAB in whatever ordinary browser window
already exists, on whatever workspace that window happens to be sitting on. No
new window is created, so no window-manager rule can place it — the AI
assistant ends up wherever general browsing lives.

So the page asks this server instead, and the server runs bin/ai-open, which
starts the assistant as its own --app window. The browser then derives that
window's WM_CLASS instance from the URL, and i3 can assign it to the AI
workspace.

Safety: this listens on the loopback interface only, and /open takes an ID, not
a URL. The only addresses it will ever launch are the ones already listed in
data/ai.json, so a stray request cannot make it open something arbitrary.
"""

import http.server
import json
import os
import subprocess
import sys
import urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AI_JSON = os.path.join(ROOT, "data", "ai.json")
AI_OPEN = os.path.join(ROOT, "bin", "ai-open")


def services():
    """Re-read on every request: editing ai.json should not need a restart."""
    try:
        with open(AI_JSON, encoding="utf-8") as fh:
            return {s["id"]: s for s in json.load(fh)}
    except (OSError, ValueError, KeyError, TypeError):
        return {}


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ROOT, **kw)

    def do_GET(self):
        parts = urllib.parse.urlsplit(self.path)
        if parts.path == "/open":
            self.handle_open(urllib.parse.parse_qs(parts.query))
            return
        super().do_GET()

    def handle_open(self, query):
        wanted = (query.get("id") or [""])[0]
        service = services().get(wanted)
        if service is None:
            self.send_error(404, "unknown assistant id")
            return
        try:
            subprocess.Popen(
                [AI_OPEN, service["url"]],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,       # outlive this server
            )
        except OSError as exc:
            self.send_error(500, "could not launch: %s" % exc)
            return
        self.send_response(204)
        self.end_headers()

    def log_message(self, *_):
        pass                                   # a launcher should be silent


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
