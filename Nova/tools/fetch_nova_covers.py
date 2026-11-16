#!/usr/bin/env python3
"""Fetch catalog-provided cover art and package it as SDL-loadable BMP files."""
import argparse
import io
import json
import os
import urllib.request
from PIL import Image, ImageOps

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    with open(args.catalog, encoding="utf-8") as f:
        items = json.load(f).get("items", [])
    os.makedirs(args.out, exist_ok=True)
    ok = 0
    for index, item in enumerate(items):
        url = str(item.get("cover_url") or "").strip()
        if not url.startswith("https://"):
            continue
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "NOVA-Cover-Builder/1.0"})
            with urllib.request.urlopen(req, timeout=15) as response:
                raw = response.read(12 * 1024 * 1024 + 1)
            if len(raw) > 12 * 1024 * 1024:
                raise ValueError("image exceeds 12 MiB")
            with Image.open(io.BytesIO(raw)) as image:
                image = ImageOps.fit(image.convert("RGB"), (128, 128), method=Image.Resampling.LANCZOS)
                image = image.quantize(colors=64, method=Image.Quantize.FASTOCTREE)
                image.save(os.path.join(args.out, "%04d.bmp" % index), format="BMP")
            ok += 1
        except Exception as exc:
            print("Cover %d skipped: %s" % (index, exc))
    print("Bundled %d cover image(s) for %d catalog entries" % (ok, len(items)))

if __name__ == "__main__":
    main()
