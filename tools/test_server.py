#!/usr/bin/env python3
"""Local test server for the downloader. Usage: test_server.py <port> <dir> [slow_bytes_per_sec]"""
import os, sys, time
from http.server import BaseHTTPRequestHandler, HTTPServer
port, root = int(sys.argv[1]), sys.argv[2]
slow = int(sys.argv[3]) if len(sys.argv) > 3 else 0
class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/trunc.bin":                      # promises 1000 bytes, sends 500, closes
            self.send_response(200); self.send_header("Content-Length", "1000"); self.end_headers()
            self.wfile.write(b"x" * 500); return
        if self.path == "/chunked.bin":
            self.send_response(200); self.send_header("Transfer-Encoding", "chunked"); self.end_headers()
            self.wfile.write(b"3\r\nabc\r\n0\r\n\r\n"); return
        if self.path == "/redirect.bin":
            self.send_response(302); self.send_header("Location", "/good.bin"); self.end_headers(); return
        p = os.path.join(root, self.path.lstrip("/"))
        if not os.path.isfile(p):
            self.send_response(404); self.end_headers(); return
        size = os.path.getsize(p)
        self.send_response(200); self.send_header("Content-Length", str(size)); self.end_headers()
        with open(p, "rb") as f:
            while True:
                b = f.read(65536 if not slow else slow // 10)
                if not b: break
                self.wfile.write(b)
                if slow: time.sleep(0.1)
HTTPServer(("127.0.0.1", port), H).serve_forever()
