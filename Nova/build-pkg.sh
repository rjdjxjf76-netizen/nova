#!/usr/bin/env bash
# Preflight + build for the OpenOrbis path. Produces build/IV0000-PSMK00001_00-PS4MARKET0000001.pkg
#   export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain     (release v0.5.4, toolchain-llvm-18.tar.gz)
#   ./build-pkg.sh
# Optional: SSL11_LIB=<dir with libssl.so.1.1 + libcrypto.so.1.1>, needed on distros that only have OpenSSL 3.
set -euo pipefail
cd "$(dirname "$0")"
miss=0; need(){ [ -e "$1" ] || { echo "MISSING: $2 ($1)"; miss=1; }; }
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to your OpenOrbis toolchain directory}"
need "$OO_PS4_TOOLCHAIN/bin/linux/create-fself"   "toolchain create-fself"
need "$OO_PS4_TOOLCHAIN/bin/linux/create-gp4"     "toolchain create-gp4"
need "$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core"   "toolchain PkgTool.Core"
need "$OO_PS4_TOOLCHAIN/link.x"                   "toolchain linker script"
need "$OO_PS4_TOOLCHAIN/include/SDL2/SDL.h"       "SDL2 headers (ship inside the toolchain)"
need "$OO_PS4_TOOLCHAIN/lib/libSDL2.a"            "libSDL2.a (ships inside the toolchain)"
need "$OO_PS4_TOOLCHAIN/samples/SDL2/sce_module/libc.prx" "runtime module from the toolchain's SDL2 sample"
need "pkg/sce_sys/icon0.png" "icon"
command -v clang++ >/dev/null || { echo "MISSING: clang++ (LLVM 18)"; miss=1; }
command -v ld.lld  >/dev/null || { echo "MISSING: ld.lld (LLVM 18)";  miss=1; }
[ $miss = 0 ] || { echo "Fix the items above, then rerun."; exit 1; }
chmod +x "$OO_PS4_TOOLCHAIN"/bin/linux/* 2>/dev/null || true
make -f Makefile.ps4 pkg
