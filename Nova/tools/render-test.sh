#!/usr/bin/env bash
# Off-console UI check: builds src/ against a tiny software SDL stand-in, replays a
# scripted key sequence and writes /tmp/frames/fNN.ppm. Needs only g++ (no SDL2, no PS4 SDK).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/frames && rm -f /tmp/frames/*.ppm
g++ -std=c++17 -Wall -Wextra -Itools/stub_sdl -Isrc src/main.cpp src/platform_ps4.cpp src/http_download.cpp src/sha256.cpp src/catalog.cpp -o /tmp/market_render_test
/tmp/market_render_test
echo "Frames written to /tmp/frames (PPM). Convert with ImageMagick: convert f03.ppm f03.png"
