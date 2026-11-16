#!/usr/bin/env bash
set -euo pipefail
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to your OpenOrbis toolchain directory}"
: "${PS4_CMAKE_TOOLCHAIN_FILE:?Set PS4_CMAKE_TOOLCHAIN_FILE to the cross toolchain file for your installed environment}"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$PS4_CMAKE_TOOLCHAIN_FILE" -DCMAKE_PREFIX_PATH="$OO_PS4_TOOLCHAIN"
cmake --build build --config Release
echo "Native binary build completed if all configured dependencies were found."
echo "PKG creation is a separate step; see pkg/README.md."
