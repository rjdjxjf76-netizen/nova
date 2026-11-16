#!/usr/bin/env python3
"""Fetch the NOVA game metadata catalog and write a safe metadata-only catalog.json.

The upstream DATA object uses package URLs as object keys. This tool deliberately
throws those keys away and writes only display metadata. It does not download
or expose package files.
"""
import argparse, json, urllib.request, os, sys

DEFAULT_URL = "https://raw.githubusercontent.com/rjdjxjf76-netizen/games-json/refs/heads/main/GAMES.json"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default=DEFAULT_URL)
    ap.add_argument("--out", default="catalog.json")
    args = ap.parse_args()
    req = urllib.request.Request(args.url, headers={"User-Agent": "NOVA-Catalog-Sync/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        raw = json.loads(r.read().decode("utf-8"))
    data = raw.get("DATA")
    if not isinstance(data, dict):
        raise SystemExit("Source JSON has no DATA object")
    items = []
    for record in data.values():
        if not isinstance(record, dict) or not record.get("name"):
            continue
        items.append({
            "name": str(record.get("name", "")),
            "type": "GAME",
            "version": str(record.get("version", "")),
            "size": str(record.get("size", "")),
            "publisher": "CATALOG",
            "description": "PS4 game metadata",
            "title_id": str(record.get("title_id", "")),
            "region": str(record.get("region", "")),
            "min_fw": str(record.get("min_fw", "")),
            "release": str(record.get("release", "")),
            "cover_url": str(record.get("cover_url", "")),
        })
    if not items:
        raise SystemExit("No valid game metadata found")
    tmp = args.out + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"items": items}, f, ensure_ascii=False, indent=2)
    os.replace(tmp, args.out)
    print("Wrote %d metadata entries to %s (package URLs excluded)" % (len(items), args.out))

if __name__ == "__main__":
    main()
