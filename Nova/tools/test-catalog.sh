#!/usr/bin/env bash
# Tests the catalog parser, make_catalog.py, serve_catalog.py and a catalog-driven download.
set -uo pipefail
cd "$(dirname "$0")/.."
W=$(mktemp -d); trap 'kill $SRV 2>/dev/null; rm -rf "$W"' EXIT
g++ -std=c++17 -Wall -Wextra -Isrc tools/catalog_test.cpp src/catalog.cpp -o "$W/ct" || exit 1
g++ -std=c++17 -Isrc tools/dl_test.cpp src/http_download.cpp src/sha256.cpp -o "$W/dl" || exit 1
fail=0; ok(){ echo "PASS  $1"; }; bad(){ echo "FAIL  $1"; fail=1; }

# generate a catalog from real files
mkdir "$W/www"; head -c 3145728 /dev/urandom > "$W/www/cool_game.pkg"; head -c 1000 /dev/urandom > "$W/www/my-tool.pkg"
echo '{"name":"Cool Game (Demo)","type":"GAME","version":"1.2.0","publisher":"Me","description":"A test game with a fairly long description so that wrapping on the details screen gets exercised properly."}' > "$W/www/cool_game.json"
python3 tools/make_catalog.py "$W/www" --base-url http://127.0.0.1:18082 --out "$W/www/catalog.json" >/dev/null && ok "make_catalog.py ran" || bad "make_catalog"
r=$("$W/ct" "$W/www/catalog.json"); echo "$r" | head -1 | grep -q '^OK 2$' && ok "parser reads generated catalog (2 items)" || bad "parse generated: $r"
echo "$r" | grep -q '^Cool Game (Demo)|GAME|3.0 MB|' && echo "$r" | grep -q '^My Tool|APP|' && ok "metadata + sidecar + filename-derived names correct" || bad "fields: $r"

# serve + catalog-driven download, verified against the hash in the catalog
python3 tools/serve_catalog.py "$W/www" 18082 >/dev/null 2>&1 & SRV=$!; sleep 1
line=$(echo "$r" | sed -n 2p); url=$(echo "$line" | cut -d'|' -f4); sha=$(echo "$line" | cut -d'|' -f5)
out=$("$W/dl" "$url" "$W/got.pkg" "$sha"); echo "$out" | grep -q '^DONE' && cmp -s "$W/got.pkg" "$W/www/cool_game.pkg" && ok "download via catalog URL + hash is byte-identical" || bad "catalog download: $out"
out=$("$W/dl" "$url" "$W/got2.pkg" "$(printf 'a%.0s' {1..64})"); echo "$out" | grep -q 'HASH MISMATCH' && ok "tampered/wrong catalog hash is rejected" || bad "wrong hash: $out"

# malformed / hostile catalogs must fail cleanly, never crash
t(){ printf '%s' "$2" > "$W/c.json"; o=$("$W/ct" "$W/c.json" 2>&1); [ $? -eq 1 ] && echo "$o" | grep -q "^ERR" && ok "rejects: $1" || bad "$1 -> $o"; }
t "empty file" ""
t "truncated json" '{"items":[{"name":"A"'
t "bad escape" '{"items":[{"name":"A\q"}]}'
t "missing name" '{"items":[{"type":"GAME"}]}'
t "item not an object" '{"items":[1]}'
t "no items array" '{"foo":1}'
t "trailing garbage" '{"items":[]} xyz'
t "deep nesting" "$(python3 -c "print('['*200+']'*200)")"
t "raw control char in string" $'{"items":[{"name":"A\tB"}]}'
python3 -c "print('{\"items\":[' + ','.join('{\"name\":\"n%d\"}'%i for i in range(3001)) + ']}')" > "$W/big.json"; o=$("$W/ct" "$W/big.json"); echo "$o" | grep -q 'TOO MANY ITEMS' && ok "rejects 3001 items" || bad "501 items: $o"
head -c 9000000 /dev/zero | tr '\0' 'x' > "$W/huge.json"; o=$("$W/ct" "$W/huge.json"); echo "$o" | grep -q '^ERR' && ok "rejects >8 MB file" || bad "huge: $o"
cat > "$W/data.json" <<'JSON'
{"DATA":{"https://example.invalid/should-not-be-used.pkg":{"title_id":"CUSA12345","region":"USA","name":"Metadata Game","version":"01.02","release":"11-15-2013","size":1234567890,"min_fw":"9.00","cover_url":"https://example.invalid/cover.png"}}}
JSON
o=$("$W/ct" "$W/data.json"); echo "$o" | grep -q '^OK 1$' && echo "$o" | grep -q '^Metadata Game|GAME|1.1 GB||$' && ok "GAMES.json DATA format imports metadata only (no package URL/hash)" || bad "DATA format: $o"
printf '{"items":[]}' > "$W/e.json"; o=$("$W/ct" "$W/e.json"); [ "$o" = "OK 0" ] && ok "empty items list is valid" || bad "empty list: $o"
printf '[{"name":"Bare Array"}]' > "$W/b.json"; o=$("$W/ct" "$W/b.json"); echo "$o" | head -1 | grep -q '^OK 1$' && ok "bare top-level array accepted" || bad "bare array: $o"
o=$("$W/ct" "$W/nonexistent.json"); echo "$o" | grep -q 'NOT FOUND' && ok "missing file reported" || bad "missing: $o"
[ $fail = 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
