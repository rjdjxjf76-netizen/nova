#!/usr/bin/env python3
"""Build NOVA's metadata-only catalog.

The source DATA object uses package URLs as keys. Those keys are deliberately
discarded: this script exports display metadata only and never downloads or
exposes package files.
"""
import argparse
import json
import os
import urllib.request
import urllib.error

DEFAULT_URL = "https://raw.githubusercontent.com/rjdjxjf76-netizen/games-json/refs/heads/main/GAMES.json"


def load_source(url):
    req = urllib.request.Request(url, headers={"User-Agent": "NOVA-Catalog-Sync/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            print("Loaded catalog from upstream URL")
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        # The repository also carries a checked-in snapshot so builds remain
        # reproducible when the external catalog URL is unavailable.
        local_path = "GAMES.json"
        if not os.path.isfile(local_path):
            raise SystemExit("Upstream catalog unavailable and GAMES.json fallback is missing: %s" % exc)
        print("WARNING: upstream catalog unavailable (%s); using checked-in GAMES.json" % exc)
        with open(local_path, encoding="utf-8") as source_file:
            return json.load(source_file)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--out", default="catalog.json")
    args = parser.parse_args()

    raw = load_source(args.url)
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
            "cover_url": str(record.get("cover_url") or record.get("cover") or record.get("image") or record.get("thumbnail") or record.get("icon") or ""),
        })

    if not items:
        raise SystemExit("No valid game metadata found")

    output_dir = os.path.dirname(args.out)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    temporary_path = args.out + ".tmp"
    with open(temporary_path, "w", encoding="utf-8") as output_file:
        json.dump({"items": items}, output_file, ensure_ascii=False, indent=2)
    os.replace(temporary_path, args.out)
    print("Wrote %d metadata entries to %s (package URLs excluded)" % (len(items), args.out))


if __name__ == "__main__":
    main()
