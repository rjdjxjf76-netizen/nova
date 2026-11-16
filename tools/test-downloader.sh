#!/usr/bin/env bash
# Tests SHA-256 against Python's hashlib and the downloader against a local server.
set -uo pipefail
cd "$(dirname "$0")/.."
W=$(mktemp -d); trap 'kill $SRV 2>/dev/null; rm -rf "$W"' EXIT
g++ -std=c++17 -Wall -Wextra -Isrc tools/dl_test.cpp src/http_download.cpp src/sha256.cpp -o "$W/dl_test" || exit 1
fail=0; ok(){ echo "PASS  $1"; }; bad(){ echo "FAIL  $1"; fail=1; }

# 1) SHA-256 vs hashlib on boundary sizes
for n in 0 1 55 56 57 63 64 65 119 120 128 1000 1000003; do
  head -c $n /dev/urandom > "$W/s.bin"
  a=$("$W/dl_test" --sha "$W/s.bin"); b=$(python3 -c "import hashlib,sys;print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$W/s.bin")
  [ "$a" = "$b" ] || { bad "sha256 size $n"; }
done; ok "sha256 matches hashlib on 13 sizes"

# 2) downloader vs local server
mkdir "$W/www"; head -c 9437211 /dev/urandom > "$W/www/good.bin"
GOOD=$(python3 -c "import hashlib;print(hashlib.sha256(open('$W/www/good.bin','rb').read()).hexdigest())")
python3 tools/test_server.py 18080 "$W/www" & SRV=$!; sleep 1
U=http://127.0.0.1:18080
run(){ "$W/dl_test" "$@" 2>&1; }
r=$(run $U/good.bin "$W/out.bin" $GOOD); echo "$r" | grep -q '^DONE' && cmp -s "$W/out.bin" "$W/www/good.bin" && [ ! -e "$W/out.bin.part" ] && ok "good download, bytes identical, no .part left" || bad "good download: $r"
r=$(run $U/good.bin "$W/o2.bin" $(printf '0%.0s' {1..64})); echo "$r" | grep -q 'HASH MISMATCH' && [ ! -e "$W/o2.bin" ] && [ ! -e "$W/o2.bin.part" ] && ok "wrong hash rejected, nothing left on disk" || bad "wrong hash: $r"
r=$(run $U/good.bin "$W/o3.bin" $(echo $GOOD | tr a-f A-F)); echo "$r" | grep -q '^DONE' && ok "uppercase expected hash accepted" || bad "uppercase hash: $r"
r=$(run $U/missing.bin "$W/o4.bin" $GOOD); echo "$r" | grep -q 'SERVER REPLIED 404' && [ ! -e "$W/o4.bin.part" ] && ok "404 refused" || bad "404: $r"
r=$(run $U/redirect.bin "$W/o5.bin" $GOOD); echo "$r" | grep -q 'SERVER REPLIED 302' && ok "redirect refused" || bad "redirect: $r"
r=$(run $U/trunc.bin "$W/o6.bin" $GOOD); echo "$r" | grep -q 'CLOSED EARLY' && [ ! -e "$W/o6.bin.part" ] && ok "truncated body refused" || bad "trunc: $r"
r=$(run $U/chunked.bin "$W/o7.bin" $GOOD); echo "$r" | grep -q 'CHUNKED' && ok "chunked refused" || bad "chunked: $r"
r=$(run http://127.0.0.1:1/x.bin "$W/o8.bin" $GOOD); echo "$r" | grep -q 'REFUSED OR FAILED' && ok "connection refused handled" || bad "refused: $r"
r=$(run https://127.0.0.1:18080/good.bin "$W/o9.bin" $GOOD); echo "$r" | grep -q 'HTTPS NOT SUPPORTED' && ok "https refused with clear message" || bad "https: $r"
r=$(run $U/good.bin "$W/o10.bin" ""); echo "$r" | grep -q 'SHA-256 IS REQUIRED' && ok "missing hash refused" || bad "no hash: $r"
r=$(run $U/good.bin "/nonexistent_dir/o.bin" $GOOD); echo "$r" | grep -q 'CANNOT OPEN OUTPUT' && ok "unwritable output dir handled" || bad "bad dir: $r"
[ $fail = 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
