#!/usr/bin/env python3
"""Serve a folder (catalog + .pkg files) over plain HTTP on your LAN.

  serve_catalog.py DIR [port]      (default port 8080; stop with Ctrl+C)
Only run this on a network you trust: it serves the whole folder to anyone who can reach it.
"""
import functools, socket, sys
from http.server import HTTPServer, SimpleHTTPRequestHandler
d = sys.argv[1]; port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
class H(SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.0"      # the console app does not support chunked/keep-alive
try:
    ip = socket.gethostbyname(socket.gethostname())
except OSError:
    ip = "<this computer's LAN IP>"
print("Serving %s on port %d. From the PS4 use http://%s:%d/<file>" % (d, port, ip, port))
HTTPServer(("0.0.0.0", port), functools.partial(H, directory=d)).serve_forever()
