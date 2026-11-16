#!/usr/bin/env python3
"""Build catalog.json from a folder of .pkg files you are authorized to distribute.

  make_catalog.py PKG_DIR --base-url http://192.168.1.50:8080 [--out catalog.json]

Optional sidecar PKG_DIR/<file stem>.json may set: name, type, version, publisher, description.
The SHA-256 and size are computed from the files, so the app can verify what it downloads.
"""
import argparse, hashlib, json, os, sys, urllib.parse

def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkg_dir"); ap.add_argument("--base-url", required=True)
    ap.add_argument("--out", default="catalog.json")
    a = ap.parse_args()
    if not a.base_url.startswith("http://"):
        print("note: the PS4 app only supports http:// URLs", file=sys.stderr)
    items = []
    for fn in sorted(os.listdir(a.pkg_dir)):
        if not fn.lower().endswith(".pkg"): continue
        path = os.path.join(a.pkg_dir, fn); stem = fn[:-4]
        meta = {}
        side = os.path.join(a.pkg_dir, stem + ".json")
        if os.path.isfile(side):
            with open(side, encoding="utf-8") as f: meta = json.load(f)
        items.append({
            "name": meta.get("name") or stem.replace("_", " ").replace("-", " ").title(),
            "type": meta.get("type", "APP"),
            "version": meta.get("version", "UNKNOWN"),
            "size": "%.1f MB" % (os.path.getsize(path) / 1048576),
            "publisher": meta.get("publisher", "UNKNOWN"),
            "description": meta.get("description", ""),
            "url": a.base_url.rstrip("/") + "/" + urllib.parse.quote(fn),
            "sha256": sha256_of(path),
        })
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump({"items": items}, f, indent=2)
    print("wrote %s with %d item(s). Only list packages you are authorized to distribute." % (a.out, len(items)))

main()
